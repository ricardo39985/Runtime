# ADR 0001 — Deliver a real local foundation before enabling external authority

Status: accepted for the initial implementation milestone.

Preserve the approved C++/Svelte/TypeScript/PostgreSQL architecture. Use a deterministic CSV receivables workflow to test the complete local preview/edit/approval/result loop. It must identify simulations and missing natural-language planning visibly.

Production and live-provider startup fail closed. A development bearer token selects a fixed development owner/workspace; it is not a substitute for production identity. The bridge contains the provider-specific Jev adapter, but does not expose billable routing without a core-owned spending reservation.

Local-only job execution occurs in a short PostgreSQL transaction. No network call may be added inside that transaction. Before adding external tools, implement the planned leased dispatch, fencing, receipts, and unknown-outcome reconciliation. The existing structural DAG tests do not claim full type/resource/authorization validation for arbitrary AI plans.

A bounded worker pool keeps synchronous PostgreSQL operations off Drogon's event loop. A later measured optimization may replace per-operation connections with a pool; every reused connection must pass tenant-context reset tests. Do not introduce a second event-loop runtime.

The initial CI may commit only the newly generated dependency lock. This bootstrap exception is not authorization for coding agents to merge unrelated features or deploy production. All subsequent dependency changes require explicit lockfile diffs and frozen installs.
