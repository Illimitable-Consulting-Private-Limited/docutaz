# Docutaz Modernization Plan

Status of the effort to bring Docutaz's build system and third-party
dependencies up to date. The MongoDB stack (mongo-cxx-driver 4.3.1,
mongo-c-driver 2.3.0, mongosh 2.8.3) is already current and is intentionally
out of scope here.

Work is tracked on the `modernization` branch, kept separate from `main` so it
does not entangle with ongoing cross-platform bug fixes.

---

## Completed (on `modernization`)

| # | Item | Outcome |
|---|------|---------|
| 1 | **Drop bundled qjson 0.8.1** | Replaced with Qt-native `QJsonDocument`. Vendored library removed; config files remain backward-compatible. |
| 2 | **CMake minimums** | Top-level `3.8 → 3.16`; vendored floors raised to `3.10`. Removed the `-DCMAKE_POLICY_VERSION_MINIMUM=3.5` workaround from all CI jobs. |
| 3 | **libssh2 → 1.11.x via package managers** | Stopped vendoring. Linux=system, macOS=Homebrew, Windows=vcpkg (pinned to baseline `2026.06.01` via `ci/vcpkg/vcpkg.json`). Picks up upstream security fixes automatically. |
| 4 | **Remove orphaned esprima.js** | Dead since the mongosh shell migration (autocomplete is server-side now). ~195 KB resource deleted. |
| 5 | **GoogleTest 1.8.1 → 1.15.2** | Switched to pinned CMake `FetchContent` (test-only, never fetched in CI). Also fixes the `-Werror` failure that blocked local test builds. |
| 6 | **Boost — easy half (P2a)** | `scoped_ptr`/`shared_ptr`/`make_shared` → `std::`; `erase_all` → `std::remove`/`erase`; `lexical_cast` → `std::to_string`. Boost build dependency still remains (date_time → see P2b). |
| 7 | **Ubuntu CI runner 22.04 → 24.04 (P3)** | `runs-on: ubuntu-24.04`; mongocxx cache key bumped to `ubuntu24.04` so the prefix rebuilds against the newer glibc/toolchain instead of restoring a 22.04 build. |
| 8 | **Boost — finish removal (P2b)** | `boost::date_time` in `ptimeutil` rewritten to `std::chrono`. The dead, unused parse functions (`ptimeFromIsoString`, `rfc1123date`) were trimmed; only `isotimeString` (the lone caller-used function, now taking ms-since-epoch) and `minDate`/`maxDate` remain. Verified byte-identical to the Boost output for the UTC path and all whole-hour timezones (incidentally fixes a latent mixed-sign `time_duration` bug for fractional-hour zones in local-time display). **Boost fully removed** — dropped from CMake, apt, brew, the vcpkg manifest, and the Windows `BOOST_ALL_NO_LIB` flag. |

Net effect: ~277k lines of vendored third-party source removed; three stale
dependencies eliminated, two moved to package managers, one CMake hack gone.
Boost is no longer a build dependency on any platform.

> ⚠️ The macOS/Windows libssh2 paths can only be validated on CI (a Linux host
> builds against the system lib). Run the workflow on `modernization` —
> manually via *Actions → Build → Run workflow*, or by opening a PR to
> `master` — before relying on those artifacts.

---

## Pending

Ordered roughly by value vs. effort. Each is intended as its own focused PR.

### 🔴 High priority

#### P1 — Qt 5 → Qt 6
The single largest item. Qt 5.15.2 (Dec 2020) is the last free Qt 5 release and
is effectively end-of-life; all platform, OpenSSL 3.x, Wayland, HiDPI and
security work now lands in Qt 6 (current LTS 6.8).

- **Scope:** `find_package(Qt5* )` → `Qt6`; `QRegExp` → `QRegularExpression`;
  `QtMacExtras` removed in Qt 6 (replace the macOS-specific calls); review
  `QString`/container API changes; bundled QScintilla must move to a
  Qt6-compatible version (see P4 — couple these).
- **CI impact:** Windows (aqtinstall version), macOS (`brew qt@5` → `qt`),
  Linux (`qtbase5-dev` → `qt6-base-dev`).
- **Risk/effort:** High. Treat as a dedicated milestone, not a quick PR.

### 🟡 Medium priority

#### P5 — Restore unit tests in CI
Tests are currently built with `-DDOCUTAZ_BUILD_TESTS=OFF` on every CI job.
GoogleTest itself is now healthy (1.15.2 builds cleanly), but enabling the flag
as-is would add little and likely break CI. Fix the test architecture first,
then turn it on.

- **Blockers to fix first:**
  - Linux is hard-disabled in `src/docutaz-unit-tests/CMakeLists.txt`
    (`if (SYSTEM_LINUX) return()` — "MongoDB linking problems"), so the cheapest
    test platform runs nothing.
  - The Windows/macOS test target links the app by **scraping hardcoded object
    files**, including CMake AUTOGEN hash dirs
    (`docutaz_autogen/YHP5W5E6RA/qrc_gui.cpp.o`, …) that drift and break.
  - Coverage is minimal (3 files: `RoboCrypt_test`, `StringOperations_test`,
    `HexUtils_test`).
- **Approach:** extract the app's core logic into a **library target** that both
  `docutaz` and `robo_unit_tests` link (removing the object-file harvesting and
  the autogen-hash fragility); resolve the Linux MongoDB linking issue; then set
  `DOCUTAZ_BUILD_TESTS=ON` in CI and add a `ctest` step.
- **Risk/effort:** Medium. Do after P2; should not ride along with dependency
  cleanups or block bug fixing.

### 🟢 Low priority

#### P4 — QScintilla 2.8.4 → current (≈2.14.x)
The bundled code editor component (2014). Functional today; the upgrade mostly
matters for HiDPI/rendering fixes and is **required** by the Qt 6 move, so fold
it into P1 rather than doing it standalone.

### Optional / nice-to-have
- **C++17 → C++20** once Qt 6 is in (Qt 6 already requires C++17). Enables
  `std::span`, concepts, etc. Low urgency.
- **README dependency table:** revisit the stated `Qt 5 5.12+` minimum — CI uses
  5.15.2; after P1 this becomes Qt 6.

---

## Suggested sequencing

1. **P5 (restore unit tests)** — test-architecture refactor.
2. **P1 + P4 (Qt 6 + QScintilla)** — the big milestone, once the smaller items
   are cleared.

---

*Last updated: 2026-06-09.*
