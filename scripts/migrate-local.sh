#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -f .env ]] || { echo 'Run ./scripts/bootstrap.sh first.' >&2; exit 2; }
# Only the dedicated local development database; no destructive migrations.
for version in 002_job_lock_privilege.sql 003_application_engine.sql; do
  docker compose --env-file .env -p runtime-local -f infra/compose.yaml exec -T db \
    psql -U postgres -d runtime -v ON_ERROR_STOP=1 -f "/migrations/$version"
done
