#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
docker compose --env-file .env -p runtime-local -f infra/compose.yaml exec -T db sh -c 'PGPASSWORD="$APP_DB_PASSWORD" psql -h 127.0.0.1 -U runtime_app -d runtime -v ON_ERROR_STOP=1' < tests/integration/application_rls.sql
