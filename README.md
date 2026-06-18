# Docutaz

A cross-platform MongoDB management tool for developers.  
Supports **MongoDB 8+** with a modern `mongosh`-based shell.

> **Pronounced:** dok · you · taz  
> *Dotaz* is the Czech word for *query*. Combined with *doc* (document), it captures in one word what MongoDB stores (documents) and what you do with them (query).

---

## Features

- Connect to local or remote MongoDB instances (standalone, replica set, Atlas)
- Browse databases, collections, and documents in a GUI explorer
- Run multi-statement JavaScript queries in a full `mongosh` shell tab
- Query editor with one-keystroke formatting, code folding, bracket-pair
  colorization, auto-closing brackets, and Mongo-aware syntax highlighting
- Copy query results into another open connection / database / collection — for
  pulling a slice of data somewhere safe to debug
- Export query and aggregation results to **JSON**, **CSV**, or **Excel (.xlsx)**
- Insert, edit, and delete documents with a visual editor
- Manage indexes, users, and collection structure
- Page through large result sets with configurable batch sizes
- Use `<database>` directly in the shell to switch contexts
- SSH tunnel and TLS/SSL connection support
- Import existing connections from Robo 3T (`~/.3T/robo-3t/`)

---

## Install

Grab a prebuilt download (Linux x86_64/arm64, macOS arm64, Windows x64) from the
[**Releases**](https://github.com/Illimitable-Consulting-Private-Limited/docutaz/releases)
page and follow the [**installation guide**](docs/INSTALL.md). You'll need
[`mongosh`](https://www.mongodb.com/try/download/shell) installed separately
(it isn't bundled). Linux users: run `./install-desktop.sh` once for the app-menu
launcher icon.

The sections below are for **building from source**.

---

## Requirements

### Runtime
- **mongosh** 2.x or later — must be on your `PATH` or configured in Preferences  
  Install from [mongodb.com/try/download/shell](https://www.mongodb.com/try/download/shell)
- **MongoDB** 8.0+ (older versions may work but are untested)

### Build dependencies
| Dependency | Version | Notes |
|---|---|---|
| CMake | ≥ 3.16 | |
| C++ compiler | C++17 | GCC 9+, Clang 10+, MSVC 2019+ (Qt 6 floor) |
| Qt 6 | 6.5+ | Core, Gui, Widgets, Network, Xml, PrintSupport |
| QScintilla | 2.14.x (Qt6) | system (Linux) / Homebrew (macOS) / from source (Windows) |
| mongo-cxx-driver | 4.x | `mongocxx` + `bsoncxx` |
| OpenSSL | 3.x | |
| libssh2 | system (Linux) / Homebrew (macOS) / vcpkg (Windows) | package-managed, 1.11.x |

Nothing is vendored under `src/third-party/` any more. GoogleTest 1.15.2 is
fetched on demand (pinned via CMake `FetchContent`) only when
`-DDOCUTAZ_BUILD_TESTS=ON`.

---

## Building

Install the system packages, then build and install the mongo-cxx-driver, and
finally build Docutaz — in that order (the Docutaz configure step needs the
driver already installed).

### 1. Linux — system packages (Fedora/RHEL)
```bash
sudo dnf install cmake ninja-build gcc-c++ qt6-qtbase-devel \
    qscintilla-qt6-devel openssl-devel libssh2-devel
```

### 1. Linux — system packages (Debian/Ubuntu)
```bash
sudo apt install cmake ninja-build g++ qt6-base-dev \
    libqscintilla2-qt6-dev libssl-dev libssh2-1-dev
```

### 2. mongo-cxx-driver
Docutaz links against `libmongocxx` and `libbsoncxx` (mongo-cxx-driver **4.x**,
which builds on mongo-c-driver 2.x). Build and install both **before** building
Docutaz:
```bash
# See https://github.com/mongodb/mongo-cxx-driver (build mongo-c-driver, then
# mongo-cxx-driver). The CI workflow (.github/workflows/build.yml) has exact,
# working build steps for all three platforms.
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo cmake --install .
```

### 3. Docutaz
```bash
# Clone
git clone https://github.com/Illimitable-Consulting-Private-Limited/docutaz.git
cd docutaz

# Configure (point CMAKE_PREFIX_PATH at the installed driver from step 2)
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/usr/local/lib64/cmake

# Build
ninja docutaz -j$(nproc)

# Binary is at:
# build/src/docutaz/docutaz        (Linux)
# build/src/docutaz/Docutaz.app    (macOS)
# build/src/docutaz/docutaz.exe    (Windows)
```

---

## Configuration

Settings are stored in `~/.Docutaz/<version>/docutaz.json`.

On first launch, Docutaz automatically imports connections from `~/.3T/robo-3t/` (Robo 3T) if found — those files are never modified.

To configure the `mongosh` path manually: **Options → Preferences → Shell**.

---

## License

GPL v3 — see [LICENSE](LICENSE). Fork of the [Robomongo](https://github.com/Studio3T/robomongo) open-source project.

---

## Contributing

Issues and pull requests are welcome — see [CONTRIBUTING.md](CONTRIBUTING.md)
for the branch model and workflow. In short: branch off `develop` and open PRs
against `develop`; `main` is the stable release branch. Please open an issue
before starting large changes so we can discuss the approach.
