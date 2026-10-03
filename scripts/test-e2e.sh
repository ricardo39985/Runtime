#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
set -a; source .env; set +a
export BASE_URL="http://localhost:${RUNTIME_PORT:-8080}"
pnpm exec playwright test
