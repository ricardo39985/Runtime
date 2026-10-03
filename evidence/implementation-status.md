# Application-engine implementation evidence

Scope: replacement of the invoice-specific executable/UI with a general AppSpec compiler, generated data interface, durable PostgreSQL application storage and lossless asset pipeline. No main merge or production deployment is implied.

## Actually executed locally

GCC14.2 compiled application.cpp, storage.cpp, json.cpp and application_test.cpp with C++23, -Wall -Wextra -Wpedantic -Werror. All 53 application/capability/storage tests passed in Debug and optimized Release (-O3). The same 53 passed AddressSanitizer and UndefinedBehaviorSanitizer at -O0 with -fno-omit-frame-pointer.

An initial optimized (-O1) sanitizer compilation hit a GCC diagnostic in the installed Boost.JSON storage_ptr reference-counting headers, treated as an error. The normal Debug/-O0 sanitizer configuration compiled and passed; this is not an optimized sanitizer pass. The pinned dependency production build remains a separate CI gate.

The suite covers absent-capability validity, unchanged-spec activation with a test-only capability, exact contract/version matching, role validation, field/date/decimal handling, unrelated app definitions, safe upgrades, file size/filename bounds, identity/zstd selection, original-byte roundtrip, digest verification, corrupt/trailing-frame rejection and decompression limits. No provider/model network calls occurred.

Python API-test syntax and shell-script syntax checks passed. No local Docker daemon, PostgreSQL server, pnpm dependency download or Drogon installation is available. HTTP/server, database, Svelte/browser and restart tests are written but are not yet declared passed in this file.

## Inherited build blocker corrected

Main CI run37127138574 passed the older isolated/web checks but failed in Docker's vcpkg-tool-meson build because python3 was absent. The new Dockerfile installs Python3. This correction requires a new actual full-stack build; it does not prove one succeeded.

## Explicit remaining boundaries

Natural-language planner disconnected; fixed development owner instead of live OIDC; finite data-oriented renderer rather than every interactive surface; no paint implementation; trusted local record-snapshot capability only; no runtime arbitrary-plugin installer, external sends/schedules or billable models. Current assets use small-object PostgreSQL BYTEA, not deployed R2. Public startup is still disabled. Do not present these as completed features.

CI is configured for real application HTTP/DB/RLS and restart tests plus desktop/mobile screenshots. The actual run conclusion, not this pending note, determines full-stack verification. Record its commit and result before advancing A01–A05 acceptance.
