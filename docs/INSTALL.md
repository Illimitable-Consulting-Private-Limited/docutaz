# Installing Docutaz

Download the build for your platform from the
[**Releases page**](https://github.com/Illimitable-Consulting-Private-Limited/docutaz/releases),
then follow the steps for your OS below.

| Platform | Download |
|----------|----------|
| Linux (Intel/AMD) | `docutaz-<version>-linux-x86_64.tar.gz` |
| Linux (ARM)       | `docutaz-<version>-linux-aarch64.tar.gz` |
| macOS (Apple Silicon) | `docutaz-<version>-macos-arm64.zip` |
| Windows (64-bit)  | `docutaz-<version>-windows-x86_64.zip` |

---

## Prerequisite for all platforms: mongosh

Docutaz runs its query shell through MongoDB's **`mongosh`**, which is **not
bundled** — install a recent **mongosh 2.x or newer** yourself:

- Download: <https://www.mongodb.com/try/download/shell>
- or via a package manager: macOS `brew install mongosh` · Windows
  `winget install MongoDB.Shell` · Linux from MongoDB's apt/yum repository.

Then make sure Docutaz can find it: either keep `mongosh` on your `PATH`
(check with `mongosh --version`), **or** set its full path in Docutaz under
**Options → Preferences → "mongosh path"** (leave empty to auto-detect).

You'll also need a **MongoDB 8.0+** server to connect to (local or remote).

---

## Linux (x86_64 and arm64)

The Linux tarball bundles the MongoDB C/C++ driver libraries; Qt 6, QScintilla,
OpenSSL and libssh2 come from your distribution.

1. **Install the runtime libraries** (plus mongosh, above):
   - **Fedora / RHEL:**
     ```sh
     sudo dnf install qt6-qtbase qt6-qtbase-gui qt6-qtsvg qscintilla-qt6 libssh2 openssl
     ```
   - **Debian / Ubuntu:**
     ```sh
     sudo apt install libqt6widgets6 libqt6network6 libqt6xml6 libqt6printsupport6 \
                      libqt6svg6 libqscintilla2-qt6-15 libssh2-1 libssl3
     ```
2. **Extract and run:**
   ```sh
   tar xzf docutaz-<version>-linux-x86_64.tar.gz   # or ...-aarch64.tar.gz
   cd docutaz-<version>-linux-x86_64
   ./docutaz.sh
   ```
   Use the `./docutaz.sh` launcher (not the bare `docutaz` binary) — it points
   the loader at the bundled driver libraries.
3. **Launcher icon & app-menu entry (recommended):**
   ```sh
   ./install-desktop.sh
   ```
   Run this once. It registers the `.desktop` file and icons under
   `~/.local/share/`, so Docutaz shows up in your application menu — and the
   icon displays correctly on **Wayland**, where it comes from the desktop
   entry rather than the window. To undo it later: `./uninstall-desktop.sh`.

---

## macOS (Apple Silicon)

A self-contained `Docutaz.app` (Qt, QScintilla and the MongoDB driver are
bundled inside it). **Apple Silicon — M1 or newer — only.**

1. Unzip `docutaz-<version>-macos-arm64.zip` and, optionally, drag `Docutaz.app`
   into **/Applications**.
2. The app is **not signed/notarized**, so the first launch is blocked by
   Gatekeeper. Do one of:
   - **Right-click** `Docutaz.app` → **Open** → **Open** in the dialog, **or**
   - run once in Terminal:
     ```sh
     xattr -dr com.apple.quarantine /Applications/Docutaz.app
     ```
   (If you see *"Docutaz is damaged and can't be opened"*, that's the same
   quarantine flag — use the `xattr` command above.)
3. Install **mongosh** (see above; `brew install mongosh` is easiest).

---

## Windows (64-bit)

A self-contained folder (Qt, QScintilla and the MongoDB / OpenSSL / libssh2
DLLs are all included). **64-bit Intel/AMD only** — arm64 isn't available yet.

1. Extract `docutaz-<version>-windows-x86_64.zip` to a folder you control
   (e.g. under your user directory or `C:\Program Files\Docutaz`).
2. Run **`docutaz.exe`**. On first launch, Windows SmartScreen may warn that
   it's from an unknown publisher (the app is unsigned): click
   **More info → Run anyway**.
3. **Desktop / Start-menu shortcut (optional):** double-click
   **`Create Desktop Shortcut.bat`** in the extracted folder to add Docutaz to
   your Desktop and Start menu (no admin needed). `Remove Desktop Shortcut.bat`
   undoes it. *Don't move the folder afterward* — the shortcut points at
   `docutaz.exe` where it is; if you move it, re-run the script.
4. Install **mongosh** (see above; `winget install MongoDB.Shell` or the
   download link).

---

## First run

1. Accept the license (GPL v3) on first launch.
2. Create a connection — host/port or a connection string — optionally with
   TLS/SSL or an SSH tunnel, then connect.
3. Open a shell tab to run queries through `mongosh`.

## Troubleshooting

- **Shell won't start / "mongosh not found":** confirm `mongosh --version` works
  in a terminal, or set the full path in **Options → Preferences → mongosh path**.
- **Linux — "error while loading shared libraries":** install the runtime
  packages listed above, and launch via `./docutaz.sh` (not the bare binary).
- **Windows — app won't start / missing DLL:** install the latest
  [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).
- **macOS — "damaged / can't be opened":** remove the quarantine flag with the
  `xattr -dr com.apple.quarantine` command above.
- **Update notifications:** Docutaz checks GitHub for newer releases and shows a
  banner when one is available; toggle it under **Options → Automatically Check
  for Updates**, or check on demand via **Help → Check for Updates**. It sends
  no data about you.
