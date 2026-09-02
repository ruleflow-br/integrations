#!/usr/bin/env node
// RuleFlow integration example — Node.js, native fetch (Node 18+), no deps.
//
// 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
// tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.

import { readFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";

const __dirname = dirname(fileURLToPath(import.meta.url));
const DECISION_FILE = join(__dirname, "..", "shared", "loan_eligibility.json");

const RULEFLOW_API = process.env.RULEFLOW_API || "https://api.ruleflow.com.br/api";
const RULEFLOW_AUTH = process.env.RULEFLOW_AUTH || "https://auth.ruleflow.com.br";
const RULEFLOW_REALM = process.env.RULEFLOW_REALM || "ruleflow";
const RULEFLOW_CLIENT = process.env.RULEFLOW_CLIENT || "ruleflow-cli";
const RULEFLOW_PROJECT = process.env.RULEFLOW_PROJECT || "lending";

// Resource Owner Password grant against the public client ruleflow-cli.
async function ropc(username, password) {
  const body = new URLSearchParams({
    grant_type: "password",
    client_id: RULEFLOW_CLIENT,
    username,
    password,
  });
  const res = await fetch(
    `${RULEFLOW_AUTH}/realms/${RULEFLOW_REALM}/protocol/openid-connect/token`,
    { method: "POST", body }
  );
  if (!res.ok) {
    throw new Error(`ROPC failed: ${res.status} ${await res.text()}`);
  }
  const json = await res.json();
  return json.access_token;
}

async function apiCall(method, path, token, body) {
  const headers = { Authorization: `Bearer ${token}` };
  let payload;
  if (body !== undefined) {
    headers["Content-Type"] = "application/json";
    payload = JSON.stringify(body);
  }
  const res = await fetch(RULEFLOW_API + path, { method, headers, body: payload });
  const text = await res.text();
  let parsed;
  try {
    parsed = text ? JSON.parse(text) : null;
  } catch {
    parsed = text;
  }
  return { status: res.status, body: parsed };
}

async function main() {
  const username = process.env.RULEFLOW_USER;
  const password = process.env.RULEFLOW_PASSWORD;
  if (!username || !password) {
    console.error("Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment.");
    process.exit(1);
  }

  // 1) Get a token.
  const token = await ropc(username, password);
  console.log("Got a token.");

  // 2) GET /whoami — print tenant + roles.
  const whoami = await apiCall("GET", "/whoami", token);
  if (whoami.status !== 200) {
    console.error(`whoami failed: ${whoami.status}`, whoami.body);
    process.exit(1);
  }
  console.log(`whoami: tenant=${whoami.body.tenant} roles=${JSON.stringify(whoami.body.roles)}`);

  // 3) POST /engine/simulate with the shared decision.
  const model = JSON.parse(readFileSync(DECISION_FILE, "utf8"));
  const decision = model.decisions[0];
  const inputs = { credit_score: 780, income: 80000, age: 40 };

  const result = await apiCall("POST", "/engine/simulate", token, { decision, inputs });
  if (result.status !== 200) {
    console.error(`simulate failed: ${result.status}`, result.body);
    process.exit(1);
  }
  console.log("outputs:", result.body.outputs);

  // --- Next steps (commented — request shape only, not executed) ---------
  //
  // Run a *stored* decision (persisted in a project) synchronously:
  //
  //   const stored = await apiCall(
  //     "POST", `/projects/${RULEFLOW_PROJECT}/decisions/<decisionId>/simulate`,
  //     token, inputs);
  //
  // Start a workflow execution asynchronously (202 + execution id), then poll:
  //
  //   const started = await apiCall(
  //     "POST", `/projects/${RULEFLOW_PROJECT}/executions`, token,
  //     { workflow: "<name>", env: "dev", input: inputs });
  //   const execId = started.body.id;
  //   const status = await apiCall(
  //     "GET", `/projects/${RULEFLOW_PROJECT}/executions/${execId}`, token);
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
