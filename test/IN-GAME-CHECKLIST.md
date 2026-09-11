# GlobalRules — In-Game Test Checklist

Follow this top to bottom while the game is running. Every test is: **reset → do the
thing → `show` the global → check the log**.

- Console key: `~` (tilde).
- Globals are read with `show <name>`, set with `set <name> to <value>`.
- Keep the log open in another terminal:
  ```bash
  tail -f /home/vuszi/Projects/sysop-brain/skse-globals/GlobalRules.log
  ```

> **Current status (2026-09-10):** the `cell_change` crash **and** the `entering` flag bug
> are **fixed** in the deployed DLL (md5 `80f14410…`). The **perk-gated tests (§11) are
> known-broken** — the `GRT_P_*` perks fail `LookupByEditorID`, so those two rules are
> skipped at load. Everything else should pass.

---

## 0. Before you start

- [ ] `GlobalRules.dll` is the fixed build (md5 `80f1441030b467e96c3c6ff356f9545c`):
  ```bash
  md5sum "/mnt/Games/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/SKSE/Plugins/GlobalRules.dll"
  ```
- [ ] Test config + rules are deployed under `Data/SKSE/Plugins/` (`GlobalRules.json`,
      `GlobalRules/01-events.json` … `04-edge-cases.json`).
- [ ] `GRTest.esp` is enabled in `Plugins.txt` (line `*GRTest.esp`).
- [ ] `GRTest.esp` was **regenerated** from `tools/GRTest-Builder.pas` so it contains the new
      `GRT_CellEnterTargeted` global (needed by the §8 non-wildcard test). If it wasn't, that
      rule is skipped at load and you'll see an extra `unresolved editorID` warning.
- [ ] `grtreset.txt` is in the game root (next to `SkyrimSE.exe`) so `bat grtreset` works.

---

## 1. Launch and verify startup

- [ ] Launch the game **through SKSE** (`skse64_loader.exe`).
- [ ] In the log you should see:
  ```
  GlobalRules plugin loaded
  loaded 18 rule(s) from 4 file(s)
  GlobalRules: 18 rule(s) indexed across 8 event type(s)
  GlobalRules: initialized
  ```
  **18** is expected *right now* because the 2 perk rules are skipped. After the perk bug
  is fixed it becomes **20** (only `GRT_MissingGlobal` and the unknown event stay skipped).

- [ ] Also expected (intentional / known):
  ```
  [warn] unresolved editorID 'GRT_P_AlwaysTrue' in 'GRTest.esp'
  [warn] rule #0 unresolved perk 'GRTest.esp|GRT_P_AlwaysTrue'; skipping
  [warn] unresolved editorID 'GRT_P_InThievesGuild' in 'GRTest.esp'
  [warn] rule #1 unresolved perk 'GRTest.esp|GRT_P_InThievesGuild'; skipping
  [warn] unresolved editorID 'GRT_MissingGlobal' in 'GRTest.esp'
  [warn] rule #1 unresolved global 'GRTest.esp|GRT_MissingGlobal'; skipping
  [warn] rule #3 unknown event 'does_not_exist'; skipping
  ```

- [ ] If you see `no rules directory` or `0 rule(s)`, the rules folder isn't deployed — stop
      and fix that first.

---

## 2. Reset everything

- [ ] In the console run:
  ```
  bat grtreset
  ```
  This zeroes all `GRT_*` globals. Re-run it any time you want a clean slate.

---

## 3. `menu` — open/close the inventory

- [ ] `bat grtreset`
- [ ] Open your inventory (`Tab`), then `show GRT_MenuOpen` → expect **1**.
- [ ] Close it, `show GRT_MenuOpen` → expect **0**.
- [ ] Log shows two lines, e.g. `GRT_MenuOpen …: 0 -> 1 (expr "opening")`.

---

## 4. `equip` — equip / unequip a weapon or armor

- [ ] `bat grtreset`
- [ ] Equip a weapon → `show GRT_EquipState` → expect **1**.
- [ ] Unequip it → `show GRT_EquipState` → expect **0**.

---

## 5. `activate` + expressions — the big one

- [ ] `bat grtreset`
- [ ] Activate a door or container (press `E`). Each activation fires all 5 expression rules
      plus the event rule.
- [ ] Check each:

| Global | Expected after one activate | Notes |
|---|---|---|
| `GRT_ActivateCount` | `1` | +1 each activation |
| `GRT_Expr` | `x + level × 0.5` | grows by half your level each time |
| `GRT_Constant` | `42` | always 42 |
| `GRT_Clamp` | `1` | rises `1,2,3…` and stops at **10** |
| `GRT_Stat` | your current gold | `gold` variable |
| `GRT_FormID` | decimal FormID of what you activated | `targetFormID` |

- [ ] Activate 12+ times and confirm `GRT_Clamp` **stops at 10**.
- [ ] Log shows `event=activate target=… rule=#… perk=<none> PASS` and one
      `GRT_… [0x…]: old -> new (expr "…")` line per write.

---

## 6. `container_changed` — pick up / drop an item

- [ ] `bat grtreset`
- [ ] Pick up an item → `show GRT_ItemDelta` → expect a **positive** number (the count).
- [ ] Drop it → `show GRT_ItemDelta` → expect it to go back down (signed count).
- [ ] This event is a wildcard, so the log will be noisy — that's expected.

---

## 7. `kill` — kill any NPC or creature

- [ ] `bat grtreset`
- [ ] Kill something (a bandit, a wolf, whatever).
- [ ] `show GRT_KillCount` → expect **1** (then 2, 3… per kill).

---

## 8. `cell_change` — the one that used to crash

- [ ] `bat grtreset`
- [ ] `show GRT_CellEnter` → note the value (0 after reset).
- [ ] `show GRT_CellEnterTargeted` → note the value (0 after reset).
- [ ] Console: `coc Riverwood` (this exact command crashed before; it must **not** crash now).
- [ ] `show GRT_CellEnter` → expect **1**.
- [ ] `show GRT_CellEnterTargeted` → expect **1** (this rule targets Riverwood `Skyrim.esm|0x00009732`; it must fire here and only here).
- [ ] Walk through a door into another cell → `GRT_CellEnter` increases again on entry, but `GRT_CellEnterTargeted` stays put (that cell isn't the target).
- [ ] Leaving a cell fires `entering=0`, so neither value decreases.
- [ ] Log shows `GRT_CellEnter …: 0 -> 1` and `GRT_CellEnterTargeted …: 0 -> 1` on entering Riverwood; leaving logs `entering=0` for both.
- [ ] If the game crashes here, grab the newest `…/SKSE/crash-*.log` and check whether
      `GlobalRules.dll` appears in the call stack.

---

## 9. `level_increase` — level up

> `player.advlevel` does **not** fire this event — it bumps the level counter directly and
> skips the normal level-up path (no attribute selection, no perk point). The engine only
> raises `LevelIncrease::Event` on a genuine level-up, so use `advskill` + the Skills menu.

- [ ] `bat grtreset`
- [ ] Console: `player.advskill OneHanded 100000` (grants XP and **banks** the level-up(s);
      it does not apply them yet).
- [ ] Close the console, open `Tab` → **Skills**, and confirm the Health/Magicka/Stamina
      choice for **each** pending level (levels are banked — confirm them all).
- [ ] `show GRT_LevelReward` → expect `newLevel × 3`.
- [ ] `show GRT_LastWins` → expect `newLevel` (both rules write it; the last one wins).
- [ ] Log shows `event=level_increase target=… rule=#7 perk=<none> PASS` plus one
      `GRT_LevelReward …: 0 -> <newLevel×3>` line per confirmed level.
- [ ] `player.advlevel` will **not** produce any of the above — that is expected.

---

## 10. `quest_stage` — advance a quest

- [ ] `bat grtreset`
- [ ] Console: `setstage MQ101 10`
- [ ] `show GRT_QuestStage` → expect **10**.

---

## 11. Gating — perk / invert  ⚠️ known-broken

> Skip this section until the perk lookup bug is fixed — both rules are skipped at load, so
> the globals will stay 0. Kept here so you can re-run it after the fix.

- [ ] `bat grtreset`
- [ ] Activate anything → `show GRT_PerkPass` → expect **+1** (perk `GRT_P_AlwaysTrue`).
- [ ] While **not** in the Thieves Guild, activate → `show GRT_PerkInvert` → expect **+1**.
- [ ] Join the guild, then activate again:
  ```
  player.addfac 0x00029DA9 1
  ```
  `GRT_PerkInvert` should now **stop** increasing (invert of "in faction").
- [ ] Leave the guild to restore it:
  ```
  player.removefac 0x00029DA9 1
  ```

---

## 12. Edge cases

- [ ] `bat grtreset`
- [ ] Activate anything. Then:

| Global | Expected | Why |
|---|---|---|
| `GRT_NonFinite` | stays **0** | `1/0` is non-finite → write skipped, **warn** in log |
| `GRT_BadExpr` | may be `0` or increment | `"x + + 1"` currently parses as `x + (+1)`; watch the log |
| `GRT_MissingGlobal` | *(no global exists)* | rule skipped at load with a warning |

- [ ] Confirm the log contains a `warn` about a non-finite result when you activate.

---

## 13. Debug toggle

- [ ] `set GRT_Debug to 1`
- [ ] Activate something → the log now prints a per-rule line for every rule, e.g.
      `event=activate target=… rule=#0 perk=<none> PASS`.
- [ ] `set GRT_Debug to 0` to quiet it again.

---

## 14. Dry-run (optional, separate launch)

- [ ] Close the game.
- [ ] Swap config:
  ```bash
  cd "/mnt/Games/SteamLibrary/steamapps/common/Skyrim Special Edition/Data/SKSE/Plugins"
  cp GlobalRules.dryrun.json GlobalRules.json
  ```
- [ ] Launch, activate something → log shows `[dry-run] GRT_… old -> new`, and `show`
      confirms the globals **do not change**.
- [ ] Restore the real config when done:
  ```bash
  cp GlobalRules.json.bak GlobalRules.json
  ```

---

## Quick reference

| Event | In-game action | Global(s) |
|---|---|---|
| `activate` | `E` on door/container/NPC | `GRT_ActivateCount`, `GRT_Expr`, `GRT_Constant`, `GRT_Clamp`, `GRT_Stat`, `GRT_FormID` |
| `equip` | equip/unequip | `GRT_EquipState` |
| `kill` | kill an actor | `GRT_KillCount` |
| `menu` | open/close inventory | `GRT_MenuOpen` |
| `container_changed` | pick up/drop | `GRT_ItemDelta` |
| `quest_stage` | `setstage MQ101 10` | `GRT_QuestStage` |
| `cell_change` | `coc Riverwood` / door | `GRT_CellEnter`, `GRT_CellEnterTargeted` |
| `level_increase` | `advskill` + confirm in Skills menu | `GRT_LevelReward`, `GRT_LastWins` |
| *(gating)* | activate | `GRT_PerkPass`, `GRT_PerkInvert` |

Console helpers: `bat grtreset`, `show <global>`, `set <global> to <v>`,
`player.advskill <skill> <amount>`, `setstage <quest> <n>`, `coc <cell>`,
`player.addfac/removefac 0x00029DA9 1`, `set GRT_Debug to 1`.

---

## If something doesn't work

- **No rules loaded** → `GRTest.esp` not enabled, or rules not in `Data/SKSE/Plugins/GlobalRules/`.
- **Rules load but nothing fires** → wrong `target`; wildcard is `"*"`.
- **Perk rules skipped** → known bug (see §11), not your setup.
- **`level_increase` never fires** → you used `advlevel`/`setlevel`; those bypass the event.
  Use `advskill` + confirm in the Skills menu (see §9).
- **Values don't persist after reload** → check the co-save / that the game saved after the change.
- **Rule edits don't apply** → rules load once at `kDataLoaded`; **restart the game**.
- **Crash** → read the newest `…/Documents/My Games/Skyrim Special Edition/SKSE/crash-*.log`.
