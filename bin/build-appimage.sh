#!/usr/bin/env bash
# Builds a self-contained AppImage of Docutaz for Linux.
#
# Unlike bin/bundle-linux.sh (which bundles only the mongo-cxx-driver and relies
# on the host's Qt6/QScintilla/OpenSSL), the AppImage bundles EVERYTHING the app
# needs — Qt6 libraries AND plugins (including the Wayland platform plugin),
# QScintilla, the mongo drivers, libssh2, SASL/GSSAPI — plus the bundled mongosh
# and MongoDB Database Tools, so it runs on any reasonably recent distro with no
# dependencies to install. Just: chmod +x, run.
#
# PORTABILITY RULES (do not "fix" by building elsewhere):
#   - Build on an OLD glibc (CI uses ubuntu-22.04 = glibc 2.35). glibc is only
#     forward compatible, so a binary built on a newer glibc will NOT run on
#     older hosts. Building here on Fedora 43 (glibc 2.42) is for local testing
#     only and will not run on most other machines.
#   - The output AppImage uses the STATICALLY-LINKED FUSE runtime (from
#     appimage/type2-runtime), so it needs neither libfuse2 nor libfuse3 on the
#     host — sidestepping the libfuse2-vs-3 split across distros. The user still
#     needs kernel FUSE / /dev/fuse to mount it (present on ~all desktops); the
#     fallback for a no-FUSE host is `./Docutaz*.AppImage --appimage-extract`
#     or `APPIMAGE_EXTRACT_AND_RUN=1`.
#
# Usage:
#   cd <repo-root>
#   bash bin/build-appimage.sh [path/to/build]
#
# Environment:
#   QMAKE   Path to the Qt6 qmake used to build the app (so linuxdeploy-plugin-qt
#           deploys the MATCHING Qt). Defaults to `qmake6` on PATH.
#
# Output:
#   Docutaz-<version>-<arch>.AppImage  (in the repo root)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${1:-$REPO_ROOT/build}"
BINARY="$BUILD_DIR/src/docutaz/docutaz"

# Bundled-helper versions — keep in lockstep with the Flatpak manifest
# (packaging/flatpak/in.illimitable.Docutaz.yaml).
MONGOSH_VERSION="2.8.3"
DBTOOLS_VERSION="100.17.0"

if [[ ! -x "$BINARY" ]]; then
    echo "ERROR: binary not found at $BINARY"
    echo "       Build first: cmake --build build --target docutaz"
    exit 1
fi

# ── Version / arch ────────────────────────────────────────────────────────────
MAJOR=$(grep 'PROJECT_VERSION_MAJOR' "$REPO_ROOT/CMakeLists.txt" | head -1 | grep -o '"[^"]*"' | tr -d '"')
MINOR=$(grep 'PROJECT_VERSION_MINOR' "$REPO_ROOT/CMakeLists.txt" | head -1 | grep -o '"[^"]*"' | tr -d '"')
PATCH=$(grep 'PROJECT_VERSION_PATCH' "$REPO_ROOT/CMakeLists.txt" | head -1 | grep -o '"[^"]*"' | tr -d '"')
VERSION="${MAJOR}.${MINOR}.${PATCH}"
ARCH="$(uname -m)"

case "$ARCH" in
    x86_64)  MONGO_ARCH="x64";   DBTOOLS_PLATFORM="ubuntu2204-x86_64" ;;
    aarch64) MONGO_ARCH="arm64"; DBTOOLS_PLATFORM="ubuntu2204-arm64" ;;
    *) echo "ERROR: unsupported arch $ARCH"; exit 1 ;;
esac

WORK="$REPO_ROOT/.appimage-build"
APPDIR="$WORK/AppDir"
TOOLS="$WORK/tools"
OUTPUT="$REPO_ROOT/Docutaz-${VERSION}-${ARCH}.AppImage"

echo "==> Building Docutaz ${VERSION} AppImage (${ARCH})"
echo "    Binary : $BINARY"
echo "    Output : $OUTPUT"
echo ""

rm -rf "$WORK"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps" \
         "$TOOLS"

# ── App binary, desktop file, icon ────────────────────────────────────────────
# linuxdeploy resolves the binary's shared-library dependencies (Qt, QScintilla,
# mongo-cxx-driver, libssh2, SASL, …) into usr/lib automatically.
cp "$BINARY" "$APPDIR/usr/bin/docutaz"
chmod +x "$APPDIR/usr/bin/docutaz"

cp "$REPO_ROOT/install/linux/in.illimitable.Docutaz.desktop" \
   "$APPDIR/usr/share/applications/in.illimitable.Docutaz.desktop"
cp "$REPO_ROOT/src/docutaz/gui/resources/icons/logo-256x256.png" \
   "$APPDIR/usr/share/icons/hicolor/256x256/apps/in.illimitable.Docutaz.png"

# ── Bundled mongosh + MongoDB Database Tools ─────────────────────────────────
# A sandboxed/relocated app can't rely on a host copy, so ship MongoDB's official
# prebuilts. They land next to the GUI binary in usr/bin, where findMongosh() and
# MongoTools::candidateDirs() look. We pass them to linuxdeploy as extra
# executables so THEIR library deps (e.g. libgssapi_krb5) are bundled too.
echo "==> Fetching mongosh ${MONGOSH_VERSION} + database tools ${DBTOOLS_VERSION}"
mongosh_tgz="$WORK/mongosh.tgz"
wget -q "https://downloads.mongodb.com/compass/mongosh-${MONGOSH_VERSION}-linux-${MONGO_ARCH}.tgz" -O "$mongosh_tgz"
tar -xzf "$mongosh_tgz" -C "$WORK"
install -Dm755 "$WORK"/mongosh-*/bin/mongosh "$APPDIR/usr/bin/mongosh"
if ls "$WORK"/mongosh-*/bin/mongosh_crypt_v1.so >/dev/null 2>&1; then
    install -Dm755 "$WORK"/mongosh-*/bin/mongosh_crypt_v1.so "$APPDIR/usr/bin/mongosh_crypt_v1.so"
fi

dbtools_tgz="$WORK/dbtools.tgz"
wget -q "https://fastdl.mongodb.org/tools/db/mongodb-database-tools-${DBTOOLS_PLATFORM}-${DBTOOLS_VERSION}.tgz" -O "$dbtools_tgz"
tar -xzf "$dbtools_tgz" -C "$WORK"
for t in mongodump mongorestore mongoexport mongoimport; do
    install -Dm755 "$WORK"/mongodb-database-tools-*/bin/"$t" "$APPDIR/usr/bin/$t"
done

# ── Download linuxdeploy + Qt plugin + static FUSE runtime ────────────────────
# The tool AppImages themselves are run with APPIMAGE_EXTRACT_AND_RUN so this
# works on CI runners without FUSE. The OUTPUT AppImage instead uses the static
# runtime below, so end users need no libfuse.
echo "==> Fetching linuxdeploy toolchain"
ld_arch="$ARCH"
fetch() { wget -q "$1" -O "$2" && chmod +x "$2"; }
fetch "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ld_arch}.AppImage" \
      "$TOOLS/linuxdeploy"
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-${ld_arch}.AppImage" \
      "$TOOLS/linuxdeploy-plugin-qt"
# The appimage OUTPUT plugin (`--output appimage`); it reads $LDAI_RUNTIME_FILE.
fetch "https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-${ld_arch}.AppImage" \
      "$TOOLS/linuxdeploy-plugin-appimage"
# Static-FUSE type2 runtime — no host libfuse2/3 required at run time.
wget -q "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-${ld_arch}" \
     -O "$TOOLS/runtime"

export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$TOOLS:$PATH"

# ── Qt: point the Qt plugin at the qmake that built the app ───────────────────
export QMAKE="${QMAKE:-qmake6}"
# Bundle the Wayland platform plugin alongside the default xcb one — the app is
# Wayland-sensitive (window minimize/restore handling), and without this it would
# fall back to XWayland or fail on Wayland-only sessions. Also bundle the
# offscreen plugin so the CI --version smoke test (QT_QPA_PLATFORM=offscreen) can
# construct a QApplication without an X server.
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so;libqoffscreen.so"
export EXTRA_QT_PLUGINS="wayland-decoration-client;wayland-graphics-integration-client;wayland-shell-integration"

# ── Assemble the AppDir and emit the AppImage ────────────────────────────────
echo "==> Running linuxdeploy"
export LDAI_RUNTIME_FILE="$TOOLS/runtime"     # static-FUSE runtime for the output
export OUTPUT="$OUTPUT"
export VERSION="$VERSION"

"$TOOLS/linuxdeploy" \
    --appdir "$APPDIR" \
    --plugin qt \
    --executable "$APPDIR/usr/bin/mongosh" \
    --executable "$APPDIR/usr/bin/mongodump" \
    --executable "$APPDIR/usr/bin/mongorestore" \
    --executable "$APPDIR/usr/bin/mongoexport" \
    --executable "$APPDIR/usr/bin/mongoimport" \
    --desktop-file "$APPDIR/usr/share/applications/in.illimitable.Docutaz.desktop" \
    --icon-file "$APPDIR/usr/share/icons/hicolor/256x256/apps/in.illimitable.Docutaz.png" \
    --output appimage

echo ""
echo "==> Done: $OUTPUT"
ls -lh "$OUTPUT"
