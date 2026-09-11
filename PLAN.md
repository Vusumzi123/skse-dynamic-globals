# GlobalRules — Perk-Gated Event → Global Modifier Framework

> Status: **implemented, in-game testing** — see [`TODO.md`](TODO.md) for open work
> Target: Skyrim Special/Anniversary Edition, runtime **1.7.104**, via SKSE + CommonLibSSE-NG v7.5.1
> Build host: Linux (CachyOS), cross-compiled with `clang-cl` + `lld-link` + `xwin` + vcpkg
> Project folder: `skse-globals/` · Plugin name: `GlobalRules`

---

## 1. Purpose

A config-driven rule engine for Skyrim SE/AE. Modders author JSON rules that, on a
chosen game event, evaluate the **conditions stored inside a perk** (subject = player,
target = event target) and, if they pass, set a **global** to a static value or a math
expression.

Perks act purely as reusable, Creation-Kit-authored **condition containers** — they do
**not** need to be assigned to the player or any actor. This is the same technique used
by [Dynamic Pricing Framework](https://github.com/shazdeh/Dynamic-Pricing-Framework)
(`perk->perkConditions.IsTrue(player, target)`).

The framework is deliberately generic: the same engine can express "when the player
activates a weapon, if `<perk conditions>` pass, add 1 to `<global>`", or "when the
player enters a cell, if `<perk conditions>` pass, set `<global>` to `level * 0.5 + x`".

---

## 2. Locked Design Decisions

| Area | Decision |
|---|---|
| Condition source | `RE::BGSPerk::perkConditions` (a `TESCondition`) evaluated with `TESCondition::IsTrue(player, targetRef)` |
| Condition negation | Per-rule `invert` boolean |
| Form identifiers | `Plugin.esp\|EditorID`, `Plugin.esp\|0xLOCALID`, and bare `EditorID` |
| Target matching | Exact form match, or `*` wildcard; menu events match by menu name |
| Multiple matching rules | Evaluate all in file order; **last write wins** |
| Else branch | **None** — failing conditions do nothing |
| Non-ref event targets | Condition target falls back to the **player**; the real target is exposed to expressions as a variable |
| No-condition rules | `perk` omitted = always true (no throwaway "always true" perks needed) |
| Cell/location conditioning | Cells aren't refs → condition target falls back to player; match the cell via JSON `target`, or condition on `Subject` with `GetInCell` / `GetInCurrentLoc` / `GetInZone` |
| Expression engine | `tinyexpr` (vcpkg port) |
| Expression scope | `x` (current global value) + player stats + event params |
| Persistence | Game savegame (change-form) **and** SKSE co-save |
| Debug mode | `GlobalRules.json` (auto-generated) + optional in-game debug global; `logChanges`/`dryRun` — see §18 |
| Events (v1) | Curated 8-event set; paired events merged with direction params (see §6) |
| Project | `skse-globals/`, plugin `GlobalRules` |

---

## 3. Architecture

```
Data/SKSE/Plugins/GlobalRules/*.json
   │  kDataLoaded: parse → resolve forms → validate → index by event
   ▼
 RuleSet
   ▼
 Event Registry  (registers only the sinks actually used by rules)
   │  Context { subject=Actor*, targetRef=TESObjectREFR*, targetForm=TESForm*, targetName, params }
   ▼
 Rule Engine  ── subject == player
   │          ── target match (exact / wildcard / menu name)
   │          ── gate: perk->perkConditions.IsTrue(player, targetRef)  [± invert]
   ▼
 Expression Evaluator (tinyexpr: x, player stats, event params)
   ▼
 Global Writer (TESGlobal->value = result)
   ▼
 Co-save applied-values map
```

### Components

| Component | Responsibility |
|---|---|
| `FormId` | Parse + resolve load-order-agnostic form identifiers; validate form types |
| `Rules` | Rule model + JSON loader (nlohmann-json) |
| `Events` | Event registry, sink registration, context adapters |
| `Condition` | Perk-condition gate (`TESCondition::IsTrue`) + invert |
| `Expression` | tinyexpr wrapper: variable binding, custom functions, evaluation |
| `Persistence` | Co-save serialization of applied global values |
| `Log` | Debug logging (spdlog via `SKSE::log`) |

---

## 4. Config Schema

Rules live in `Data/SKSE/Plugins/GlobalRules/*.json`. **Each file is a JSON array of
rule objects.** All files are loaded; rules are evaluated in load order (file order,
then array order).

```json
[
  {
    "event": "activate",
    "target": "*",
    "perk": "MyMod.esp|CND_IsWeapon",
    "invert": false,
    "global": "MyMod.esp|GLOB_Activations",
    "value": "x + 1"
  },
  {
    "event": "kill",
    "target": "Skyrim.esm|0x0001A2B3",
    "perk": "MyMod.esp|CND_PlayerLevel20",
    "global": "MyMod.esp|GLOB_KillScore",
    "value": "x + level * 0.5"
  }
]
```

### Rule fields

| Field | Required | Type | Description |
|---|---|---|---|
| `event` | yes | string | Event name from the vocabulary (§6) |
| `target` | no | string | Form identifier or `*`. **Omitted = `*`** (any target). Menu events use the menu name. |
| `perk` | no | string | Form identifier of the `BGSPerk` whose conditions are evaluated. **Omitted = always true.** |
| `invert` | no | bool | If true, the rule fires when the perk conditions **fail** (default `false`) |
| `global` | yes | string | Form identifier of the `TESGlobal` to write |
| `value` | yes | string | Constant (`"1"`) or expression (`"x + level * 0.5"`). Always a numeric expression — globals never hold strings. |

Invalid rules (unresolvable forms, wrong form type, unknown event, parse error) are
logged and skipped; the rest of the file still loads.

> **`value` is always a numeric expression.** A `TESGlobal` stores only a number
> (`float`), regardless of its type (short/long/float). A token like `equipped` is a
> **variable** that evaluates to `1`/`0`, not a string literal. Write
> `"if(equipped, 1, 0)"` when you want the intent to be explicit.

> Rules are separate from the framework config, which lives in
> `Data/SKSE/Plugins/GlobalRules.json` (alongside the DLL, **auto-generated** on first
> start if missing) — see §18.

---

## 5. Form Identifier Resolution

Accepted formats:

| Format | Example | Resolution |
|---|---|---|
| Plugin + local FormID | `MyMod.esp\|0x000ABC` | `TESDataHandler::LookupForm(localID, "MyMod.esp")` |
| Plugin + EditorID | `MyMod.esp\|CND_IsWeapon` | Find loaded plugin, resolve editorID within it |
| Bare EditorID | `CND_IsWeapon` | `TESForm::LookupByEditorID` (may be ambiguous) |

- Resolved **once at load** (`kDataLoaded`), after all plugins are loaded.
- Type validation: `global` → `TESGlobal`, `perk` → `BGSPerk`, `target` → `TESForm`.
- Prefer `Plugin.esp|0xLOCALID` when editorIDs may collide.
- Bare editorID is a convenience; collisions resolve to the first match (log a warning).

---

## 6. Event Registry (v1)

Subject is the **player** for every rule. Events whose actor is not the player are
filtered out. For events whose target is not a `TESObjectREFR`, the condition target
falls back to the player and the real target is exposed via a variable.

Paired events are merged into a single name plus a direction/state parameter.

| event | sink / source | target | params |
|---|---|---|---|
| `activate` | `TESActivateEvent` (ScriptEventSourceHolder) | `objectActivated` | — |
| `equip` | `TESEquipEvent` | `baseObject` | `equipped` (1 = equipped, 0 = unequipped) |
| `kill` | `ActorKill::GetEventSource()` | `victim` | — |
| `menu` | `MenuOpenCloseEvent` (via `UI`) | menu name | `opening` (1 = open, 0 = close) |
| `container_changed` | `TESContainerChangedEvent` | `baseObj` | `count` |
| `quest_stage` | `TESQuestStageEvent` | quest | `stage` |
| `cell_change` | `BGSActorCellEvent` (via `PlayerCharacter::AsBGSActorCellEventSource()`) | cell (`cellID` → `TESObjectCELL`) | `entering` (1 = enter, 0 = leave) |
| `level_increase` | `LevelIncrease::GetEventSource()` | — | `newLevel` |

### Target vs. perk: division of labor

`target` and `perk` answer different questions — do **not** encode the same condition
in both.

| Mechanism | Answers | Use it for |
|---|---|---|
| `target` | *Which form is this event about?* | A cheap, exact pre-filter. Omitted = `*` (any). |
| `perk` (conditions) | *Is this a valid situation?* | Rich, CK-authored logic: factions, stats, keywords, relationships, location, AND/OR groups. |

- **Exact thing, no extra logic** → set `target`, omit `perk`.
- **Category / complex logic** → `target: "*"`, put the logic in the perk.
- **Both** → only when they check *different* things (e.g. `target` = a specific NPC as
  a fast filter, perk = `Subject → GetLevel >= 20`). Never re-check the target in the perk.

### Cells and locations

A cell is a `TESObjectCELL` (a `TESForm`, **not** a `TESObjectREFR`), so the condition
target falls back to the player. Condition on a cell/location two ways:

- **Specific cell** → JSON `target` = the cell form (the adapter resolves `cellID` and
  compares forms). No perk required.
- **Via perk conditions** → run on **`Subject`** (the player), not `Target`:
  `GetInCell(<cell>)`, `GetInCurrentLoc(<location>)`, `GetInZone(<keyword>)`
  (e.g. `LocTypeDungeon`).

These read the player's *current* position, so on `leave` events the player may already
be outside — gate on entry (`value: "x + entering"`, or match on enter).

### Deferred to a later phase (no schema change required)

`open`/`close`, `death`, `combat_start`/`combat_end`, `location_change`, `book_read`,
`spell_cast`, `hit`, `fast_travel`, `item_crafted`, `skill_increase`. Each is added by
registering one more sink adapter — the rule schema and engine are unchanged.

### Registration sources

| Source | Registration |
|---|---|
| `ScriptEventSourceHolder` events (`activate`, `equip`, `container_changed`, `quest_stage`) | `RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<T>(sink)` |
| `menu` | `RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(sink)` |
| `cell_change` | `RE::PlayerCharacter::GetSingleton()->AsBGSActorCellEventSource()->AddEventSink(sink)` |
| `kill` | `RE::ActorKill::GetEventSource()->AddEventSink(sink)` |
| `level_increase` | `RE::LevelIncrease::GetEventSource()->AddEventSink(sink)` |

Only sinks for events referenced by at least one loaded rule are registered.

### Context

Each adapter produces a normalized context:

```cpp
struct Context {
    RE::Actor*     subject;      // always the player (events with other actors filtered)
    RE::TESObjectREFR* targetRef; // condition target; falls back to player
    RE::TESForm*   targetForm;   // for exact/wildcard matching
    std::string_view targetName; // menu name for `menu`
    std::unordered_map<std::string, double> params; // equipped, opening, entering, count, stage, newLevel
};
```

---

## 7. Condition Gate

```cpp
bool passed = rule.perk->perkConditions.IsTrue(
    RE::PlayerCharacter::GetSingleton(),   // actionRef = subject = player
    context.targetRef);                    // target (falls back to player)
if (rule.invert) passed = !passed;
```

- Uses the perk's **top-level** `TESCondition` (`BGSPerk::perkConditions`, offset 0x58).
- `BGSPerk::perkEntries` / `PerkEntryPoint` are **not** used.
- An **empty** condition list evaluates true — convenient for smoke tests.
- Modders author CTDA in the Creation Kit referencing `Subject` (player) and `Target`
  (event target).
- Per-rule pass/fail is logged when debug is enabled.

---

## 8. Expression Engine (tinyexpr)

### Variables

| Variable | Meaning |
|---|---|
| `x` | Current value of the target global |
| `level` | Player level |
| `gold` | Player gold (`Actor::GetGoldAmount()`) |
| `health`, `magicka`, `stamina` | Player actor values |
| `carryweight` | Player carry weight actor value |
| `speech`, `barter` | Player speech / barter actor values |
| `equipped` | `equip` direction (1 = equipped, 0 = unequipped) |
| `opening` | `menu` direction (1 = open, 0 = close) |
| `entering` | `cell_change` direction (1 = enter, 0 = leave) |
| `count` | `container_changed` item count |
| `stage` | `quest_stage` stage |
| `newLevel` | `level_increase` new level |
| `targetFormID` | Numeric FormID of the resolved event target (0 if none) |

### Functions

- Built-ins: `sqrt`, `pow`, `exp`, `log`, `log10`, `abs`, `floor`, `ceil`, `sin`,
  `cos`, `tan`, `min`, `max`.
- Custom (registered): `clamp(v,lo,hi)`, `round(v)`, `if(c,a,b)`, `mod(a,b)`.

### Binding

Variables are bound to stable storage and updated before each evaluation; the compiled
expression is reused across events (no recompile per fire). A constant `value` is
detected and evaluated once.

---

## 9. Persistence

### Savegame
Writing `TESGlobal::value` at runtime lands in the savegame's change-form data (baseline
persistence).

### Co-save (SKSE serialization)
Registered with `SKSE::GetSerializationInterface()` and a framework unique ID (`'GLBL'`).

| Callback | Action |
|---|---|
| `SetSaveCallback` | Serialize `{ FormID → last applied float }` for all framework-touched globals |
| `SetLoadCallback` | Read the map and re-apply values to resolved globals |
| `SetRevertCallback` | Clear the framework-touched set |

Record format (versioned):

```
uint32  version
uint32  count
repeat count:
    uint32  formID
    float   value
```

This guards against ESP resets or unreliable change-form saving.

---

## 10. Lifecycle

| Moment | Action |
|---|---|
| `kDataLoaded` | Parse all JSON, resolve forms, validate, index rules, register needed sinks |
| `kPostLoadGame` (co-save load) | Re-apply persisted values |
| `kSaveGame` (co-save save) | Write applied-values map |
| Optional `GlobalRules reload` console command | Re-parse JSON + re-resolve (later phase) |

---

## 11. Project Layout & Build

```
skse-globals/
  PLAN.md                              # this document
  CMakeLists.txt                       # FetchContent CommonLibSSE-NG v7.5.1 + add_commonlibsse_plugin
  CMakePresets.json                    # linux-clangcl preset (copied from skse-test)
  vcpkg.json                           # nlohmann-json, tinyexpr (+ CommonLib deps)
  cmake/toolchain-linux-clangcl.cmake  # copied
  custom-triplets/x64-windows-clangcl.cmake   # copied
  custom-ports/directxtk/              # copied (Wine/fxc2 shader workaround)
  PCH.h
  src/
    plugin.cpp        # entry point: load, register
    Rules.{h,cpp}     # rule model + JSON loader
    Config.{h,cpp}    # GlobalRules.json (config + debug/log, auto-generated)
    FormId.{h,cpp}    # identifier resolver
    Events.{h,cpp}    # event registry + context adapters
    Condition.{h,cpp} # perk gate
    Expression.{h,cpp}# tinyexpr wrapper
    Persistence.{h,cpp}
    Log.{h,cpp}
```

- **Toolchain**: identical to the proven `skse-test` setup (see §13). Copy
  `cmake/toolchain-linux-clangcl.cmake`, `custom-triplets/`, `custom-ports/directxtk/`,
  and `CMakePresets.json`, then adjust paths.
- **Plugin output**: `GlobalRules.dll` deployed to `Data/SKSE/Plugins/`.
- **Rule files**: `Data/SKSE/Plugins/GlobalRules/*.json`.
- **Config file**: `Data/SKSE/Plugins/GlobalRules.json` (alongside the DLL; auto-generated — see §18).

---

## 12. Implementation Phases

1. **Scaffold** — create project, copy toolchain files, `PCH.h`, minimal `plugin.cpp`;
   add `nlohmann-json` + `tinyexpr` to `vcpkg.json`; verify a clean cross-compile.
2. **FormId resolver** — parse the three identifier formats; resolve + validate types.
3. **Rules** — rule model + JSON loader with error reporting.
4. **Condition gate** — `TESCondition::IsTrue` wrapper + `invert`.
5. **Events** — registry + adapters, starting with `activate`, `kill`, `cell_change`,
   `menu`.
6. **Expression** — tinyexpr wrapper with variables + custom functions.
7. **Rule engine wiring** — global writer + end-to-end path.
8. **Persistence** — co-save serialization.
9. **Deferred events pool** (§6) + debug mode (§18) + optional `reload`/`dump` commands.
10. **In-game test** on AE 1.7.104.

---

## 13. Build / Toolchain Reference (from `skse-test`)

Verified working on this machine:

| Piece | Location / version |
|---|---|
| `clang-cl`, `lld-link`, `llvm-rc` | `/usr/bin` (LLVM 22.1.8) |
| `xwin` | `~/.local/bin/xwin` (0.10.0) |
| MSVC CRT + Windows SDK sysroot | `~/.cache/xwin/out` (MSVC 14.44.17.14, SDK 10.0.26100) |
| `vcpkg` | `~/Projects/vcpkg` (2026-07-27) |
| `llvm-mingw` (for directxtk `fxc2`) | `/opt/llvm-mingw` (`LLVM_MINGW_BIN`) |
| Wine | 11.17 |
| Skyrim install | `/mnt/Games/SteamLibrary/steamapps/common/Skyrim Special Edition` |
| Proton prefix | `/mnt/Games/SteamLibrary/steamapps/compatdata/489830` |

Build commands:

```bash
export VCPKG_ROOT=$HOME/Projects/vcpkg
cd skse-globals
cmake --preset linux-clangcl
cmake --build --preset linux-clangcl
```

Verification:

```bash
file build/linux-clangcl/GlobalRules.dll          # PE32+ ... x86-64
llvm-readobj --coff-exports build/linux-clangcl/GlobalRules.dll   # SKSEPlugin_* exports
```

---

## 14. Risks & Mitigations

| Risk | Mitigation |
|---|---|
| Testing needs an authored perk (Creation Kit, Windows) | Use a vanilla perk with empty top-level conditions for the first smoke test; document the CK authoring convention |
| EditorID collisions across plugins | Prefer `Plugin.esp\|0xLOCALID`; log warnings on bare-editorID resolution |
| Events without a ref target | Fall back to the player as condition target; expose the real target via `targetFormID`/`targetName` |
| Frequent events (`container_changed`) | Index rules by event; cheap pre-filter (target match) before condition evaluation |
| tinyexpr variable binding | Bind to stable storage; reuse compiled expression |
| Perk condition authoring mistakes | Debug logging of every evaluated rule's pass/fail |
| Co-save / change-form divergence | Re-apply co-save values on load; last-write-wins |

---

## 15. Testing Plan

The test kit lives in `test/` and the xEdit form-builder in `tools/`. Full instructions
live in [`test/README.md`](test/README.md); the in-game, follow-along version is
[`test/IN-GAME-CHECKLIST.md`](test/IN-GAME-CHECKLIST.md). This section is the canonical
summary.

### 15.1 Static checks (no game)

```bash
export VCPKG_ROOT=$HOME/Projects/vcpkg
cmake --preset linux-clangcl && cmake --build --preset linux-clangcl
file build/linux-clangcl/GlobalRules.dll                        # PE32+ … x86-64
llvm-readobj --coff-exports build/linux-clangcl/GlobalRules.dll # SKSEPlugin_Load/Query/Version
python3 -m json.tool test/GlobalRules/01-events.json > /dev/null # JSON validity
```

### 15.2 Test kit (`test/`)

| File | Purpose |
|---|---|
| `GlobalRules.json` | Test config: `debug:true`, `logChanges:true`, `dryRun:false`, `debugGlobal:GRTest.esp\|GRT_Debug` |
| `GlobalRules.dryrun.json` | Safe config: writes nothing |
| `GlobalRules/01-events.json` | One rule per event type (8) |
| `GlobalRules/02-expressions.json` | `x`, `level`, `gold`, `targetFormID`, `clamp`, constant |
| `GlobalRules/03-gating.json` | Perk gate, `invert`, last-write-wins |
| `GlobalRules/04-edge-cases.json` | Non-finite guard + 3 intentionally invalid rules |
| `grtreset.txt` | Console batch: `bat grtreset` resets all `GRT_*` globals |
| `README.md` | Forms, actions, expected results |

### 15.3 Test forms — `GRTest.esp`

Built by [`tools/GRTest-Builder.pas`](tools/GRTest-Builder.pas) (xEdit script): **19 float
globals** (`GRT_*`) and **2 perks** used as condition containers:

- `GRT_P_AlwaysTrue` — empty condition list (always passes)
- `GRT_P_InThievesGuild` — `Subject → GetInFaction(Thieves Guild) == 1`

`GRT_MissingGlobal` is intentionally **not** created (tests the unresolved-form skip).

### 15.4 Deployment

| Item | Destination |
|---|---|
| `GlobalRules.dll` | `Data/SKSE/Plugins/` |
| `GlobalRules.json` | `Data/SKSE/Plugins/` (auto-generated if absent) |
| `GlobalRules/*.json` | `Data/SKSE/Plugins/GlobalRules/` |
| `GRTest.esp` | `Data/` |
| `grtreset.txt` | game root (next to `SkyrimSE.exe`) |
| `GRTest-Builder.pas` | xEdit `Edit Scripts/` |

**Enable the ESP (no mod manager):** add `*GRTest.esp` to `Plugins.txt` under
`%LOCALAPPDATA%\Skyrim Special Edition\` (Proton:
`…/compatdata/<appid>/pfx/drive_c/users/steamuser/AppData/Local/Skyrim Special Edition/Plugins.txt`).
`*` = enabled. SKSE DLL plugins need no enabling.

### 15.5 In-game test matrix

Run `bat grtreset`, then perform each action and read the global with `show <global>`:

| Test | Action | Expected |
|---|---|---|
| `activate` + expressions | Activate any door/container/NPC | `GRT_ActivateCount` +1; `GRT_Expr` +level×0.5; `GRT_Constant`=42; `GRT_Clamp`≤10; `GRT_Stat`=gold; `GRT_FormID`=target FormID |
| `equip` | Equip then unequip | `GRT_EquipState` = 1 → 0 |
| `kill` | Kill any actor | `GRT_KillCount` +1 |
| `menu` | Open/close inventory | `GRT_MenuOpen` = 1 → 0 |
| `container_changed` | Pick up / drop an item | `GRT_ItemDelta` changes by ±count |
| `quest_stage` | `setstage MQ101 10` | `GRT_QuestStage` = 10 |
| `cell_change` | Walk through a door, or `coc Riverwood` | `GRT_CellEnter` +1 on entry, unchanged on leave |
| `cell_change` (targeted) | `coc Riverwood` (`Skyrim.esm\|0x00009732`) | `GRT_CellEnterTargeted` +1 only in Riverwood; other cells leave it at 0 |
| `level_increase` | `player.advskill OneHanded 100000`, then confirm each banked level in `Tab → Skills` | `GRT_LevelReward` = newLevel×3; `GRT_LastWins` = newLevel (`player.advlevel` does **not** fire this event) |
| perk gate | Activate an object | `GRT_PerkPass` +1 |
| `invert` | Activate while not in Thieves Guild; then `player.addfac …` and repeat | `GRT_PerkInvert` +1, then stops |
| non-finite guard | Activate an object | `GRT_NonFinite` unchanged; `warn` in log |

Useful console commands: `show <global>`, `set <global> to <v>`,
`player.advskill <skill> <n>` (then confirm levels in `Tab → Skills`), `setstage <quest> <n>`,
`coc <cell>`, `player.addfac/removefac <faction> 1`, `set GRT_Debug to 1` (runtime debug toggle).

### 15.6 Dry-run test

Swap in `GlobalRules.dryrun.json` as `GlobalRules.json`. Actions then log
`[dry-run] GLOB … old -> new` while globals stay unchanged and nothing is persisted.

### 15.7 Persistence test

Trigger some changes, save, quit to desktop, reload. The globals must retain their values
via the co-save (and/or the game save's change-form data).

### 15.8 Load / error tests

At startup the log should show `18 rule(s) indexed across 8 event type(s)` and warnings for
the two unresolved `GRT_P_*` perks, the unresolved `GRT_MissingGlobal`, and the unknown event
(see checklist §1). Once the perk lookup is fixed it becomes `20`, with only the latter two
warnings.

### 15.9 Log location

Standard SKSE log (`Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`).
Under Proton:
`…/compatdata/<appid>/pfx/drive_c/users/steamuser/Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`
(a `GlobalRules.log` symlink in the project root points at it for easy `tail -f`).

### 15.10 Known failure modes

- **`REX::EnumSet::any()` is a bitmask test (fixed 2026-09-10).** `BGSActorCellEvent::flags`
  holds a **raw** `CellFlag` value (0 = enter, 1 = leave), not a bitmask. `flags.any(kEnter)`
  masks against `kEnter == 0`, so it is always `false` and `cell_change` reported
  `entering = 0` even on entry. Compare with `flags == CellFlag::kEnter` instead — `EnumSet`
  defines `operator==(EnumSet, E)`, which compares `underlying()` to the raw value.
- **Multi-runtime layout hazard (fixed 2026-09-10).** CommonLibSSE-NG defaults
  `ENABLE_SKYRIM_SE/AE/VR` all **ON**, which compiles against a "conflict-free" class
  layout where `ActorValueOwner` sits at a placeholder offset (`0x90`). The AE runtime has
  it at `0xB8`, so a direct `player->GetActorValue(...)` dispatched through the wrong
  vtable and crashed (`EXCEPTION_ACCESS_VIOLATION`, `call [rax+0x08]`, `rax=0`). NG's own
  header warns: *"In multi-runtime builds, base classes can't be in hierarchy due to offset
  conflicts."* **Always access runtime-offset bases through their accessor**, e.g.
  `player->AsActorValueOwner()->GetActorValue(...)`. `GetLevel()`/`GetGoldAmount()` are
  safe because NG implements them as relocated game-function calls. Perks still fail
  editorID lookup — see below.
- **Perk editorID lookup.** `GRT_P_AlwaysTrue` / `GRT_P_InThievesGuild` resolve by FormID
  but not via `TESForm::LookupByEditorID`, while GLOBs from the same plugin resolve fine.
  The ESP is structurally valid (correct GRUP, EDID, DATA); vanilla perks additionally
  carry `PRKE`/`PRKF` entries. Under investigation.
- **Exact `cell_change` targets are skipped at load.** Exterior `TESObjectCELL` forms aren't
  in the global form map at `kDataLoaded`, so `LookupForm`/`LookupByID` return null and the
  rule is dropped. Fix: match by **FormID** at event time, not a cached pointer — see
  [`docs/CELL-TARGET-FIX.md`](docs/CELL-TARGET-FIX.md).

---

## 16. Reference

- Perk-as-condition-container: `RE::BGSPerk::perkConditions` +
  `RE::TESCondition::IsTrue(TESObjectREFR*, TESObjectREFR*)`
- Global write: `RE::TESGlobal::value` (`float`)
- Player stats: `RE::Actor::GetLevel()`, `GetGoldAmount()`, actor values via
  `ActorValueOwner`
- Load-order-agnostic forms: `RE::TESForm::LookupByEditorID`,
  `RE::TESDataHandler::LookupForm`
- Co-save: `SKSE::GetSerializationInterface()`
- Inspiration: [Dynamic Pricing Framework](https://github.com/shazdeh/Dynamic-Pricing-Framework)

---

## 17. Worked Examples

### How a rule executes (end-to-end)

Goal: Lydia likes you more each time you talk to her. There are two equivalent ways —
pick **one**, don't encode the same condition in both `target` and `perk`.

**Variant A — exact target, no perk (fast, no CK work):**
```json
{ "event": "activate", "target": "Skyrim.esm|0x000A2C94",
  "global": "MyMod.esp|GLOB_LydiaAffinity", "value": "x + 1" }
```

**Variant B — wildcard target, perk narrows:**
```json
{ "event": "activate", "target": "*", "perk": "MyMod.esp|CND_TargetIsLydia",
  "global": "MyMod.esp|GLOB_LydiaAffinity", "value": "x + 1" }
```
**CK perk:** `Target → GetIsID(Lydia) == 1`.

Execution:

1. Engine loads at `kDataLoaded`, resolves forms, indexes rules by event.
2. Player activates Lydia → `TESActivateEvent`.
3. Context built: `subject = player`, `targetRef`/`targetForm` = Lydia.
4. Rule matches the event (and the target, for variant A).
5. Gate (variant B only): `CND_TargetIsLydia->perkConditions.IsTrue(player, Lydia)`.
6. Pass → `GLOB_LydiaAffinity.value = x + 1`, where `x` is the current value.
7. New value recorded in the co-save so it survives save/load.

### Example 1 — Murder counter (only innocent NPCs)

`kill` fires for every kill, so let the perk narrow it.
```json
{ "event": "kill", "target": "*", "perk": "MyMod.esp|CND_VictimIsNPC",
  "global": "MyMod.esp|GLOB_MurderCount", "value": "x + 1" }
```
**CK perk:** `Target → HasKeyword(ActorTypeNPC) == 1`.
**Result:** +1 per NPC killed; bandits/animals ignored.

### Example 2 — Relationship gain on interaction

```json
{ "event": "activate", "target": "*", "perk": "MyMod.esp|CND_IsFriend",
  "global": "MyMod.esp|GLOB_FriendBonus", "value": "x + 1" }
```
**CK perk:** `Target → GetRelationshipRank(Subject) >= 2` (Friend or better).
**Result:** activating any NPC who already considers you a friend grants a bonus.

### Example 3 — Barter fee scaled by Speech (menu + expression)

```json
{ "event": "menu", "target": "BarterMenu", "perk": "MyMod.esp|CND_Speech50",
  "global": "MyMod.esp|GLOB_HaggleBonus", "value": "speech * 0.02" }
```
**CK perk:** `Subject → GetActorValue(Speech) >= 50`.
**Result:** when the barter menu opens, if Speech ≥ 50, `GLOB_HaggleBonus = speech × 0.02`.

### Example 4 — Count dungeon entries only (direction param)

```json
{ "event": "cell_change", "target": "*", "perk": "MyMod.esp|CND_InDungeon",
  "global": "MyMod.esp|GLOB_DungeonsEntered", "value": "x + entering" }
```
**CK perk:** `Subject → GetInZone(LocTypeDungeon) == 1`.
**Result:** `entering` is 1 on enter, 0 on leave → the counter only rises on entry.

### Example 5 — Level-up reward (overwrite, not increment)

```json
{ "event": "level_increase", "target": "*",
  "global": "MyMod.esp|GLOB_RewardPoints", "value": "newLevel * 3" }
```
**Result:** each level-up sets reward points to `newLevel × 3` (replaces the old value).
No perk needed — omitted `perk` = always true.

### Example 6 — Inverted condition (penalty when *not* a member)

```json
{ "event": "activate", "target": "*", "perk": "MyMod.esp|CND_InThievesGuild",
  "invert": true, "global": "MyMod.esp|GLOB_Heat", "value": "x + 1" }
```
**CK perk:** `Subject → GetInFaction(ThievesGuildFaction) == 1`.
**Result:** `invert: true` flips the result, so the rule fires when you are **not** in
the guild → heat rises.

### Example 7 — Equipment state mirror (direction param)

```json
{ "event": "equip", "target": "MyMod.esp|0x000D62",
  "global": "MyMod.esp|GLOB_SetBonusActive", "value": "if(equipped, 1, 0)" }
```
**Result:** `equipped` is 1 when the item is put on and 0 when taken off — the global
tracks a boolean state. `value` is a numeric expression; nothing string-like is stored.

### Example 8 — Inventory mirror (container_changed delta)

```json
{ "event": "container_changed", "target": "Skyrim.esm|0x00064B43",
  "global": "MyMod.esp|GLOB_SweetrollCount", "value": "x + count" }
```
**Result:** `count` is the signed change (+ on pickup, − on removal), so `x + count`
keeps the global equal to your current sweetroll total.

### Example 9 — Two rules, last-wins (quest chapter)

```json
[
  { "event": "quest_stage", "target": "MyMod.esp|MQ00",
    "global": "MyMod.esp|GLOB_Chapter", "value": "1" },
  { "event": "quest_stage", "target": "MyMod.esp|MQ00", "perk": "MyMod.esp|CND_Stage20",
    "global": "MyMod.esp|GLOB_Chapter", "value": "2" }
]
```
**Result:** at stage 20 both match; the later rule wins → `GLOB_Chapter = 2`. Can also be
collapsed to one rule: `"value": "if(stage >= 20, 2, 1)"`.

### Example 10 — Compound expression with decay

```json
{ "event": "level_increase", "target": "*",
  "global": "MyMod.esp|GLOB_Focus", "value": "x * 0.9 + level" }
```
**Result:** on each level-up, focus decays 10% then adds the new level — shows `x`,
player stats, and event params combined.

### Example 11 — Specific cell via JSON target (no perk)

```json
{ "event": "cell_change", "target": "MyMod.esp|0x0001A2B3",
  "global": "MyMod.esp|GLOB_VisitedBreezehome", "value": "1" }
```
**Result:** fires when the event's cell is that form. To act only on entry, use
`"value": "if(entering, 1, x)"` or add a perk condition `Subject → GetInCell(<cell>) == 1`.

### Mechanics worth remembering

- **Wildcard `*`** = any target; pair with a perk to narrow. **Exact form** = fast pre-filter.
- **`invert`** = fire on condition failure.
- **Multiple matches** = all evaluated in file order, **last write wins**.
- **`perk` omitted** = always true; **`target` omitted** = `*`.
- **Non-ref targets** (`menu`, `cell_change`, `level_increase`) → condition target falls
  back to the player; the real target is still available (`targetName`, params, `targetFormID`).
- **`value` is always a numeric expression** — globals never hold strings.

---

## 18. Debug Mode

Debug mode makes every global change visible in the SKSE log, so rules can be verified
in-game without a debugger.

### Config file

`Data/SKSE/Plugins/GlobalRules.json` (alongside the DLL; **auto-generated** on first
start if missing, never overwritten):

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

| Field | Type | Default | Meaning |
|---|---|---|---|
| `enabled` | bool | `true` | Master kill-switch — load nothing / register no sinks when `false` |
| `debug` | bool | `false` | Verbose: log every rule's PASS/FAIL (rule-eval lines) |
| `logChanges` | bool | `true` | Log the global-change line independent of `debug` |
| `logLevel` | string | `"info"` | `trace` \| `debug` \| `info` \| `warn` \| `error`; `debug: true` forces at least `debug` |
| `dryRun` | bool | `false` | Evaluate + log intended changes but do not write or persist |
| `rulesDirectory` | string | `"GlobalRules"` | Subdir under `Plugins/` to read rule files from |
| `debugGlobal` | string | `""` | Optional `TESGlobal`; if non-zero at runtime, debug is forced on (toggle in-game: `set <global> to 1`) |

### Log destination

Standard SKSE log via `SKSE::log` (spdlog):
`Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`.

### What gets logged

| Level | When | Example |
|---|---|---|
| `info` | Load summary | `loaded 3 rule files, 12 rules (activate=4, kill=3, menu=2, ...)` |
| `info`/`warn`/`error` | Form resolution | `resolved MyMod.esp\|GLOB_LydiaAffinity -> 0x01000D62` / `unresolved form ... (skipped)` |
| `debug` | Rule evaluated (when `debug`) | `event=activate target="Lydia" (0x000A2C94) rule=#3 perk=CND_TargetIsLydia PASS` |
| `info` | **Global change** (when `logChanges`) | `GLOB_LydiaAffinity [0x01000D62]: 3 -> 4  (expr "x + 1")` |
| `info` | Dry-run intended change | `[dry-run] GLOB_LydiaAffinity [0x01000D62]: 3 -> 4  (expr "x + 1")` |
| `warn` | Non-finite result | `GLOB_X [0x...] expression produced inf/nan — write skipped` |

The **global-change** line is the key one: it prints the global's name + FormID, the old
value, the new value, and the expression that produced it.

### Behavior

- When debug is off and `logChanges` is off, only `info`-and-above messages are emitted
  (load summary, errors).
- `debugGlobal`, when set and non-zero, overrides `debug` at runtime so it can be toggled
  in-game without editing files.
- `dryRun` logs the intended change but leaves the global untouched (safe rule testing).
- Non-finite results (`NaN`/`Inf`) are never written; they are logged as warnings and the
  global is left unchanged.
- (Later phase) a `GlobalRules dump` console command prints every framework-touched global
  and its current value.
