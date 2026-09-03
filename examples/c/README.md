# C example

Runs the RuleFlow credit decision (`loan_eligibility`) using
[libcurl](https://curl.se/libcurl/) for HTTP. There's no JSON library
dependency: request bodies are assembled as strings, the shared fixture's
`decisions[0]` object is pulled out with a small brace-matching scan, and
the `/whoami` and `/engine/simulate` responses are printed raw (see the
comment at the top of `main.c`). Production code should use a real JSON
library, e.g. [cJSON](https://github.com/DaveGamble/cJSON).

## Prerequisites

- A C compiler (`cc`/`gcc`/`clang`)
- **libcurl development headers**, which is the one non-stdlib prerequisite
  this example has:
  - Debian/Ubuntu: `sudo apt-get install libcurl4-openssl-dev`
  - Fedora/RHEL: `sudo dnf install libcurl-devel`
  - macOS: libcurl ships with the Xcode Command Line Tools
    (`xcode-select --install`); Homebrew's `curl` formula also works if you
    need a newer version
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
make && ./rf-example
```

Run it from this directory (`examples/c`) — the shared fixture is read as
`../shared/loan_eligibility.json`.

## Expected output

```
Got a token.
whoami: {"tenant":"<your-tenant>","roles":["business-analyst"]}
outputs (raw response): {"outputs":{"risk_tier":"LOW","approved":true,"credit_limit":24000},"trace":[...]}
```

The `/whoami` and `/engine/simulate` lines print the raw JSON response body
rather than pulling out individual fields, since this example doesn't link
a JSON library — see `main.c` for why.

## What it does

1. `http_request(curl, token_url, "POST_FORM", NULL, form, &status)` gets a
   bearer token via the Resource Owner Password grant; `extract_access_token()`
   pulls `access_token` out of the response with a minimal substring scan.
2. `http_request(curl, whoami_url, "GET", access_token, NULL, &status)`
   prints the raw `/whoami` response (tenant + roles).
3. `extract_first_decision()` pulls the
   [`loan_eligibility`](../shared/loan_eligibility.json) decision object out
   of the shared fixture, and `http_request(curl, simulate_url, "POST_JSON",
   access_token, simulate_body, &status)` runs it with inputs
   `{"credit_score":780,"income":80000,"age":40}`, printing the raw response.

The bottom of `main.c` has a commented block showing the next-step request
shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
