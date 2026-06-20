# Flatpak (Linux)

Docutaz packaged as a Flatpak. The app is sandboxed and ships its own copy of
MongoDB's `mongosh`, so it works on any distro without the user installing Qt,
the Mongo drivers, or the shell separately.

## Files

- **`in.illimitable.Docutaz.yaml`** — the manifest. Builds `libssh2`, the Mongo
  C/C++ drivers, QScintilla and Docutaz against the **KDE 6.10** runtime, and
  bundles the prebuilt `mongosh` into `/app/bin`.
- **`../../install/linux/in.illimitable.Docutaz.desktop`** — desktop entry.
- **`../../install/linux/in.illimitable.Docutaz.metainfo.xml`** — AppStream
  metadata (validate with `appstreamcli validate <file>`).
- **`../../install/linux/icons/`** — hicolor PNG icons.

The Flatpak install layout comes from `-DDOCUTAZ_FLATPAK=ON`, which makes CMake
install only Docutaz's own artifacts into `/app` and bundle no host libraries
(the runtime provides Qt/ICU/OpenSSL). The glibc-only pre-flight launcher is not
used inside Flatpak — the runtime guarantees the libraries the GUI links against.

## Build & run locally

```sh
flatpak install flathub org.kde.Platform//6.10 org.kde.Sdk//6.10
flatpak-builder --user --install --force-clean build-flatpak \
    packaging/flatpak/in.illimitable.Docutaz.yaml
flatpak run in.illimitable.Docutaz
```

To build from your working tree instead of the published tag, change the
`docutaz` module source to:

```yaml
    sources:
      - type: dir
        path: ../..
```

## Status

The manifest's source URLs and `sha256` sums are real and verified. The build
recipes have **not** yet been confirmed with a `flatpak-builder` run; expect the
`qscintilla` module (qmake-only, relocated from the runtime Qt prefix into
`/app`) to be the first thing to adjust if a build fails.

## Updating

- **mongosh**: bump the version and both arch `sha256`s in the `mongosh` module.
- **Mongo drivers**: keep in sync with `MONGOC_VERSION` / `MONGOCXX_VERSION` in
  `.github/workflows/build.yml`.
- **Runtime**: bump `runtime-version`/`sdk` when moving to a newer KDE runtime.

## Flathub submission (later)

Submitting to Flathub is a PR to `flathub/flathub` adding this manifest. Before
that: add real application screenshots to the metainfo (the placeholder must be
replaced), pin the `docutaz` source to a tag **and** commit, and confirm a clean
`flatpak-builder` build on x86_64 and aarch64.
