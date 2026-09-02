# Integration guide

This is a lean, language-agnostic walkthrough of what the examples in this
repo do and why. For the full API reference, see
[docs.ruleflow.com.br](https://docs.ruleflow.com.br) — this page links the
deep reference rather than duplicating it.

## Overview

RuleFlow has no published client SDK and no static API key. You integrate
directly over HTTP:

1. Authenticate as a Keycloak identity and get a short-lived bearer token.
2. Call `/api/*` endpoints with `Authorization: Bearer <token>`.
3. Run decision logic either ad hoc (`/api/engine/simulate`, no stored
   artifact needed) or against a project's stored, versioned decisions and
   workflows.

- API base: `https://api.ruleflow.com.br/api`
- Auth base: `https://auth.ruleflow.com.br`, realm `ruleflow`

## Authentication (ROPC)

RuleFlow issues tokens via Keycloak's Resource Owner Password Credentials
grant, against the **public** client `ruleflow-cli` (no client secret):

```bash
curl -s -X POST \
  https://auth.ruleflow.com.br/realms/ruleflow/protocol/openid-connect/token \
  -d grant_type=password \
  -d client_id=ruleflow-cli \
  -d username="$RULEFLOW_USER" \
  -d password="$RULEFLOW_PASSWORD" | jq -r .access_token
```

The response is a standard OIDC token JSON payload; read `access_token` out
of it and send it as `Authorization: Bearer <token>` on every subsequent
`/api/*` call. Tokens are short-lived — mint a fresh one per run rather than
caching it across long-lived processes.

Deep reference: [API — Authentication](https://docs.ruleflow.com.br/api/authentication.html).

## Endpoints the examples use

### `GET /api/whoami`

Echoes your verified claims: `{"sub","email","tenant","roles"}`. Every
example calls this right after authenticating, as a cheap end-to-end proof
that the token is valid and carries a tenant.

### `POST /api/engine/simulate`

Runs a decision **without** any pre-existing project or stored artifact —
the request body carries the whole decision model plus the inputs:

```json
{
  "decision": { "...": "the decision model JSON" },
  "inputs": { "credit_score": 780, "income": 80000, "age": 40 }
}
```

The response is the engine result: `{"outputs": {...}, "trace": [...]}`.
This is why every example in this repo can run against a brand-new trial
tenant with zero setup — there is nothing to provision first.

## The credit example

All examples use the same fixture,
[`examples/shared/loan_eligibility.json`](../examples/shared/loan_eligibility.json)
— the real `loan_eligibility` decision. It takes:

- `credit_score` (integer)
- `income` (number)
- `age` (integer)

...and produces:

- `risk_tier` (`"LOW"` / `"MEDIUM"` / `"HIGH"`)
- `approved` (boolean)
- `credit_limit` (number)

Worked example — input `{"credit_score":780,"income":80000,"age":40}`
produces `{"risk_tier":"LOW","approved":true,"credit_limit":24000}`.

## Next steps beyond this repo

Once you're ready to move past ad-hoc simulation, the platform's real
lifecycle is: author a decision/workflow (usually in the
[Studio](https://app.ruleflow.com.br)) → simulate it → cut a release →
deploy it to an environment → call it from your application. Two shapes:

- **Stored decision (sync):**
  `POST /api/projects/{project}/decisions/{decisionId}/simulate` — body is
  the raw inputs object, response is `{outputs, trace}`.
- **Workflow execution (async):**
  `POST /api/projects/{project}/executions` — body
  `{"workflow": "...", "env": "...", "input": {...}}`, returns `202` with an
  execution id; poll `GET /api/projects/{project}/executions/{id}` for
  status and output.

Deep reference: [API — Executions](https://docs.ruleflow.com.br/api/executions.html),
[API — Projects & artifacts](https://docs.ruleflow.com.br/api/artifacts.html).

## Error codes

Uniform error shape: `{"error": "<message>"}`.

| Status | Meaning |
| --- | --- |
| `400` | Invalid request. |
| `402` | Trial expired — choose a plan. |
| `403` | No tenant in token, or tenant suspended. |
| `404` | Unknown resource, or cross-tenant access. |
| `409` | Idempotency key reused with a different body. |
| `429` | Monthly execution quota exceeded. |
| `503` | A subsystem is not configured (engine/search) or self-signup disabled. |

## Getting started from scratch

If you haven't created a workspace yet, start with the platform quickstart:
[Getting started — Quickstart](https://docs.ruleflow.com.br/getting-started/quickstart.html).
