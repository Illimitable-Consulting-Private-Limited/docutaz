# Contributing to Docutaz

Thanks for your interest in improving Docutaz! This guide covers how the
repository is organized and the workflow for getting a change merged.

For large or structural changes, please **open an issue first** so we can agree
on the approach before you invest time.

## Branch model

| Branch | Purpose |
|--------|---------|
| **`main`** | Stable / release branch. Always buildable and green on CI. Releases are cut from here. Do not push directly. |
| **`develop`** | Day-to-day development and bug fixes. This is the base for everyday work and the target for most pull requests. |
| **`modernization`** | Parked. Reserved for large dependency/build-system modernization efforts (the most recent one is recorded in [`docs/MODERNIZATION.md`](docs/MODERNIZATION.md)). Not used for routine work. |

## Workflow

1. **Branch off `develop`:**
   ```sh
   git checkout develop
   git pull
   git checkout -b fix/short-description   # or feature/short-description
   ```
2. Make your change. Keep commits focused, with clear messages; reference the
   issue number where relevant (e.g. `Fix crash on empty result set (#123)`).
3. **Open a pull request into `develop`.** Describe what changed and why, and
   how you verified it.
4. A maintainer reviews and merges. Releases happen by merging `develop` into
   `main`.

> `main` is protected — changes reach it only via `develop`.

## Continuous integration

CI (`.github/workflows/build.yml`) builds and tests on **Linux, Windows, and
macOS**, and smoke-tests that the packaged binary launches on each:

- **Pushes to `main`** and **pull requests targeting `main`** run CI
  automatically.
- The **`develop`** branch is validated **on demand** via
  *Actions → Build → Run workflow* (`workflow_dispatch`), and again
  automatically when `develop` is PR'd into `main`. A maintainer may trigger a
  run on your PR branch when reviewing.

## Building and testing

See the [README](README.md) for per-platform build prerequisites and steps.
The CI workflow (`.github/workflows/build.yml`) contains exact, working build
commands for all three platforms and is the source of truth if the README and
your environment disagree.

Unit tests use GoogleTest (fetched automatically when
`-DDOCUTAZ_BUILD_TESTS=ON`, the default):

```sh
cmake -S . -B build -DDOCUTAZ_BUILD_TESTS=ON
cmake --build build --target docutaz_unit_tests
ctest --test-dir build --output-on-failure
```

Please add or update tests when changing behavior that is unit-testable
(e.g. BSON/EJSON handling, string/crypto utilities).

## Coding notes

- The project targets **C++17** and **Qt 6**, and builds against any Qt 6.x —
  avoid Qt private APIs (`*_p.h` / `private/...`), which pin the binary to one
  Qt minor version.
- It is a MongoDB client: the only network activity is user-initiated MongoDB
  connections and SSH tunnels. **Do not add telemetry, analytics, or
  phone-home behavior.**

## License

By contributing, you agree that your contributions are licensed under the
project's [GPL v3](LICENSE).
