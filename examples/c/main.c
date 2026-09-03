/*
 * RuleFlow integration example — C, using libcurl for HTTP.
 *
 * 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
 * tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.
 *
 * A full JSON parser is a lot of ceremony for a short example, so this file
 * keeps JSON handling deliberately minimal:
 *   - request bodies are assembled as plain strings;
 *   - the decisions[0] object is pulled out of the shared fixture with a
 *     small brace-matching scan (not a real parser — it just finds the
 *     "decisions" array and returns its first balanced {...} object);
 *   - the access_token field is pulled out of the token response with a
 *     tiny substring scan;
 *   - the /whoami and /engine/simulate responses are printed raw.
 * Production code should use a real JSON library, e.g. cJSON
 * (https://github.com/DaveGamble/cJSON).
 */

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct buffer {
    char *data;
    size_t len;
};

static size_t write_cb(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    struct buffer *buf = (struct buffer *)userp;
    char *ptr = realloc(buf->data, buf->len + total + 1);
    if (!ptr) {
        return 0;
    }
    buf->data = ptr;
    memcpy(buf->data + buf->len, contents, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

static const char *env_or(const char *key, const char *fallback) {
    const char *v = getenv(key);
    return (v && *v) ? v : fallback;
}

/* Very small, deliberately naive scanner: finds "access_token":"<value>" in
 * a JSON response and copies <value> into out (up to out_size). Not a real
 * JSON parser — see the file header comment. */
static int extract_access_token(const char *json, char *out, size_t out_size) {
    const char *key = "\"access_token\"";
    const char *p = strstr(json, key);
    if (!p) {
        return 0;
    }
    p = strchr(p + strlen(key), ':');
    if (!p) {
        return 0;
    }
    p++;
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p != '"') {
        return 0;
    }
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 1;
}

/* Reads a whole file into a NUL-terminated malloc'd buffer. Caller frees. */
static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Extracts the substring for decisions[0] out of the raw shared-fixture
 * JSON text: finds the "decisions" array and returns its first top-level
 * {...} object (brace-matched). Caller frees the returned string. Good
 * enough here because this fixture never nests a '{' inside a string
 * value — see the file header comment for why we don't do this properly. */
static char *extract_first_decision(const char *json) {
    const char *key = "\"decisions\"";
    const char *p = strstr(json, key);
    if (!p) {
        return NULL;
    }
    p = strchr(p, '[');
    if (!p) {
        return NULL;
    }
    p = strchr(p, '{');
    if (!p) {
        return NULL;
    }

    const char *start = p;
    int depth = 0;
    for (; *p; p++) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                size_t len = (size_t)(p - start) + 1;
                char *out = malloc(len + 1);
                if (!out) {
                    return NULL;
                }
                memcpy(out, start, len);
                out[len] = '\0';
                return out;
            }
        }
    }
    return NULL;
}

/* Performs one HTTP request and captures the response body + status.
 * method is one of "GET", "POST_FORM" (application/x-www-form-urlencoded
 * body), or "POST_JSON" (application/json body). token may be NULL. */
static struct buffer http_request(CURL *curl, const char *url, const char *method,
                                   const char *token, const char *body, long *status) {
    struct buffer buf;
    buf.data = malloc(1);
    buf.data[0] = '\0';
    buf.len = 0;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    struct curl_slist *headers = NULL;
    char auth_header[4096];
    if (token) {
        snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
        headers = curl_slist_append(headers, auth_header);
    }

    if (strcmp(method, "POST_FORM") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    } else if (strcmp(method, "POST_JSON") == 0) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    if (headers) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "request error: %s\n", curl_easy_strerror(res));
        if (headers) {
            curl_slist_free_all(headers);
        }
        free(buf.data);
        exit(1);
    }
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);

    if (headers) {
        curl_slist_free_all(headers);
    }
    return buf;
}

int main(void) {
    const char *ruleflow_api = env_or("RULEFLOW_API", "https://api.ruleflow.com.br/api");
    const char *ruleflow_auth = env_or("RULEFLOW_AUTH", "https://auth.ruleflow.com.br");
    const char *ruleflow_realm = env_or("RULEFLOW_REALM", "ruleflow");
    const char *ruleflow_client = env_or("RULEFLOW_CLIENT", "ruleflow-cli");
    const char *ruleflow_project = env_or("RULEFLOW_PROJECT", "lending");
    (void)ruleflow_project; /* referenced only by the commented next-step examples below */

    const char *username = getenv("RULEFLOW_USER");
    const char *password = getenv("RULEFLOW_PASSWORD");
    if (!username || !*username || !password || !*password) {
        fprintf(stderr, "Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment.\n");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl) {
        fprintf(stderr, "curl_easy_init failed\n");
        return 1;
    }

    /* 1) Get a token. */
    char token_url[512];
    snprintf(token_url, sizeof(token_url), "%s/realms/%s/protocol/openid-connect/token",
             ruleflow_auth, ruleflow_realm);

    char *username_esc = curl_easy_escape(curl, username, 0);
    char *password_esc = curl_easy_escape(curl, password, 0);
    char *client_esc = curl_easy_escape(curl, ruleflow_client, 0);

    char form[4096];
    snprintf(form, sizeof(form), "grant_type=password&client_id=%s&username=%s&password=%s",
             client_esc, username_esc, password_esc);
    curl_free(username_esc);
    curl_free(password_esc);
    curl_free(client_esc);

    long status = 0;
    struct buffer token_resp = http_request(curl, token_url, "POST_FORM", NULL, form, &status);
    if (status != 200) {
        fprintf(stderr, "ROPC failed: %ld %s\n", status, token_resp.data);
        free(token_resp.data);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    char access_token[4096];
    if (!extract_access_token(token_resp.data, access_token, sizeof(access_token))) {
        fprintf(stderr, "could not find access_token in response: %s\n", token_resp.data);
        free(token_resp.data);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    free(token_resp.data);
    printf("Got a token.\n");

    /* 2) GET /whoami — print the raw JSON response (contains tenant + roles). */
    char whoami_url[512];
    snprintf(whoami_url, sizeof(whoami_url), "%s/whoami", ruleflow_api);
    struct buffer whoami_resp = http_request(curl, whoami_url, "GET", access_token, NULL, &status);
    if (status != 200) {
        fprintf(stderr, "whoami failed: %ld %s\n", status, whoami_resp.data);
        free(whoami_resp.data);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    printf("whoami: %s\n", whoami_resp.data);
    free(whoami_resp.data);

    /* 3) POST /engine/simulate with the shared decision. Run this example
     * from examples/c so the shared fixture resolves as a sibling of this
     * directory. */
    char *shared_json = read_file("../shared/loan_eligibility.json");
    if (!shared_json) {
        fprintf(stderr, "could not read ../shared/loan_eligibility.json "
                         "(run this example from examples/c)\n");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    char *decision = extract_first_decision(shared_json);
    free(shared_json);
    if (!decision) {
        fprintf(stderr, "could not find decisions[0] in shared fixture\n");
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }

    size_t body_size = strlen(decision) + 128;
    char *simulate_body = malloc(body_size);
    snprintf(simulate_body, body_size,
             "{\"decision\": %s, \"inputs\": {\"credit_score\":780,\"income\":80000,\"age\":40}}",
             decision);
    free(decision);

    char simulate_url[512];
    snprintf(simulate_url, sizeof(simulate_url), "%s/engine/simulate", ruleflow_api);
    struct buffer sim_resp = http_request(curl, simulate_url, "POST_JSON", access_token, simulate_body, &status);
    free(simulate_body);
    if (status != 200) {
        fprintf(stderr, "simulate failed: %ld %s\n", status, sim_resp.data);
        free(sim_resp.data);
        curl_easy_cleanup(curl);
        curl_global_cleanup();
        return 1;
    }
    /* Raw response — a real integration would parse this with a JSON
     * library (e.g. cJSON) and print just the "outputs" field. */
    printf("outputs (raw response): %s\n", sim_resp.data);
    free(sim_resp.data);

    curl_easy_cleanup(curl);
    curl_global_cleanup();

    /* --- Next steps (commented — request shape only, not executed) -------
     *
     * Run a *stored* decision (persisted in a project) synchronously:
     *
     *   char url[512];
     *   snprintf(url, sizeof(url), "%s/projects/%s/decisions/<decisionId>/simulate",
     *            ruleflow_api, ruleflow_project);
     *   struct buffer r = http_request(curl, url, "POST_JSON", access_token,
     *       "{\"credit_score\":780,\"income\":80000,\"age\":40}", &status);
     *
     * Start a workflow execution asynchronously (202 + execution id), then poll:
     *
     *   snprintf(url, sizeof(url), "%s/projects/%s/executions", ruleflow_api, ruleflow_project);
     *   struct buffer started = http_request(curl, url, "POST_JSON", access_token,
     *       "{\"workflow\":\"<name>\",\"env\":\"dev\","
     *       "\"input\":{\"credit_score\":780,\"income\":80000,\"age\":40}}", &status);
     *   // ... extract the execution id from started.data, then:
     *   snprintf(url, sizeof(url), "%s/projects/%s/executions/%s",
     *            ruleflow_api, ruleflow_project, exec_id);
     *   struct buffer polled = http_request(curl, url, "GET", access_token, NULL, &status);
     */

    return 0;
}
