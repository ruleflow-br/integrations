# Node.js example

Runs the RuleFlow credit decision (`loan_eligibility`) using Node's native
`fetch` — no dependencies to install.

## Prerequisites

- Node.js 18+ (for built-in `fetch`)
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
node index.mjs
```

## Expected output

```
Got a token.
whoami: tenant=<your-tenant> roles=["business-analyst"]
outputs: { risk_tier: 'LOW', approved: true, credit_limit: 24000 }
```

## What it does

1. `ropc()` gets a bearer token via the Resource Owner Password grant.
2. `apiCall("GET", "/whoami", token)` prints the tenant and roles.
3. `apiCall("POST", "/engine/simulate", token, ...)` runs the shared
   [`loan_eligibility`](../shared/loan_eligibility.json) decision with
   inputs `{"credit_score":780,"income":80000,"age":40}` and prints the
   outputs.

The bottom of `index.mjs` has a commented block showing the next-step
request shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
