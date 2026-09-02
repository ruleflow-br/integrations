# Go example

Runs the RuleFlow credit decision (`loan_eligibility`) using only the Go
standard library (`net/http`, `encoding/json`) — no dependencies to fetch.

## Prerequisites

- Go 1.21+
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
go run main.go
```

Run it from this directory (`examples/go`) — the shared fixture is read as
`../shared/loan_eligibility.json`.

## Expected output

```
Got a token.
whoami: tenant=<your-tenant> roles=[business-analyst]
outputs: map[approved:true credit_limit:24000 risk_tier:LOW]
```

## What it does

1. `ropc()` gets a bearer token via the Resource Owner Password grant.
2. `apiCall(http.MethodGet, "/whoami", token, nil)` prints the tenant and
   roles.
3. `apiCall(http.MethodPost, "/engine/simulate", token, ...)` runs the
   shared [`loan_eligibility`](../shared/loan_eligibility.json) decision
   with inputs `{"credit_score":780,"income":80000,"age":40}` and prints the
   outputs.

The bottom of `main.go` has a commented block showing the next-step request
shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
