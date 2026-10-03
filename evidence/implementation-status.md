# General application engine — verification record

Work branch: feat/general-application-engine. Main has not been merged or deployed.

The invoice-specific executable path has been replaced in this branch by a general AppSpec-driven server and renderer. Native AppSpec validation, generic record/space/role API, revisions, private object storage/compression and capability dependency handling are implemented. Optional natural-language composition produces full application definitions through the configured bridge, rather than picking a fixture.

## Executed locally

37 application-core tests, 17 lossless-object-storage tests, 5 integer-budget tests and 11 provider-composition contract tests passed. Application and storage native tests were also exercised with AddressSanitizer/UndefinedBehaviorSanitizer. Provider transport was mocked; no live model calls were made. Local Python and Bash syntax checks passed.

## Requires actual CI evidence

The authoring environment has no Docker daemon, dependency-download connectivity or nlohmann headers. New C++ JSON/HTTP server compilation, TypeScript/Svelte checking, real PostgreSQL migrations and RLS, API/capability/file behavior, restart persistence and browser rendering must be verified in the network-enabled CI runner. Pending tests must not be described as passed. This record will be updated with observed results.

## Not claimed

Painting, arbitrary remote/side-effect abilities, general workflow/rule coverage, real public authentication/onboarding, R2, large/resumable uploads, full asset deletion/retention, production encryption/restore or a deployment. The present local storage limits and UI vocabulary are explicit. A live model account and owner-authorized billable acceptance are still needed to verify end-to-end natural-language composition.
