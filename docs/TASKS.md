# Application-platform implementation sequence

The owner's generic-app direction supersedes the original invoice T00–T24 product sequence. Reuse its security and operations work where relevant; do not continue adding vertical workflows instead of the engine.

1. **A01 Application contract/compiler:** generic entities, typed fields, roles, pages, capability requirements and non-destructive versioning. Evidence: arbitrary/unseen definitions and rejection cases. Implemented locally; see evidence for executed scope.
2. **A02 Durable data:** PostgreSQL apps/history/records/links, idempotency, optimistic edits, migration compatibility and app/tenant isolation. Native validation exists; HTTP/database acceptance must pass on actual stack.
3. **A03 Shared UI:** AppSpec-driven navigation/forms/tables/files; a development designer/JSON input; partial availability. Real browser and mobile acceptance required.
4. **A04 Storage/compression:** bounded uploads, immutable original hashes, identity/zstd choice, scoped deduplication, atomic metadata/content and verified downloads. Native byte tests pass; actual database/restart tests remain separately tracked.
5. **A05 Capability lifecycle:** absent requirements must not invalidate apps; exact version/contract resolver, trusted local registry, unchanged-spec activation tests. Extend with administrator-approved installation, signatures, capability input/output schemas and interactive surface registration.
6. **A06 Intent-to-application generator:** provider-independent adapter emits general AppSpec from text and existing app state; strict compilation, change preview, bounded repair, budget reservations and authorized model credentials. No predefined app chooser. This is the next product-facing layer, not another business workflow.
7. **A07 Real identity/access:** OIDC, sessions, CSRF, workspace/app membership, roles and source grants. Replace fixed development owner before public hosting; test each user role in real requests.
8. **A08 Rich behavior/UI:** typed local rules, state machines, aggregates, dashboards/calendars and optional custom-surface contracts. Preserve unavailable specialized ports rather than narrowing requested applications.
9. **A09 External abilities:** authenticated bridge, encrypted credentials, leases/fencing, exact approvals, durable receipts, unknown outcomes and safe scheduling. Provider-specific tools plug into the shared contract.
10. **A10 Object-store backend and operations:** R2 streaming/multipart, cleanup/GC, metrics/redaction, quotas/encryption, backups/restoration, recovery after lost receipts.
11. **A11 End-to-end generative acceptance:** unrelated unseen natural-language apps; create/refine without app-specific code; preserve data and assets; compatible ability installed later; multiuser authorization and restart/recovery.
12. **A12 Staging/pilot/release:** explicitly authorized infrastructure and immutable deployment; actual nontechnical-user acceptance and restore drill. No automatic production merge or paid provisioning.

Task reports include commit, actual test commands and exit codes, model/provider scope, browser evidence, known limitations and external blockers. Engine foundations A01–A05 are not a substitute for A06's natural-language product experience. Keep the whole target while distinguishing incomplete layers.
