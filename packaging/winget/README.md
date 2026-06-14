# Distributing Docutaz via Winget

This documents how Docutaz reaches the Windows Package Manager
(`winget install Illimitable.Docutaz`) and the one-time setup it needs.

## How it works

The NSIS installer (`install/windows/docutaz.nsi`) is built and attached to
every GitHub Release as `docutaz-<ver>-windows-x86_64-setup.exe`. When a release
is **published as a full release**, `.github/workflows/winget.yml` runs
[`winget-releaser`](https://github.com/vedantmgoyal/winget-releaser) (which wraps
[`komac`](https://github.com/russellbanks/Komac)). It generates the three
manifest files, computes the installer SHA256, and opens a pull request against
[`microsoft/winget-pkgs`](https://github.com/microsoft/winget-pkgs). Microsoft's
automated validation installs it in a sandbox; once merged it's live.

`PackageIdentifier` is **`Illimitable.Docutaz`**; in the repo the manifests live
under `manifests/i/Illimitable/Docutaz/<version>/`.

## One-time setup (required before the first auto-submit)

The default `GITHUB_TOKEN` cannot push to a fork in another org or open a
cross-repo PR to `microsoft/winget-pkgs`, so a personal access token is needed:

1. **Fork** `microsoft/winget-pkgs` into the
   `Illimitable-Consulting-Private-Limited` org (winget-releaser pushes the
   version branch to this fork, then PRs upstream from it).
2. **Create a classic PAT** with the `public_repo` scope (fine-grained tokens
   also work with *Contents: read/write* + *Pull requests: read/write* on the
   fork). Use an account that has access to the fork.
3. Add it as the repository secret **`WINGET_TOKEN`**
   (Settings → Secrets and variables → Actions).

## Sequencing — the first submittable release

⚠️ **The already-published v2.1.0 release has no installer** — the NSIS work
landed after v2.1.0 was tagged. The setup `.exe` first appears in the *next*
release. So the first Winget submission targets that next version (e.g. v2.1.1 /
v2.2.0), not v2.1.0. The workflow handles this automatically: it simply does
nothing until a release that actually carries `…-setup.exe` is published.

## First submission — metadata to verify

`komac` infers most fields, but a brand-new package's `defaultLocale` manifest
should carry the values below. After the first version is accepted, later
versions inherit this metadata automatically, so this only matters once. If the
generated PR is missing or wrong on any of these, fix them on the PR branch:

```yaml
PackageIdentifier: Illimitable.Docutaz
PackageName: Docutaz
Publisher: Illimitable Consulting Private Limited
PublisherUrl: https://illimitable.in
PublisherSupportUrl: https://github.com/Illimitable-Consulting-Private-Limited/docutaz/issues
PackageUrl: https://illimitable-consulting-private-limited.github.io/docutaz/
License: GPL-3.0-or-later
LicenseUrl: https://github.com/Illimitable-Consulting-Private-Limited/docutaz/blob/main/LICENSE
Moniker: docutaz
ShortDescription: Cross-platform MongoDB management tool (MongoDB 8+ GUI).
Description: >-
  Docutaz is a cross-platform desktop GUI for MongoDB 8+, a maintained fork of
  Robomongo/Robo 3T. It provides a shell, query editor, result tree/table/text
  views, and connection management including mongodb+srv:// (Atlas) support.
Tags:
  - mongodb
  - mongo
  - database
  - nosql
  - gui
  - dba
```

Installer manifest essentials (komac fills these from the asset):

```yaml
InstallerType: nullsoft
Scope: machine
Architecture: x64
InstallModes:
  - silent
  - silentWithProgress
```

## Verifying locally (optional)

With winget's `wingetcreate` or `komac` installed you can dry-run a manifest, and
`winget validate <manifest-dir>` checks schema. After merge:

```
winget install Illimitable.Docutaz
```
