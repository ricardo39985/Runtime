#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ -f .env ]] || { echo 'Run scripts/bootstrap.sh first.' >&2; exit 2; }
compose=(docker compose --env-file .env -p runtime-local -f infra/compose.yaml)
for migration in db/migrations/*.sql; do
  name=$(basename "$migration")
  version=${name%%_*}
  version=$((10#$version))
  applied=$("${compose[@]}" exec -T db psql -U postgres -d runtime -Atqc "SELECT count(*) FROM schema_migrations WHERE version=$version")
  if [[ "$applied" == 0 ]]; then
    "${compose[@]}" exec -T db psql -U postgres -d runtime -v ON_ERROR_STOP=1 -f "/migrations/$name"
  fi
done
