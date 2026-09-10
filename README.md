# skse-dynamic-globals (GlobalRules)

An SKSE plugin for Skyrim SE/AE that lets modders **change global variables when game
events happen, gated by perk conditions** — no Papyrus scripts, no recompiled code.
Everything is configured through JSON files.

-------------

Skyrim mods constantly want to react to game state ("every time the player kills a bandit,
add 1 to `GLOB_BanditsKilled`"). Normally that means Papyrus scripts or a bespoke SKSE
plugin. GlobalRules turns the pattern into data: you author a **Perk** (used purely as a
reusable container of Creation Kit conditions) and a **Global**, then write a short JSON
rule that connects them to a game event.

```
on <event> for <target>, if <perk>'s conditions pass, set <global> = <value|expression>
```

The perk does **not** need to be assigned to anyone. The rule's perk conditions run with
**Subject = the player** and **Target = the event target**.

```json
{
  "event": "activate",
  "target": "*",
  "perk": "MyMod.esp|CND_IsFriend",
  "global": "MyMod.esp|GLOB_FriendBonus",
  "value": "x + 1"
}
```

Reads as: *when the player activates anything, and `CND_IsFriend`'s conditions pass, add 1
to `GLOB_FriendBonus`.*

## Events

| Event | Fires when | Direction param |
|---|---|---|
| `activate` | the player activates a reference | — |
| `equip` | the player equips/unequips an item | `equipped` (1/0) |
| `kill` | the player kills an actor | — |
| `menu` | a UI menu opens/closes | `opening` (1/0) |
| `container_changed` | an item enters/leaves the player's inventory | `count` (signed) |
| `quest_stage` | a quest stage changes | `stage` |
| `cell_change` | the player enters/leaves a cell | `entering` (1/0) |
| `level_increase` | the player levels up | `newLevel` |

## Features

- **Config-driven** — rules and settings are JSON, editable at runtime.
- **Perk-as-condition-container** — reuse CK conditions (factions, stats, keywords,
  relationships, AND/OR groups).
- **Negation** — fire when conditions *fail* (`invert`).
- **Wildcard or exact targets**.
- **Math expressions** — `x + level * 0.5`, plus custom `min`/`max`/`clamp`/`round`/`if`/`mod`.
- **Player-scoped variables** — `level`, `gold`, `health`, `magicka`, `stamina`,
  `carryweight`, `speech`, event params, `targetFormID`.
- **Last-write-wins** — matching rules run in file order.
- **Persistence** — globals survive save/load via the game save **and** an SKSE co-save.
- **Debug mode** and **dry-run**.

## How it works

```
kDataLoaded
  → load config (GlobalRules.json, generated if missing)
  → load rule files, resolve forms, compile expressions
  → register the 8 event sinks

<event fires>
  → sink builds an EventContext { subject, target, params }
  → Engine::OnEvent: filter → target match → perk gate → evaluate → write global
```

Rules are compiled once at load (expressions via `tinyexpr`) and evaluated per event with
live values. The condition gate is `perk->perkConditions.IsTrue(player, targetRef)`.

| Source | Responsibility |
|---|---|
| `src/plugin.cpp` | SKSE entry point + messaging/serialization callbacks |
| `src/Config.*` | `GlobalRules.json` load/generate |
| `src/FormId.*` | form identifier resolution (`Plugin\|0xID`, `Plugin\|EditorID`, `EditorID`) |
| `src/Rules.*` | rule model + JSON loader |
| `src/Condition.*` | perk-condition gate (with invert) |
| `src/Events.*` | the 8 `BSTEventSink` adapters |
| `src/Expression.*` | `tinyexpr` wrapper (variables, custom functions) |
| `src/Persistence.*` | SKSE co-save (`'GLBL'` record) |
| `src/Engine.*` | match → gate → evaluate → write |

## Install layout

```
Data/SKSE/Plugins/
  GlobalRules.dll
  GlobalRules.json      # auto-generated on first start
  GlobalRules/          # one or more rule JSON files
    *.json
```

Log: `Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`.

## Build (cross-compiled from Linux)

Target `x64-windows-msvc` with `clang-cl` + `lld-link` + `llvm-rc` (Windows SDK/CRT via
`xwin`), CommonLibSSE-NG **v7.5.1** (FetchContent), `nlohmann-json` and `tinyexpr` (vcpkg).

```bash
export VCPKG_ROOT=$HOME/Projects/vcpkg
cmake --preset linux-clangcl
cmake --build --preset linux-clangcl
```

Output: `build/linux-clangcl/GlobalRules.dll` (PE32+ x86-64).

## Status

Early development — the rule schema and events are still settling.

## License

GPL-3.0 — see [LICENSE](LICENSE).
