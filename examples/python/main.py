#!/usr/bin/env python3
"""RuleFlow integration example — Python, standard library only.

3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.
"""
import json
import os
import sys
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DECISION_FILE = os.path.join(HERE, "..", "shared", "loan_eligibility.json")

RULEFLOW_API = os.environ.get("RULEFLOW_API", "https://api.ruleflow.com.br/api")
RULEFLOW_AUTH = os.environ.get("RULEFLOW_AUTH", "https://auth.ruleflow.com.br")
RULEFLOW_REALM = os.environ.get("RULEFLOW_REALM", "ruleflow")
RULEFLOW_CLIENT = os.environ.get("RULEFLOW_CLIENT", "ruleflow-cli")
RULEFLOW_PROJECT = os.environ.get("RULEFLOW_PROJECT", "lending")


def ropc(username, password):
    """Resource Owner Password grant against the public client ruleflow-cli.
    Returns the access token."""
    data = urllib.parse.urlencode({
        "grant_type": "password",
        "client_id": RULEFLOW_CLIENT,
        "username": username,
        "password": password,
    }).encode()
    url = f"{RULEFLOW_AUTH}/realms/{RULEFLOW_REALM}/protocol/openid-connect/token"
    req = urllib.request.Request(url, data=data, method="POST")
    req.add_header("Content-Type", "application/x-www-form-urlencoded")
    with urllib.request.urlopen(req, timeout=20) as r:
        return json.load(r)["access_token"]


def api_call(method, path, token=None, body=None):
    """Call the RuleFlow control-plane API. Returns (status, parsed_json)."""
    req = urllib.request.Request(RULEFLOW_API + path, method=method)
    if token:
        req.add_header("Authorization", "Bearer " + token)
    data = None
    if body is not None:
        req.add_header("Content-Type", "application/json")
        data = json.dumps(body).encode()
    try:
        with urllib.request.urlopen(req, data, timeout=30) as r:
            raw = r.read().decode()
            return r.status, (json.loads(raw) if raw.strip() else None)
    except urllib.error.HTTPError as e:
        raw = e.read().decode()
        try:
            return e.code, json.loads(raw)
        except json.JSONDecodeError:
            return e.code, raw


def main():
    username = os.environ.get("RULEFLOW_USER")
    password = os.environ.get("RULEFLOW_PASSWORD")
    if not username or not password:
        print("Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment.", file=sys.stderr)
        sys.exit(1)

    # 1) Get a token.
    token = ropc(username, password)
    print("Got a token.")

    # 2) GET /whoami — print tenant + roles.
    status, whoami = api_call("GET", "/whoami", token)
    if status != 200:
        print(f"whoami failed: {status} {whoami}", file=sys.stderr)
        sys.exit(1)
    print(f"whoami: tenant={whoami.get('tenant')} roles={whoami.get('roles')}")

    # 3) POST /engine/simulate with the shared decision.
    with open(DECISION_FILE) as f:
        model = json.load(f)
    decision = model["decisions"][0]
    inputs = {"credit_score": 780, "income": 80000, "age": 40}

    status, result = api_call("POST", "/engine/simulate", token,
                               {"decision": decision, "inputs": inputs})
    if status != 200:
        print(f"simulate failed: {status} {result}", file=sys.stderr)
        sys.exit(1)
    print(f"outputs: {result.get('outputs')}")

    # --- Next steps (commented — request shape only, not executed) --------
    #
    # Run a *stored* decision (persisted in a project) synchronously:
    #
    #   status, result = api_call(
    #       "POST", f"/projects/{RULEFLOW_PROJECT}/decisions/<decisionId>/simulate",
    #       token, inputs)
    #
    # Start a workflow execution asynchronously (202 + execution id), then poll:
    #
    #   status, result = api_call(
    #       "POST", f"/projects/{RULEFLOW_PROJECT}/executions", token,
    #       {"workflow": "<name>", "env": "dev", "input": inputs})
    #   exec_id = result["id"]
    #   status, result = api_call(
    #       "GET", f"/projects/{RULEFLOW_PROJECT}/executions/{exec_id}", token)


if __name__ == "__main__":
    main()
