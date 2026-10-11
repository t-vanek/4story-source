# Phase A implementation record — not runtime verification

Baseline: `2fa2396`. Code revision: `16cc7f7`; the six local implementation
commits and full hashes are in [implementation.json](implementation.json).
The complete Phase A gate is **not met**. New code is unverified. Phase B/C have
not started. This directory adds evidence without replacing earlier reports.

## Compilation actually performed

All six modern targets compiled and linked in Debug on Fedora44 / x86_64 with
GCC16.2.1, CMake4.3.0 and Ninja1.13.2. The PostgreSQL backend is enabled. No binary
was launched, including for a version/help query. No tests were built or run.

The host lacked development headers/libraries. Fedora RPMs were downloaded to
`/tmp/fourstory-phase-a-rpms` and extracted into
`/tmp/fourstory-phase-a-deps`; they were not installed system-wide. The build used
Boost1.90, SOCI4.1.2, fmt11.2, spdlog1.15.3 and libpq18. The JSON retains RPM and
binary SHA256 values. These temporary dependencies support this local compile;
they do not establish reproducibility of a production image.

The final successful configuration was:

```sh
cmake -S . -B build/phase-a-compile -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=OFF \
  -DFOURSTORY_BUILD_LEGACY_TNETLIB=OFF \
  -DFOURSTORY_ENABLE_POSTGRESQL=ON \
  -DCMAKE_PREFIX_PATH=/tmp/fourstory-phase-a-deps/usr \
  -DCMAKE_LIBRARY_PATH=/tmp/fourstory-phase-a-deps/usr/lib64 \
  -DCMAKE_CXX_FLAGS=-I/tmp/fourstory-phase-a-deps/usr/include \
  -DCMAKE_EXE_LINKER_FLAGS=-L/tmp/fourstory-phase-a-deps/usr/lib64 \
  -DCMAKE_DISABLE_FIND_PACKAGE_SOCI=ON
cmake --build build/phase-a-compile --target \
  tloginsvr_asio tworldsvr_asio tmapsvr_asio \
  tcontrolsvr_asio tpatchsvr_asio tlogsvr_asio -j2
```

SOCI package discovery was disabled only in this ignored build directory because
the host's newer package exports different targets and probes a MySQL dependency.
The project's existing SOCI link fallback remains in use. BUILD_TESTING=OFF does
not remove tests or modify their normal project configuration. The post-build
operation copies the OpenSSL provider; no verification script runs there.

[cluster-debug.log](cluster-debug.log) records the first successful all-target
build. [control-debug-initial-failure.log.gz](control-debug-initial-failure.log.gz)
preserves the exact compiler output (gzip) for the missing-CoOffload-include error; the include was fixed and
[control-debug-repaired.log](control-debug-repaired.log) records successful linking.
The other logs are incremental builds using the same configuration. The latest
Map change is in [quest-routing-debug.log](quest-routing-debug.log); the final
all-target invocation is [control-snapshot-debug.log](control-snapshot-debug.log).
Unchanged targets are up to date in these incremental logs.

Existing warnings remain in the legacy generic PeerAuthRow write mapping,
SpCall timestamp formatting and older SOCI APIs. The native trust repository
bypasses that generic write mapping; its warning still needs a separate repair.
Compilation proves neither SQL execution nor gameplay/protocol correctness.
Release and sanitizer configurations were not built for this increment.

## Static inspections actually performed

- Read original handlers, quest classes, database calls, backup catalog definitions
  and client/protocol references; the [coverage rows](../../legacy-modern-coverage.csv)
  retain concrete counterparts and missing branches.
- Parse the AST of `migrate.py`, `extract_reference.py`,
  `activate_quest_catalog.py`, `import_quest_progress.py`,
  `import_patch_metadata.py` and `import_control_metadata.py` using Python
  `ast.parse`. No import, connection or program entry point was executed.
- `git diff --check` and staged equivalent: whitespace checks pass.
- Git comparison against the baseline shows no changes to original server/client
  sources or migrations001–043. Backup identity is copied from the preserved
  artifact inventory; this is not a fresh database reconciliation result.

## Deferred acceptance, not executed

**Zero tests executed.** No CTest/pytest, unit/integration/regression/protocol/PG
test, sanitizer, fuzz or benchmark run belongs to this increment. No migration,
archive extraction/import or reconciliation was executed. No server/client
process, Docker/Podman build, container startup or cluster restart was performed.
No push, release, image publication or external deployment occurred.

Migrations044–050 and the new import tools are written only. Future Phase B must
cover both fresh installation and upgrade, all imported rows/bytes, grants,
stale owners/snapshots, uncertain commits, disconnect/reconnect and rollback.
The Control topology reload/dial timeout race, event dispatch/recovery and native
quest transaction ordering need explicit later scenarios. This is a scenario
list, not a new test suite or a claim that tests passed.

The original client EXE/assets and runnable installation are unavailable. There
is no supported executable hash/build to report. Source-only compatibility
investigation does not replace real-client acceptance.

Continue at [the implementation gate](../../full-rewrite-status.md#exact-continuation--implementation-gate).
