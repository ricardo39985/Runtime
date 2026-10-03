#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker compose --env-file .env -p runtime-local -f infra/compose.yaml down
# Deliberately no --volumes: user data survives normal shutdown.
