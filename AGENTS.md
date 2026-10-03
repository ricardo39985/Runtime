# Runtime agent instructions

## Product boundary — supersedes the invoice-first roadmap

Runtime is a general application-generation platform. The primary artifact is a persistent, versioned AppSpec: data/entities/relationships, UI/pages, roles, behavior and capability requirements. Workflows are components of applications, not a predefined menu of products.

A missing specialized capability MUST NOT invalidate or erase the rest of an application. Compile its known requirements, build the available UI/data/storage, and mark only the dependent feature unavailable. Installations must satisfy exact contracts/versions before activation. Do not fake a result or silently substitute a lesser ability.

UI generation, typed persistence, versioned schema evolution and the asset/compression pipeline belong to the shared platform. Examples such as a painting studio, support desk or booking system must remain externally supplied AppSpecs/tests. No entity names or business-specific routes in the core. A visual designer is a development input, not a replacement for the requested natural-language product.

## Stack and working procedure

Preserve C++23/Drogon, Svelte5/TypeScript, Node24 integration bridge and PostgreSQL authority. Use task branches, preserve concurrent changes and do not force-push shared history. No main merge, paid resources, billable model evaluation, real external actions or production deployment without owner authorization.

Read current contracts/code, BUILD_PLAN, TASKS and evidence first. Add behavioral/failure tests; implement a coherent change; execute commands; record exit codes and scope. Inspect real browser output. Missing credentials/permissions are BLOCKED_EXTERNAL, not a reason to replace integration behavior with an unlabelled mock.

## Core invariants

The C++ core owns validation, identity/grants, tenant context, budgets, application versions, action authorization and durable state. The bridge translates provider protocols; it cannot invent new authority. Models propose application definitions, not arbitrary executable code or SQL. Generated UI uses registered components and trusted data bindings, not raw HTML/eval.

Use restricted DB roles, forced RLS, composite scoped foreign keys and transaction-local tenant context. No runtime superuser or BYPASSRLS. Persist application history and audit. Reject stale writes. Additive schema changes must preserve records; destructive changes need explicit migrations/backfill. File IDs and relationships cannot cross app/tenant boundaries.

Assets require bounded upload/decompression, original-byte hashes, lossless recovery, authenticated reads, metadata/blob consistency and quotas. Compression is chosen only if useful. Treat content and filenames as untrusted; do not execute or unpack uploaded files. Byte hashes are not authorization. App-scoped deduplication must not expose another tenant's content. Move large objects to an object-storage backend with crash/reconciliation tests rather than silently removing integrity guarantees.

The current local capability registry executes only trusted deterministic, read-only functions. Do not add external/provider I/O inside a database transaction. External capabilities require persisted dispatch, grants/approval where consequential, fencing/idempotency and unknown-outcome reconciliation. An API timeout is not evidence an action failed remotely.

Real roles require authenticated principals and memberships; a role argument in a unit test is not implemented sign-in. Public/live startup remains disabled until its security gates pass. No tokens in model prompts, commits, screenshots or ordinary logs. Maintain encryption/rotation/redaction/retention boundaries.

Jev may route bounded requests; it cannot approve actions or prove tests passed. Reserve worst-case billable calls before dispatch, including reasoning, retry and concurrency. Unknown usage stays reserved. Keep provider/model IDs configurable and verify authorized account access before enabling calls. No hidden downgrade from app generation to predefined workflows.

## Evidence

Record task/commit, dependencies, changed interfaces/migrations, actual commands/results, real-provider scope, browser screenshots, failure cases, remaining blockers and next task. A scaffold/mock/CI file is not proof. Never label the public product complete merely because the local engine works. Keep the full architecture while reporting implementation progress precisely.
