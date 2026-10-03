# ADR 0002 — Applications are the product; abilities are independent dependencies

Status: accepted from the owner's explicit product correction.

The invoice-specific demo is not the product architecture. Runtime must accept an application definition containing its own entities, relationships, pages, roles, actions and storage policy. The C++ server and Svelte renderer must not select a hardcoded application based on its name or topic.

UI, persistent records, customer data-space isolation, revision handling, and private lossless object storage are platform facilities. A specialized ability has an immutable versioned input/output contract. An application may declare an ability before its implementation exists. Missing or incompatible abilities disable only the dependent action; they do not prevent publishing, editing records, uploading files, or using unrelated pages. Never fabricate successful execution or silently substitute a lesser product.

Natural-language composition produces a complete ApplicationSpec, not an index into a catalog. Generated definitions are validated by the platform. Publication is explicit and distinct from generation. Updating an existing app retains stable entity/field IDs and requires an expected revision; destructive changes require an explicit migration rather than data loss.

The first concrete storage backend is a private persistent volume behind an ObjectStore interface. Objects use server-generated identifiers, bounded Zstandard compression only when useful, a versioned lossless envelope, SHA-256 checks, atomic publication, and authorization through PostgreSQL metadata. This does not claim to implement R2, media transcoding, archive extraction, antivirus scanning, encryption at rest, or unlimited file sizes. Those are separate additions.

The installed capability mechanism currently executes reviewed deterministic C++ handlers. A real word-count handler proves input/output bindings and persistence without pretending to implement painting. Remote/side-effect providers need their own authorized dispatch, budgets and recovery. An app definition is not permission to install executable code.

The new application server replaces the invoice route in the executable. Old invoice source is retained for historical tests, not presented as the application's general planner. New capabilities extend the engine; additional examples must not become application-specific backend branches.
