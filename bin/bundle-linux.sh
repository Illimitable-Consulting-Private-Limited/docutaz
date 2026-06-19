#!/usr/bin/env bash
# Creates a self-contained tarball of Docutaz for Linux.
# The tarball bundles the binary + non-standard shared libraries (mongo-cxx-driver).
# Everything else — Qt6 (libraries AND plugins), QScintilla (qt6), OpenSSL 3,
# libssh2 — is provided by the host's package manager (see README requirements);
# Qt plugins are intentionally not bundled (they must match the host's libQt6Core).
#
# Also includes the .desktop file and icons so the launcher icon works correctly
# on Wayland (where setWindowIcon() is ignored — the icon comes from the icon theme).
# Run ./install-desktop.sh after extracting to register with the desktop environment.
#
# Usage:
#   cd <repo-root>
#   bash bin/bundle-linux.sh [path/to/build]
#
# Output:
#   docutaz-<version>-linux-x86_64.tar.gz  (in the repo root)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build}"
BINARY="$BUILD_DIR/src/docutaz/docutaz"
# Tiny glibc-only launcher (see src/docutaz/app/preflight.c). Shipped as the
# top-level `docutaz`; it checks the host libraries and execs the GUI binary.
STUB="$BUILD_DIR/src/docutaz/docutaz-preflight"

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary not found at $BINARY"
    echo "       Build first: cd build && ninja docutaz -j\$(nproc)"
    exit 1
fi

if [[ ! -x "$STUB" ]]; then
    echo "ERROR: preflight launcher not found at $STUB"
    echo "       Build first: cd build && ninja docutaz_preflight -j\$(nproc)"
    exit 1
fi

# Read version from CMakeLists.txt
MAJOR=$(grep 'PROJECT_VERSION_MAJOR' "$REPO_ROOT/CMakeLists.txt" | head -1 | grep -o '"[^"]*"' | tr -d '"')
MINOR=$(grep 'PROJECT_VERSION_MINOR' "$REPO_ROOT/CMakeLists.txt" | head -1 | grep -o '"[^"]*"' | tr -d '"')
PATCH=$(grep 'PROJECT_VERSION_PATCH' "$REPO_ROOT/CMakeLists.txt" | head -1 | grep -o '"[^"]*"' | tr -d '"')
VERSION="${MAJOR}.${MINOR}.${PATCH}"

# Architecture suffix from the build host: x86_64 or aarch64 (arm64).
ARCH="$(uname -m)"

BUNDLE_NAME="docutaz-${VERSION}-linux-${ARCH}"
BUNDLE_DIR="$REPO_ROOT/$BUNDLE_NAME"
TARBALL="$REPO_ROOT/${BUNDLE_NAME}.tar.gz"

echo "==> Bundling Docutaz ${VERSION}"
echo "    Binary : $BINARY"
echo "    Output : $TARBALL"
echo ""

# ── Clean previous bundle ────────────────────────────────────────────────────
rm -rf "$BUNDLE_DIR"
mkdir -p "$BUNDLE_DIR/lib" \
         "$BUNDLE_DIR/libexec" \
         "$BUNDLE_DIR/icons"

# ── Copy binaries ─────────────────────────────────────────────────────────────
# The GUI binary goes in libexec/ (named docutaz-bin) so users don't run it
# directly and bypass the dependency check; the top-level `docutaz` they launch
# is the glibc-only preflight launcher, which verifies the host libraries and
# then exec()s libexec/docutaz-bin.
cp "$BINARY" "$BUNDLE_DIR/libexec/docutaz-bin"
cp "$STUB"   "$BUNDLE_DIR/docutaz"
chmod +x "$BUNDLE_DIR/libexec/docutaz-bin" "$BUNDLE_DIR/docutaz"

# ── Bundle non-standard shared libraries (mongo-cxx-driver) ──────────────────
# These are not available in distro package managers and must be bundled.
MONGO_LIBS=(
    libmongocxx.so._noabi
    libbsoncxx.so._noabi
    libmongoc2.so.2
    libbson2.so.2
)

MONGO_LIB_DIRS=(/usr/local/lib64 /usr/local/lib /usr/lib64 /usr/lib)

for lib in "${MONGO_LIBS[@]}"; do
    found=0
    for dir in "${MONGO_LIB_DIRS[@]}"; do
        # Resolve the real file behind the symlink
        reallib=$(find "$dir" -maxdepth 1 -name "${lib%.*.*}*.so.*.*" 2>/dev/null | head -1)
        if [[ -n "$reallib" && -f "$reallib" ]]; then
            echo "    bundling $lib  ->  $(basename "$reallib")"
            cp "$reallib" "$BUNDLE_DIR/lib/"
            # Create the soname symlink the binary actually looks for
            ln -sf "$(basename "$reallib")" "$BUNDLE_DIR/lib/$lib"
            found=1
            break
        fi
    done
    if [[ $found -eq 0 ]]; then
        echo "    WARNING: $lib not found — bundle may not run without it"
    fi
done

# ── Generate the runtime dependency manifest for the preflight launcher ──────
# List the SYSTEM shared libraries the GUI binary hard-links (its DT_NEEDED
# entries), minus the ones we bundle ourselves (mongo driver) and the C runtime/
# loader (always present). The launcher dlopen-probes each at startup and tells
# the user how to install whatever is missing instead of crashing. Deriving the
# list from the actual binary keeps it correct as dependencies change.
list_needed() {
    if command -v readelf &>/dev/null; then
        readelf -d "$1" | awk -F'[][]' '/NEEDED/ {print $2}'
    else
        objdump -p "$1" | awk '/NEEDED/ {print $2}'
    fi
}
list_needed "$BINARY" \
    | grep -Ev '^(libmongocxx|libbsoncxx|libmongoc2|libbson2|libc\.so|libm\.so|libdl\.so|libpthread\.so|librt\.so|ld-linux)' \
    | sort -u > "$BUNDLE_DIR/libexec/required-libs.txt"
echo "    generated libexec/required-libs.txt ($(wc -l < "$BUNDLE_DIR/libexec/required-libs.txt") libs to verify)"

# ── Qt plugins: deliberately NOT bundled (use the host's) ────────────────────
# We do not bundle the Qt6 core libraries (libQt6Core/Gui/Widgets/...) — they
# come from the host's Qt6 install (see README requirements). A Qt plugin must
# match the exact libQt6Core that is actually loaded (platform plugins reference
# version-specific private symbols, e.g. Qt_6.x_PRIVATE_API, and libQt6XcbQpa).
# Bundling a plugin built against one Qt 6.x and loading it against the host's
# different Qt 6.x breaks startup ("no Qt platform plugin could be initialized").
#
# Qt resolves its plugin directory from the loaded libQt6Core (QLibraryInfo), so
# with no bundled plugins and no qt.conf the host's Qt finds its own matching
# platform/imageformat plugins automatically. That is why we ship neither a
# plugins/ tree nor a qt.conf.

# ── Bundle icons ─────────────────────────────────────────────────────────────
ICON_SRC="$REPO_ROOT/src/docutaz/gui/resources/icons"
cp "$ICON_SRC/logo-256x256.png" "$BUNDLE_DIR/icons/docutaz-256x256.png"
cp "$ICON_SRC/logo-20x20.png"   "$BUNDLE_DIR/icons/docutaz-20x20.png"
echo "    bundled icons"

# ── Bundle .desktop file ──────────────────────────────────────────────────────
# Named by the reverse-DNS app id (matches StartupWMClass / Wayland app_id set
# via QApplication::setDesktopFileName, and the Flatpak app id).
cp "$REPO_ROOT/install/linux/in.illimitable.Docutaz.desktop" "$BUNDLE_DIR/in.illimitable.Docutaz.desktop"
echo "    bundled in.illimitable.Docutaz.desktop"

# ── Write launcher script (compatibility shim) ───────────────────────────────
# The real launcher is now the compiled `docutaz` (the preflight binary), which
# sets up the library path itself. This shim is kept only so older instructions
# / muscle memory ("./docutaz.sh") still work — it just forwards to ./docutaz.
cat > "$BUNDLE_DIR/docutaz.sh" <<'LAUNCHER'
#!/usr/bin/env bash
# Compatibility shim — runs ./docutaz (the launcher does dependency checks and
# sets the library path). Prefer running ./docutaz directly.
DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$DIR/docutaz" "$@"
LAUNCHER
chmod +x "$BUNDLE_DIR/docutaz.sh"

# ── Write desktop integration install/uninstall scripts ──────────────────────
# install-desktop.sh: registers the .desktop file and icons so the launcher
# icon appears correctly on Wayland (and in app menus on X11).
cat > "$BUNDLE_DIR/install-desktop.sh" <<'INSTALL'
#!/usr/bin/env bash
# Registers Docutaz with the desktop environment.
# Required for the launcher icon to appear on Wayland.
# Safe to run multiple times.

set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"

ICON_DIR="$HOME/.local/share/icons/hicolor"
APPS_DIR="$HOME/.local/share/applications"
APP_ID="in.illimitable.Docutaz"

mkdir -p "$ICON_DIR/256x256/apps" "$ICON_DIR/20x20/apps" "$APPS_DIR"

# Icons are installed under the app id so the Icon= key and the Wayland app_id
# resolve to them.
cp "$DIR/icons/docutaz-256x256.png" "$ICON_DIR/256x256/apps/$APP_ID.png"
cp "$DIR/icons/docutaz-20x20.png"   "$ICON_DIR/20x20/apps/$APP_ID.png"

# Write .desktop with the absolute path to this install's launcher (the
# preflight `docutaz`, which checks deps then execs the GUI in libexec/).
sed "s|Exec=docutaz|Exec=$DIR/docutaz|g" "$DIR/$APP_ID.desktop" > "$APPS_DIR/$APP_ID.desktop"

# Refresh icon cache if gtk-update-icon-cache is available
if command -v gtk-update-icon-cache &>/dev/null; then
    gtk-update-icon-cache -f -t "$ICON_DIR" 2>/dev/null || true
fi
# Refresh app database
if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$APPS_DIR" 2>/dev/null || true
fi

echo "Docutaz registered with your desktop environment."
echo "You can now launch it from your app menu, or run:  $DIR/docutaz.sh"
INSTALL
chmod +x "$BUNDLE_DIR/install-desktop.sh"

cat > "$BUNDLE_DIR/uninstall-desktop.sh" <<'UNINSTALL'
#!/usr/bin/env bash
# Removes Docutaz desktop registration (icons + .desktop file).
rm -f "$HOME/.local/share/icons/hicolor/256x256/apps/in.illimitable.Docutaz.png"
rm -f "$HOME/.local/share/icons/hicolor/20x20/apps/in.illimitable.Docutaz.png"
rm -f "$HOME/.local/share/applications/in.illimitable.Docutaz.desktop"
if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$HOME/.local/share/applications" 2>/dev/null || true
fi
echo "Docutaz desktop registration removed."
UNINSTALL
chmod +x "$BUNDLE_DIR/uninstall-desktop.sh"

# ── Write README ─────────────────────────────────────────────────────────────
cat > "$BUNDLE_DIR/README.txt" <<EOF
Docutaz ${VERSION} — Linux ${ARCH}
==================================

Requirements
------------
Install these from your package manager before running:

  Fedora / RHEL:
    sudo dnf install qt6-qtbase qt6-qtbase-gui qt6-qtsvg qscintilla-qt6 libssh2 openssl mongosh

  Debian / Ubuntu:
    sudo apt install libqt6widgets6 libqt6network6 libqt6xml6 libqt6printsupport6 libqt6svg6 libqscintilla2-qt6-15 libssh2-1 libssl3 mongosh

  mongosh (all distros):
    https://www.mongodb.com/try/download/shell

Running
-------
  ./docutaz

If any required system library above is missing, ./docutaz won't crash — it
prints exactly which libraries are missing and the command to install them
(and shows a dialog when launched from the desktop menu). Once the libraries
are installed, run it again.

(./docutaz.sh still works as a compatibility shim; it just runs ./docutaz.)

Desktop integration (launcher icon, app menu, Wayland icon)
------------------------------------------------------------
Run once after extracting:
  bash install-desktop.sh

This registers the .desktop file and installs the icons into your
~/.local/share/ icon theme so the launcher icon appears correctly
on both X11 and Wayland. To undo:
  bash uninstall-desktop.sh

The bundled lib/ directory contains the mongo-cxx-driver libraries
(libmongocxx, libbsoncxx, libmongoc2, libbson2) which are not available
in standard distro repositories.

Source code: https://github.com/Illimitable-Consulting-Private-Limited/docutaz
License: GNU GPL v3
EOF

# ── Create tarball ────────────────────────────────────────────────────────────
cd "$REPO_ROOT"
tar -czf "$TARBALL" "$BUNDLE_NAME/"
rm -rf "$BUNDLE_DIR"

SIZE=$(du -sh "$TARBALL" | cut -f1)
echo ""
echo "==> Done: $TARBALL  ($SIZE)"
echo ""
echo "    Share this file with colleagues."
echo "    They extract it, run:  bash install-desktop.sh  (once)"
echo "    Then launch with:      ./docutaz"
