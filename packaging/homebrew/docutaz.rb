# Docutaz — Homebrew Cask (own tap: Illimitable-Consulting-Private-Limited/homebrew-docutaz)
#
# This file is the canonical source for the cask. Seed the tap with it once
# (copy to the tap's Casks/docutaz.rb); thereafter .github/workflows/homebrew.yml
# rewrites the `version` and `sha256` lines from each published GitHub Release
# and pushes the result to the tap. Keep those two lines in this exact shape
# (literal " on the same line) so the workflow's sed can find them.
cask "docutaz" do
  version "0.0.0"  # overwritten by the release workflow
  sha256 "0000000000000000000000000000000000000000000000000000000000000000"  # overwritten by the release workflow

  url "https://github.com/Illimitable-Consulting-Private-Limited/docutaz/releases/download/v#{version}/docutaz-#{version}-macos-arm64.zip"
  name "Docutaz"
  desc "Cross-platform MongoDB 8+ management GUI"
  homepage "https://illimitable-consulting-private-limited.github.io/docutaz/"

  livecheck do
    url "https://github.com/Illimitable-Consulting-Private-Limited/docutaz/releases/latest"
    strategy :github_latest
  end

  # arm64-only build today (the CI macOS asset is Apple Silicon). mongosh is a
  # runtime dependency; Homebrew installs it via its own formula.
  depends_on arch: :arm64
  depends_on formula: "mongosh"

  app "Docutaz.app"

  zap trash: "~/.Docutaz"

  caveats <<~EOS
    Docutaz is ad-hoc signed but NOT notarized (no Apple Developer ID yet), so
    Gatekeeper blocks it unless the quarantine flag is cleared at install time:

      brew install --cask --no-quarantine docutaz

    Docutaz uses the MongoDB Shell (mongosh) at runtime; this cask installs it
    as a dependency.
  EOS
end
