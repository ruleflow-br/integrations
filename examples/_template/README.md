# Adding a new language

This is the checklist/skeleton for adding a RuleFlow integration example in
a language not covered yet (Rust, Ruby, PHP, C, ...). See also the root
[CONTRIBUTING.md](../../CONTRIBUTING.md).

The fastest path is to **copy an existing example** (`examples/python` and
`examples/go` are the clearest stdlib-only references) and translate it,
rather than starting from a blank file.

## Directory layout

```
examples/<language>/
  <entry point>       # e.g. main.rs, main.rb, index.php
  README.md           # this checklist filled in for your language
  <manifest, if any>  # e.g. Cargo.toml, Gemfile — only if the language needs one
```

## Config (env vars — all optional except the two marked required)

| Variable | Default | Notes |
| --- | --- | --- |
| `RULEFLOW_API` | `https://api.ruleflow.com.br/api` | |
| `RULEFLOW_AUTH` | `https://auth.ruleflow.com.br` | |
| `RULEFLOW_REALM` | `ruleflow` | |
| `RULEFLOW_CLIENT` | `ruleflow-cli` | public client, no secret |
| `RULEFLOW_USER` | — | **required** |
| `RULEFLOW_PASSWORD` | — | **required** |
| `RULEFLOW_PROJECT` | `lending` | only used by the commented next-step calls |

## The 3 steps your example must implement, in order

1. **Get a token.** POST form-encoded
   `grant_type=password&client_id=ruleflow-cli&username=<RULEFLOW_USER>&password=<RULEFLOW_PASSWORD>`
   to `$RULEFLOW_AUTH/realms/$RULEFLOW_REALM/protocol/openid-connect/token`
   and read `access_token` from the JSON response. Print "Got a token." (or
   equivalent) on success.
2. **Call `GET $RULEFLOW_API/whoami`** with `Authorization: Bearer <token>`
   and print the `tenant` and `roles` fields from the response.
3. **Call `POST $RULEFLOW_API/engine/simulate`** with body
   `{"decision": <decisions[0] from ../shared/loan_eligibility.json>, "inputs": {"credit_score": 780, "income": 80000, "age": 40}}`
   and print the `outputs` field of the response.

Then add a **commented-out** block (not executed) showing the next-step
calls — see any existing example (e.g. `examples/python/main.py`) for the
exact shape to mirror: a stored-decision simulate call and a workflow
execution start + poll.

## Expected output (all examples produce the same shape)

```
Got a token.
whoami: tenant=<your-tenant> roles=[...]
outputs: {"risk_tier":"LOW","approved":true,"credit_limit":24000}
```

## Run

Document your language's exact run command here, e.g.:

```bash
export RULEFLOW_USER=you@example.com
export RULEFLOW_PASSWORD=your-password
<your run command>
```

## Before opening a PR

- [ ] Copied `../shared/loan_eligibility.json` at runtime — did not hand-copy
      the decision JSON into the example.
- [ ] No dependency required beyond what ships with the language's standard
      toolchain, where practical.
- [ ] Added a lint/format/compile-check step with no network/credentials to
      `.github/workflows/ci.yml`.
- [ ] Added the new directory to the table in the root `README.md`.
- [ ] Filled in this README (or a copy of it) with your language's specifics.
