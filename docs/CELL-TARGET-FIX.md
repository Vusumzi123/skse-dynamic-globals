# GlobalRules — Cell Target Resolution (FormID Matching)

> Why an exact-form `cell_change` target is skipped at startup, and why the fix is to match
> by **FormID at event time** rather than caching a form pointer. For the broader internals
> see [`ARCHITECTURE.md`](ARCHITECTURE.md).

Status: **design agreed, implementation pending** (2026-09-10).

---

## 1. Symptom

A wildcard `cell_change` rule fires, but an exact-form one is dropped at load:

```
[W] rule #7 unresolved target 'Skyrim.esm|0x00009732'; skipping
[D] event=cell_change target=Riverwood rule=#6 perk=<none> PASS
[I]   GRT_CellEnter [0x0F000807]: 0 -> 1  (expr "x + entering")
```

`GRT_CellEnter` (wildcard) updates; `GRT_CellEnterTargeted` (exact target `0x00009732`) never
loads, so it can never fire. `0x00009732` is confirmed as the Riverwood `TESObjectCELL` in
`Skyrim.esm`, and no plugin overrides it.

## 2. Root cause — exterior cells are registered lazily

`Skyrim.esm|0x00009732` resolves through `TESDataHandler::LookupForm`:

```
LookupForm(localID, mod) → LookupFormID(localID, mod) → TESForm::LookupByID(fullID)
```

- `LookupFormID` only needs the plugin's load-order index, so it computes `0x00009732`
  correctly at startup.
- `TESForm::LookupByID` reads the **global form map**. Exterior `TESObjectCELL` forms are not
  all present at `kDataLoaded`; they are created/registered when the cell is actually loaded.
  At startup the player is nowhere near Riverwood, so the lookup returns `null`.
- `ParseRule` treats a null target as an invalid rule and skips it (`src/Rules.cpp:90-93`).

At **event time** the cell exists (the player is entering it), which is why the wildcard rule
works and why `LookupByID(cellID)` in the sink succeeds.

Supporting evidence: `TESDataHandler` keeps interior cells in a dedicated `interiorCells`
array while exterior cells live in `TESWorldSpace::cellMap` keyed by grid coordinate;
`Jonahex/SkyrimIngameEditor` must call an engine load routine when `cellMap` misses;
`powerof3/LightPlacer` resolves `cellID` at event time, not at load.

## 3. Why not a perk condition

A perk (`Subject → GetInCell(<cell>) == 1`) can gate a wildcard rule, but:

- exact-form targets are the documented, intended path (`USAGE.md:127`, `PLAN.md` Example 11);
- perk editorID lookup is currently broken (see `PLAN.md` §15.10), so perks are the least
  reliable option;
- it adds ESP authoring for something the config should express directly.

## 4. Decision — match by FormID, not by cached pointer

Resolve the target's **full runtime FormID once at startup** (no object needed) and compare it
to the event target's FormID at event time. This works because:

- the ID is known from the file at startup, even if the object does not exist yet;
- the event supplies a live target form, whose `GetFormID()` is valid at that instant;
- no long-lived pointer to a lazily-created cell is ever held.

## 5. Implementation

| File | Change |
|---|---|
| `src/Rules.h` | Add `RE::FormID targetFormID = 0;` to `Rule` (0 = none). |
| `src/FormId.h/.cpp` | Add `RE::FormID ResolveFormID(std::string_view)`: for `plugin\|0xLOCAL` return `TESDataHandler::GetSingleton()->LookupFormID(local, plugin)`; for editorID specs return the resolved form's `GetFormID()`, else 0. |
| `src/Rules.cpp` | In `ParseRule`, for a non-wildcard target: set `hasTarget`, keep the pointer resolve, set `targetFormID = ResolveFormID(spec)`. Skip only when **both** the pointer and the ID are unavailable. |
| `src/Engine.cpp` | In `TargetMatches`, after the `hasTarget`/null checks, prefer `a_ctx.targetForm->GetFormID() == a_rule.targetFormID`; fall back to the pointer compare. |

`menu` rules keep using `targetName`; editorID targets that resolve at load keep working.

## 6. Pointer vs FormID (memory & lifetime)

Form objects are owned by the game; the plugin neither allocates nor frees them.

| | Cached pointer | FormID |
|---|---|---|
| Size | 8 bytes | 4 bytes |
| Available at startup (cell not loaded) | no (`null`) | yes |
| Can dangle if a transient form is freed | yes | no |
| Relocated by a moving GC | no (Skyrim has none) | no |
| Per-event work | pointer compare | integer compare |

"Many rules pointing at different objects" is not a memory problem: the pointers are 8 bytes
each, own nothing, and keep nothing alive. `Engine::Load()` clears and rebuilds `rules_` on
reload, so no leak. The real hazard for cells is a **null or freed** pointer, which the FormID
approach avoids.

## 7. Performance

- `LookupFormID` runs **once per rule at startup** (linear mod-name scan + arithmetic).
- Per event: read the live form's ID and compare one integer — the same order of cost as the
  current pointer compare. No hash-map lookup per event.
- The target check still runs **before** `CheckPerk`, so it remains the cheap pre-filter that
  short-circuits expensive condition evaluation.

The only slower variant is naive lazy *object* resolution on every event; it is still cheap and
cacheable, but offers no advantage over storing the ID.

## 8. Verification

1. Rebuild + redeploy; startup log shows `loaded 18 rule(s)` with **no** unresolved target for
   rule #7.
2. `bat grtreset` → `coc Riverwood` → `GRT_CellEnter` **1**, `GRT_CellEnterTargeted` **1**.
3. Enter a different cell → `GRT_CellEnter` rises, `GRT_CellEnterTargeted` stays.
4. Leave a cell → `entering=0` logged for both; neither value decreases.

## 9. References

- `build/…/commonlibsse-ng-src/include/RE/T/TESForm.h` — `LookupByID` reads `GetAllForms()`.
- `…/src/RE/T/TESDataHandler.cpp:113-141` — `LookupForm`/`LookupFormID`.
- `…/include/RE/T/TESObjectCELL.h`, `TESWorldSpace.h` — cell runtime state / `cellMap`.
- `powerof3/LightPlacer` (`src/Manager.cpp`) — event-time `cellID` resolution.
- `Jonahex/SkyrimIngameEditor` — load-on-demand exterior cells.
