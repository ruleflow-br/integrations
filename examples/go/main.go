// Command main is a RuleFlow integration example — Go, standard library only
// (net/http + encoding/json).
//
// 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
// tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.
package main

import (
	"bytes"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"os"
	"path/filepath"
)

func envOr(key, fallback string) string {
	if v := os.Getenv(key); v != "" {
		return v
	}
	return fallback
}

var (
	ruleflowAPI     = envOr("RULEFLOW_API", "https://api.ruleflow.com.br/api")
	ruleflowAuth    = envOr("RULEFLOW_AUTH", "https://auth.ruleflow.com.br")
	ruleflowRealm   = envOr("RULEFLOW_REALM", "ruleflow")
	ruleflowClient  = envOr("RULEFLOW_CLIENT", "ruleflow-cli")
	ruleflowProject = envOr("RULEFLOW_PROJECT", "lending")
)

// ropc performs the Resource Owner Password grant against the public client
// ruleflow-cli and returns the access token.
func ropc(username, password string) (string, error) {
	form := url.Values{
		"grant_type": {"password"},
		"client_id":  {ruleflowClient},
		"username":   {username},
		"password":   {password},
	}
	tokenURL := fmt.Sprintf("%s/realms/%s/protocol/openid-connect/token", ruleflowAuth, ruleflowRealm)
	resp, err := http.PostForm(tokenURL, form)
	if err != nil {
		return "", err
	}
	defer resp.Body.Close()
	body, err := io.ReadAll(resp.Body)
	if err != nil {
		return "", err
	}
	if resp.StatusCode != http.StatusOK {
		return "", fmt.Errorf("ROPC failed: %d %s", resp.StatusCode, string(body))
	}
	var parsed struct {
		AccessToken string `json:"access_token"`
	}
	if err := json.Unmarshal(body, &parsed); err != nil {
		return "", err
	}
	return parsed.AccessToken, nil
}

// apiCall calls the RuleFlow control-plane API and returns the status code
// plus the raw response body.
func apiCall(method, path, token string, body interface{}) (int, []byte, error) {
	var reqBody io.Reader
	if body != nil {
		data, err := json.Marshal(body)
		if err != nil {
			return 0, nil, err
		}
		reqBody = bytes.NewReader(data)
	}
	req, err := http.NewRequest(method, ruleflowAPI+path, reqBody)
	if err != nil {
		return 0, nil, err
	}
	req.Header.Set("Authorization", "Bearer "+token)
	if body != nil {
		req.Header.Set("Content-Type", "application/json")
	}
	resp, err := http.DefaultClient.Do(req)
	if err != nil {
		return 0, nil, err
	}
	defer resp.Body.Close()
	respBody, err := io.ReadAll(resp.Body)
	if err != nil {
		return 0, nil, err
	}
	return resp.StatusCode, respBody, nil
}

type decisionModel struct {
	Decisions []json.RawMessage `json:"decisions"`
}

func main() {
	username := os.Getenv("RULEFLOW_USER")
	password := os.Getenv("RULEFLOW_PASSWORD")
	if username == "" || password == "" {
		fmt.Fprintln(os.Stderr, "Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment.")
		os.Exit(1)
	}

	// 1) Get a token.
	token, err := ropc(username, password)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Println("Got a token.")

	// 2) GET /whoami — print tenant + roles.
	status, body, err := apiCall(http.MethodGet, "/whoami", token, nil)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	if status != http.StatusOK {
		fmt.Fprintf(os.Stderr, "whoami failed: %d %s\n", status, string(body))
		os.Exit(1)
	}
	var whoami struct {
		Tenant string   `json:"tenant"`
		Roles  []string `json:"roles"`
	}
	if err := json.Unmarshal(body, &whoami); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("whoami: tenant=%s roles=%v\n", whoami.Tenant, whoami.Roles)

	// 3) POST /engine/simulate with the shared decision.
	// Run this example from examples/go (`go run main.go`), so the shared
	// fixture resolves as a sibling of this directory.
	decisionPath := filepath.Join("..", "shared", "loan_eligibility.json")
	raw, err := os.ReadFile(decisionPath)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	var model decisionModel
	if err := json.Unmarshal(raw, &model); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	if len(model.Decisions) == 0 {
		fmt.Fprintln(os.Stderr, "no decisions found in shared fixture")
		os.Exit(1)
	}

	reqBody := map[string]interface{}{
		"decision": json.RawMessage(model.Decisions[0]),
		"inputs": map[string]interface{}{
			"credit_score": 780,
			"income":       80000,
			"age":          40,
		},
	}
	status, body, err = apiCall(http.MethodPost, "/engine/simulate", token, reqBody)
	if err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	if status != http.StatusOK {
		fmt.Fprintf(os.Stderr, "simulate failed: %d %s\n", status, string(body))
		os.Exit(1)
	}
	var result struct {
		Outputs map[string]interface{} `json:"outputs"`
	}
	if err := json.Unmarshal(body, &result); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
	fmt.Printf("outputs: %v\n", result.Outputs)

	_ = ruleflowProject // referenced only by the commented next-step examples below

	// --- Next steps (commented — request shape only, not executed) --------
	//
	// Run a *stored* decision (persisted in a project) synchronously:
	//
	//   status, body, err := apiCall(http.MethodPost,
	//     fmt.Sprintf("/projects/%s/decisions/<decisionId>/simulate", ruleflowProject),
	//     token, map[string]interface{}{"credit_score": 780, "income": 80000, "age": 40})
	//
	// Start a workflow execution asynchronously (202 + execution id), then poll:
	//
	//   status, body, err := apiCall(http.MethodPost,
	//     fmt.Sprintf("/projects/%s/executions", ruleflowProject), token,
	//     map[string]interface{}{
	//       "workflow": "<name>", "env": "dev",
	//       "input": map[string]interface{}{"credit_score": 780, "income": 80000, "age": 40},
	//     })
	//   // ... read execution id from body, then:
	//   status, body, err = apiCall(http.MethodGet,
	//     fmt.Sprintf("/projects/%s/executions/%s", ruleflowProject, execID), token, nil)
}
