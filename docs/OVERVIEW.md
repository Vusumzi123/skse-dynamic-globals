# GlobalRules — What It Does

> An SKSE plugin for Skyrim SE/AE that lets modders **change global variables when game
> events happen, gated by perk conditions** — no Papyrus scripts, no recompiled code.
> Everything is configured through JSON files.

---

## The problem it solves

Skyrim mods very often want to track or react to game state: "every time the player kills
a bandit, add 1 to `GLOB_BanditsKilled`", or "when the player enters a dungeon, set
`GLOB_InDungeon` to 1". Normally this means writing Papyrus scripts, or a bespoke SKSE
plugin, or abusing in-game forms.

**GlobalRules** turns that pattern into data. A modder writes a short JSON rule and a
Condition (CTDA) in the Creation Kit — the plugin does the wiring at runtime.

---

## The core idea

The engine sits on top of three pieces you already know how to author:

| Piece | What it is | Where you author it |
|---|---|---|
| **Event** | A game moment (`activate`, `kill`, `menu`, …) | Chosen by name in the JSON rule |
| **Perk** | A **condition container** — a bundle of CTDA conditions | Creation Kit (a normal Perk record) |
| **Global** | A numeric variable you want to set | Creation Kit (a normal Global record) |

The rule connects them:

```
on <event> for <target>, if <perk>'s conditions pass, set <global> = <value|expression>
```

Crucially, the **perk does not need to be assigned to anyone**. It is used purely as a
reusable, CK-authored container of conditions. This is the same technique used by the
[Dynamic Pricing Framework](https://github.com/shazdeh/Dynamic-Pricing-Framework).

### A concrete example

```json
{
  "event": "activate",
  "target": "*",
  "perk": "MyMod.esp|CND_IsFriend",
  "global": "MyMod.esp|GLOB_FriendBonus",
  "value": "x + 1"
}
```

Reads as: *when the player activates any object (`*`), and the perk `CND_IsFriend`'s
conditions pass, increase the global `GLOB_FriendBonus` by 1.* The perk's conditions run
with **Subject = the player** and **Target = the activated object**.

---

## Supported events (v1)

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

---

## Feature summary

- **Config-driven** — rules and settings are JSON files edited at runtime, no rebuild.
- **Perk-as-condition-container** — reuse your existing CK conditions for arbitrary logic
  (factions, stats, keywords, relationships, AND/OR groups).
- **Negation** — a rule can fire when conditions *fail* (`invert`).
- **Wildcard or exact targets** — match one specific form, or any.
- **Math expressions** — values are not just constants; `x + level * 0.5` works.
- **Player-scoped variables** — `level`, `gold`, `health`, `magicka`, `stamina`,
  `carryweight`, `speech` are available in expressions.
- **Last-write-wins** — multiple matching rules all run, in file order; the last one wins.
- **Persistence** — globals survive save/load via the game save **and** a SKSE co-save.
- **Debug mode** — logs every global change (`old -> new`) to the SKSE log.
- **Dry-run** — test rules without writing anything.

---

## What it is *not*

- **Not a Papyrus framework.** No scripts to compile or attach.
- **Not a scripting language.** Expressions are simple math (`tinyexpr`), not a general
  programming language.
- **Does not create perks or globals.** You author those in the Creation Kit; the plugin
  only reads and writes their *values* at runtime.
- **Does not fire arbitrary engine events** — only the 8 listed (extensible in code).

---

## Related documents

- **How it works** (internals): see [`ARCHITECTURE.md`](ARCHITECTURE.md).
- **How to use it** (setup, reference, examples): see [`USAGE.md`](USAGE.md).
- Full design decisions: see the root [`PLAN.md`](../PLAN.md).
