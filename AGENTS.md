# Runtime engineering instructions

## Product boundary — authoritative correction

Runtime is a general SaaS application builder, NOT a catalog of predefined workflows. Read docs/adr/0002-general-application-engine.md first. The owner explicitly requires universal UI, data persistence, storage/compression, and abilities that can be added independently. A missing paint implementation must leave projects, permissions, files, forms and saved records usable. Do not replace a requested app with a smaller unrelated workflow.

The executable now uses services/core/src/studio_main.cpp and the generic application engine. Legacy invoice code is historical, not a feature template or the active application server. Example definitions are data and test fixtures; never switch application behavior by example name.

## Architecture

Preserve C++23/Drogon core, Svelte5/TypeScript UI, TypeScript/Node provider bridge and PostgreSQL authoritative state. ApplicationSpec describes typed entities/relations, roles, UI pages, capability dependencies/actions and storage policy. Capabilities have immutable versions and typed inputs/outputs; they cannot be marked installed without a real handler. Unknown handlers fail explicitly while independent application features remain available.

Schema revisions preserve user data and IDs. Reject destructive or narrowing changes until an explicit reviewed migration is provided. Keep published definitions separate from individual customer data spaces. Authorization is server-derived, never a user-supplied role/tenant selector. Current fixed local development identities do not satisfy public OIDC/membership onboarding.

Keep forced PostgreSQL RLS, restricted application roles, composite scope foreign keys, transaction-local scope, audited changes, optimistic record/app versions and idempotent writes. Test concurrent updates and isolated spaces. The object store uses opaque keys rather than user paths, immutable bytes, finite decompression, checksums and a persistent volume; metadata carries authorization. Do not erase the volume or make uploads public to fix a test.

No external provider calls inside database transactions. Current installed application abilities are reviewed pure handlers. Before remote side effects, add fenced dispatch, exact authorization, receipts and uncertain-outcome reconciliation. Timeouts do not prove a provider did nothing. Missing abilities are not an excuse for fabricated receipts.

Natural-language composition produces an AppSpec preview, not a template ID. Validate all output. Model inference requires explicit configuration and durable worst-case usage reservation. No secrets in schemas, prompts, logs or screenshots. Unknown provider usage remains reserved; do not automatically repeat a costly uncertain call. Published CRUD must not require reasoning calls.

No arbitrary generated SQL, HTML, JavaScript, shell commands, URLs or executables. Extend reviewed component/operation/capability registries when needed. User content and model output are untrusted data. Specialized compute belongs behind bounded authenticated capability interfaces, not eval.

## Work and evidence

The current feature branch is feat/general-application-engine. Preserve other changes. Do not merge main, provision paid services, enable billable calls, send real external messages or deploy production without explicit authorization. A request to implement code permits feature-branch code and ordinary CI, not production access.

Read evidence/implementation-status.md and docs/TASKS.md. Write tests, run real commands, inspect actual browser behavior and record exit codes. Do not call a mocked provider a live integration. Do not equate written code or a green isolated suite with a working deployment. Record external blockers without weakening acceptance. Follow through on CI failures and preserve executed evidence. Keep user data and secrets out of artifacts.
