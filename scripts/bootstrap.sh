#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
umask 077
if [[ ! -f .env ]]; then
  python3 - <<'PY'
from pathlib import Path
import secrets
values = ['APP_ENV=development','PROVIDER_MODE=fixture','RUNTIME_PORT=8080']
for name in ['DEV_API_TOKEN','BRIDGE_TOKEN','DB_PASSWORD','APP_DB_PASSWORD']:
    values.append(f'{name}={secrets.token_hex(32)}')
Path('.env').write_text('\n'.join(values)+'\n')
PY
fi
command -v docker >/dev/null || { echo 'Install Docker with Compose to run the complete local stack. Domain tests do not need Docker.' >&2; exit 2; }
docker compose version >/dev/null
if [[ ! -f pnpm-lock.yaml ]]; then
  echo 'Resolving the initial dependency lock in an isolated Node container.'
  docker run --rm -v "$PWD:/src" -w /src node:24-bookworm-slim sh -c 'npm install -g pnpm@10.11.0 >/dev/null && pnpm install --lockfile-only --ignore-scripts'
fi
echo 'Local configuration ready. The access token is in .env; it is never committed.'
