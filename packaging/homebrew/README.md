# Homebrew Cask (macOS)

Docutaz is distributed on macOS through our **own Homebrew tap**, not the
official `homebrew/cask` repo (that requires notability + notarization, which we
revisit later). The cask installs the ad-hoc-signed `Docutaz.app` from the
GitHub Release.

## Install (end users)

```sh
brew tap illimitable-consulting-private-limited/docutaz
brew install --cask --no-quarantine docutaz
```

`--no-quarantine` is required: the app is ad-hoc signed but **not notarized**
(no Apple Developer ID yet), so Gatekeeper would otherwise block it. mongosh is
pulled in automatically as a Homebrew formula dependency.

## How it's wired

- **`docutaz.rb`** here is the canonical cask source.
- **`.github/workflows/homebrew.yml`** runs on each *published* (non-draft,
  non-prerelease) GitHub Release: it downloads the `…-macos-arm64.zip` asset,
  computes its `sha256`, rewrites the `version`/`sha256` lines, and pushes the
  result to the tap repo's `Casks/docutaz.rb`.
- The macOS CI job ad-hoc signs the bundle (`codesign --force --deep --sign -`)
  before zipping, so the arm64 app launches at all.

## One-time setup (maintainer)

1. **Create the tap repo** `Illimitable-Consulting-Private-Limited/homebrew-docutaz`
   (Homebrew requires the `homebrew-` prefix). Add `Casks/docutaz.rb` by copying
   this directory's `docutaz.rb`.
2. **Create a classic PAT** with `repo` scope that can push to that tap, and add
   it as the **`HOMEBREW_TAP_TOKEN`** secret on this repository.
3. The first release published after this lands will populate the cask's real
   `version` + `sha256`. (Releases built before the ad-hoc-sign CI step won't run
   on Apple Silicon, so the first cask-able version is the next release.)

## Notes / future

- **arm64 only** today — the CI macOS asset is Apple Silicon. Add an x86_64
  asset + `depends_on arch:` split if Intel demand appears.
- Graduating to the official `homebrew/cask` needs notarization (the $99/yr
  Apple Developer ID) and meeting Homebrew's notability bar.
