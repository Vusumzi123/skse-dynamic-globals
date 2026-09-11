# GlobalRules — Code Review Pass: Issues, Leaks & Optimizations

**Scope:** fix latent bugs, remove a memory-corruption hazard, cut avoidable hot-path
allocations, and shrink the codebase while preserving every existing behavior. Six work
items (A–F), each self-contained.

## Summary table

| # | Category | What | Impact | Where | Fix |
|---|----------|------|--------|-------|-----|
| A | Correctness (latent) | `Expression` move ctor/assignment leave `vars_` and the compiled `expr_` pointing into the *moved-from* object's `values_` array | Use-after-free if a move ever fires; currently dead code | `Expression.h:21-22`, `Expression.cpp:62-85` | Delete the move operations |
| B | Optimization | `Engine::OnEvent` builds a `std::string` on every dispatched event just to look up `byEvent_` | Per-event heap alloc on hot events (`container_changed` exceeds the 15-char SSO buffer) | `Engine.cpp:116` | Transparent hash → `find(std::string_view)` with no allocation |
| C | Correctness (logs) | `rule.index` restarts at 0 for each JSON file | `rule=#3` is ambiguous across files | `Rules.cpp:139-144` | Single running counter across all files |
| D | Safety | Variable count `15` hardcoded in two places | A future edit to one side silently desyncs the `values_` array size → memory corruption | `Expression.h:47`, `Expression.cpp:10` | One `kVarCount` constant in the header |
| E | Readability / size | `Config::Load` repeats the same `contains`+`is_*`+`get` pattern seven times | ~25 redundant lines | `Config.cpp:85-105` | Two tiny helper lambdas |
| F | Correctness / perf | All 8 sinks registered regardless of rules, contradicting the documented "only used sinks" design | Unused sinks still build a context + dispatch on hot events (`activate`, `equip`, `container_changed`) | `Events.cpp:177-195`, `Engine.cpp:80-85` | Register only sinks for events referenced by ≥1 rule |

No actual memory *leaks* were found: `te_free(expr_)` is balanced in the destructor,
move-assign, and re-`Compile`, and the `Persistence::AppliedValues()` static map is
intentionally process-lifetime. Item A is the closest thing to a memory hazard.

Each item below cites the best-practice source that confirms its fix (C++ Core Guidelines,
cppreference, and CommonLibSSE-NG API docs), gathered during an external review pass.

---

## A. `Expression` move operations — dangling pointers (latent use-after-free)

### What it is

`Expression` owns a tinyexpr AST (`te_expr* expr_`) and a bound-variable array
(`double values_[15]`). At compile time, `te_compile` copies the *addresses* of
`values_[i]` into the compiled tree, and `vars_` holds the `te_variable` descriptors whose
`address` fields point into the same `values_` array.

The move-assignment operator (`Expression.cpp:67-85`) does:

```cpp
std::copy(std::begin(a_rhs.values_), std::end(a_rhs.values_), values_);  // copies VALUES
vars_ = std::move(a_rhs.vars_);                                         // MOVES descriptors
```

It copies the *numbers* out of the source's `values_`, but `vars_` — and, critically, the
already-compiled `expr_` that was moved over — still contain raw pointers **into
`a_rhs.values_`**. The copied numbers land at *different* addresses (`this->values_`), so
after the move the compiled expression reads through pointers that no longer point at the
copy.

### Why it matters

If the moved-from `Expression` is destroyed, those pointers dangle. Evaluating the moved-to
object then reads freed stack/heap memory → undefined behavior (garbage values, or a
crash). It is **latent** because nothing in the current codebase ever moves an `Expression`:

- `Rule` holds `std::unique_ptr<Expression> expr;`
- moving a `Rule` (e.g. `rules.push_back(std::move(*rule))`) moves the *pointer*, never the
  pointee
- `Expression` is never stored by value or returned by value

So the buggy path is dead code today, but it is an incorrect public API that will bite the
first person who stores an `Expression` by value.

### How to fix

Delete the move constructor and move assignment entirely. `Expression` is already
non-copyable (copy ops are `= delete`), and nothing needs it to be movable.

- `Expression.h` — remove:
  ```cpp
  Expression(Expression&&) noexcept;
  Expression& operator=(Expression&&) noexcept;
  ```
- `Expression.cpp` — remove the two definitions (lines 62–85).

> **Best-practice note (C++ Core Guidelines).** A type whose compiled AST holds pointers
> into its own member storage is an *identity/owner* type — the implicit member-wise
> copy/move is a shallow copy that leaves those pointers dangling, so the Rule of Zero does
> not apply ([C.20](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-zero)).
> The two legitimate options are (a) implement correct copy/move that *rebases* the
> pointers, or (b) `=delete` them. Because deep copy/move has no meaningful semantics here,
> deleting is the simpler, safer choice — [C.21](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c21-if-you-define-or-delete-any-copy-move-or-destructor-function-define-or-delete-them-all)
> requires defining-or-deleting the whole set together, and [C.81](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rc-delete)
> endorses `=delete`. (Copy is already `=delete` and a destructor is defined, so deleting
> the moves completes the set.)

### Result

`Expression` becomes move-only-in-practice via `unique_ptr`; the class itself is
non-copyable and non-movable. No behavioral change, −25 lines, hazard gone.

---

## B. Per-event `std::string` allocation in `Engine::OnEvent`

### What it is

`byEvent_` is `std::unordered_map<std::string, std::vector<std::size_t>>`. The event
dispatcher hands `OnEvent` a `std::string_view` (`a_name`). The lookup is:

```cpp
auto it = byEvent_.find(std::string(a_name));   // Engine.cpp:116
```

Constructing that `std::string` happens on **every dispatched event**, even ones with zero
rules (where the `find` misses and the function returns immediately).

### Why it matters

`std::string` has a small-string optimization (15 chars on the MSVC STL used by the
`clang-cl` cross-compile). Most event names fit in SSO and cost nothing, but
`"container_changed"` is 17 characters — it overflows SSO and triggers a **heap allocation
on every call**. `container_changed` is one of the most frequently fired events (every item
pickup/drop by the player). The event names are all compile-time literals, so the
allocation is pure waste.

### How to fix

Give `byEvent_` a transparent hasher/equality so `find` accepts a `std::string_view` without
materializing a `std::string`. Keys stay `std::string` (safe lifetime), lookups become
allocation-free.

- `Engine.h` — add above the class:
  ```cpp
  struct StringHash {
      using is_transparent = void;
      std::size_t operator()(std::string_view a_s) const noexcept {
          return std::hash<std::string_view>{}(a_s);
      }
  };
  ```
  and change the member to:
  ```cpp
  std::unordered_map<std::string, std::vector<std::size_t>, StringHash, std::equal_to<>> byEvent_;
  ```
  (`std::string` implicitly converts to `std::string_view`, so one overload serves both the
  stored keys and the lookup.)
- `Engine.cpp:116` — change to:
  ```cpp
  auto it = byEvent_.find(a_name);
  ```

> **C++ version note.** Heterogeneous `unordered_map` lookup is a **C++20** feature
> (`__cpp_lib_generic_unordered_lookup`), not C++17 — in C++17 `std::unordered_map` has no
> `template<class K>` `find` overload. This project is C++23, so it's available
> ([cppreference `unordered_map::find`](https://en.cppreference.com/w/cpp/container/unordered_map/find),
> [std::equal_to](https://en.cppreference.com/w/cpp/utility/functional/equal_to)). The same
> transparent lookup also benefits `contains`/`count`/`equal_range`. This is a general
> C++20/23 idiom, not a CommonLibSSE-specific one.

### Result

No allocation on the hot path; identical lookup semantics. `operator[]` (used in `Load()`)
continues to work unchanged.

---

## C. `rule.index` restarts per file

### What it is

`LoadRules` iterates rule files in sorted order. For each file it declares a fresh counter:

```cpp
std::size_t idx = 0;
for (const auto& entry : root) {
    auto rule = ParseRule(entry, idx++);
    ...
}
```

`rule.index` is only used for logging (e.g. `rule=#3`), so the reset means the *third rule
in each file* is logged as `#3`.

### Why it matters

Log ambiguity. With multiple rule files, `rule=#3` can't be tied back to a specific rule. It
also makes debug output harder to correlate with the load summary.

### How to fix

Hoist a single counter out of the file loop and share it across all files:

```cpp
std::size_t index = 0;                       // before the file loop
...
for (const auto& entry : root) {
    auto rule = ParseRule(entry, index++);   // inside, unchanged per-rule
    ...
}
```

### Result

Globally unique rule indices in all logs. No runtime behavior change.

---

## D. Hardcoded variable count `15` in two places

### What it is

The expression variable array size is written twice, independently:

- `Expression.h:47` — `double values_[15]{};`
- `Expression.cpp:10` — `constexpr std::size_t kVarCount = 15;` (with `kVarNames[kVarCount]`)

`BuildVariables` loops `i < kVarCount` and pushes `&values_[i]` for every name in
`kVarNames`. The two `15`s must agree.

### Why it matters

If someone adds a variable name to `kVarNames` (say, a new player stat) but forgets to bump
`values_[15]`, the loop writes past the end of the array — out-of-bounds stack write →
corruption/crash. Keeping the count in one place makes that class of bug impossible.

### How to fix

- `Expression.h` — add inside the class:
  ```cpp
  static constexpr std::size_t kVarCount = 15;
  ```
  and change the member to `double values_[kVarCount]{};`
- `Expression.cpp` — delete the local `constexpr std::size_t kVarCount = 15;` and reference
  `Expression::kVarCount` where the local was used (`kVarNames[kVarCount]`, the
  `BuildVariables` loop, and `vars_.reserve(kVarCount + 6)`).

> **Best-practice note.** This follows [ES.45](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Res-magic)
> ("avoid magic constants") and [Con.1](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rconst-immutable):
> name the value once as `constexpr` and reuse it, giving compile-time evaluation with no
> duplicated literal.

### Result

Single source of truth; the compiler enforces agreement between the names array, the value
array, and the loops.

---

## E. `Config::Load` verbose field parsing

### What it is

Each of the seven config fields follows the same three-step pattern:

```cpp
if (root.contains("enabled") && root["enabled"].is_boolean()) {
    config.enabled = root["enabled"].get<bool>();
}
if (root.contains("debug") && root["debug"].is_boolean()) {
    config.debug = root["debug"].get<bool>();
}
// ... 5 more
```

### Why it matters

~25 lines of identical logic. The `contains` + `is_*` guards are important (they avoid
nlohmann's type-mismatch exception), so we can't just swap in `root.value(...)`, but we *can*
factor the guard pattern out.

### How to fix

Add two small local lambdas at the top of the parsing `try` block and replace the seven
blocks:

```cpp
const auto readBool = [&root](const char* a_key, bool& a_out) {
    if (root.contains(a_key) && root[a_key].is_boolean()) {
        a_out = root[a_key].get<bool>();
    }
};
const auto readStr = [&root](const char* a_key, std::string& a_out) {
    if (root.contains(a_key) && root[a_key].is_string()) {
        a_out = root[a_key].get<std::string>();
    }
};

readBool("enabled",        config.enabled);
readBool("debug",          config.debug);
readBool("logChanges",     config.logChanges);
readBool("dryRun",         config.dryRun);
readStr ("logLevel",       config.logLevel);
readStr ("rulesDirectory", config.rulesDirectory);
readStr ("debugGlobal",    config.debugGlobal);
```

> **Best-practice note.** [F.50](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-lambda)
> recommends a lambda when a function won't do (capturing locals, local helper). Capturing
> `root` by reference is correct here because the lambdas are used only within `Load`'s scope
> ([F.52](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rf-reference-capture)).
> If the same helper is ever needed in another scope, promote it to a free function.

### Result

Identical validation and defaults; −~20 lines; easier to add future fields.

---

## F. All sinks registered regardless of rules

### What it is

`EventManager::RegisterAll()` registers all eight event sinks unconditionally. The
documented design (PLAN §3 and §6) says *"Only sinks for events referenced by at least one
loaded rule are registered."* The code never implemented that.

### Why it matters

Each registered sink wakes on its event, checks the player filter, builds an `EventContext`
(which can allocate a `std::unordered_map` param entry), and calls `Dispatch` → `OnEvent` →
a `byEvent_` lookup that misses. For `activate`, `equip`, and `container_changed` this
happens constantly even when the modder has no rules for those events. Registering only
what's used is both the documented behavior and a real reduction in wasted work.

### How to fix

Pass the set of used event names into `RegisterAll` and guard each registration.

- `Events.h` — include `<unordered_set>` and change the signature:
  ```cpp
  void RegisterAll(const std::unordered_set<std::string>& a_events);
  ```
- `Events.cpp` — wrap each `AddEventSink` call:
  ```cpp
  if (a_events.contains("activate")) {
      holder->AddEventSink<RE::TESActivateEvent>(&g_activateSink);
  }
  // ... same for equip, container_changed, quest_stage, kill, level_increase, menu, cell_change
  ```
  Keep the `registered_` bool guard (idempotency), and leave `UnregisterAll()` unchanged —
  `RemoveEventSink` on a never-added sink is a harmless no-op.
- `Engine.cpp` — in `OnDataLoaded`, build the used set from `byEvent_` keys and pass it:
  ```cpp
  std::unordered_set<std::string> used;
  used.reserve(byEvent_.size());
  for (const auto& [name, _] : byEvent_) {
      used.insert(name);
  }
  EventManager::Get().RegisterAll(used);
  ```

> **Best-practice notes (CommonLibSSE-NG).**
> - Each event type is a separate `BSTEventSource<T>` with its own sink list; there is no
>   "register all sinks" API, so attaching only to the sources you actually use is the
>   recommended pattern (attaching to unused sources just adds dispatch overhead).
> - **Lifecycle:** form-dependent sinks must be registered after `kDataLoaded`. This plugin
>   already registers in the `kDataLoaded` handler, so that ordering is correct.
> - **Thread-safety:** `BSTEventSource` is internally thread-safe (recursive `BSSpinLock` plus
>   pending add/remove queues), and `SendEvent` holds that lock across every `ProcessEvent`
>   call. These gameplay events are raised on the main game thread — keep callbacks small and
>   avoid allocating/blocking; defer heavy work via `SKSE::GetTaskInterface()->AddTask(...)`
>   if needed. (Selective registration in this item directly reduces that per-event work.)
> - **No unload hook:** SKSE has no `SKSEPlugin_Unload` export — shutdown just `FreeLibrary`s
>   the DLL. So `UnregisterAll()` is only for intentional runtime teardown (e.g. a future
>   reload feature), not for unloading. `Reload()` currently never re-registers sinks, so no
>   behavior change there.

### Result

Only sinks for events that actually have rules are registered. Matches the documentation,
avoids per-event context construction + dispatch for unused events. `Reload()` is unchanged
(it already never re-registers sinks, so no regression).

---

## Verification

1. **Clean cross-compile** (no local unit tests exist; the suite is in-game):
   ```bash
   export VCPKG_ROOT=$HOME/Projects/vcpkg
   cd ~/Projects/sysop-brain/skse-globals
   cmake --preset linux-clangcl && cmake --build --preset linux-clangcl
   ```
2. **Startup log** should still show `20 rule(s) indexed across 8 event type(s)`
   (test/IN-GAME-CHECKLIST §1), now with globally-unique `rule=#N` indices.
3. **Sink registration** — confirm only the events referenced by the loaded rules register
   (visible via the reduced dispatch traffic; behavior of all rule types unchanged).
4. Re-run the in-game test matrix if redeploying (`test/IN-GAME-CHECKLIST.md`).

---

## Additional findings surfaced during research (not part of A–F)

While checking C++ and SKSE best practices, two toolchain/runtime risks came up that are
worth verifying separately. Neither changes the A–F fixes above, but both are worth a quick
check during the build/test pass.

1. **`lld-link`-produced DLL loading.** The CommonLibSSE-NG "Compiling with Clang" wiki
   states SKSE cannot load DLLs linked with `lld-link` (missing the required named export /
   ordinal 0). This project cross-compiles with `clang-cl` + `lld-link`
   (`cmake/toolchain-linux-clangcl.cmake`). The alandtse fork ships this exact preset, but
   **confirm the produced `GlobalRules.dll` actually loads on runtime 1.7.104** before
   relying on it. (Build-toolchain concern, independent of the A–F source changes.)
2. **Address Library V5 encoding (runtime 1.7.99+).** SKSE plugins for runtime 1.7.99+ must
   set `kVersionIndependentEx_AddressLibraryV5` in `PluginVersionData`; upstream SKSE master
   enforces this. Confirm this project's CommonLibSSE-NG fork emits it for 1.7.104.
