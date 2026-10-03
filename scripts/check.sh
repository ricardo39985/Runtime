#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake --preset domain
cmake --build --preset domain
ctest --preset domain
node --experimental-strip-types --test services/bridge/src/jev.test.ts
if [[ "${1:-}" != --domain ]]; then
  pnpm install --frozen-lockfile
  pnpm check
  pnpm build
fi
