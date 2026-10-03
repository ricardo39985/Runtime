# Runtime coding-agent instructions

Read docs/BUILD_PLAN.md, docs/TASKS.md, and evidence/implementation-status.md before continuing. The implementation plan is the target, not a declaration that features already work.

Preserve C++23/Drogon core, Svelte 5 UI, TypeScript provider bridge and PostgreSQL authoritative state. Do not replace the core with a JavaScript backend or add NATS, Valkey, gRPC, Kubernetes, generated code/Wasm, or local GPU inference without a justified approved architecture decision.

The owner authorized the initial main push. Subsequent features use task branches unless explicitly instructed otherwise. Preserve concurrent changes, do not force-push shared branches, and obtain authorization for merges, billable provider calls, paid provisioning, real external sends, and production promotion.

For each task: inspect dependencies and current code; add behavior and failure tests; implement one coherent change; execute checks; record commands, exit codes, commit, browser evidence and limitations. Compiler/test output—not a model opinion—determines pass/fail. Inspect rendered UI for frontend changes. Mocks, source files and screenshots do not prove live-provider integration. Mark missing credentials or account approvals BLOCKED_EXTERNAL and continue independent work rather than weakening acceptance.

## Invariants

The core owns identity, tenant context, grants, budgets, workflow state, approvals and execution authority. The bridge translates provider protocols; it cannot select new recipients, tenants, capabilities or spending limits. No generic credential retrieval API.

Use a restricted application role, forced PostgreSQL RLS, composite tenant foreign keys and transaction-local tenant settings. Runtime roles must not own protected tables, be superusers or have BYPASSRLS. Test connection reuse and context reset. Audit records are append-only to the application.

The initial worker executes only deterministic local work and simulation in short transactions. Before external tools, implement leased/fenced dispatch, persisted intent, receipts and unknown-outcome reconciliation. Never hold a database transaction during provider/model I/O. Timeout does not prove an email was not sent. Never blindly retry an uncertain external write.

Approval binds the exact sender, recipient, content, source conditions, tenant, tool/version, proposal hash and expiry. Editing invalidates it. Recheck membership, grants, limits, approval and source conditions immediately before dispatch. Cancellation cannot undo a delivered message. Schedules cannot inherit permission for future sends.

Treat emails, files, CSV cells, model outputs and MCP descriptions as untrusted data. No arbitrary eval, HTML, JavaScript, SQL, shell execution, package installation, URL fetching or secret lookup. Render only registered components with server-owned data bindings. Unknown capabilities fail closed.

Use maintained cryptographic libraries. Encrypt credentials with authenticated encryption and versioned keys outside the database. Never put secrets/customer data in prompts, commits, screenshots, test fixtures or ordinary logs. Keep environments isolated.

Jev recommends a bounded processing route; it does not authorize actions, calculate invoices, or decide whether tests passed. Prefer deterministic functions and validated saved workflows. Keep lower-cost planning disabled until held-out evaluation passes. Reserve worst-case model usage before dispatch, including concurrency, reasoning, retries and repairs. Unknown usage stays reserved until reconciled. Never retry around a policy denial.

The example model YAML is not loaded by the fixture build. Probe actual account access and current model IDs/rates before activation. No silent model upgrades or unbounded agent loops.

## Evidence and release

Task evidence records task ID, commit, dependencies, changed contracts/migrations, commands/exit codes, deterministic and real-provider results, redacted browser evidence, failure/security cases, limitations, external blockers and next eligible task.

No production claims before actual identity/isolation/dispatch/budget/live-provider/recovery gates pass. Build immutable images and promote the tested digest. After database restoration, pause external writes and reconcile the recovery window before resuming. Do not run destructive automatic rollback migrations. Report partial work honestly.
