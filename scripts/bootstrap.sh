#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
umask 077
python3 - <<'PY'
from pathlib import Path
import secrets
p=Path('.env')
text=p.read_text() if p.exists() else 'APP_ENV=development\nRUNTIME_PORT=8080\nPLANNER_ENABLED=false\n'
existing={line.split('=',1)[0] for line in text.splitlines() if '=' in line and not line.startswith('#')}
for key in ['DEV_API_TOKEN','DEV_READER_TOKEN','BRIDGE_TOKEN','DB_PASSWORD','APP_DB_PASSWORD']:
    if key not in existing: text+=f'{key}={secrets.token_hex(32)}\n'
p.write_text(text)
p.chmod(0o600)
PY
command -v docker >/dev/null || { echo 'Docker with Compose is required for the full local stack.' >&2; exit 2; }
docker compose version >/dev/null
[[ -f pnpm-lock.yaml ]] || { echo 'The committed dependency lock is missing. Restore it before building.' >&2; exit 2; }
echo 'Local configuration ready. Credentials remain in the ignored .env file.'
