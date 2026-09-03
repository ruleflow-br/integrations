<?php

declare(strict_types=1);

/**
 * RuleFlow integration example — PHP, built-in cURL extension only
 * (curl_*) — no Composer dependencies.
 *
 * 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
 * tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.
 */

$DECISION_FILE = __DIR__ . '/../shared/loan_eligibility.json';

$RULEFLOW_API = getenv('RULEFLOW_API') ?: 'https://api.ruleflow.com.br/api';
$RULEFLOW_AUTH = getenv('RULEFLOW_AUTH') ?: 'https://auth.ruleflow.com.br';
$RULEFLOW_REALM = getenv('RULEFLOW_REALM') ?: 'ruleflow';
$RULEFLOW_CLIENT = getenv('RULEFLOW_CLIENT') ?: 'ruleflow-cli';
$RULEFLOW_PROJECT = getenv('RULEFLOW_PROJECT') ?: 'lending';

/**
 * Resource Owner Password grant against the public client ruleflow-cli.
 * Returns the access token.
 */
function ropc(string $auth, string $realm, string $client, string $username, string $password): string
{
    $url = "$auth/realms/$realm/protocol/openid-connect/token";
    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_POST => true,
        CURLOPT_POSTFIELDS => http_build_query([
            'grant_type' => 'password',
            'client_id' => $client,
            'username' => $username,
            'password' => $password,
        ]),
        CURLOPT_RETURNTRANSFER => true,
    ]);
    $body = curl_exec($ch);
    if ($body === false) {
        fwrite(STDERR, 'ROPC failed: ' . curl_error($ch) . "\n");
        exit(1);
    }
    $status = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);
    if ($status !== 200) {
        fwrite(STDERR, "ROPC failed: $status $body\n");
        exit(1);
    }
    $parsed = json_decode($body, true);
    return (string) $parsed['access_token'];
}

/**
 * Calls the RuleFlow control-plane API. Returns [status, parsed body].
 *
 * @return array{0: int, 1: mixed}
 */
function apiCall(string $api, string $method, string $path, string $token, ?array $body = null): array
{
    $ch = curl_init($api . $path);
    $headers = ["Authorization: Bearer $token"];
    $opts = [
        CURLOPT_CUSTOMREQUEST => $method,
        CURLOPT_RETURNTRANSFER => true,
    ];
    if ($body !== null) {
        $headers[] = 'Content-Type: application/json';
        $opts[CURLOPT_POSTFIELDS] = json_encode($body);
    }
    $opts[CURLOPT_HTTPHEADER] = $headers;
    curl_setopt_array($ch, $opts);

    $raw = curl_exec($ch);
    if ($raw === false) {
        fwrite(STDERR, 'request error: ' . curl_error($ch) . "\n");
        exit(1);
    }
    $status = (int) curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    $parsed = ($raw !== '') ? json_decode($raw, true) : null;
    if ($parsed === null && $raw !== '' && $raw !== 'null') {
        $parsed = $raw;
    }
    return [$status, $parsed];
}

function main(): void
{
    global $RULEFLOW_API, $RULEFLOW_AUTH, $RULEFLOW_REALM, $RULEFLOW_CLIENT, $RULEFLOW_PROJECT, $DECISION_FILE;

    $username = getenv('RULEFLOW_USER');
    $password = getenv('RULEFLOW_PASSWORD');
    if (!$username || !$password) {
        fwrite(STDERR, "Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment.\n");
        exit(1);
    }

    // 1) Get a token.
    $token = ropc($RULEFLOW_AUTH, $RULEFLOW_REALM, $RULEFLOW_CLIENT, $username, $password);
    echo "Got a token.\n";

    // 2) GET /whoami — print tenant + roles.
    [$status, $whoami] = apiCall($RULEFLOW_API, 'GET', '/whoami', $token);
    if ($status !== 200) {
        fwrite(STDERR, "whoami failed: $status " . json_encode($whoami) . "\n");
        exit(1);
    }
    echo "whoami: tenant={$whoami['tenant']} roles=" . json_encode($whoami['roles']) . "\n";

    // 3) POST /engine/simulate with the shared decision.
    $model = json_decode(file_get_contents($DECISION_FILE), true);
    $decision = $model['decisions'][0];
    $inputs = ['credit_score' => 780, 'income' => 80000, 'age' => 40];

    [$status, $result] = apiCall($RULEFLOW_API, 'POST', '/engine/simulate', $token, [
        'decision' => $decision,
        'inputs' => $inputs,
    ]);
    if ($status !== 200) {
        fwrite(STDERR, "simulate failed: $status " . json_encode($result) . "\n");
        exit(1);
    }
    echo 'outputs: ' . json_encode($result['outputs']) . "\n";

    // --- Next steps (commented — request shape only, not executed) --------
    //
    // Run a *stored* decision (persisted in a project) synchronously:
    //
    //   [$status, $result] = apiCall($RULEFLOW_API, 'POST',
    //       "/projects/$RULEFLOW_PROJECT/decisions/<decisionId>/simulate",
    //       $token, $inputs);
    //
    // Start a workflow execution asynchronously (202 + execution id), then poll:
    //
    //   [$status, $result] = apiCall($RULEFLOW_API, 'POST',
    //       "/projects/$RULEFLOW_PROJECT/executions", $token,
    //       ['workflow' => '<name>', 'env' => 'dev', 'input' => $inputs]);
    //   $execId = $result['id'];
    //   [$status, $result] = apiCall($RULEFLOW_API, 'GET',
    //       "/projects/$RULEFLOW_PROJECT/executions/$execId", $token);
}

main();
