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

You'll also need a **MongoDB** server to connect to (local or remote). Docutaz is built and
tested against MongoDB 8.x; earlier supported server versions (roughly 4.2+, per the bundled
driver and your mongosh) generally work but aren't officially tested.

---

## Linux (x86_64 and arm64)

Two ways to install: a portable **tarball** (any distro, x86_64 or arm64) or a
**Flatpak** bundle (x86_64).

### Tarball (x86_64 and arm64)

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
   ./docutaz
   ```
   `./docutaz` is a small launcher that points the loader at the bundled driver
   libraries and verifies the system libraries above are installed. If any are
   missing it tells you exactly which ones and how to install them (rather than
   crashing), so install those and run it again. (`./docutaz.sh` still works as a
   compatibility shim.)
3. **Launcher icon & app-menu entry (recommended):**
   ```sh
   ./install-desktop.sh
   ```
   Run this once. It registers the `.desktop` file and icons under
   `~/.local/share/`, so Docutaz shows up in your application menu — and the
   icon displays correctly on **Wayland**, where it comes from the desktop
   entry rather than the window. To undo it later: `./uninstall-desktop.sh`.

### Flatpak (x86_64)

Docutaz ships a single-file Flatpak bundle on the
[Releases page](https://github.com/Illimitable-Consulting-Private-Limited/docutaz/releases).
It is **not on Flathub**: Flathub requires every component to be built from
source, which isn't practical for the bundled MongoDB `mongosh` shell (a large
Node/TypeScript app), so the bundle is distributed directly instead.

1. Download `docutaz-<version>-linux-x86_64.flatpak` from the release.
2. Install and run (no root needed):
   ```sh
   flatpak install --user ./docutaz-<version>-linux-x86_64.flatpak
   flatpak run in.illimitable.Docutaz
   ```

**Runtime note:** the bundle contains only Docutaz, not the shared KDE runtime it
links against (`org.kde.Platform`). If you already have the **Flathub remote**
configured (most desktops do), the runtime is fetched automatically. If not, the
bundle embeds a hint pointing at Flathub, so Flatpak offers to add it and pull the
runtime — you're using Flathub only for that standard shared runtime, not for
Docutaz itself. `mongosh` is bundled inside the Flatpak, so there's nothing else
to install.

**Updating:** the in-app notice links you to the new release; download the newer
`.flatpak` and install it over the top. Your settings live in the Flatpak's own
data dir and carry across updates.

---

## macOS (Apple Silicon)

A self-contained `Docutaz.app` (Qt, QScintilla and the MongoDB driver are
bundled inside it). **Apple Silicon — M1 or newer — only.**

### Homebrew (recommended)

```sh
brew install --cask --no-quarantine illimitable-consulting-private-limited/docutaz/docutaz
```

This installs Docutaz from our tap and pulls in **mongosh** as a dependency, so
there's nothing else to set up. The `--no-quarantine` flag is required because
the app is ad-hoc signed but not notarized (no Apple Developer ID yet), so
Gatekeeper would otherwise block it.

### Manual

1. Unzip `docutaz-<version>-macos-arm64.zip` and, optionally, drag `Docutaz.app`
   into **/Applications**.
2. The app is **ad-hoc signed but not notarized**, so the first launch is blocked
   by Gatekeeper. Do one of:
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

**64-bit Intel/AMD only** — arm64 isn't available yet.

### Installer (recommended)

1. Install with [winget](https://learn.microsoft.com/windows/package-manager/winget/):
   ```sh
   winget install Illimitable.Docutaz
   ```
   …or run **`docutaz-<version>-windows-x86_64-setup.exe`** from the release.
2. The installer puts Docutaz in Program Files, adds Start-menu and Desktop
   shortcuts, and registers an *Apps & features* entry for a clean uninstall. On
   first launch, SmartScreen may warn that it's from an unknown publisher (the app
   is unsigned): click **More info → Run anyway**.

### Portable (zip)

A self-contained folder (Qt, QScintilla and the MongoDB / OpenSSL / libssh2 DLLs
are all included).

1. Extract `docutaz-<version>-windows-x86_64.zip` to a folder you control
   (e.g. under your user directory or `C:\Program Files\Docutaz`).
2. Run **`docutaz.exe`** (same SmartScreen note as above).
3. **Desktop / Start-menu shortcut (optional):** double-click
   **`Create Desktop Shortcut.bat`** in the extracted folder to add Docutaz to
   your Desktop and Start menu (no admin needed). `Remove Desktop Shortcut.bat`
   undoes it. *Don't move the folder afterward* — the shortcut points at
   `docutaz.exe` where it is; if you move it, re-run the script.

Then install **mongosh** (see above; `winget install MongoDB.Shell` or the
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
- **Linux — missing libraries:** launch via `./docutaz`; it lists any missing
  runtime libraries and the install command. Install the packages listed above
  and run it again. (If you run `libexec/docutaz-bin` directly you bypass this
  check and get the raw loader error "error while loading shared libraries".)
- **Windows — app won't start / missing DLL:** install the latest
  [Microsoft Visual C++ Redistributable (x64)](https://aka.ms/vs/17/release/vc_redist.x64.exe).
- **macOS — "damaged / can't be opened":** remove the quarantine flag with the
  `xattr -dr com.apple.quarantine` command above.
- **Update notifications:** Docutaz checks GitHub for newer releases and shows a
  banner when one is available; toggle it under **Options → Automatically Check
  for Updates**, or check on demand via **Help → Check for Updates**. It sends
  no data about you.
