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
- Insert, edit, and delete documents with a visual editor
- Manage indexes, users, and collection structure
- Page through large result sets with configurable batch sizes
- Use `<database>` directly in the shell to switch contexts
- SSH tunnel and TLS/SSL connection support
- Import existing connections from Robo 3T (`~/.3T/robo-3t/`)

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
| C++ compiler | C++17 | GCC 8+, Clang 7+, MSVC 2019+ |
| Qt 5 | 5.12+ | Core, Gui, Widgets, Network, Xml, PrintSupport |
| mongo-cxx-driver | 4.x | `mongocxx` + `bsoncxx` |
| OpenSSL | 3.x | |
| libssh2 | system (Linux) / Homebrew (macOS) / vcpkg (Windows) | package-managed, 1.11.x |

The following are bundled under `src/third-party/`:
- QScintilla 2.8.4
- Esprima 2.7.3
- GoogleTest 1.8.1

---

## Building

```bash
# Clone
git clone https://github.com/Illimitable-Consulting-Private-Limited/docutaz.git
cd docutaz

# Configure
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Release

# Build
ninja docutaz -j$(nproc)

# Binary is at:
# build/src/docutaz/docutaz        (Linux)
# build/src/docutaz/Docutaz.app    (macOS)
# build/src/docutaz/docutaz.exe    (Windows)
```

### Linux — additional packages (Fedora/RHEL)
```bash
sudo dnf install cmake ninja-build gcc-c++ qt5-qtbase-devel \
    qt5-qtnetwork-devel qt5-qtxml-devel qt5-qtsvg-devel \
    openssl-devel libssh2-devel
```

### Linux — additional packages (Debian/Ubuntu)
```bash
sudo apt install cmake ninja-build g++ qtbase5-dev libssl-dev libssh2-1-dev
```

### mongo-cxx-driver
Docutaz links against `libmongocxx` and `libbsoncxx`. Build and install them first:
```bash
# See https://mongocxx.org/mongocxx-v3/installation/
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo cmake --install .
```
Then pass `-DCMAKE_PREFIX_PATH=/usr/local/lib64/cmake` (or your install prefix) to the Docutaz cmake configure step.

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

Issues and pull requests are welcome.  
Please open an issue before starting large changes so we can discuss the approach.
