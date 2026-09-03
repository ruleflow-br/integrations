# PHP example

Runs the RuleFlow credit decision (`loan_eligibility`) using only the
built-in cURL extension (`curl_*`) and `json_encode`/`json_decode` — no
Composer dependencies.

## Prerequisites

- PHP 8.0+ with the `curl` extension enabled (bundled with most PHP
  distributions; on Debian/Ubuntu install `php-curl` if it's missing)
- A RuleFlow trial workspace — see the root [README](../../README.md#get-credentials).

## Run

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
php main.php
```

Run it from this directory (`examples/php`) — the shared fixture is read
as `__DIR__ . '/../shared/loan_eligibility.json'`.

## Expected output

```
Got a token.
whoami: tenant=<your-tenant> roles=["business-analyst"]
outputs: {"risk_tier":"LOW","approved":true,"credit_limit":24000}
```

## What it does

1. `ropc()` gets a bearer token via the Resource Owner Password grant.
2. `apiCall($api, 'GET', '/whoami', $token)` prints the tenant and roles.
3. `apiCall($api, 'POST', '/engine/simulate', $token, ...)` runs the shared
   [`loan_eligibility`](../shared/loan_eligibility.json) decision with
   inputs `{"credit_score":780,"income":80000,"age":40}` and prints the
   outputs.

The bottom of `main.php` has a commented block showing the next-step
request shapes (stored decisions, workflow executions) — see
[docs/integration.md](../../docs/integration.md) for details.
