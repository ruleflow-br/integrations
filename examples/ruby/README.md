# Ruby example

Runs the RuleFlow credit decision (`loan_eligibility`) using only the Ruby
standard library (`net/http`, `uri`, `json`) — no gems to install.

## Prerequisites

- Ruby 2.7+ (any Ruby with `net/http` and `json` in the standard library)
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
ruby main.rb
```

Run it from this directory (`examples/ruby`) — the shared fixture is read
as `../shared/loan_eligibility.json`, relative to the script.

## Expected output

```
Got a token.
whoami: tenant=<your-tenant> roles=["business-analyst"]
outputs: {"risk_tier"=>"LOW", "approved"=>true, "credit_limit"=>24000}
```

## What it does

1. `ropc()` gets a bearer token via the Resource Owner Password grant.
2. `api_call("GET", "/whoami", token)` prints the tenant and roles.
3. `api_call("POST", "/engine/simulate", token, ...)` runs the shared
   [`loan_eligibility`](../shared/loan_eligibility.json) decision with
   inputs `{"credit_score":780,"income":80000,"age":40}` and prints the
   outputs.

The bottom of `main.rb` has a commented block showing the next-step request
shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
