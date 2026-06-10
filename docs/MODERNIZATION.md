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
| 9 | **Node.js 20 → 24 GitHub Actions** | Bumped `checkout` v4→v6, `cache` v4→v5, `upload-artifact` v4→v7 (all `runs.using: node24`) ahead of GitHub's 2026-06-16 forced cutover. |
| 10 | **Restore unit tests — architecture + Linux CI (P5)** | App sources extracted into a shared `docutaz_core` OBJECT library that both `docutaz` and `robo_unit_tests` link. Removed the brittle object-file harvesting (hardcoded AUTOGEN hash dirs), the dead `mongodb`/`RoboCrypt_test` references, and the Linux hard-disable. Tests now build and run on Linux via `ctest`; Linux CI configures `-DDOCUTAZ_BUILD_TESTS=ON` and runs them. See P5-follow-up for Windows/macOS. |
| 11 | **Qt 5 → Qt 6 (P1)** | `find_package(Qt5*)` → `Qt6`; `Qt5::WinMain` → `Qt6::EntryPoint`; `Qt5MacExtras` dropped (removed in Qt6). Source migration: `QRegExp`/`QRegExpValidator` → `QRegularExpression`; `QMutex::Recursive` → `QRecursiveMutex`; `qsrand`/`qrand` → `QRandomGenerator`; `qChecksum(ptr,len)` → `qChecksum(view)`; implicit `QString`→`QUuid` made explicit; `QStringList::toSet`/`QSet::toList` → range ctor/`values()`; `QDesktopWidget`/`QApplication::desktop()` → `QScreen`/`primaryScreen()`; `QFontMetrics::width` → `horizontalAdvance`; `QLayout::setMargin` → `setContentsMargins`; `Qt::MidButton` → `MiddleButton`; `Qt::CTRL + Qt::Key_*` → `\|`; `Qt::TextColorRole` → `ForegroundRole`. Builds against Qt 6.10.3 locally; CI uses 6.8.x (Win) / distro+brew (Linux/macOS). |
| 12 | **QScintilla 2.8.4 → 2.14.x, de-vendored (P4)** | Bundled Qt4/Qt5-only tree (~9 MB, 519 files) removed. Resolved per platform via `find_path`/`find_library` into an INTERFACE `qscintilla` target: Linux system (`libqscintilla2-qt6-dev`), macOS Homebrew (`qscintilla2`), Windows built from source against the aqtinstall Qt6. The About dialog reads `QSCINTILLA_VERSION_STR` from the headers. |

Net effect: ~555k lines of vendored third-party source removed (qjson,
esprima, googletest, libssh2, **QScintilla**); four stale dependencies
eliminated, three moved to package managers, one CMake hack gone. Boost is no
longer a build dependency, and the app now targets Qt 6.

> ⚠️ The macOS/Windows libssh2 paths can only be validated on CI (a Linux host
> builds against the system lib). Run the workflow on `modernization` —
> manually via *Actions → Build → Run workflow*, or by opening a PR to
> `master` — before relying on those artifacts.

---

## Pending

Ordered roughly by value vs. effort. Each is intended as its own focused PR.

> ⚠️ The macOS/Windows **Qt 6 + QScintilla** paths (like libssh2) can only be
> validated on CI — a Linux host builds against the distro packages. The
> Windows job builds QScintilla from source against the aqtinstall Qt6; the
> macOS job uses Homebrew `qt`/`qscintilla2`. Run the workflow on
> `modernization` before relying on those artifacts.

### 🟡 Medium priority

#### P5-follow-up — Enable unit tests on Windows/macOS CI
The test architecture is fixed and Linux CI runs the suite (see Completed P5).
What remains is turning the tests on for the other two CI jobs:

- **What's left:** flip `-DDOCUTAZ_BUILD_TESTS=OFF → ON` in the Windows and
  macOS jobs, build `robo_unit_tests`, and add a `ctest` step. The target
  already builds via `docutaz_core`; the open question is purely **runtime
  library discovery** for the test binary — Qt DLLs on Windows, the mongocxx /
  Qt dylibs on macOS — which can only be ironed out on real runners.
- **Optional:** expand coverage beyond the current three suites
  (`DocutazCrypt`, `StringOperations`, `HexUtils`).
- **Risk/effort:** Low–medium; isolated to CI YAML and needs runner iteration.

### Optional / nice-to-have
- **C++17 → C++20** now that Qt 6 is in (Qt 6 already requires C++17). Enables
  `std::span`, concepts, etc. Low urgency.

---

## Suggested sequencing

1. **P5-follow-up (tests on Windows/macOS CI)** — small, needs runner iteration;
   now also covers QScintilla runtime discovery for the test binary.

With P1+P4 done, the dependency/build modernization is essentially complete.
The `modernization` branch can be merged to `main` once CI is green on all
three platforms.

---

*Last updated: 2026-06-11.*
