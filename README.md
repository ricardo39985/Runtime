# Runtime

Intent-native software: describe an operation, assemble a constrained workspace, review its proposed actions, and execute only what was approved.

**Current milestone: local-development foundation, not the complete MVP.** The C++ core, Svelte interface, PostgreSQL queue/audit storage, and TypeScript bridge are implemented. The first workflow imports invoice CSVs, calculates overdue balances, renders a schema-driven workspace, supports draft editing, and requires exact-version approval before producing **simulated receipts**. It sends no emails. Natural-language planning is not connected yet.

## Run locally

Requirements: Docker with Compose v2 and Python 3. Initial builds need internet access to download dependencies. Use a fresh checkout:

```sh
git clone git@github.com:ricardo39985/Runtime.git
cd Runtime
./scripts/bootstrap.sh
./scripts/dev-up.sh
```

Open `http://localhost:8080`. Copy the `DEV_API_TOKEN` value from the local `.env` file into the development sign-in form. The bootstrap script generates random local credentials; do not commit or share `.env`.

Load the sample CSV, build the workspace, edit a draft, acknowledge the source snapshot, and choose **Approve simulation**. Two simulated receipts should appear. History and event replay persist in PostgreSQL, including across a service restart.

`./scripts/dev-down.sh` stops services without deleting the database volume. Local services bind the public gateway to loopback. Do not expose this development build to the internet. Both services refuse live/production mode.

## Stack

- C++23, Drogon, OpenSSL, PostgreSQL client; CMake/Ninja and a pinned vcpkg baseline.
- Svelte 5, SvelteKit 2 static application, TypeScript, Draft 7 JSON Schema/AJV.
- Node 24, TypeScript, Fastify integration bridge.
- PostgreSQL 17 with a separate restricted application role, forced row-level security, composite tenant keys, transactional jobs, and append-only events.

The core is authoritative. Models do not calculate money, grant permissions, or approve actions. The bridge contains a tested Jev System One adapter with conservative fallback; it is **not enabled for billable calls** until usage reservations, credentials, and evaluation gates are implemented.

## Checks

```sh
./scripts/check.sh --domain       # standard C++ domain and isolated Jev tests
cmake --preset sanitizers
cmake --build --preset sanitizers
ctest --preset sanitizers
./scripts/test-integration.sh    # running Docker stack required
./scripts/test-security.sh       # PostgreSQL isolation tests
./scripts/test-e2e.sh             # pnpm and Playwright Chromium required
```

Full frontend checks use Node 24 and `pnpm install --frozen-lockfile`, then `pnpm check && pnpm test && pnpm build`. On the initial network-enabled CI run only, the missing dependency lock is generated and committed; subsequent installs are frozen. Review the generated lock before relying on reproducible builds.

CI builds both containers, starts the real local stack, exercises HTTP approvals and database isolation, and captures desktop/mobile browser evidence. A workflow existing in the repository is **not proof of a green run**. See `evidence/implementation-status.md` and the actual CI result.

## Implemented boundaries

Integer minor-unit money; separate totals per currency; bounded CSV parsing; exclusion of paid/disputed/void/suppressed invoices; idempotent request creation; at most ten actions per approval; frozen payload hash and version; 30-minute approval lifetime; editing invalidates approval; duplicate approval submissions do not create duplicate simulation jobs; cancellation of undispatched work; durable event replay; no arbitrary code or HTML execution.

The workflow validator and conservative model router have isolated tests. The current UI exposes one deterministic receivables workflow, not a generic natural-language planner. The local worker uses short transactions for local operations only; it is not an external-dispatch lease/reconciliation implementation.

## Remaining MVP work

Google OIDC and secure sessions; memberships and connection grants; encrypted credentials; live Google/Stripe adapters; durable external dispatch with unknown-outcome reconciliation; model spending reservations and provider integration; arbitrary validated workflow composition; saved mini-apps and schedules; WebSocket replay; production observability, deployment, and backup restoration.

`./scripts/release-check.sh` deliberately fails until release blockers are resolved. No paid infrastructure or external accounts are provisioned by this repository.

Read `AGENTS.md`, `docs/BUILD_PLAN.md`, and `docs/TASKS.md` before continuing. The plan is the product target; the implementation-status file states what exists now.
