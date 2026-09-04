# Example decision models

Ready-to-run RuleFlow decision models — full, valid models with typed
inputs/outputs, nodes, and embedded tests. Each was validated against the live
engine. Handy to run via `POST /api/engine/simulate` or to import into the Studio.

| File | Decision | Inputs → Outputs |
| --- | --- | --- |
| `loan_eligibility.json` | Credit eligibility | `credit_score`, `income`, `age` → `risk_tier`, `approved`, `credit_limit` |
| `reajuste_coletivo.json` | Health — group-contract readjustment | `sinistralidade`, `vidas`, `vcmh`, `meses_desde_ultimo_reajuste` → `faixa_risco`, `reajuste_pct`, `aprovacao_manual` |
| `reajuste_faixa_etaria.json` | Health — age-band readjustment (ANS RN 63/2003) | `idade`, `coeficiente_anterior` → `faixa`, `coeficiente`, `mudou_faixa`, `reajuste_faixa_pct` |

The language examples (`../python`, `../node`, `../go`, …) run
`loan_eligibility.json`. Point one at another model by changing which file it
loads — the API call and shapes are identical.

## Notes on the health models
- **`reajuste_coletivo`** — tiers a group contract by loss ratio (`sinistralidade`),
  computes the readjustment as `vcmh + excess over 70% loss ratio`, and flags
  `aprovacao_manual` when it needs Negociação (below the 12-month carência,
  loss ratio > 100%, or readjustment above 25%).
- **`reajuste_faixa_etaria`** — the 10 ANS age bands (RN 63/2003) with example
  coefficients that comply with the rule (last band ≤ 6× the first; the
  1st→7th variation ≤ the 7th→10th). Coefficients are illustrative — each
  operator sets its own within the ANS limits.

## Workflow example

`reajuste_beneficiario.json` is a full **project** (both health decisions +
a workflow) that chains them end to end:

```
faixa (decision: reajuste_faixa_etaria)
  → contrato (decision: reajuste_coletivo)
    → gate (aprovacao_manual == true ?)
        → aprovacao (human_task: Negociação)  → aplicar
        → aplicar (service_task: aplicar-reajuste)  → fim
```

It computes the beneficiary's age-band change and the contract-level
readjustment, then routes to a human approval step (Negociação) when the
contract falls into the manual-approval band — otherwise applies directly. The
workflow compiles to AWS Step Functions; validated live (`/engine/validate` +
`/engine/compile-workflow`).
