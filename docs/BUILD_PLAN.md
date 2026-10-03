# Runtime MVP — repository implementation specification

This is a condensed repository edition of the full implementation package supplied with the project. The original product target remains authoritative; this file makes its boundaries available to repository agents. See TASKS.md for order and ../evidence/implementation-status.md for verified progress.

## Required product loop

Authenticated objective → supported capabilities and data → validated workflow → generated reviewable interface → frozen action proposal → exact approval → verified execution → persistent, reusable mini-app. Ask a necessary clarification or explain unsupported requests instead of fabricating success.

Three required real-account demonstrations:

1. Receivables: CSV, selected Sheets or read-only Stripe; deterministic overdue filtering and separate currency totals; editable drafts; exact approval; actual permitted test email and provider verification. Exclude paid/void/disputed/suppressed/zero balances. Recheck live sources before send; explicitly acknowledge snapshot-only sources. Ten-action approval batches, repeated-invoice follow-up suppression.
2. Daily operations: selected calendar and authorized Gmail reading; tomorrow's appointments and source-backed unanswered quote candidates; local tasks and approved no-attendee calendar creation. Show timezone, query window, incomplete searches and uncertainty.
3. Previously unseen quote-tracker mini-app: compose general data/filter/sort/collection/form/table capabilities rather than selecting a hardcoded demo. Test ten unseen variations, saved parameters, current-data reruns, restart and reopen.

The initial fixture-only invoice workflow is a foundation for these demonstrations, not completion of them.

## Stack and infrastructure

C++23/GCC14/Drogon modular core, CMake/Ninja/pinned vcpkg. Svelte5/SvelteKit2 static SPA served on the core origin. TypeScript/Node24/Fastify private provider bridge. PostgreSQL17 authoritative data, queue, workflow state, approvals, events and usage. Build-time JSON Schema validators; constrained UI composition, never generated browser source per request.

Target hosting: separate local/staging/production Render resources, core/private bridge/paid PostgreSQL in one region. Private Cloudflare R2 object storage using supported S3 operations. GitHub Actions, immutable container images, OpenTelemetry and redacted structured logging/Grafana Cloud. Verify actual tiers, region, private TLS and pricing before requesting provisioning. Initial proposed infrastructure allowance $200/month, not a quote. No paid resources are created by this repository.

Local Docker Compose exposes only the loopback gateway, with persistent PostgreSQL and synthetic providers. Production must reject dev authentication and fixtures. Current binaries reject all live/production startup until those gates exist. No NATS/Valkey/gRPC/Kubernetes/Wasm in the MVP without a measured, reviewed need.

## Identity, isolation and provider access

Google OIDC: issuer/subject identity, signature/issuer/audience/expiry, state/nonce and single-use OAuth transaction. Login does not confer Gmail access. Opaque random server-managed sessions with hashed storage, Secure/HttpOnly/SameSite cookies, CSRF and Origin checks, rotation and expiry. Owner/operator/viewer membership, expiring email-bound invitations, explicit per-user/workspace connection grants and revocation.

Separate migration and runtime roles. Forced RLS, transaction-local tenant settings, composite tenant foreign keys and context-reset tests. Encrypt provider credentials with authenticated encryption, tenant/connection-bound associated data and versioned keys outside the database. No generic secret API; model prompts and normal logs never receive tokens.

The bridge executes only server-controlled tool operations. External dispatch needs authenticated private transport and a reviewed signed grant bound to tenant, tool/version, payload hash, approval, dispatch ID, fencing generation and expiry. Browser-selected workspace IDs are not authority.

## Canonical contracts

ToolManifest: immutable ID/version, finite schemas, required scopes/resources, effect class, approval, timeout, retries, idempotency/verification strategy, provider hosts, pagination/completeness, limits and data classification. Models and MCP prose cannot redefine risk.

WorkflowSpec: bounded typed DAG with read/filter/join/aggregate/sort/map/model-transform/render/internal-write/prepare/approval/execute/verify nodes. Validate cycles, references, types, tenant/resource grants, bounded fan-out, immutable capabilities, approval ancestry and budget. No arbitrary SQL or executable expressions.

UISpec: registered workspace/section/metric/table/chart/form/text/status/source/action/approval/timeline components. Data references bind server results; models do not invent displayed totals. Include loading, empty, partial, stale, failed, expired approval and reconnect states. Compile Draft7 JSON Schema in C++ and TypeScript; positive and negative fixtures must agree, with remote schema fetching disabled.

ActionProposal freezes exact content, sender/recipients or calendar fields, source preconditions, tenant, tool/version, hash, count, cost and expiry. Money uses integer minor units plus currency, JSON strings for large integers; no implicit currency conversion. UTC instants retain IANA zones; all-day dates stay dates.

## Execution and recovery

Persist a request before 202. Atomic PostgreSQL job claims with leases, heartbeats, fencing, attempts, bounded retry/backoff/Retry-After and account/provider concurrency. Commit state/events together; publish after commit. Never hold a transaction during external/model I/O.

Persist dispatch intent before transmission. A provider can succeed before its response is lost: represent unknown outcome, reconcile via receipts/readback, and do not blindly resend. There is no universal exactly-once guarantee across PostgreSQL and third parties. Gmail Message-ID is not assumed to make sending idempotent. Validate provider-supported Calendar client IDs. Cancellation stops undispatched work, not already delivered actions.

Approvals expire after 30 minutes and bind exact principal/tenant/version/payload. Edits invalidate them. Recheck grants, limits, freshness and source preconditions before dispatch. Scheduled refresh/draft preparation must not inherit future-send approval. Specify DST behavior and coalesce missed refreshes rather than replaying historical sends.

Authenticated WebSocket subscriptions replay durable sequence-numbered events, with authorized polling fallback. Closing the browser does not destroy persisted runs. The current local transactional worker/polling UI does not yet implement the full external lease/streaming design.

## Models and budgets

Proposed roles: Jev jev-1.13.0 bounded router; routine gpt-6-luna; new-workflow planner gpt-6.1-sol; bounded escalation gpt-6-astra; separately evaluated claude-sonnet-5-5 fallback. This is an example catalog, not enabled access. Verify IDs, account capabilities and rates before activation.

Deterministic functions and saved validated workflows bypass models. New requests default to the planner. Jev receives a small redacted envelope and cannot grant tools, authority or budget. Router failure falls back conservatively. Permit one schema repair and one bounded escalation, not retries around a denial.

Before cheaper planning activation, evaluate 300 tasks against fixed planning with held-out adversarial cases. Proposed gate: at least 20% measured cost reduction, no safety regression, and at most two percentage points lower functional success. Otherwise retain the fixed planner.

Reserve worst-case billable usage before dispatch, including reasoning, nested calls, repairs, retry and concurrency. Unknown usage stays reserved. Proposed guards: 24 DAG nodes, 30 tool calls, 8 generative calls, 2 router calls, 100 mapped items, 10 actions/approval, 20 global and 4 workspace concurrent runs, 300 automated seconds excluding human approval wait. Proposed $0.25 initial run allowance, $1 explicitly approved ceiling, $100 global monthly model guard; these are limits, not predicted charges.

## Scope and release gates

Include selected Sheets, Gmail send and separately gated reading, Calendar read/no-attendee creation, read-only Stripe, CSV and finite-field collections, saved apps/safe schedules, one operator-controlled read-only MCP reference server. Exclude arbitrary browser automation/MCP installation/generated code, payment/refund operations, destructive provider tools, bulk marketing, autonomous scheduled sending, calendar invitations, native apps and marketplaces.

Google mailbox-reading access is an external verification/security-assessment dependency. Keep it gated while implementing send-only authorized tests. OAuth testing may require reconnect; implement it rather than assuming refresh tokens last indefinitely. Real account tests are required for scopes/Picker/provider restrictions.

Before launch: privacy/retention/deletion/secret rotation, redacted observability, bounded spending, adversarial and concurrent tests, provider outage/timeout/restart/recovery testing, actual provider receipts, nontechnical pilot, separate staging and successful isolated backup restore. Managed backups without a demonstrated restoration are insufficient. After DB restore, pause writes and reconcile actions whose records may have disappeared. Production requires explicit owner promotion of tested immutable images.
