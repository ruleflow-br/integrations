# Rust example

Runs the RuleFlow credit decision (`loan_eligibility`) using
[`ureq`](https://crates.io/crates/ureq) (a small blocking HTTP client) and
`serde_json` — no async runtime, no HTTP framework.

## Prerequisites

- Rust 1.70+ (stable), via [rustup](https://rustup.rs)
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
cargo run
```

Run it from this directory (`examples/rust`) — the shared fixture is
embedded at compile time from `../shared/loan_eligibility.json` via
`include_str!`, so no path resolution happens at runtime.

## Expected output

```
Got a token.
whoami: tenant=<your-tenant> roles=["business-analyst"]
outputs: {"approved":true,"credit_limit":24000,"risk_tier":"LOW"}
```

## What it does

1. `ropc()` gets a bearer token via the Resource Owner Password grant.
2. `api_call(&api, "GET", "/whoami", &token, None)` prints the tenant and
   roles.
3. `api_call(&api, "POST", "/engine/simulate", &token, ...)` runs the
   shared [`loan_eligibility`](../shared/loan_eligibility.json) decision
   with inputs `{"credit_score":780,"income":80000,"age":40}` and prints the
   outputs.

The bottom of `src/main.rs` has a commented block showing the next-step
request shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
