# Runtime — application engine

Runtime builds persistent applications from a versioned application definition, not from a predefined business-workflow catalogue. The owner clarified the product boundary: specialized capabilities can arrive later; an application must still have its pages, data and file pipeline without them.

The default executable and web entry point now use the generic application engine. The previous invoice implementation remains in the tree as legacy source and regression tests; it is no longer the application runtime or default UI.

## Implemented in this branch

- **Application compiler:** arbitrary named entities, typed fields, relationships, pages, roles, versioned capability ports, strict validation and safe additive schema upgrades.
- **Generated interface:** an application designer plus JSON input, navigation, collection tables, typed create/edit forms, relationship/file selectors, missing-capability panels and a shared file workspace.
- **Persistence:** PostgreSQL application definitions/history, records, optimistic versions, normalized relationship links, archive protection and append-only events. Requests are scoped to a development workspace using forced row-level security and a restricted database role.
- **Asset pipeline:** authenticated upload, size/quota limits, SHA-256 integrity, app-scoped content deduplication, optional lossless Zstandard compression, immutable byte storage, metadata publication in the same transaction, verified original-byte download. Uploaded archives are stored, not extracted; images are not lossily recompressed.
- **Capability separation:** `media.paint` can be declared while absent. The app is valid and its records/files remain usable. Only that action is unavailable. Exact version and contract must match a registered implementation. A test installs a compatible implementation and invokes it through the unchanged definition.

No entity name, page name, or business-specific data schema is compiled into the engine. Examples are test inputs, not a product catalogue.

## Honest current boundary

This is a local-development implementation of the shared engine, not the completed public SaaS builder. Natural-language planning is **not connected**; definitions currently enter through the visual designer or AppSpec API. The present renderer covers data-oriented forms/tables/files; specialized interactive surfaces and richer layouts remain UI capabilities to add. Real sign-in/membership, deployed multiuser operation, external dispatch, schedules and model spending/credentials remain separate gates. The HTTP service runs as a fixed development owner; role-policy tests do not constitute implemented multiuser login.

The storage backend is presently transactional PostgreSQL BYTEA with an 8 MiB object limit and 128 MiB logical app quota. R2 multipart/streaming storage, garbage collection and disaster recovery are not implemented. Compression is a tested byte pipeline, not a claim of universal compression savings.

The only production-code capability registered here is a deterministic local record snapshot. There is no paint implementation, arbitrary plugin upload, billable model call, external email send or deployment. Unavailable actions return an explicit status; they never report invented success.

## Local run

Docker Compose v2 and Python3 are required. From the feature branch:

```sh
git clone git@github.com:ricardo39985/Runtime.git
cd Runtime
git switch feat/application-engine
./scripts/bootstrap.sh
./scripts/dev-up.sh
```

Open `http://localhost:8080` and enter `DEV_API_TOKEN` from your local `.env`. Do not share that file or expose the development port publicly. Create an application, define collections/fields and optional capabilities, then create records and upload files. Files and records are stored on the server, not in browser localStorage.

For an existing local database, after its container starts, run `./scripts/migrate-local.sh`. Migrations add application tables without deleting invoice-era data. Normal `./scripts/dev-down.sh` preserves the database volume.

## Tests and evidence

```sh
pnpm install --frozen-lockfile
pnpm check && pnpm test && pnpm build
cmake --preset domain && cmake --build --preset domain && ctest --preset domain
cmake --preset sanitizers && cmake --build --preset sanitizers && ctest --preset sanitizers
./scripts/test-integration.sh
./scripts/test-security.sh
./scripts/test-restart.sh
./scripts/test-e2e.sh
```

Native application checks additionally require OpenSSL, Boost.JSON headers and Zstandard development libraries; Docker installs its pinned dependencies. CI builds the actual server and database, tests two independent application definitions, editing/version conflicts, idempotency, missing capabilities, compressed file roundtrips, RLS, a real service restart and desktop/mobile browser flows.

`evidence/implementation-status.md` distinguishes actual passes from tests written but not yet executed. Do not infer a green full-stack result from source files or CI configuration.

## Continue from here

Read `AGENTS.md`, `docs/BUILD_PLAN.md` and `docs/TASKS.md`. The next generation layer must emit this general application contract from user intent and refine existing apps. Do not add more hardcoded vertical workflows as a substitute. UI, data, storage and capability contracts are platform foundations; specialized abilities extend the platform rather than determine which apps users may create.
