# Runtime — general application engine

Describe an application; Runtime composes its definition and provides its UI, persistent records, isolated customer data spaces, and private lossless file storage. Specialized abilities are independent, versioned dependencies—not a predefined workflow menu.

The active C++ executable is the **general application server**, not the original invoice demo. It accepts application-defined entities, typed fields and relationships, pages/forms/tables/cards, roles, actions and storage policy. The Svelte studio renders those definitions without application-specific code. Example JSON files are portable sample definitions, not built-in product choices.

## Current implementation

- Versioned AppSpec compiler with graph/type/role validation, exact decimal integer strings, record references, uniqueness and file links.
- Generic PostgreSQL CRUD, customer data-space isolation, permissions, idempotent creation, optimistic version checks, audit events and persistent application revisions. Additive/defaulted upgrades preserve existing records; destructive/narrowing changes are rejected pending a migration.
- Generic Svelte application library, schema review/publishing, dynamic pages and controls, record editing/deletion/pagination, assets and dependency visibility.
- Missing abilities remain declared and block only their actions. A real installed `core.text.word_count@1` handler demonstrates typed inputs, output persistence and idempotent execution. `media.paint@1` is deliberately **not implemented**.
- Persistent ObjectStore interface and private local-volume backend. Bounded 8 MiB originals, configurable lower per-app limits, quota accounting, immutable opaque keys, atomic publication, lossless Zstandard when useful, otherwise identity encoding, and SHA-256 verification on restoration. No forced lossy conversion.
- Optional natural-language composition through an authenticated TypeScript model bridge. Generates a full definition preview, validates it in C++, and publishes only after review. Model/version/pricing must be explicitly configured. Durable usage reservations and uncertain-outcome handling are included; credentials and billable calls are disabled by default.

This is a development implementation, **not a deployed public SaaS**. Fixed development identities, local object storage and reviewed pure capabilities are real local functionality; they do not substitute for production identity, remote capability dispatch, R2, full file lifecycle or production recovery.

## Run locally

Requirements: Docker with Compose v2, Python 3 and network access for first dependency downloads.

```sh
git clone git@github.com:ricardo39985/Runtime.git
cd Runtime
git switch feat/general-application-engine
./scripts/bootstrap.sh
./scripts/dev-up.sh
```

Open `http://localhost:8080` and use `DEV_API_TOKEN` from the generated local `.env`. Never commit or share that file. Existing local databases are migrated without deleting volumes. `./scripts/dev-down.sh` preserves both PostgreSQL and uploaded objects. Do not expose the development gateway to the public internet.

With no model configured, use **Build an application → Import JSON** to publish an AI-authored definition. `examples/creative-studio.json` demonstrates missing painting without disabling data/files. `examples/editorial-desk.json` demonstrates a different schema and a real installed word-count action. Import and edit arbitrary definitions that fit the declared platform contract; the engine does not select between these examples.

## Enable natural-language composition deliberately

Set `PLANNER_ENABLED=true`, `PLANNER_MODEL` to a model actually available to the authorized account, and `OPENAI_API_KEY` in your private `.env`. Set current `PLANNER_INPUT_MICROUSD_PER_MILLION` and `PLANNER_OUTPUT_MICROUSD_PER_MILLION` to conservative account-verified prices (one dollar = 1,000,000 microdollars). The configured per-generation and monthly guards must cover the maximum reservation; the core refuses an insufficient budget instead of silently sending a call.

Recreate the local services after configuration. Describe the complete application or the desired change. Review the generated AppSpec and unresolved dependencies before publishing. The same ambiguous generation is not automatically resent. Configured model access is not evidence that a live-provider acceptance test has passed; consult the implementation evidence.

## Checks

```sh
pnpm install --frozen-lockfile
pnpm check && pnpm test && pnpm build
cmake --preset domain && cmake --build --preset domain && ctest --preset domain
cmake --preset sanitizers && cmake --build --preset sanitizers && ctest --preset sanitizers
./scripts/test-integration.sh  # real stack, customer isolation, storage and restart
./scripts/test-security.sh    # actual PostgreSQL RLS/privilege checks
./scripts/test-e2e.sh         # installed Playwright Chromium required
```

The CI workflow builds the actual core/bridge/web, runs the real database and application, exercises generic applications and file restoration, restarts the core, and checks desktop/mobile browser behavior. Read the actual CI result and `evidence/implementation-status.md`; a test existing is not proof it passed.

## Continue development

Start with `AGENTS.md`, `docs/adr/0002-general-application-engine.md`, and `docs/TASKS.md`. The earlier invoice-oriented build plan is historical; the general application architecture and the owner's subsequent instructions govern new work. Add generic capabilities without reducing the requested application to a prewritten template.
