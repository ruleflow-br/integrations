# RuleFlow Integrations

Multi-language, copy-pasteable examples for calling the RuleFlow HTTP API.

Part of the [RuleFlow](https://github.com/ruleflow-br) platform.

## The API in 60 seconds

Base URL: `https://api.ruleflow.com.br/api`

RuleFlow has no static API key. You authenticate as a Keycloak identity and
present a bearer token, obtained via the Resource Owner Password grant
against the public client `ruleflow-cli`:

```bash
TOKEN=$(curl -s -X POST \
  https://auth.ruleflow.com.br/realms/ruleflow/protocol/openid-connect/token \
  -d grant_type=password \
  -d client_id=ruleflow-cli \
  -d username="$RULEFLOW_USER" \
  -d password="$RULEFLOW_PASSWORD" | jq -r .access_token)
```

Run a decision — no pre-existing project or stored artifact required —
using the credit-eligibility fixture in this repo:

```bash
curl -s -X POST https://api.ruleflow.com.br/api/engine/simulate \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "{\"decision\": $(jq -c . examples/shared/loan_eligibility.json | jq -c '.decisions[0]'), \
       \"inputs\": {\"credit_score\":780,\"income\":80000,\"age\":40}}"
# => {"outputs":{"risk_tier":"LOW","approved":true,"credit_limit":24000},"trace":[...]}
```

## Get credentials

1. Create a workspace at [app.ruleflow.com.br](https://app.ruleflow.com.br) —
   30-day trial, no card required.
2. Export your login as environment variables:

   ```bash
   export RULEFLOW_USER=you@example.com
   export RULEFLOW_PASSWORD=your-password
   ```

Never commit these values. Each example reads them from the environment.

## Examples

| Language | Directory | Run |
| --- | --- | --- |
| curl + jq | [`examples/curl`](examples/curl) | `./run.sh` |
| Python (stdlib only) | [`examples/python`](examples/python) | `python3 main.py` |
| Node.js (native `fetch`) | [`examples/node`](examples/node) | `node index.mjs` |
| Go (stdlib only) | [`examples/go`](examples/go) | `go run main.go` |
| Rust (`ureq` + `serde_json`) | [`examples/rust`](examples/rust) | `cargo run` |
| Ruby (stdlib only) | [`examples/ruby`](examples/ruby) | `ruby main.rb` |
| PHP (built-in cURL extension) | [`examples/php`](examples/php) | `php main.php` |
| C (libcurl) | [`examples/c`](examples/c) | `make && ./rf-example` |
| Your language here | [`examples/_template`](examples/_template) | see [CONTRIBUTING.md](CONTRIBUTING.md) |

Each example does the same three things: get a token, call `GET /api/whoami`
to confirm the tenant, then run the `loan_eligibility` decision via
`POST /api/engine/simulate`. A commented block at the end shows the
next-step calls (stored decisions, workflow executions).

Ready-to-run **decision models** live in [`examples/shared`](examples/shared) —
credit eligibility plus two health-plan pricing examples (group-contract and
ANS age-band readjustment). Point any example at a different model to try them.

## Docs

- Full reference: [docs.ruleflow.com.br](https://docs.ruleflow.com.br)
- Guide for AI assistants / LLMs: [llms.txt](https://ruleflow.com.br/llms.txt) /
  [llms-full.txt](https://ruleflow.com.br/llms-full.txt)
- This repo's own guide: [docs/integration.md](docs/integration.md)

## Contributing

Adding a new language takes one file (usually). See
[CONTRIBUTING.md](CONTRIBUTING.md).

## License

[Apache-2.0](LICENSE)
