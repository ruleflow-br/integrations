# Contributing

Thanks for improving RuleFlow's integration examples. This repo is
intentionally small: each language gets one directory with one example that
does the same three things against the real API.

## Adding a new language

1. Copy `examples/_template/README.md` into a new directory,
   `examples/<language>/`, and follow its checklist.
2. Implement the 3-step contract (below) using only what ships with the
   language's standard toolchain where practical — no dependency should be
   required just to run the example. If an HTTP/JSON library is genuinely
   part of the standard distribution (e.g. Go's `net/http`, Node's `fetch`),
   use it. Otherwise pick the single most common, permissively-licensed
   library and say why in your PR.
3. Read the shared fixture from `../shared/loan_eligibility.json` — don't
   duplicate or hand-copy the decision JSON into your example.
4. Add a short `README.md` in your directory: prerequisites, how to set
   `RULEFLOW_USER` / `RULEFLOW_PASSWORD`, and the exact run command.
5. Add your directory to the table in the root `README.md`.
6. If your language has a lint/format/typecheck/compile-check step that
   needs no network access and no credentials, add it to
   `.github/workflows/ci.yml` next to the existing ones.

## The 3-step contract

Every example implements exactly this, in this order, and prints enough to
see it worked:

1. **Get a token.** POST form-encoded
   `grant_type=password&client_id=ruleflow-cli&username=<RULEFLOW_USER>&password=<RULEFLOW_PASSWORD>`
   to `https://auth.ruleflow.com.br/realms/ruleflow/protocol/openid-connect/token`
   and read `access_token` from the JSON response.
2. **Call `GET /api/whoami`** with `Authorization: Bearer <token>` and print
   the `tenant` and `roles` fields — this proves auth works end to end.
3. **Call `POST /api/engine/simulate`** with body
   `{"decision": <the loan_eligibility decision from the shared fixture>, "inputs": {"credit_score": 780, "income": 80000, "age": 40}}`
   and print the `outputs` (expect
   `{"risk_tier":"LOW","approved":true,"credit_limit":24000}`).

After that, add a **commented-out** block showing the next-step,
request-only calls (do not execute them, and do not assert response field
names, since they touch project-scoped state a trial account may not have):

- `POST /api/projects/{project}/decisions/{decisionId}/simulate` — run a
  *stored* decision (body = the raw inputs object).
- `POST /api/projects/{project}/executions` with
  `{"workflow": "...", "env": "...", "input": {...}}` — start a workflow
  execution (returns `202` with an id), polled via
  `GET /api/projects/{project}/executions/{id}`.

Use the same env vars as the other examples: `RULEFLOW_API`,
`RULEFLOW_AUTH`, `RULEFLOW_REALM`, `RULEFLOW_CLIENT`, `RULEFLOW_USER`,
`RULEFLOW_PASSWORD`, `RULEFLOW_PROJECT` (see any existing example for
defaults).

## Running your example locally

Set credentials from a trial workspace (see the root README's "Get
credentials"), then run the command your directory's README documents.
Examples call the live API — they need real credentials and are not run in
CI.

## Local lint

Run whatever your language's `ci.yml` step runs, e.g.:

```bash
python3 -m py_compile examples/python/main.py
node --check examples/node/index.mjs
cd examples/go && gofmt -l . && go vet ./...
bash -n examples/curl/run.sh
```

## Opening a PR

- Keep the diff scoped to your language directory plus the README/CI/
  Dependabot entries it needs.
- Don't commit credentials, tokens, or `.env` files.
- Describe in the PR what you ran locally and what it printed.
