#!/usr/bin/env bash
#
# deploy.sh — copy the built plugin + test kit into the Amethyst "GlobalRules" mod.
#
# Amethyst deploys mods from its staging folder into the game's Data/ and keeps the
# original Data as a temporary Data_Core/ during deploy. Staging GlobalRules as a mod
# keeps it tracked and surviving restore/deploy cycles.
#
# Usage: ./deploy.sh [--build] [--esp PATH] [--clean-loose] [--dry-run] [--help]
#
# Then in Amethyst: enable the "GlobalRules" mod, enable "GRTest.esp" (Plugins tab),
# click Deploy, launch via SKSE.
#
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$REPO_DIR"

MOD_DIR="${AMETHYST_MOD_DIR:-/mnt/Data/Mod Staging/Default/Skyrim Special Edition/mods/GlobalRules}"
GAME_DIR="${SKYRIM_DIR:-/mnt/Games/SteamLibrary/steamapps/common/Skyrim Special Edition}"
XEDIT_SCRIPTS_DIR="${XEDIT_SCRIPTS_DIR:-/mnt/Data/Mod Staging/Default/Skyrim Special Edition/Applications/SSEEdit/Edit Scripts}"
BUILD_DLL="$REPO_DIR/build/linux-clangcl/GlobalRules.dll"

BUILD=0; DRY_RUN=0; CLEAN_LOOSE=0; ESP_SRC=""

usage() { cat <<'EOF'
Usage: ./deploy.sh [--build] [--esp PATH] [--clean-loose] [--dry-run]

  --build        Run `ninja` in build/linux-clangcl first.
  --esp PATH     Copy this GRTest.esp into the mod (overrides the seeded copy).
  --clean-loose  Remove the untracked duplicate GlobalRules files from Data_Core/.
  --dry-run      Print actions without changing anything.

Also deploys tools/GRTest-Builder.pas to xEdit's Edit Scripts folder (override
XEDIT_SCRIPTS_DIR).
EOF
}
die() { echo "error: $*" >&2; exit 1; }
info() { echo "==> $*"; }

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build) BUILD=1 ;;
        --esp) ESP_SRC="${2:-}"; shift ;;
        --clean-loose) CLEAN_LOOSE=1 ;;
        --dry-run) DRY_RUN=1 ;;
        --help|-h) usage; exit 0 ;;
        *) die "unknown option '$1' (try --help)" ;;
    esac
    shift
done

run() { if [[ "$DRY_RUN" -eq 1 ]]; then echo "  [dry-run] $*"; else "$@"; fi; }

[[ -d "$MOD_DIR" ]] || die "mod folder not found: $MOD_DIR (create the empty mod in Amethyst first)"
[[ "$BUILD" -eq 1 ]] && { info "Building"; ninja -C "$REPO_DIR/build/linux-clangcl"; }
[[ -f "$BUILD_DLL" ]] || die "built DLL not found: $BUILD_DLL (use --build)"

PLUG_DIR="$MOD_DIR/SKSE/Plugins"; RULES_DIR="$PLUG_DIR/GlobalRules"
info "Deploying into $MOD_DIR"
run mkdir -p "$PLUG_DIR" "$RULES_DIR"

run cp -f "$BUILD_DLL" "$PLUG_DIR/GlobalRules.dll"
run cp -f "$REPO_DIR/test/GlobalRules.json"        "$PLUG_DIR/GlobalRules.json"
run cp -f "$REPO_DIR/test/GlobalRules.dryrun.json" "$PLUG_DIR/GlobalRules.dryrun.json"
run cp -f "$REPO_DIR/test/GlobalRules.json"        "$PLUG_DIR/GlobalRules.json.bak"
for f in "$REPO_DIR"/test/GlobalRules/0[1-4]-*.json; do run cp -f "$f" "$RULES_DIR/"; done

# Prefer an explicit --esp. Otherwise, if xEdit has written a fresh GRTest.esp
# into the game Data folder, sync it into the mod. Use -ef so a Data symlink
# pointing back at the mod file is detected (avoids a same-file cp error).
if [[ -z "$ESP_SRC" && -f "$GAME_DIR/Data/GRTest.esp" ]]; then
    ESP_SRC="$GAME_DIR/Data/GRTest.esp"
fi
if [[ -n "$ESP_SRC" ]]; then
    [[ -f "$ESP_SRC" ]] || die "ESP not found: $ESP_SRC"
    if [[ "$ESP_SRC" -ef "$MOD_DIR/GRTest.esp" ]]; then
        info "GRTest.esp already in sync (same file)"
    else
        info "Syncing GRTest.esp: $ESP_SRC -> $MOD_DIR/GRTest.esp"
        run cp -f "$ESP_SRC" "$MOD_DIR/GRTest.esp"
    fi
elif [[ ! -f "$MOD_DIR/GRTest.esp" ]]; then
    echo "warning: no GRTest.esp in the mod and none at $GAME_DIR/Data/GRTest.esp" >&2
fi

[[ -f "$REPO_DIR/tools/GRTest-Builder.pas" ]] || die "tools/GRTest-Builder.pas not found"
[[ -d "$XEDIT_SCRIPTS_DIR" ]] || die "xEdit Edit Scripts dir not found: $XEDIT_SCRIPTS_DIR"
info "Deploying GRTest-Builder.pas -> $XEDIT_SCRIPTS_DIR"
run cp -f "$REPO_DIR/tools/GRTest-Builder.pas" "$XEDIT_SCRIPTS_DIR/GRTest-Builder.pas"

if [[ "$CLEAN_LOOSE" -eq 1 ]]; then
    CORE="$GAME_DIR/Data_Core"
    info "Removing loose duplicates from $CORE"
    run rm -f "$CORE/SKSE/Plugins/GlobalRules.dll" "$CORE/SKSE/Plugins/GlobalRules.json" \
              "$CORE/SKSE/Plugins/GlobalRules.dryrun.json" "$CORE/SKSE/Plugins/GlobalRules.json.bak" \
              "$CORE/GRTest.esp"
    run rm -rf "$CORE/SKSE/Plugins/GlobalRules"
fi

info "Done. In Amethyst: enable 'GlobalRules' mod + 'GRTest.esp' plugin, then Deploy."
