# General application engine — authoritative direction

This replaces the earlier invoice-first product roadmap following the owner's explicit correction. Runtime must create applications, not offer predefined workflows. Shared UI, data persistence and storage/compression are foundational. Missing specialized abilities are compatible with a usable partial application; only dependent features remain blocked.

## Application lifecycle

User intent → model proposes a versioned AppSpec → C++ validates types, roles, references, bounded behavior and capability contracts → preview generated UI and schema changes → publish → use deterministic application operations → refine intent → non-destructive versioned upgrade. Model output never grants authority or declares an unavailable ability installed.

The current compiled protocol supplies arbitrary entities and typed fields; relationships/file references; page/navigation definitions; role policies; capability ports with versions/contracts; and additive upgrades. The renderer interprets definitions rather than generating source per request. The API accepts AppSpec directly, allowing generation to remain decoupled from model selection.

## Missing capabilities

A painting request can yield project/gallery/asset data and screens even when painting itself is absent. Persist the required capability ID/version/contract and explain its unavailable status at that action. Do not delete the app, suppress unrelated data, substitute a unrelated workflow or invent an output. On installation, resolve the same stored definition against the registry. Runtime handles UI/persistence/assets; the capability provides the specialized operation.

The current universal port passes a validated record envelope and returns structured results. Additional contract versions may support streaming, interactive surfaces, richer input bindings and produced assets. They must remain versioned registered interfaces, not arbitrary execution escape hatches.

## Platform subsystems

**C++ application compiler:** schema/role/reference validation, a typed expression/rule vocabulary as it is implemented, capability resolution, app versions, migration compatibility and execution limits. Keep application-specific names out of native source.

**Svelte renderer:** navigation, forms, tables, files and availability panels now; registered dashboard/chart/calendar/interactive-surface capabilities next. Layout richness and application behavior are independent extensible parts of the AppSpec. Declaring an unsupported visual or rule must produce an explicit requirement rather than pretend it was implemented.

**PostgreSQL persistence:** scoped app/version/record/link/file/event state. Optimistic concurrency, idempotent create, normalized references, archive checks and additive schema upgrades. Real principals/memberships and row-level app policies must be bound to authenticated users before hosting.

**Asset pipeline:** validate bounds/metadata, hash original bytes, choose identity or lossless zstd, atomically persist content+metadata, authorize lookup, bounded decompression, verify length/hash, deliver original bytes. The current BYTEA backend provides transactional consistency for small local-development objects. Object storage must later implement staging/finalization, abandoned-upload cleanup, streaming/multipart, quotas, encryption, GC and recovery tests. Deduplication is scoped; uploaded archive extraction and lossy media conversion are separate optional capabilities.

**Generation/runtime:** provider-independent model request boundary, small router such as Jev only when useful, planner produces the general contract, core validates before publication. Explicit clarification/requirements for genuinely unsupported behavior; repair bounded schema errors. Editing existing apps requires a schema-change preview and preserved data. The current branch does not yet call a model.

**Capability execution:** registry metadata is trusted platform configuration, not model-authored authority. Deterministic local read-only capabilities may run synchronously. External/consequential capabilities require grants, frozen approvals, durable dispatch, reconciliation and cancellation semantics. No network calls in DB transactions.

## Acceptance beyond a demo

Use at least ten unseen application definitions across unrelated domains against the same native code. Validate arbitrary entity/field names, relationships, precision, files, roles, page generation and optional capability requirements. Install a compatible test capability without modifying the stored app and verify only its feature changes availability. Reject wrong versions/contracts and fabricated installation claims.

Real-stack tests must exercise two independent apps, durable records/assets, compressed original-byte roundtrips, conflicting edits, safe schema changes, archived references, cross-app/tenant access, quotas, malformed definitions and actual restart/reopen. Browser tests create apps and interact with generated forms/files on desktop/mobile.

The future user-facing acceptance gate is natural-language creation and refinement of those unseen applications, not manually implementing one backend per app. Full public SaaS readiness additionally requires real identity, deployments, model budgets, external dispatch, operational recovery and secure capability installation. Do not confuse a correct architectural foundation with a completed product.
