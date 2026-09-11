# GlobalRules — Test Kit

> A ready-to-drop set of config + rule files that exercise **every** code path of the
> plugin: all 8 events, expressions, perk gating, invert, last-write-wins, and the
> error/edge-case guards. Use it to verify the plugin works in-game.

> **Running the tests in-game?** Follow the step-by-step
> [`IN-GAME-CHECKLIST.md`](IN-GAME-CHECKLIST.md) — it walks through every action,
> console command, and expected value in order.

---

## 1. What this is

```
test/
  GlobalRules.json            # test config (debug on, dryRun off)
  GlobalRules.dryrun.json     # safe config (writes nothing)
  GlobalRules/
    01-events.json            # one rule per event type
    02-expressions.json       # expression variables / functions
    03-gating.json            # perk gate, invert, last-write-wins
    04-edge-cases.json        # invalid rules + non-finite guard
```

The folder mirrors the deployed layout, so you copy it straight into `Data/SKSE/Plugins/`.

---

## 2. Install

1. Copy `GlobalRules.dll` to `Data/SKSE/Plugins/`.
2. Copy the contents of this `test/` folder into `Data/SKSE/Plugins/`:
   ```
   Data/SKSE/Plugins/GlobalRules.json
   Data/SKSE/Plugins/GlobalRules/01-events.json
   Data/SKSE/Plugins/GlobalRules/02-expressions.json
   Data/SKSE/Plugins/GlobalRules/03-gating.json
   Data/SKSE/Plugins/GlobalRules/04-edge-cases.json
   ```
3. Create the test forms below in a plugin named **`GRTest.esp`** (xEdit or CK).
4. **Enable `GRTest.esp`** (see below).
5. Launch, then watch `Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`.

### Enabling the ESP (no mod manager)

Skyrim SE reads the enabled-plugin list from `Plugins.txt` under
`%LOCALAPPDATA%\Skyrim Special Edition\`. Add a line with a leading `*` to enable:

```
*GRTest.esp
```

- `*GRTest.esp` = enabled, `GRTest.esp` = disabled. Order = load order (masters first).
- Under Proton the file is at
  `…/steamapps/compatdata/<appid>/pfx/drive_c/users/steamuser/AppData/Local/Skyrim Special Edition/Plugins.txt`.
- **SKSE DLL plugins need no enabling** — `GlobalRules.dll` loads automatically from
  `Data/SKSE/Plugins/`. Only the `.esp` needs this step.

> Manual edits to `Plugins.txt` are the standard manager-free method. If the game ever
> rewrites it, just re-add the line.

---

## 3. Test forms to create in `GRTest.esp`

> **Automated option:** an xEdit script that builds all of these forms for you lives at
> [`tools/GRTest-Builder.pas`](../tools/GRTest-Builder.pas). Copy it into xEdit's
> `Edit Scripts` folder, then in xEdit: right-click a plugin → **Apply Script…** →
> `GRTest-Builder` → OK, and **save** the new `GRTest.esp`. The table below documents what
> it creates (for manual authoring or verification).

All globals are **Global** records of type **Float**, not constant. Editor IDs must match
exactly (the rules reference them by `GRTest.esp|EditorID`).

| Global | Purpose |
|---|---|
| `GRT_Debug` | Runtime debug toggle (set to 1 in console) |
| `GRT_ActivateCount` | `activate` event |
| `GRT_EquipState` | `equip` event (1/0) |
| `GRT_KillCount` | `kill` event |
| `GRT_MenuOpen` | `menu` event (1/0) |
| `GRT_ItemDelta` | `container_changed` event |
| `GRT_QuestStage` | `quest_stage` event |
| `GRT_CellEnter` | `cell_change` event (wildcard target) |
| `GRT_CellEnterTargeted` | `cell_change` event (non-wildcard target: Riverwood) |
| `GRT_LevelReward` | `level_increase` event |
| `GRT_Expr` | expression test |
| `GRT_Constant` | constant value test |
| `GRT_Clamp` | function test |
| `GRT_Stat` | player-stat variable test |
| `GRT_FormID` | `targetFormID` variable test |
| `GRT_PerkPass` | perk gate (pass) |
| `GRT_PerkInvert` | invert test |
| `GRT_LastWins` | last-write-wins test |
| `GRT_NonFinite` | non-finite guard test |
| `GRT_BadExpr` | bad-expression skip test |

| Perk | Conditions to author |
|---|---|
| `GRT_P_AlwaysTrue` | *(none — empty condition list)* |
| `GRT_P_InThievesGuild` | `Subject → GetInFaction(Thieves Guild) == 1` |

> **Intentionally do NOT create** `GRT_MissingGlobal`. The edge-case file references it to
> prove that unresolvable forms are skipped with a warning.

---

## 4. What each file tests

### `01-events.json` — every event fires

| Trigger in game | Expect |
|---|---|
| Activate any object | `GRT_ActivateCount` +1 |
| Equip/unequip anything | `GRT_EquipState` = 1 / 0 |
| Kill any actor | `GRT_KillCount` +1 |
| Open the inventory menu | `GRT_MenuOpen` = 1 (0 on close) |
| Pick up / drop an item | `GRT_ItemDelta` changes by the signed count |
| Advance any quest stage | `GRT_QuestStage` = the new stage |
| Enter/leave any cell | `GRT_CellEnter` +1 on enter, unchanged on leave |
| Enter/leave **Riverwood** (`Skyrim.esm\|0x00009732`) | `GRT_CellEnterTargeted` +1 on enter, unchanged on leave; other cells leave it at 0 |
| Level up | `GRT_LevelReward` = `newLevel × 3` |

### `02-expressions.json`

| Global | Expression | Expect |
|---|---|---|
| `GRT_Expr` | `x + level * 0.5` | grows by half your level per activation |
| `GRT_Constant` | `42` | always set to 42 |
| `GRT_Clamp` | `clamp(x + 1, 0, 10)` | rises to 10 then stays |
| `GRT_Stat` | `gold` | equals your current gold |
| `GRT_FormID` | `targetFormID` | equals the activated object's FormID (decimal) |

### `03-gating.json`

| Test | Expect |
|---|---|
| Perk gate | `GRT_PerkPass` +1 only if `GRT_P_AlwaysTrue` passes (it always does) |
| Invert | `GRT_PerkInvert` +1 only while **not** in the Thieves Guild |
| Last-write-wins | both rules write `GRT_LastWins`; final value = `newLevel` |

### `04-edge-cases.json` — error handling

| Rule | Expected load/runtime behavior |
|---|---|
| `1/0` → `GRT_NonFinite` | Rule loads; at runtime logs a **warn** and **skips the write** |
| `GRT_MissingGlobal` | Skipped at load: `unresolved global … (skipping)` |
| `x + + 1` → `GRT_BadExpr` | Skipped at load: expression fails to compile |
| `event: "does_not_exist"` | Skipped at load: `unknown event … (skipping)` |

---

## 5. Running a test

1. Reset a global before testing: `set GRT_ActivateCount to 0`.
2. Perform the in-game action.
3. Read the value: `show GRT_ActivateCount`.
4. Check the log for the change line:
   ```
   [info]   GRT_ActivateCount [0x01000XXX]: 0 -> 1  (expr "x + 1")
   ```
5. With `debug: true` you also get per-rule lines:
   ```
   [debug] event=activate target=WhiterunGate rule=#0 perk=<none> PASS
   ```

`GRT_Debug` is wired as `debugGlobal` — set it to `1` in the console to force debug on
without editing the config.

---

## 6. Dry-run mode

To watch what rules *would* do without changing anything, swap in the safe config:

```bash
cp GlobalRules.dryrun.json Data/SKSE/Plugins/GlobalRules.json
```

Log lines become:

```
[info]   [dry-run] GRT_ActivateCount [0x01000XXX]: 0 -> 1  (expr "x + 1")
```

No global is written and nothing is persisted.

---

## 7. Expected log on startup

```
[info] GlobalRules plugin v1.0.0 loaded
[info] loaded 18 rule(s) from 4 file(s)
[warn] unresolved editorID 'GRT_P_AlwaysTrue' in 'GRTest.esp'
[warn] rule #0 unresolved perk 'GRTest.esp|GRT_P_AlwaysTrue'; skipping
[warn] unresolved editorID 'GRT_P_InThievesGuild' in 'GRTest.esp'
[warn] rule #1 unresolved perk 'GRTest.esp|GRT_P_InThievesGuild'; skipping
[warn] unresolved editorID 'GRT_MissingGlobal' in 'GRTest.esp'
[warn] rule #1 unresolved global 'GRTest.esp|GRT_MissingGlobal'; skipping
[warn] rule #3 unknown event 'does_not_exist'; skipping
[info] GlobalRules: 18 rule(s) indexed across 8 event type(s)
```

22 rules exist across the 4 files. **Two** are intentionally skipped (`GRT_MissingGlobal`,
unknown event) → **20** loaded. Right now **18** are loaded because the two `GRT_P_*` perk
rules also fail to resolve (known bug — see [`IN-GAME-CHECKLIST.md`](IN-GAME-CHECKLIST.md) §11).

> Note: `"x + + 1"` (the supposed bad-expression case) currently parses as `x + (+1)` and
> loads fine; the real error path is exercised by `1/0` at runtime instead.

---

## 8. Notes

- `container_changed` and `quest_stage` are **wildcard** here, so they fire often and will
  make the log noisy — that is expected.
- `menu` matches the menu **name** (`InventoryMenu`); other menus are ignored.
- If a rule never fires, confirm its form resolved (no `unresolved` warning at load).
- To point the tests at your own forms, replace `GRTest.esp|<EditorID>` with your own
  identifiers — nothing else needs to change.
