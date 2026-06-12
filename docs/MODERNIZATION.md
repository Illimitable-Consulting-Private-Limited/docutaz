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
| 10 | **Restore unit tests — architecture + Linux CI (P5)** | App sources extracted into a shared `docutaz_core` OBJECT library that both `docutaz` and `docutaz_unit_tests` link. Removed the brittle object-file harvesting (hardcoded AUTOGEN hash dirs), the dead `mongodb`/`RoboCrypt_test` references, and the Linux hard-disable. Tests now build and run on Linux via `ctest`; Linux CI configures `-DDOCUTAZ_BUILD_TESTS=ON` and runs them. (Windows/macOS enabled in #13.) |
| 11 | **Qt 5 → Qt 6 (P1)** | `find_package(Qt5*)` → `Qt6`; `Qt5::WinMain` → `Qt6::EntryPoint`; `Qt5MacExtras` dropped (removed in Qt6). Source migration: `QRegExp`/`QRegExpValidator` → `QRegularExpression`; `QMutex::Recursive` → `QRecursiveMutex`; `qsrand`/`qrand` → `QRandomGenerator`; `qChecksum(ptr,len)` → `qChecksum(view)`; implicit `QString`→`QUuid` made explicit; `QStringList::toSet`/`QSet::toList` → range ctor/`values()`; `QDesktopWidget`/`QApplication::desktop()` → `QScreen`/`primaryScreen()`; `QFontMetrics::width` → `horizontalAdvance`; `QLayout::setMargin` → `setContentsMargins`; `Qt::MidButton` → `MiddleButton`; `Qt::CTRL + Qt::Key_*` → `\|`; `Qt::TextColorRole` → `ForegroundRole`. Builds against Qt 6.10.3 locally; CI uses 6.8.x (Win) / distro+brew (Linux/macOS). |
| 12 | **QScintilla 2.8.4 → 2.14.x, de-vendored (P4)** | Bundled Qt4/Qt5-only tree (~9 MB, 519 files) removed. Resolved per platform via `find_path`/`find_library` into an INTERFACE `qscintilla` target: Linux system (`libqscintilla2-qt6-dev`), macOS Homebrew (`qscintilla2`), Windows built from source against the aqtinstall Qt6. The About dialog reads `QSCINTILLA_VERSION_STR` from the headers. |
| 13 | **Unit tests on Windows/macOS CI (P5-follow-up)** | Flipped `-DDOCUTAZ_BUILD_TESTS=ON` on the Windows and macOS jobs, built `docutaz_unit_tests`, and added `ctest` steps with the runtime-library discovery those binaries need (Windows: every runtime DLL dir on `PATH`, incl. the from-source `qscintilla2_qt6.dll` placed deterministically into the Qt bin dir; macOS: `DYLD_FALLBACK_LIBRARY_PATH` over the mongocxx prefix + Homebrew Qt/QScintilla). The suite now builds and runs on all three platforms. |
| 14 | **Cross-Qt-version portability — drop Qt private API** | The binary linked Qt's *private* `QZipReader`, pinning it to the exact Qt minor (`Qt_6.x_PRIVATE_API`) — a build on one Qt 6.x failed to launch on a host with another (`version 'Qt_6_PRIVATE_API' not found`). Removed the sole user (a legacy Studio 3T `.zip` telemetry-ID import) and the vendored `qzip/` private-header tree; the binary now references only public, forward-compatible Qt symbols. Latent on all three platforms; Windows/macOS only escaped it by bundling the matching Qt. |
| 15 | **Linux tarball: rely on host Qt6 (de-vendor plugins)** | `bin/bundle-linux.sh` stopped bundling a Qt platform plugin + writing a `qt.conf` (the glob still pointed at Qt5 dirs, and a bundled plugin can't be loaded against the host's `libQt6Core`) — the cause of *"no Qt platform plugin could be initialized."* Qt now resolves its plugins from the host's `libQt6Core` via `QLibraryInfo`. README requirements refreshed to Qt6 + QScintilla (qt6). |
| 16 | **CI: packaged-binary launch smoke test** | Added a `--version` flag that constructs the `QApplication` (loading the platform plugin + every linked library) then exits 0 *before* the EULA. Each job runs the **packaged** artifact (tarball / windeployqt dir / `.app`) under `QT_QPA_PLATFORM=offscreen`, catching startup and packaging regressions — missing libs, a broken bundle, an ABI-incompatible Qt — that building alone cannot. |
| 17 | **De-brand leftover Robomongo/Robo 3T references** | Renamed `robo_unit_tests`→`docutaz_unit_tests`, `ROBO_SRC_DIR`→`DOCUTAZ_SRC_DIR`, `aboutRobomongo`→`aboutDocutaz`; fixed stale `// Robomongo` namespace comments, product doc comments, a user-facing error that mixed both names, three stale config-path doc comments, and the mongosh preamble banner. **Kept** (not branding): the GPLv3 upstream attribution in the About dialog, the Robo 3T/Robomongo config-migration paths and their comments, and historical "original Robomongo" lineage notes. |
| 18 | **Privacy: remove the updates.3t.io update-check** | By default the app phoned home to Studio 3T's server 30 s after launch and hourly, leaking each user's `anonymousID`, OS and connected MongoDB versions — identified as Robomongo (`softwareId=8`) — and would have surfaced Robomongo's version in an "update available" bar. Removed the whole feature: the network call, timers, `on_networkReply`, the menu toggle, the update-bar UI (+ its hover `eventFilter`), the `QNetworkAccessManager`, and the `checkForUpdates` setting. |
| 19 | **Unit-test coverage — BsonBridge roundtrips** | The standalone `src/tests/bson_bridge_roundtrip.cpp` was `EXCLUDE_FROM_ALL` (never built), stale (dead `Robomongo::` namespace) but had far broader coverage than the live test. Ported all of it — byte-identical BSON↔EJSON roundtrips across every type mongosh emits (ObjectId, Date, Timestamp, Binary/UUID, Decimal128 incl. NaN/Infinity, regex, nested/array, 64 KiB string, mixed docs) — into the live gtest suite (now 10 tests, all pass); deleted `src/tests`. |

Net effect: ~556k lines of vendored third-party source removed (qjson,
esprima, googletest, libssh2, **QScintilla**, the dead `qzip`/`src/tests`
trees); four stale dependencies eliminated, three moved to package managers,
one CMake hack gone. Boost is no longer a build dependency, the app targets
Qt 6 and runs on any Qt 6.x (no private-API pinning), no longer phones home,
and is de-branded from its Robomongo/Robo 3T heritage (upstream attribution
preserved).

> ⚠️ The macOS/Windows libssh2 paths can only be validated on CI (a Linux host
> builds against the system lib). Run the workflow on `modernization` —
> manually via *Actions → Build → Run workflow*, or by opening a PR to
> `master` — before relying on those artifacts.

---

## Pending

> ⚠️ The macOS/Windows **Qt 6 + QScintilla** paths (like libssh2) can only be
> validated on CI — a Linux host builds against the distro packages. The
> Windows job builds QScintilla from source against the aqtinstall Qt6; the
> macOS job uses Homebrew `qt`/`qscintilla2`. The packaged-binary smoke test
> (#16) now exercises each artifact on its own runner, but still run the
> workflow on `modernization` before relying on those artifacts.

### Optional / nice-to-have
- **C++17 → C++20** now that Qt 6 is in (Qt 6 already requires C++17). Enables
  `std::span`, concepts, etc. Low urgency.
- **Retire `dbVersionsConnected`** — still populated on connect but unread now
  that the update-check (#18) is gone. Pure dead state; remove when convenient.

---

## Status

The dependency/build modernization is complete: Qt 5→6 and QScintilla done,
Boost gone, all four vendored third-party trees de-vendored, unit tests build
and run on all three CI platforms, and each platform's packaged artifact is
launch-smoke-tested. The app also runs on any Qt 6.x (no private-API pinning),
no longer phones home, and is de-branded from its Robomongo/Robo 3T heritage.

The `modernization` branch is ready to merge to `main` once a CI run is green
on all three platforms. Only the optional C++20 bump remains.

---

*Last updated: 2026-06-12.*
