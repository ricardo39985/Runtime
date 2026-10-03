#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
set -a; source .env; set +a
export BASE_URL="http://localhost:${RUNTIME_PORT:-8080}"
docker compose --env-file .env -p runtime-local -f infra/compose.yaml restart core
for attempt in $(seq 1 30); do
  if curl -fsS "$BASE_URL/health/ready" >/dev/null; then
    python3 tests/integration/api.py --resume
    exit 0
  fi
  sleep 1
done
echo 'Core did not become ready after actual restart.' >&2
exit 1
