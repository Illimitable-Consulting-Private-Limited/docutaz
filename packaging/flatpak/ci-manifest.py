#!/usr/bin/env python3
"""Emit a CI variant of the Flatpak manifest.

The committed manifest builds the `docutaz` module from the published git tag so
it is Flathub-submittable as-is. CI instead needs to build the *checked-out*
tree, so this rewrites only the last module's (docutaz's) source list to a local
`dir` source pointing at the repo root. Everything else — runtime, the dependency
modules, their pinned URLs+sha256 — is left untouched.

No third-party deps (no PyYAML): docutaz is the final module, so everything from
its `sources:` key to end-of-file is its source list and is replaced wholesale.

Usage: ci-manifest.py <in-manifest> <out-manifest>
The out-manifest must sit in the same directory as the in-manifest so the
`path: ../..` stays correct relative to the manifest location.
"""
import sys

def main() -> int:
    src_path, out_path = sys.argv[1], sys.argv[2]
    text = open(src_path).read()
    marker = "\n    sources:"
    idx = text.rindex(marker)  # the docutaz module is last → its sources block
    rewritten = text[:idx] + "\n    sources:\n      - type: dir\n        path: ../..\n"
    open(out_path, "w").write(rewritten)
    print(f"wrote {out_path} (docutaz source -> dir ../..)")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
