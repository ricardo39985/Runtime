#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -f .env ]] || { echo 'Run scripts/bootstrap.sh first.' >&2; exit 2; }
docker compose --env-file .env -p runtime-local -f infra/compose.yaml up -d --wait db
./scripts/migrate-local.sh
docker compose --env-file .env -p runtime-local -f infra/compose.yaml up --build -d --wait --wait-timeout 180
printf '\nRuntime Studio is ready on the configured localhost port. Use DEV_API_TOKEN from .env.\n'
