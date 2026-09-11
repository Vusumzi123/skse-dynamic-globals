# GlobalRules — How It Works

> Technical internals: pipeline, modules, lifecycle, condition/expression/persistence
> mechanics, and the cross-compile build. For the feature-level picture see
> [`OVERVIEW.md`](OVERVIEW.md); for setup and examples see [`USAGE.md`](USAGE.md).

---

## Deployed layout

```
Data/SKSE/Plugins/
  GlobalRules.dll          # the plugin (SKSEPlugin_Load/Query/Version)
  GlobalRules.json         # framework config (auto-generated on first start)
  GlobalRules/             # rule files (one or more JSON arrays)
    *.json
```

Log output: `Documents/My Games/Skyrim Special Edition/SKSE/GlobalRules.log`.

---

## Pipeline

```
kDataLoaded
  │  Config::Load()  →  GlobalRules.json (generated if missing)
  │  LoadRules(dir)  →  parse → resolve forms → validate → compile expressions
  ▼
rules_ (vector) + byEvent_ (name → rule indices)
  │  EventManager::RegisterAll()  (only if enabled)
  ▼
<event fires>
  │  sink → EventContext { subject, targetRef, targetForm, targetName, params }
  ▼
Engine::OnEvent
  ├─ subject is the player?               (filter)
  ├─ target matches (exact / wildcard / menu name)?
  ├─ perk gate: perkConditions.IsTrue(player, target) [± invert]
  ├─ expression: tinyexpr(x, player stats, params)
  └─ write global + record in co-save map  (or log only, if dry-run)
```

---

## Source modules

| File | Responsibility |
|---|---|
| `src/plugin.cpp` | Entry point: `SKSE::Init`, `SKSE::log::init`, messaging listener, register co-save callbacks |
| `src/Config.{h,cpp}` | `GlobalRules.json` load/generate; `PluginDir()` via `GetModuleFileNameW` |
| `src/FormId.{h,cpp}` | Resolve form identifiers (`Plugin\|0xID`, `Plugin\|EditorID`, `EditorID`) |
| `src/Rules.{h,cpp}` | `Rule` model + JSON loader (enumerate dir, parse, resolve, compile) |
| `src/Condition.{h,cpp}` | Perk-condition gate (`TESCondition::IsTrue` + invert) |
| `src/Events.{h,cpp}` | 8 `BSTEventSink` adapters → normalized `EventContext` |
| `src/Expression.{h,cpp}` | `tinyexpr` wrapper: variable binding, custom functions, constant fast-path |
| `src/Persistence.{h,cpp}` | SKSE co-save (save/load/revert callbacks) |
| `src/Engine.{h,cpp}` | Glue: index rules, match, gate, evaluate, write, log |

---

## Lifecycle

| Moment | Action |
|---|---|
| `SKSEPlugin_Load` | `SKSE::Init`, `SKSE::log::init`, register messaging listener + serialization callbacks |
| `kDataLoaded` | `Engine::OnDataLoaded` → load config, apply log level, resolve `debugGlobal`, load rules, index by event, register event sinks |
| event fires | sink builds context → `Engine::OnEvent` |
| `kSaveGame` | `Persistence::Save` writes `{FormID → value}` for touched globals |
| `kPostLoadGame` | `Persistence::Load` re-applies saved values to globals |
| new game / revert | `Persistence::Revert` clears the applied-values map |

---

## Condition gate

For each matching rule:

```cpp
bool passed = rule.perk
    ? rule.perk->perkConditions.IsTrue(RE::PlayerCharacter::GetSingleton(), targetRef)
    : true;                    // null perk == always true
if (rule.invert) passed = !passed;
```

- Uses the perk's **top-level** `TESCondition` (`BGSPerk::perkConditions`), not its
  `perkEntries`.
- `Subject` in the CK maps to the player; `Target` maps to the event target.
- A cell/level-up/menu event has no reference target, so `targetRef` falls back to the
  player. To condition on a *location/cell* from a perk, run the condition on **Subject**
  (`GetInCell`, `GetInCurrentLoc`, `GetInZone`).
- An empty condition list evaluates true.

## Target matching

- `menu` events match by menu **name** (exact); `"*"`/omitted = any menu.
- All other events match by form pointer: `rule.target == ctx.targetForm`; `"*"`/omitted =
  any.

## Expression engine (`tinyexpr`)

Compiled once per rule at load; evaluated per event with live values.

**Variables** (bound to stable storage, updated before each eval):

| Variable | Meaning |
|---|---|
| `x` | current value of the target global |
| `level` | player level |
| `gold` | player gold |
| `health`, `magicka`, `stamina` | player actor values |
| `carryweight` | carry weight actor value |
| `speech` | speech actor value |
| `count` | `container_changed` item delta |
| `stage` | `quest_stage` stage |
| `newLevel` | `level_increase` new level |
| `equipped`, `opening`, `entering` | direction params (1/0) |
| `targetFormID` | numeric FormID of the event target (0 if none) |

**Custom functions:** `min(a,b)`, `max(a,b)`, `clamp(v,lo,hi)`, `round(v)`,
`if(c,a,b)`, `mod(a,b)`.

**Built-ins** (from tinyexpr): `abs`, `sqrt`, `pow`, `exp`, `log`, `log10`, `floor`,
`ceil`, `sin`, `cos`, `tan`, plus constants `pi`, `e`. Note `log` is the **natural**
logarithm (`ln` is an alias).

A value with no operators/variables is detected as a constant and evaluated once.

---

## Persistence

Two layers:

1. **Game save** — writing `TESGlobal::value` at runtime lands in the save's change-form
   data.
2. **SKSE co-save** — a `'GLBL'` record stores `{FormID → last applied value}` for every
   global the framework has touched. On load these values are re-applied, guarding against
   ESP resets or unreliable change-form saving.

Record format (versioned):

```
uint32 version
uint32 count
repeat count: uint32 formID, float value
```

---

## Debug mode

Controlled by `GlobalRules.json` (see [`USAGE.md`](USAGE.md#config-reference)).

- `logChanges` (default `true`) logs each write at `info`:
  `GLOB_LydiaAffinity [0x01000D62]: 3 -> 4  (expr "x + 1")`.
- `debug` adds per-rule PASS/FAIL lines at `debug`.
- `debugGlobal` lets a `TESGlobal` force debug on at runtime (`set <global> to 1`).
- `dryRun` logs the intended change but never writes.
- `debug: true` forces the effective log level to at least `debug`.

---

## Build (cross-compiled from Linux)

CachyOS host, target `x64-windows-msvc`:

```bash
export VCPKG_ROOT=$HOME/Projects/vcpkg
cmake --preset linux-clangcl
cmake --build --preset linux-clangcl
```

- Toolchain: `clang-cl` + `lld-link` + `llvm-rc`, Windows SDK/CRT via `xwin` sysroot.
- Dependencies: CommonLibSSE-NG **v7.5.1** (FetchContent), `nlohmann-json`, `tinyexpr`
  (vcpkg), `spdlog`/`fmt` (via CommonLibSSE).
- Output: `build/linux-clangcl/GlobalRules.dll` (PE32+ x86-64), exports
  `SKSEPlugin_Load/Query/Version`.
- Verify: `file …/GlobalRules.dll` and `llvm-readobj --coff-exports …/GlobalRules.dll`.

Full toolchain details live in the root [`PLAN.md`](../PLAN.md) §13.
