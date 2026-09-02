# curl example

Runs the RuleFlow credit decision (`loan_eligibility`) using only `curl` and
`jq`.

## Prerequisites

- `curl`
- `jq`
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
./run.sh
```

## Expected output

```
Got a token.
whoami: {"tenant":"<your-tenant>","roles":["business-analyst"]}
outputs: {"risk_tier":"LOW","approved":true,"credit_limit":24000}
```

## What it does

1. Gets a bearer token via the Resource Owner Password grant.
2. Calls `GET /api/whoami` and prints the tenant and roles.
3. Calls `POST /api/engine/simulate` with the shared
   [`loan_eligibility`](../shared/loan_eligibility.json) decision and inputs
   `{"credit_score":780,"income":80000,"age":40}`, and prints the outputs.

The bottom of `run.sh` has a commented block showing the next-step request
shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
