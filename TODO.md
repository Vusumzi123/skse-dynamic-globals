# GlobalRules — TODO

Open work for the `GlobalRules` SKSE plugin. Keep this in sync with the in-game test
results (see [`test/IN-GAME-CHECKLIST.md`](test/IN-GAME-CHECKLIST.md)).

Legend: `[ ]` open · `[~]` in progress · `[x]` done · `[!]` blocked.

---

## Bugs (open)

*(none)*

---

## Investigated — not a bug

### [x] 2. Perk editorIDs fail `TESForm::LookupByEditorID`

**Finding.** Not an ESP defect. `LookupByEditorID` reads the engine's `allFormsByEditorID`
map, which is populated only for form types the engine natively caches (GLOB, KEYWORD, RACE,
QUEST, CELL, WSP, …). `BGSPerk` is not one of them, so a perk's EDID is never inserted — a
fully CK-valid perk fails the lookup too. GLOBs resolve because `TESGlobal` overrides
`GetFormEditorID`. FormID refs resolve because `TESDataHandler::LookupForm` uses the
universal `allForms` map.

**Resolution.**
1. The deterministic gating rules now reference the perks by **FormID**
   (`GRTest.esp|0x000814`, `GRTest.esp|0x000815`) — no external dependency.
2. EditorID references still work for perks **only when powerofthree's Tweaks is installed**
   (its `SetFormEditorID` vfunc hook inserts uncached types into the engine map). GlobalRules
   soft-detects po3 Tweaks at load and logs whether editorID refs to uncached types will
   resolve. `test/GlobalRules/05-editorid-po3.json` exercises this path — swap it in for
   `03-gating.json` to verify (deploy one or the other, not both, or `GRT_PerkPass`/`GRT_PerkInvert`
   will double-increment).
3. `tools/GRTest-Builder.pas` was fixed to emit exactly one `CTDA` per condition (it previously
   produced a stray empty condition). Adding a minimal `PRKE`/`PRKF` section is still an optional
   hygiene item — it does **not** affect the lookup.

### [x] 3. `level_increase` doesn't fire on `player.advlevel`

**Finding.** `advlevel` is the console `AdvancePCLevel` command: it bumps the level counter
directly and **bypasses `RE::LevelIncrease::Event`** (no attribute selection, no perk point).
The engine only raises that event on a genuine level-up — skill XP banked, then confirmed in
the Skills menu. The sink was registered and working; `advlevel` simply never calls it.

**Resolution.** Keep the native event only (semantically pure). No code change. Checklist §9
now triggers a real level-up via `player.advskill OneHanded 100000` + confirming each banked
level in `Tab → Skills`.

---

## Tasks (open)

- [ ] **Rebuild + redeploy** after the code fixes:
  ```bash
  cd ~/Projects/sysop-brain/skse-globals/build/linux-clangcl && ninja
  cp GlobalRules.dll "/mnt/Games/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/SKSE/Plugins/"
  md5sum GlobalRules.dll
  ```
  (last deployed md5 `80f1441030b467e96c3c6ff356f9545c`; superseded by this change.)
- [ ] **Regenerate `GRTest.esp`** with the fixed `tools/GRTest-Builder.pas` (deduped `CTDA`),
      then confirm the perk local FormIDs are `0x000814`/`0x000815`.
- [ ] **Re-run in-game tests** per `test/IN-GAME-CHECKLIST.md` — §8 `cell_change`, then §11
      gating (FormID refs), then §11.5 po3 editorID check.
- [ ] **Persistence test** — trigger changes, save, quit to desktop, reload; confirm the
      globals retain values via the co-save.
- [ ] **Dry-run test** — swap `GlobalRules.dryrun.json` in as `GlobalRules.json`, confirm
      no writes, then restore from `GlobalRules.json.bak`.

---

## Done

- [x] **`cell_change` crash fixed** (`EXCEPTION_ACCESS_VIOLATION`, `GlobalRules.dll+0x461AA`).
      `player->GetActorValue()` dispatched through the wrong multi-target vtable offset;
      now uses `player->AsActorValueOwner()->GetActorValue(...)`. Rebuilt + redeployed
      (md5 `842911d19ebdd4f9c06df676dd264dd9`; superseded by the `entering` fix below).
- [x] **`cell_change` `entering` always `0` fixed.** `REX::EnumSet::any()` is a bitmask test,
      but `CellFlag::kEnter == 0`, so it was always false. Switched to
      `flags == CellFlag::kEnter` (`src/Events.cpp:122`); `EnumSet` provides
      `operator==(EnumSet, E)`. Rebuilt + redeployed (md5 `80f1441030b467e96c3c6ff356f9545c`).
      Added a non-wildcard regression rule (`GRT_CellEnterTargeted`, target
      `Skyrim.esm|0x00009732`) beside the wildcard rule; regenerate `GRTest.esp` with
      `tools/GRTest-Builder.pas` to create the new global.
- [x] Deployed `grtreset.txt` to the game root, log symlink in the project, `*GRTest.esp`
      enabled in `Plugins.txt`.
- [x] Wrote `test/IN-GAME-CHECKLIST.md` and expanded `PLAN.md` §15.
- [x] **Perk editorID bug resolved.** Gating rules switched to FormID refs; added po3 Tweaks
      soft-detection + startup log, a po3-only editorID test file, and the `CTDA` dedupe fix
      in `GRTest-Builder.pas`. See the corrected `[x]` note above.
