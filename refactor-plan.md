# Refactoring Plan: Rename Robomongo → Docutaz in Source

**Scope:** ~1,150 references across 242 source files + build system.  
**What stays untouched:** Legacy config import paths (`~/.3T/robo-3t/`, Studio3T `.dat` files) — these exist purely to migrate user data from old installs and must keep pointing at the right filesystem locations.

---

## Phase 1 — Automated text replacements (sed, no file moves)

Purely mechanical, done in one scripted pass. **Do not move any files in this phase.**

| Target | From | To | Approx. count |
|---|---|---|---|
| C++ namespace declarations | `namespace Robomongo` | `namespace Docutaz` | 224 |
| Namespace closing comments | `} // namespace Robomongo` etc. | `} // namespace Docutaz` | ~20 |
| using-namespace directives | `using namespace Robomongo` | `using namespace Docutaz` | ~10 |
| Qt resource paths in C++ | `:/robomongo/` | `:/docutaz/` | 81 |
| Qt resource prefix in .qrc files | `prefix="/robomongo"` | `prefix="/docutaz"` | 2 |
| CMake display variables | `ROBOMONGO_DISPLAY_NAME` etc. | `DOCUTAZ_DISPLAY_NAME` etc. | ~6 |
| Stylesheet object selector | `Robomongo--ExplorerTreeWidget` | `Docutaz--ExplorerTreeWidget` | 1 |
| Include guards | `ROBOMONGO_SSH_H` etc. | `DOCUTAZ_SSH_H` | 3 |
| User-visible error strings | `"open a new Robomongo instance"` etc. | `"open a new Docutaz instance"` | ~5 |
| Internal comments | `// Robomongo` | `// Docutaz` | ~30 |

**Note:** `#include "robomongo/..."` paths are NOT changed here — they update automatically once the directory is moved in Phase 2.

**Commit:** one commit for the entire phase.

---

## Phase 2 — Move source directory

`src/robomongo/` → `src/docutaz/`

This is the largest structural change.

1. `git mv src/robomongo src/docutaz`
2. Rewrite every `#include "robomongo/..."` → `#include "docutaz/..."` (~778 occurrences, sed)
3. Optionally rename `src/robomongo-unit-tests/` → `src/docutaz-unit-tests/` (lower priority)
4. Full rebuild must pass (`cd build && ninja -j$(nproc)`) before committing.

**Commit:** one commit for the directory move + include rewrite.

---

## Phase 3 — CMake build system

| File / Variable | Change |
|---|---|
| `CMakeLists.txt` line 2 | `project(Robomongo)` → `project(Docutaz)` |
| `src/docutaz/CMakeLists.txt` | `add_executable(robomongo …)` → `add_executable(docutaz …)` + all `target_*` references; update source path from `src/robomongo` to `src/docutaz` |
| `cmake/Robomongo*.cmake` (10 files) | Rename to `cmake/Docutaz*.cmake` |
| Root `include(Robomongo*)` calls | Update to `include(Docutaz*)` (6 calls) |
| `RobomongoCommon.cmake` | Remove / rename `ROBOMONGO_DISPLAY_NAME` variables |
| `RobomongoPackage.cmake` | Update `www.robomongo.org` URL; update `.ico` / `.icns` file references |

**Commit:** one commit for the build system changes.

---

## Phase 4 — Rename install and packaging files

| From | To | CMake file to update |
|---|---|---|
| `install/macosx/robomongo.icns` | `install/macosx/docutaz.icns` | `cmake/RobomongoInstall.cmake` |
| `install/macosx/robomongo.iconset/` | `install/macosx/docutaz.iconset/` | same |
| `install/windows/robomongo.ico` | `install/windows/docutaz.ico` | `src/docutaz/CMakeLists.txt` |
| `install/linux/robomongo.sh` | `install/linux/docutaz.sh` | standalone |
| `src/docutaz/resources/robo.qrc` | `src/docutaz/resources/docutaz.qrc` | CMakeLists |
| `src/docutaz/gui/resources/gui.qrc` | `src/docutaz/gui/resources/gui.qrc` | no change needed |

**Commit:** one commit for packaging file renames.

---

## Phase 5 — Class and file renames

Low risk, low priority. Can be done last.

| From | To | Files affected |
|---|---|---|
| `class RoboCrypt` | `class DocutazCrypt` | `RoboCrypt.h`, `RoboCrypt.cpp`, all call sites |
| `class RoboScintilla` | `class DocutazScintilla` | `PlainJavaScriptEditor.h`, `FindFrame.h`, all call sites |
| File `RoboCrypt.h/.cpp` | `DocutazCrypt.h/.cpp` | includes + CMakeLists |

**Commit:** one commit per class rename.

---

## Execution rules

- **Phases must be done in order.** Moving the directory while doing text replacement creates conflicting diffs that are hard to review and revert.
- **Each phase = one commit** so any phase can be reverted independently if the build breaks.
- **Full rebuild after Phase 2** (`cmake .. -GNinja && ninja -j$(nproc)`) must succeed before proceeding to Phase 3.
- **Qt resource paths are critical.** Phase 1 renames `:/robomongo/` → `:/docutaz/` in both the `.qrc` prefix and all C++ string literals together. A single missed occurrence causes a silent load failure (as seen with the license file bug).

---

## What is NOT changed

- Legacy config import paths in `SettingsManager.cpp` (`.3T/robo-3t/`, Studio3T paths) — kept so existing Robo 3T users can migrate their connections into Docutaz.
- Source under `src/third-party/`
- Source under `mongo-cxx-driver/`
