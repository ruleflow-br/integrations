#!/usr/bin/env ruby
# frozen_string_literal: true

# RuleFlow integration example — Ruby, standard library only
# (net/http, uri, json) — no gems to install.
#
# 3 steps: (1) get a bearer token via ROPC, (2) GET /whoami to confirm the
# tenant, (3) POST /engine/simulate with the shared loan_eligibility decision.

require "net/http"
require "uri"
require "json"

DECISION_FILE = File.join(__dir__, "..", "shared", "loan_eligibility.json")

RULEFLOW_API = ENV.fetch("RULEFLOW_API", "https://api.ruleflow.com.br/api")
RULEFLOW_AUTH = ENV.fetch("RULEFLOW_AUTH", "https://auth.ruleflow.com.br")
RULEFLOW_REALM = ENV.fetch("RULEFLOW_REALM", "ruleflow")
RULEFLOW_CLIENT = ENV.fetch("RULEFLOW_CLIENT", "ruleflow-cli")
RULEFLOW_PROJECT = ENV.fetch("RULEFLOW_PROJECT", "lending")

# Resource Owner Password grant against the public client ruleflow-cli.
# Returns the access token.
def ropc(username, password)
  uri = URI("#{RULEFLOW_AUTH}/realms/#{RULEFLOW_REALM}/protocol/openid-connect/token")
  res = Net::HTTP.post_form(uri, {
                               "grant_type" => "password",
                               "client_id" => RULEFLOW_CLIENT,
                               "username" => username,
                               "password" => password
                             })
  unless res.is_a?(Net::HTTPSuccess)
    warn "ROPC failed: #{res.code} #{res.body}"
    exit 1
  end
  JSON.parse(res.body)["access_token"]
end

# Calls the RuleFlow control-plane API. Returns [status, parsed_body].
def api_call(method, path, token, body = nil)
  uri = URI(RULEFLOW_API + path)
  req_class = method == "GET" ? Net::HTTP::Get : Net::HTTP::Post
  req = req_class.new(uri)
  req["Authorization"] = "Bearer #{token}"
  if body
    req["Content-Type"] = "application/json"
    req.body = body.to_json
  end

  res = Net::HTTP.start(uri.host, uri.port, use_ssl: uri.scheme == "https") do |http|
    http.request(req)
  end

  parsed = begin
    res.body && !res.body.empty? ? JSON.parse(res.body) : nil
  rescue JSON::ParserError
    res.body
  end
  [res.code.to_i, parsed]
end

def main
  username = ENV["RULEFLOW_USER"]
  password = ENV["RULEFLOW_PASSWORD"]
  if username.nil? || username.empty? || password.nil? || password.empty?
    warn "Set RULEFLOW_USER and RULEFLOW_PASSWORD in your environment."
    exit 1
  end

  # 1) Get a token.
  token = ropc(username, password)
  puts "Got a token."

  # 2) GET /whoami — print tenant + roles.
  status, whoami = api_call("GET", "/whoami", token)
  if status != 200
    warn "whoami failed: #{status} #{whoami}"
    exit 1
  end
  puts "whoami: tenant=#{whoami['tenant']} roles=#{whoami['roles']}"

  # 3) POST /engine/simulate with the shared decision.
  model = JSON.parse(File.read(DECISION_FILE))
  decision = model["decisions"][0]
  inputs = { "credit_score" => 780, "income" => 80000, "age" => 40 }

  status, result = api_call("POST", "/engine/simulate", token,
                             { "decision" => decision, "inputs" => inputs })
  if status != 200
    warn "simulate failed: #{status} #{result}"
    exit 1
  end
  puts "outputs: #{result['outputs']}"

  # --- Next steps (commented — request shape only, not executed) --------
  #
  # Run a *stored* decision (persisted in a project) synchronously:
  #
  #   status, result = api_call("POST",
  #     "/projects/#{RULEFLOW_PROJECT}/decisions/<decisionId>/simulate",
  #     token, inputs)
  #
  # Start a workflow execution asynchronously (202 + execution id), then poll:
  #
  #   status, result = api_call("POST", "/projects/#{RULEFLOW_PROJECT}/executions",
  #     token, { "workflow" => "<name>", "env" => "dev", "input" => inputs })
  #   exec_id = result["id"]
  #   status, result = api_call("GET",
  #     "/projects/#{RULEFLOW_PROJECT}/executions/#{exec_id}", token)
end

main if __FILE__ == $PROGRAM_NAME
