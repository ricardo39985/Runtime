# General application engine — execution priorities

This supersedes the invoice-first roadmap. The product contract is in adr/0002-general-application-engine.md. Applications, not predefined workflows, are the unit of composition.

1. Verify the current generic engine end to end: publish arbitrary definitions, operate their forms/records, attach exact restored files, enforce customer isolation, reject stale/narrowing changes, and reopen after a process restart. Exercise at least two structurally different applications plus variations with new entity/field/action IDs.
2. Verify natural-language composition with an authorized model account and configured prices. Request an application absent from examples, inspect the generated complete definition, publish it, add records, then request a compatible revision. The planner must preserve missing capability contracts instead of reducing scope.
3. Add real identity and application/customer onboarding. Replace fixed development principals with verified OIDC, secure sessions, invitation/membership administration, CSRF defenses and revocation. Reuse and test existing server-side entity/space/storage permissions.
4. Expand the generic UI and operation vocabulary when required by real app requests: additional layouts, dashboards, search/filter/sort, validation/rules, workflow/state primitives, scheduling and realtime. A new app must not require its own C++ controller or hardcoded Svelte page.
5. Add a real specialized ability through the versioned registry/authorized execution interface. For example, painting consumes a declared prompt and produces an authorized stored asset. Prove that an already published app and its records stay intact while its formerly unavailable action becomes usable. Remote side effects require durable authorized dispatch, budget limits, cancellation and uncertain-outcome reconciliation.
6. Complete file lifecycle and production storage: resumable large uploads, paginated browsing, safe deletion/reference lifecycle, orphan/pending reconciliation, approved type-specific transforms, private R2 adapter, encryption/key management, retention/export/restore. Lossless platform compression must not silently transcode originals.
7. Harden publishing and migrations: explicit reversible migration plans, indexed queries, advanced app roles/row rules, tested rollback, audit and versioned capability upgrades. Preserve customer data and app revisions.
8. Complete production operations, load/adversarial testing, authorized pilot and restore drill. Promote immutable tested artifacts only after explicit approval.

## Definition of useful acceptance

A creative studio with unavailable painting still has a working project model, forms, records and files. An unrelated editorial app uses the same engine and a real registered capability. An unforeseen user request composes a new app rather than selecting either fixture. Ordinary operations are deterministic and persist without model calls. No test bypasses missing implementation by labeling it successful.

## Evidence per change

Record commit, interfaces/migrations, commands and exit codes, native/JSON/HTTP/DB/browser scope, actual or mocked model-provider scope, screenshots, failure/concurrency cases, limits and external blockers. Keep incomplete capability and deployment gates explicit.
