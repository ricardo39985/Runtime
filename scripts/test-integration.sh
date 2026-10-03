#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
set -a; source .env; set +a
export BASE_URL="http://localhost:${RUNTIME_PORT:-8080}"
python3 tests/integration/application_api.py
# Restart the actual core process; database and object volumes must survive.
docker compose --env-file .env -p runtime-local -f infra/compose.yaml restart core
for ((i=0;i<30;i++)); do
  if curl --silent --fail "$BASE_URL/health/ready" >/dev/null; then break; fi
  sleep 1
done
python3 tests/integration/application_api.py --after-restart
