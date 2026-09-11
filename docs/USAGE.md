# GlobalRules — Usage Guide

> How to install, configure, and author rules, with simple worked examples. See
> [`OVERVIEW.md`](OVERVIEW.md) for what it does and [`ARCHITECTURE.md`](ARCHITECTURE.md)
> for internals.

---

## 1. Installation

1. Copy `GlobalRules.dll` into:
   `…/Skyrim Special Edition/Data/SKSE/Plugins/`
2. Launch the game once (or until the main menu). The plugin **auto-creates**
   `Data/SKSE/Plugins/GlobalRules.json` with defaults.
3. Create your rule files in `Data/SKSE/Plugins/GlobalRules/`.

Resulting layout:

```
Data/SKSE/Plugins/
  GlobalRules.dll
  GlobalRules.json          # created automatically
  GlobalRules/
    my-rules.json           # your rules (any *.json filename)
```

> **Requirements:** SKSE64 for your game version, the Address Library, and any perk/global
> forms you reference must already exist (authored in the Creation Kit).

---

## 2. First run — `GlobalRules.json`

Generated automatically the first time. Edit it to taste:

```json
{
  "enabled": true,
  "debug": false,
  "logChanges": true,
  "logLevel": "info",
  "dryRun": false,
  "rulesDirectory": "GlobalRules",
  "debugGlobal": ""
}
```

### Config reference

| Field | Type | Default | Meaning |
|---|---|---|---|
| `enabled` | bool | `true` | Master switch. `false` = load nothing, register nothing. |
| `debug` | bool | `false` | Log every rule's PASS/FAIL (verbose). |
| `logChanges` | bool | `true` | Log every global write (`old -> new`). |
| `logLevel` | string | `"info"` | `trace` / `debug` / `info` / `warn` / `error`. `debug:true` forces at least `debug`. |
| `dryRun` | bool | `false` | Evaluate + log, but **never write** globals. Great for testing. |
| `rulesDirectory` | string | `"GlobalRules"` | Subfolder (under `Plugins/`) to read rules from. |
| `debugGlobal` | string | `""` | Optional global; if non-zero in-game, forces debug on. Toggle with `set <global> to 1`. |

The plugin never overwrites your `GlobalRules.json` — it is only written when missing.

---

## 3. Rule files

Each `*.json` file in the rules directory is a **JSON array of rule objects**. All files
are loaded (sorted by filename), rules are evaluated in order. When several rules match
the same event, **the last one to write a global wins**.

```json
[
  {
    "event": "kill",
    "target": "*",
    "perk": "MyMod.esp|CND_VictimIsNPC",
    "global": "MyMod.esp|GLOB_MurderCount",
    "value": "x + 1"
  }
]
```

### Rule fields

| Field | Required | Type | Meaning |
|---|---|---|---|
| `event` | yes | string | Event name (see §5) |
| `target` | no | string | Form identifier or `*`; **omitted = `*`** (any). Menu events use the menu name. |
| `perk` | no | string | Perk whose conditions are evaluated. **Omitted = always true.** |
| `invert` | no | bool | Fire when the perk conditions **fail** (default `false`) |
| `global` | yes | string | Global to write |
| `value` | yes | string | Constant (`"1"`) or expression (`"x + level * 0.5"`) |

Invalid rules (unresolvable form, wrong type, bad expression) are **logged and skipped**;
the rest of the file still loads.

---

## 4. Form identifiers

Three formats are accepted for `target`, `perk`, and `global`:

| Format | Example | Meaning |
|---|---|---|
| Plugin + local FormID | `MyMod.esp\|0x000ABC` | Most precise; recommended |
| Plugin + EditorID | `MyMod.esp\|CND_IsWeapon` | EditorID scoped to that plugin |
| Bare EditorID | `CND_IsWeapon` | Convenience; may be ambiguous |

> **EditorID resolution is best-effort.** `LookupByEditorID` only works for form types the
> engine natively caches — `GLOB`, `KYWD`, `RACE`, `QUST`, `CELL`, `WRLD`, and a few others.
> Perks, spells, armors, weapons, etc. resolve by EditorID **only if powerofthree's Tweaks is
> installed** (it hooks `SetFormEditorID` to cache the uncached types). GlobalRules logs at
> startup whether po3 Tweaks is present. When it is absent, use the FormID format for those
> types. FormID refs always work and are the recommended, dependency-free choice.

---

## 5. Event reference

| Event | Target | Params available in `value` |
|---|---|---|
| `activate` | activated reference | — |
| `equip` | equipped base object | `equipped` (1 = on, 0 = off) |
| `kill` | victim | — |
| `menu` | menu name | `opening` (1 = open, 0 = close) |
| `container_changed` | base object | `count` (signed change) |
| `quest_stage` | quest | `stage` |
| `cell_change` | cell | `entering` (1 = enter, 0 = leave) |
| `level_increase` | — | `newLevel` |

Notes:

- Only events where the **player** is the subject fire (`menu`, `quest_stage`,
  `level_increase` are inherently player/global).
- `cell_change`'s target is a **cell** (`TESObjectCELL`), not a reference. Match a specific
  cell with `"target": "MyMod.esp|0x..."`, or use a perk condition on **Subject**:
  `GetInCell(<cell>)`, `GetInCurrentLoc(<location>)`, `GetInZone(<keyword>)`.
- `container_changed` fires when an item enters or leaves the player's inventory; `count`
  is positive on pickup, negative on removal.

---

## 6. Expressions

`value` is always a math expression (or a constant). Available variables:

| Variable | Meaning |
|---|---|
| `x` | current value of the target global |
| `level`, `gold` | player level / gold |
| `health`, `magicka`, `stamina` | player actor values |
| `carryweight`, `speech` | player actor values |
| `count`, `stage`, `newLevel` | event params (see §5) |
| `equipped`, `opening`, `entering` | direction params (1/0) |
| `targetFormID` | numeric FormID of the event target (0 if none) |

Functions: `min`, `max`, `clamp(v,lo,hi)`, `round`, `if(c,a,b)`, `mod(a,b)`, plus math
built-ins (`abs`, `sqrt`, `pow`, `exp`, `log`, `floor`, `ceil`, `sin`, `cos`, …) and
constants `pi`, `e`. (`log` is the natural logarithm.)

Examples: `"1"`, `"x + 1"`, `"newLevel * 3"`, `"if(entering, 1, x)"`,
`"clamp(x + count, 0, 100)"`, `"x * 0.9 + level"`.

---

## 7. Worked examples

> These use `MyMod.esp` placeholders — replace with your own plugin/forms. To author the
> perks/globals you need a plugin + the Creation Kit (or xEdit).

### Example A — counter on activate (no perk)

Increment `GLOB_LydiaAffinity` every time you talk to Lydia.

```json
{ "event": "activate", "target": "Skyrim.esm|0x000A2C94",
  "global": "MyMod.esp|GLOB_LydiaAffinity", "value": "x + 1" }
```

No perk = always true; the exact target does the filtering.

### Example B — category filter via perk

Count only NPC kills (not animals/bandits with a different keyword).

```json
{ "event": "kill", "target": "*", "perk": "MyMod.esp|CND_VictimIsNPC",
  "global": "MyMod.esp|GLOB_MurderCount", "value": "x + 1" }
```

**CK perk `CND_VictimIsNPC`:** condition `Target → HasKeyword(ActorTypeNPC) == 1`.

### Example C — expression with player stats

On level-up, set reward points to `newLevel × 3`.

```json
{ "event": "level_increase", "target": "*",
  "global": "MyMod.esp|GLOB_RewardPoints", "value": "newLevel * 3" }
```

### Example D — direction param (count only entries)

Count dungeon entries (not exits).

```json
{ "event": "cell_change", "target": "*", "perk": "MyMod.esp|CND_InDungeon",
  "global": "MyMod.esp|GLOB_DungeonsEntered", "value": "x + entering" }
```

**CK perk `CND_InDungeon`:** condition `Subject → GetInZone(LocTypeDungeon) == 1`.

### Example E — inverted condition (penalty when *not* a member)

Raise "heat" when activating an object while **not** in the Thieves Guild.

```json
{ "event": "activate", "target": "*", "perk": "MyMod.esp|CND_InThievesGuild",
  "invert": true, "global": "MyMod.esp|GLOB_Heat", "value": "x + 1" }
```

### Example F — menu event (barter fee by speech)

```json
{ "event": "menu", "target": "BarterMenu", "perk": "MyMod.esp|CND_Speech50",
  "global": "MyMod.esp|GLOB_HaggleBonus", "value": "speech * 0.02" }
```

### Example G — inventory mirror

Keep a global equal to how many sweetrolls you hold.

```json
{ "event": "container_changed", "target": "Skyrim.esm|0x00064B43",
  "global": "MyMod.esp|GLOB_SweetrollCount", "value": "x + count" }
```

### Example H — last-write-wins (quest chapter)

```json
[
  { "event": "quest_stage", "target": "MyMod.esp|MQ00",
    "global": "MyMod.esp|GLOB_Chapter", "value": "1" },
  { "event": "quest_stage", "target": "MyMod.esp|MQ00", "perk": "MyMod.esp|CND_Stage20",
    "global": "MyMod.esp|GLOB_Chapter", "value": "2" }
]
```

At stage 20 both match; the second (later) rule wins → `2`. Could also be one rule with
`"value": "if(stage >= 20, 2, 1)"`.

---

## 8. Testing & debugging

1. Set `"dryRun": true` and `"logChanges": true` to watch what rules *would* do without
   touching anything.
2. Flip `"debug": true` (or set your `debugGlobal` to 1 in-game) for per-rule PASS/FAIL.
3. Watch the log:
   `Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`.

Expected lines:

```
[info] loaded 3 rule(s) from 1 file(s)
[info] GlobalRules: 3 rule(s) indexed across 2 event type(s)
[debug] event=kill target=Bandit rule=#0 perk=CND_VictimIsNPC PASS
[info]   GLOB_MurderCount [0x01000D62]: 3 -> 4  (expr "x + 1")
```

---

## 9. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Nothing happens | Check `"enabled": true`; confirm forms resolve (look for `unresolved` warnings in the log). |
| Rule skipped on load | Wrong form type (`global` must be a Global, `perk` a Perk) or bad expression. |
| Perk never passes | The condition's `Subject`/`Target` must match how the engine calls it (Subject = player, Target = event target). |
| Cell/location conditions don't work | Run them on **Subject** (cells aren't references, so `Target` falls back to the player). |
| Globals reset on reload | Confirm the co-save is active (no `serialization interface unavailable` error in the log). |
| Non-finite value | Your expression produced NaN/Inf (e.g. divide by zero) — the write is skipped with a warning. |
