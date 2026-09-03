// RuleFlow integration example — Rust, using `ureq` (blocking HTTP client)
// and `serde_json`.
//
// 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
// tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.

use serde_json::{json, Value};
use std::env;
use std::process;

// Run this example from examples/rust (`cargo run`); the shared fixture is
// embedded at compile time as a sibling of this crate's directory.
const DECISION_JSON: &str = include_str!("../../shared/loan_eligibility.json");

fn env_or(key: &str, fallback: &str) -> String {
    env::var(key).unwrap_or_else(|_| fallback.to_string())
}

/// Resource Owner Password grant against the public client ruleflow-cli.
/// Returns the access token.
fn ropc(auth: &str, realm: &str, client: &str, username: &str, password: &str) -> String {
    let url = format!("{}/realms/{}/protocol/openid-connect/token", auth, realm);
    let result = ureq::post(&url).send_form(&[
        ("grant_type", "password"),
        ("client_id", client),
        ("username", username),
        ("password", password),
    ]);
    let resp = match result {
        Ok(r) => r,
        Err(e) => {
            eprintln!("ROPC failed: {}", e);
            process::exit(1);
        }
    };
    let body: Value = resp.into_json().unwrap_or(Value::Null);
    body["access_token"].as_str().unwrap_or_default().to_string()
}

/// Calls the RuleFlow control-plane API. Returns (status, parsed body).
fn api_call(api: &str, method: &str, path: &str, token: &str, body: Option<&Value>) -> (u16, Value) {
    let url = format!("{}{}", api, path);
    let req = match method {
        "GET" => ureq::get(&url),
        "POST" => ureq::post(&url),
        _ => unreachable!("unsupported method"),
    }
    .set("Authorization", &format!("Bearer {}", token));

    let result = match body {
        Some(b) => req.send_json(b.clone()),
        None => req.call(),
    };

    match result {
        Ok(resp) => {
            let status = resp.status();
            let parsed = resp.into_json().unwrap_or(Value::Null);
            (status, parsed)
        }
        Err(ureq::Error::Status(code, resp)) => {
            let parsed = resp.into_json().unwrap_or(Value::Null);
            (code, parsed)
        }
        Err(e) => {
            eprintln!("request error: {}", e);
            process::exit(1);
        }
    }
}

fn main() {
    let ruleflow_api = env_or("RULEFLOW_API", "https://api.ruleflow.com.br/api");
    let ruleflow_auth = env_or("RULEFLOW_AUTH", "https://auth.ruleflow.com.br");
    let ruleflow_realm = env_or("RULEFLOW_REALM", "ruleflow");
    let ruleflow_client = env_or("RULEFLOW_CLIENT", "ruleflow-cli");
    let ruleflow_project = env_or("RULEFLOW_PROJECT", "lending");
    let _ = &ruleflow_project; // referenced only by the commented next-step examples below

    let username = env::var("RULEFLOW_USER").unwrap_or_default();
    let password = env::var("RULEFLOW_PASSWORD").unwrap_or_default();
    if username.is_empty() || password.is_empty() {
        eprintln!("Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment.");
        process::exit(1);
    }

    // 1) Get a token.
    let token = ropc(&ruleflow_auth, &ruleflow_realm, &ruleflow_client, &username, &password);
    println!("Got a token.");

    // 2) GET /whoami — print tenant + roles.
    let (status, whoami) = api_call(&ruleflow_api, "GET", "/whoami", &token, None);
    if status != 200 {
        eprintln!("whoami failed: {} {}", status, whoami);
        process::exit(1);
    }
    println!(
        "whoami: tenant={} roles={}",
        whoami["tenant"].as_str().unwrap_or_default(),
        whoami["roles"]
    );

    // 3) POST /engine/simulate with the shared decision.
    let model: Value = serde_json::from_str(DECISION_JSON).expect("parse shared decision JSON");
    let decision = &model["decisions"][0];
    let body = json!({
        "decision": decision,
        "inputs": { "credit_score": 780, "income": 80000, "age": 40 },
    });

    let (status, result) = api_call(&ruleflow_api, "POST", "/engine/simulate", &token, Some(&body));
    if status != 200 {
        eprintln!("simulate failed: {} {}", status, result);
        process::exit(1);
    }
    println!("outputs: {}", result["outputs"]);

    // --- Next steps (commented — request shape only, not executed) --------
    //
    // Run a *stored* decision (persisted in a project) synchronously:
    //
    //   let (status, result) = api_call(&ruleflow_api, "POST",
    //       &format!("/projects/{}/decisions/<decisionId>/simulate", ruleflow_project),
    //       &token, Some(&json!({"credit_score": 780, "income": 80000, "age": 40})));
    //
    // Start a workflow execution asynchronously (202 + execution id), then poll:
    //
    //   let (status, result) = api_call(&ruleflow_api, "POST",
    //       &format!("/projects/{}/executions", ruleflow_project), &token,
    //       Some(&json!({"workflow": "<name>", "env": "dev", "input": {
    //           "credit_score": 780, "income": 80000, "age": 40
    //       }})));
    //   let exec_id = result["id"].as_str().unwrap();
    //   let (status, result) = api_call(&ruleflow_api, "GET",
    //       &format!("/projects/{}/executions/{}", ruleflow_project, exec_id), &token, None);
}
