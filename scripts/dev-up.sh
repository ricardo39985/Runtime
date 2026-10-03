#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -f .env ]] || { echo 'Run ./scripts/bootstrap.sh first.' >&2; exit 2; }
docker compose --env-file .env -p runtime-local -f infra/compose.yaml up --build -d --wait --wait-timeout 120
printf '\nRuntime is available on localhost at the RUNTIME_PORT configured in .env.\nUse DEV_API_TOKEN from .env to open the development workspace. No real emails can be sent.\n'
