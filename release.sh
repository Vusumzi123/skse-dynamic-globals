#!/usr/bin/env bash
#
# release.sh — cut a GlobalRules release.
#
# Promotes the [Unreleased] section of CHANGELOG.md to a dated version, bumps the
# version in CMakeLists.txt, builds the plugin, packages a zip, then commits, tags,
# pushes, and creates the GitHub release.
#
# Usage:
#   ./release.sh <major.minor.patch> [--dry-run] [--yes]
#
#   1. Add your release notes under "## [Unreleased]" in CHANGELOG.md and commit them.
#   2. Run ./release.sh 1.0.1
#
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_DIR"

VERSION=""
DRY_RUN=0
ASSUME_YES=0

usage() {
    cat <<'EOF'
Usage: ./release.sh <major.minor.patch> [--dry-run] [--yes]

  --dry-run   Do the version bump, changelog promotion, build, and packaging,
              but stop before committing/tagging/pushing/publishing.
  --yes       Skip the confirmation prompt.
EOF
}

die() {
    echo "error: $*" >&2
    exit 1
}

info() {
    echo "==> $*"
}

confirm() {
    [[ "$ASSUME_YES" -eq 1 ]] && return 0
    local reply
    read -r -p "$1 [y/N] " reply
    [[ "$reply" =~ ^[Yy](es)?$ ]]
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --dry-run) DRY_RUN=1 ;;
        --yes|-y)  ASSUME_YES=1 ;;
        --help|-h) usage; exit 0 ;;
        -*)        die "unknown option '$1' (try --help)" ;;
        *)         [[ -z "$VERSION" ]] || die "unexpected argument '$1'"; VERSION="$1" ;;
    esac
    shift
done

[[ -n "$VERSION" ]] || { usage; exit 1; }
[[ "$VERSION" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || die "version must be MAJOR.MINOR.PATCH (got '$VERSION')"

TAG="v$VERSION"
TODAY="$(date +%F)"
NOTES_FILE="$REPO_DIR/build/release-notes-$VERSION.md"
ZIP="$REPO_DIR/build/GlobalRules-$VERSION.zip"

# ---------------------------------------------------------------------------
# Guards
# ---------------------------------------------------------------------------
command -v gh >/dev/null || die "gh (GitHub CLI) not found"
command -v awk >/dev/null || die "awk not found"
command -v zip >/dev/null || die "zip not found"

[[ "$(git rev-parse --abbrev-ref HEAD)" == "main" ]] || die "must be on branch 'main'"
[[ -z "$(git status --porcelain)" ]] || die "working tree is dirty — commit or stash first"

git fetch --quiet origin main
[[ "$(git rev-parse HEAD)" == "$(git rev-parse origin/main)" ]] || die "local main is not up to date with origin/main"

git rev-parse -q --verify "refs/tags/$TAG" >/dev/null && die "tag $TAG already exists locally"
git ls-remote --exit-code --tags origin "$TAG" >/dev/null 2>&1 && die "tag $TAG already exists on origin"

awk '
    /^## \[Unreleased\]/ { insec = 1; next }
    insec && /^## \[/    { exit }
    insec && NF          { found = 1 }
    END { exit found ? 0 : 1 }
' CHANGELOG.md || die "CHANGELOG.md [Unreleased] section is empty — add your notes first"

# Normalize the remote to a browsable https URL for changelog links.
WEB_URL="$(git remote get-url origin)"
WEB_URL="${WEB_URL%.git}"
if [[ "$WEB_URL" =~ ^git@([^:]+):(.+)$ ]]; then
    WEB_URL="https://${BASH_REMATCH[1]}/${BASH_REMATCH[2]}"
fi

info "Releasing GlobalRules $VERSION (tag $TAG)"
if [[ "$DRY_RUN" -eq 1 ]]; then
    info "DRY RUN — no commits, tags, pushes, or releases will be made"
fi
confirm "Proceed?" || die "aborted"

# ---------------------------------------------------------------------------
# Bump version + promote changelog
# ---------------------------------------------------------------------------
info "Bumping CMakeLists.txt to $VERSION"
sed -i -E \
    "s/^(project\(GlobalRules VERSION )[0-9]+\.[0-9]+\.[0-9]+( LANGUAGES CXX\))/\1${VERSION}\2/" \
    CMakeLists.txt
grep -q "project(GlobalRules VERSION ${VERSION} LANGUAGES CXX)" CMakeLists.txt \
    || die "failed to bump version in CMakeLists.txt"

info "Promoting CHANGELOG.md [Unreleased] -> [$VERSION] - $TODAY"
awk -v ver="$VERSION" -v date="$TODAY" '
    !done && $0 == "## [Unreleased]" {
        print "## [Unreleased]"
        print ""
        print "## [" ver "] - " date
        done = 1
        next
    }
    { print }
' CHANGELOG.md > CHANGELOG.md.tmp && mv CHANGELOG.md.tmp CHANGELOG.md

awk -v ver="$VERSION" -v url="$WEB_URL" '
    /^\[Unreleased\]:/ {
        sub(/compare\/v[0-9]+\.[0-9]+\.[0-9]+\.\.\.HEAD/, "compare/v" ver "...HEAD")
        print
        print "[" ver "]: " url "/releases/tag/v" ver
        next
    }
    { print }
' CHANGELOG.md > CHANGELOG.md.tmp && mv CHANGELOG.md.tmp CHANGELOG.md

awk -v ver="$VERSION" '
    /^## \[/ {
        if (insec) exit
        if (index($0, "## [" ver "]") == 1) { insec = 1; next }
    }
    insec { print }
' CHANGELOG.md > "$NOTES_FILE"

# ---------------------------------------------------------------------------
# Build + package
# ---------------------------------------------------------------------------
export VCPKG_ROOT="${VCPKG_ROOT:-$HOME/Projects/vcpkg}"
[[ -d "$VCPKG_ROOT" ]] || die "VCPKG_ROOT not found: $VCPKG_ROOT"

info "Configuring and building (preset linux-clangcl)"
cmake --preset linux-clangcl
cmake --build --preset linux-clangcl

info "Packaging $ZIP"
STAGE="$REPO_DIR/build/package/GlobalRules-$VERSION"
rm -rf "$REPO_DIR/build/package"
mkdir -p "$STAGE/GlobalRules"
cp build/linux-clangcl/GlobalRules.dll "$STAGE/"
cp examples/GlobalRules.json "$STAGE/"
cp examples/GlobalRules/rules.json "$STAGE/GlobalRules/"
cp README.md LICENSE CHANGELOG.md "$STAGE/"
( cd "$REPO_DIR/build/package" && zip -r -q "$ZIP" "GlobalRules-$VERSION" )

if [[ "$DRY_RUN" -eq 1 ]]; then
    info "DRY RUN complete. Working tree modified:"
    echo "    CMakeLists.txt, CHANGELOG.md (unstaged); $ZIP"
    echo "    Revert with: git checkout -- CMakeLists.txt CHANGELOG.md"
    echo "    Would run:"
    echo "      git commit -am 'Release $TAG'"
    echo "      git tag -a $TAG -m 'GlobalRules $VERSION'"
    echo "      git push --follow-tags origin main"
    echo "      gh release create $TAG $ZIP --title 'GlobalRules $TAG' --notes-file $NOTES_FILE"
    exit 0
fi

# ---------------------------------------------------------------------------
# Commit, tag, push, publish
# ---------------------------------------------------------------------------
info "Committing and tagging"
git add CMakeLists.txt CHANGELOG.md
git commit -m "Release $TAG"
git tag -a "$TAG" -m "GlobalRules $VERSION"

info "Pushing to origin/main"
git push --follow-tags origin main

info "Creating GitHub release $TAG"
gh release create "$TAG" "$ZIP" --title "GlobalRules $TAG" --notes-file "$NOTES_FILE"

info "Done: $WEB_URL/releases/tag/$TAG"
