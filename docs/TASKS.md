# Dependency-ordered implementation tasks

Condensed roadmap corresponding to the supplied 25-task package. Read BUILD_PLAN.md, ../AGENTS.md and current implementation evidence. Each task requires executed acceptance evidence, not just a scaffold. Missing account approvals or credentials are BLOCKED_EXTERNAL; do not replace real-provider gates with mocks.

| ID | Deliverable | Dependencies | Acceptance evidence |
|---|---|---|---|
| T00 | Bootstrap decisions/access/ADRs | None | Confirm repository/toolchain/account availability and authorization; record blockers, never secrets. |
| T01 | Monorepo/build/local stack/CI | T00 | Fresh checkout, frozen lockfiles, core/web/bridge build, actual PostgreSQL health and test results. |
| T02 | Shared contracts | T01 | Draft7 tool/workflow/UI/action positive/negative fixtures agree in C++ and TypeScript; no remote schema resolution. |
| T03 | Database and isolation | T01,T02 | Migrations, restricted role, forced RLS, composite tenant FKs, cross-tenant and context-reset tests. |
| T04 | Real identity/session/membership | T03 | OIDC validation, secure cookies, CSRF, logout/rotation/expiry, roles and connection grants. |
| T05 | Credential broker/private dispatch | T02,T03,T04 | Authenticated encryption, scoped bridge, token refresh/revocation, signed grant/replay/hash/tenant checks. |
| T06 | Durable jobs/events/recovery | T03 | Leases/fencing/heartbeat/cancel/retry, atomic state/events, restart recovery and replay; no provider I/O in DB transactions. |
| T07 | Capability registry/workflow compiler | T02,T04,T06 | Immutable effects, typed DAG validation, grants, bounded fan-out and approval barriers. |
| T08 | UI shell/schema renderer | T02,T04,T06 | Responsive accessible registered components, trusted bindings/actions, all loading/error/reconnect states and browser evidence. |
| T09 | CSV/collections/deterministic operations | T03,T07,T08 | Bounded parsing, exact money/dates/IDs, invalid record handling, finite collections, provenance and correct aggregates. |
| T10 | Drafts/exact approval/receipts | T05,T06,T07,T09 | Stale edits/expiry/principal/hash/source checks, cancellation and concurrent duplicate approvals. |
| T11 | Real Gmail send-only loop | T05,T10 | Owner-authorized allowlisted test send and provider readback, duplicate protection, lost-response reconciliation. |
| T12 | Model gateway/reservations/planner | T02,T05,T07,T09 | Real account probes, typed bounded plans, schema repair, concurrent spending reservations and uncertain-usage reconciliation. |
| T13 | Jev calibration | T12 | 300-task fixed-planner comparison, held-out safety/cost/success gates, conservative failure handling; no premature cheap routing. |
| T14 | Selected Sheets | T05,T09,T11 | Least-privilege Picker grants, selected-file boundaries, paginated real reads, provenance/freshness and pre-send recheck. |
| T15 | Read-only Stripe | T05,T09,T11 | Restricted test key, exact amounts/currencies/statuses, cursor/completeness and source revalidation; no payment/refund tools. |
| T16 | Calendar | T05,T10,T12 | Real selected-calendar reads, zone/all-day/DST handling, approved no-attendee event, conflict recheck and verified receipt. |
| T17 | Gated Gmail reading/daily workspace | T12,T16 | External access gate, real bounded reads, source-backed candidates, completeness and prompt-injection tests. |
| T18 | Saved mini-apps/schedules | T06,T07,T08,T12 | Current-data rerun, editable parameters, ten unseen compositions, restart/reopen, revocation/DST/coalescing and no future-send inheritance. |
| T19 | Controlled read-only MCP | T05,T07,T12 | Operator allowlist, pinned metadata, schema/host/risk boundaries and poisoned-description tests. |
| T20 | Observability/privacy/retention | T03,T06,T12 | Redacted logs/traces, queue/usage metrics, frontend errors, audit access, deletion and key rotation. |
| T21 | Adversarial/concurrency/recovery/load | T11–T20 | Tenant/replay attacks, provider outages, approval/budget races, crash/cancel/restart and measured browser/load tests. |
| T22 | Staging/backup restore | T20,T21 | Authorized isolated infrastructure/secrets, private TLS, health/runbooks/rollback and demonstrated restore/reconciliation. |
| T23 | Pilot acceptance/runbooks | T22 | Nontechnical invited users complete three real-account demos and unseen variants; defects and measured costs recorded. |
| T24 | Production promotion | T23 | Explicit approval of tested immutable images/configuration, all blockers resolved, controlled rollout/rollback/monitoring. |

## Present milestone

One deterministic CSV receivables workflow, local simulated actions, draft editing, exact approvals, PostgreSQL jobs/events, tenant-isolation schema, Svelte interface, isolated Jev adapter tests and CI. This is partial progress across foundation tasks, not a generic autonomous planner or completion of the MVP.

Finish full-stack verification, then T04/T05 and external-dispatch T06 before real sends. T12 spending reservations precede billable routing. The current example configuration is disabled.

## Evidence template

Task/commit; checked dependencies; changed contracts/migrations; actual commands and exit codes; deterministic results; real-provider scope; redacted browser evidence; failure/security cases; limitations; external blockers; next eligible task.
