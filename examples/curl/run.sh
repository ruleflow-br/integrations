#!/usr/bin/env bash
# RuleFlow integration example — curl + jq.
#
# 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
# tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.
set -euo pipefail

RULEFLOW_API="${RULEFLOW_API:-https://api.ruleflow.com.br/api}"
RULEFLOW_AUTH="${RULEFLOW_AUTH:-https://auth.ruleflow.com.br}"
RULEFLOW_REALM="${RULEFLOW_REALM:-ruleflow}"
RULEFLOW_CLIENT="${RULEFLOW_CLIENT:-ruleflow-cli}"
RULEFLOW_PROJECT="${RULEFLOW_PROJECT:-lending}"

: "${RULEFLOW_USER:?set RULEFLOW_USER to your RuleFlow login email}"
: "${RULEFLOW_PASSWORD:?set RULEFLOW_PASSWORD to your RuleFlow password}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DECISION_FILE="$SCRIPT_DIR/../shared/loan_eligibility.json"

# --- 1) Get a token (ROPC against the public client ruleflow-cli) ---------
TOKEN_RESPONSE=$(curl -s -X POST \
  "$RULEFLOW_AUTH/realms/$RULEFLOW_REALM/protocol/openid-connect/token" \
  -d grant_type=password \
  -d "client_id=$RULEFLOW_CLIENT" \
  -d "username=$RULEFLOW_USER" \
  -d "password=$RULEFLOW_PASSWORD")

TOKEN=$(echo "$TOKEN_RESPONSE" | jq -r .access_token)
if [ -z "$TOKEN" ] || [ "$TOKEN" = "null" ]; then
  echo "Failed to get a token. Response was:" >&2
  echo "$TOKEN_RESPONSE" >&2
  exit 1
fi
echo "Got a token."

# --- 2) GET /whoami — print tenant + roles ---------------------------------
WHOAMI=$(curl -s "$RULEFLOW_API/whoami" -H "Authorization: Bearer $TOKEN")
echo "whoami: $(echo "$WHOAMI" | jq -c '{tenant, roles}')"

# --- 3) POST /engine/simulate with the shared decision ---------------------
DECISION=$(jq -c '.decisions[0]' "$DECISION_FILE")
INPUTS='{"credit_score":780,"income":80000,"age":40}'
BODY=$(jq -nc --argjson decision "$DECISION" --argjson inputs "$INPUTS" \
  '{decision: $decision, inputs: $inputs}')

RESULT=$(curl -s -X POST "$RULEFLOW_API/engine/simulate" \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d "$BODY")

echo "outputs: $(echo "$RESULT" | jq -c '.outputs')"

# --- Next steps (commented — request shape only, not executed) -------------
#
# Run a *stored* decision (persisted in a project) synchronously:
#
#   curl -s -X POST \
#     "$RULEFLOW_API/projects/$RULEFLOW_PROJECT/decisions/<decisionId>/simulate" \
#     -H "Authorization: Bearer $TOKEN" \
#     -H "Content-Type: application/json" \
#     -d '{"credit_score":780,"income":80000,"age":40}'
#
# Start a workflow execution asynchronously (202 + execution id), then poll:
#
#   curl -s -X POST "$RULEFLOW_API/projects/$RULEFLOW_PROJECT/executions" \
#     -H "Authorization: Bearer $TOKEN" \
#     -H "Content-Type: application/json" \
#     -d '{"workflow":"<name>","env":"dev","input":{"credit_score":780,"income":80000,"age":40}}'
#
#   curl -s "$RULEFLOW_API/projects/$RULEFLOW_PROJECT/executions/<executionId>" \
#     -H "Authorization: Bearer $TOKEN"
