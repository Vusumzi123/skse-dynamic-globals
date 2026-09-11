# GlobalRules — TODO

Open work for the `GlobalRules` SKSE plugin. Keep this in sync with the in-game test
results (see [`test/IN-GAME-CHECKLIST.md`](test/IN-GAME-CHECKLIST.md)).

Legend: `[ ]` open · `[~]` in progress · `[x]` done · `[!]` blocked.

---

## Bugs (open)

### [ ] 2. Perk editorIDs fail `TESForm::LookupByEditorID`  — HIGH

**Symptom.** Both `GRT_P_*` perks are skipped at load, while GLOBs from the *same*
`GRTest.esp` resolve fine:

```
[W] unresolved editorID 'GRT_P_AlwaysTrue' in 'GRTest.esp'
[W] rule #0 unresolved perk 'GRTest.esp|GRT_P_AlwaysTrue'; skipping
[W] unresolved editorID 'GRT_P_InThievesGuild' in 'GRTest.esp'
[W] rule #1 unresolved perk 'GRTest.esp|GRT_P_InThievesGuild'; skipping
```

**What we know.** `GRTest.esp` is structurally valid (correct GRUP labels, EDID, DATA; a
missing `FULL` is fine — vanilla `IrilethVsDragons` has none). Vanilla perks additionally
carry `PRKE`/`PRKF` entries; ours do not.

**Next steps.**
1. Point the gating rules at the perks by **FormID** (`GRTest.esp|0x000813`,
   `0x000814`) and see whether they resolve. This isolates "not loaded" vs
   "not in the editorID map".
2. If FormID works, either add an editorID fallback (scan `TESDataHandler`) or fix the
   ESP.
3. Otherwise, update `tools/GRTest-Builder.pas` to give each perk a minimal
   `PRKE`/`PRKF` entry and re-run xEdit.

---

## Investigated — not a bug

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

- [x] **Rebuild + redeploy** after the code fixes:
  ```bash
  cd ~/Projects/sysop-brain/skse-globals/build/linux-clangcl && ninja
  cp GlobalRules.dll "/mnt/Games/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/SKSE/Plugins/"
  md5sum GlobalRules.dll
  ```
  Deployed md5 `80f1441030b467e96c3c6ff356f9545c`.
- [ ] **Re-run in-game tests** per `test/IN-GAME-CHECKLIST.md` — §8 `cell_change` (wildcard
      `GRT_CellEnter` + targeted `GRT_CellEnterTargeted`), then §11 gating once the perks resolve.
- [x] **Document** the `REX::EnumSet` / `kEnter == 0` pitfall in `PLAN.md` §15.10.
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
