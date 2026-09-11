# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Event sinks are now registered only for the events referenced by at least one loaded
  rule, instead of all eight. Unused event types no longer construct an event context
  or dispatch a lookup, matching the documented design.

### Changed

- `Engine::OnEvent` resolves its event bucket through a transparent hash, removing a
  `std::string` heap allocation on every dispatched event (notably `container_changed`).
- Rule indices in logs are now unique across all rule files instead of restarting at 0
  per file.
- The expression variable count is now a single `Expression::kVarCount` constant shared
  by the array size, variable table, and loops.
- `Config::Load` field parsing is factored into `readBool`/`readStr` helpers.

### Fixed

- Removed the `Expression` move constructor and move assignment, which copied the
  `values_` array but left `vars_` and the compiled AST pointing into the moved-from
  object (a latent use-after-free). `Expression` is now non-copyable and non-movable and
  is only ever held via `std::unique_ptr`.

## [1.1.1] - 2026-09-11

### Fixed

- po3 Tweaks detection now runs at `kPostLoad` instead of plugin load. SKSE loads
  plugins in sequence and `GlobalRules` sorts before `po3_Tweaks`, so the old
  check always reported "not detected" even when po3 Tweaks was installed.
- `GetPo3EditorIDFn()` caches po3's `GetFormEditorID` export only on success, so
  an early failed lookup retries instead of latching `nullptr`.
- `tools/GRTest-Builder.pas` builds into a fresh `GRTest.esp` (deleting any stale
  on-disk copy first) so FormIDs stay deterministic (globals `0x800-0x813`,
  perks `0x814-0x815`) and the FormID-gated rules always resolve.

## [1.1.0] - 2026-09-10

### Added

- powerofthree's Tweaks soft-dependency: detected at load and logged; when present,
  editorID references resolve for form types the engine does not natively cache (e.g.
  `BGSPerk`), and perk names appear in logs via po3's `GetFormEditorID`.
- `deploy.sh` to stage the built plugin, test kit, and `GRTest-Builder.pas` into an
  Amethyst Mod Manager mod.

### Fixed

- Perk gating now references the `GRT_P_*` perks by FormID (`GRTest.esp|0x000814`,
  `0x000815`), and `tools/GRTest-Builder.pas` no longer emits a stray empty condition.
  The earlier `LookupByEditorID` failure was an engine limitation (`BGSPerk` is not
  natively cached), not an ESP defect.

## [1.0.0] - 2026-09-10

### Added

- JSON rule engine connecting game events to global-variable writes: eight events
  (`activate`, `equip`, `kill`, `menu`, `container_changed`, `quest_stage`,
  `cell_change`, `level_increase`), wildcard and exact targets, perk-condition gating
  with `invert`, math expressions, player-scoped variables, and last-write-wins.
- Persistence of applied global values via the game save and an SKSE co-save (`'GLBL'`).
- Debug mode, dry-run mode, and a configurable log level.
- Exact-form `cell_change` targets matched by runtime FormID, so lazily-registered
  exterior cells (e.g. `Skyrim.esm|0x00009732`) resolve correctly at load.
- Version string logged at plugin load (`GlobalRules plugin vX.Y.Z loaded`).

### Fixed

- `cell_change` enter/leave direction was always reported as "leave" because the event
  flags value was treated as a bitmask instead of a raw enum.

[Unreleased]: https://github.com/Vusumzi123/skse-dynamic-globals/compare/v1.1.1...HEAD
[1.1.1]: https://github.com/Vusumzi123/skse-dynamic-globals/releases/tag/v1.1.1
[1.1.0]: https://github.com/Vusumzi123/skse-dynamic-globals/releases/tag/v1.1.0
[1.0.0]: https://github.com/Vusumzi123/skse-dynamic-globals/releases/tag/v1.0.0
