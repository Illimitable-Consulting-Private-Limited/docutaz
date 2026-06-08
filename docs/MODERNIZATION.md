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

Net effect: ~277k lines of vendored third-party source removed; three stale
dependencies eliminated, two moved to package managers, one CMake hack gone.

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

#### P2 — Remove Boost
The project is C++17 and uses Boost only header-only and shallowly. Direct
standard-library replacements:

| Boost usage | Replace with |
|-------------|--------------|
| `boost::scoped_ptr` / `shared_ptr` / `make_shared` | `std::unique_ptr` / `std::shared_ptr` / `std::make_shared` |
| `boost::filesystem` | `std::filesystem` |
| `boost::lexical_cast` | `std::to_string` / `std::from_chars` |
| `boost::algorithm` (`erase_all`, `to_lower`, …) | small helpers / `QString` |
| `boost::posix_time` / `gregorian` (epoch-millis math in `BsonUtils.cpp`, `Notifier.cpp`) | `<chrono>` |

- **CI impact:** drop boost from apt / brew / the vcpkg CI manifest.
- **Risk/effort:** Medium, mechanical. Removes a heavyweight dependency for
  marginal use.

### 🟢 Low priority

#### P3 — Ubuntu CI runner 22.04 → 24.04
22.04 ends standard support in 2027; 24.04 ships newer Qt 5 / toolchains.
Trivial change to `runs-on:` in `.github/workflows/build.yml`.

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

1. **P2 (Boost removal)** and **P3 (runner bump)** — small, self-contained,
   independently verifiable.
2. **P1 + P4 (Qt 6 + QScintilla)** — the big milestone, once the small items are
   cleared.

---

*Last updated: 2026-06-08.*
