#!/usr/bin/env bash
set -euo pipefail
cat >&2 <<'MSG'
BLOCKED: this milestone is a development-only foundation, not a production release.
Required gates include Google OIDC, connection grants, signed external dispatch,
lease/reconciliation testing, durable model budgets, real provider verification,
routing evaluation, production secret handling, and an isolated restore drill.
No deployment or promotion has been performed.
MSG
exit 2
