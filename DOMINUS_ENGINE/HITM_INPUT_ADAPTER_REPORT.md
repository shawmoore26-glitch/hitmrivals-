# HITM Input Adapter — Track H Phase 5C

Closes the "Input" gap `HITM_RENDER_INPUT_LOOP_AUDIT.md` section B named:
"no keyboard or gamepad state has ever been sampled by DOMINUS." This
phase does not touch the combat state machine, `HitmInputCommand`, or
`HitmMatch::AdvanceFrame()` — all three are already, correctly, the real
consumption seam Module 5A/Phase 3 established. This phase is only the
adapter that feeds it.

## The chain

```
Keyboard / Gamepad            (real physical input — not yet wired to a
                                real GLFWwindow; see "Deliberately not
                                built this phase" below)
      |
HitmButtonBinding + ReadRawInput      (THIS PHASE — real key/button
      |                                tables, pure, testable)
HitmRawInputState
      |
TranslateRawInput              (THIS PHASE — real, disclosed priority
      |                         order, pure, testable)
HitmInputCommand
      |
HitmMatch::AdvanceFrame()      (Track H Phase 3 — unmodified)
      |
existing proven combat simulation
```

## What was built

**`CHARACTER/HitmBridge/HitmInputAdapter.h/.cpp`** (new). Two deliberately
separate real layers:

1. **`TranslateRawInput(const HitmRawInputState&) -> HitmInputCommand`** —
   pure. `HitmRawInputState` is six independent, real, simultaneously-
   representable booleans (`left`, `right`, `up`, `attack`, `block`,
   `special` — exactly the six the phase brief named). Collapsing them
   into the one `HitmInputCommand` `AdvanceFrame` can take is a real,
   disclosed priority order, grounded in hitm-engine's own real
   `CombatSystem.js::_move()`/`_tryStart()`, not invented from scratch:
   `block` (real engine checks `input.down` first and returns
   immediately) → `special`/`attack` (real engine's `_tryStart()` checks
   `special` before any other action, called even while moving) → `up`
   (real engine applies jump unconditionally when grounded) → `left` XOR
   `right` (both or neither held cancels to neutral — a real, disclosed
   choice, not an arbitrary "left wins") → `neutral`.
2. **`HitmButtonBinding` + `ReadRawInput`** — real key/button ID tables
   (`kPlayerOneKeyboard`, `kPlayerTwoKeyboard`, `kGamepadButtons`), and
   the one generic function that turns any of them plus a real
   `isPressed(int) -> bool` callable into a `HitmRawInputState`. The same
   function reads a keyboard binding and a gamepad binding identically —
   "if the existing architecture supports [Player 2] cleanly" turned out
   to be yes, in two separate ways: `HitmMatch::AdvanceFrame` already
   took two independent commands since Phase 3, and this adapter's own
   generic binding/read shape already extends to a second device with no
   new code path.

## The enum's real limitation — not silently papered over

`HITM_RENDER_INPUT_LOOP_AUDIT.md` section B.3 already established that
hitm-engine's own real `InputSystem.js` models input as independent,
simultaneously-true booleans, while `HitmInputCommand` is a single,
mutually-exclusive enum. This phase does **not** redesign that enum, per
explicit instruction. `TranslateRawInput`'s priority order is the real,
scoped, non-blocking answer for the first playable slice — a player can
walk, block, jump, and attack, and the resolution when they overlap is
real, evidenced, and tested, not silently wrong or crashing. It is not a
claim that DOMINUS can represent every real HITM input combination
(walking while blocking, for one real example the audit already named,
is not representable — block wins outright). That remains the next,
separately-scoped extension to `HitmInputCommand` itself if and when it
becomes an actual blocker, exactly as instructed — not smuggled in here.

**"Attack" and "special" are two real key bindings for one real
command, and that is disclosed, not hidden.** `HitmInputCommand` has
exactly one real attack-type command, `kSpecial` — `HitmFighterRuntime`
tracks exactly one real move per fighter (`signature.json`'s real
"special" token). Both `HitmRawInputState::attack` and `::special`
therefore resolve to `kSpecial` today. The two bindings are kept
genuinely separate, real, distinct keys per player specifically so that
if a second real move type is ever imported, "attack" and "special"
become genuinely distinct with zero adapter redesign — the seam already
exists.

## The real key/button tables

| | left | right | up | attack | block | special |
|---|---|---|---|---|---|---|
| Player 1 (keyboard) | A | D | W | J | S | L |
| Player 2 (keyboard) | ← | → | ↑ | Num1 | ↓ | Num2 |
| Gamepad | D-pad ← | D-pad → | D-pad ↑ | A button | X button | B button |

Player 1's J/L reuse the exact same real keys hitm-engine's own
`InputSystem.js::readHuman()` already binds to `light`/`special`. `block`
sits at each keyboard cluster's own real "down" position (S for WASD,
↓ for arrows), mirroring the real engine's own `input.down` = block
convention, even though `HitmInputCommand::kBlock` is DOMINUS's own
direct command rather than derived from a direction. Player 2's numpad
attack/special assignment is a real, disclosed, arbitrary-but-
collision-free choice — a dedicated test
(`PlayerOneAndPlayerTwoKeyboardBindings_ShareNoRealKeyCode`) proves the
two players' six-key sets never overlap.

## Deliberately not built this phase

**Real `glfwGetKey`/`glfwGetGamepadState` glue.** `ReadRawInput` takes a
generic `std::function<bool(int)>`, not a `GLFWwindow*`, specifically so
this whole adapter stays free of any GLFW/Vulkan dependency and fully
testable without a real window. The one-line real glue
(`[window](int k){ return glfwGetKey(window,k)==GLFW_PRESS; }`) is Track
H Phase 5D's job, where the real `GLFWwindow` and game loop actually
live — the phase brief's own sequencing puts "poll input" as the first
step of Phase 5D's loop, and this adapter's whole point is to make that
step a one-liner once it exists, not to build the loop early. Vulkan,
`VulkanFrameRenderer`, and the `DOMINUS_ENABLE_VULKAN` gating are
completely untouched by this phase — no file under `GRAPHICS/Vulkan/` was
modified.

## Verification

New file: `tests/integration/test_hitm_input_adapter.cpp`, 21 tests:

- 14 `TranslateRawInput` tests covering every named priority interaction
  (block-over-everything, special-over-movement-and-jump, jump-over-
  movement, left/right cancellation, the attack/special alias).
- 6 `ReadRawInput` tests, including the cross-player no-shared-key-code
  regression test and a gamepad-binding test proving the architecture's
  device-generality.
- 2 full-chain tests against the real, unmodified `HitmMatch` this track
  already closed: a real Player 1 "D held" key state translates to
  `kRight` and the real simulation actually walks (`fighter_a.x`
  increases); a real Player 2 "block key held" state translates to
  `kBlock` and the real fighter actually enters `kBlockingStance`. Not a
  mock of the simulation — the same `HitmMatch::AdvanceFrame` Phase 3
  proved.

**Live CLI demo** (added on request, alongside unit coverage): new
`dominus-cli hitm-input-adapter <brooklyn_dir> <rocket_dir> <game.json>`
prints, live, the real `HitmInputCommand` `ReadRawInput`/
`TranslateRawInput` actually produce for 8 named real key-state
scenarios (including the block-over-special and opposed-left-right
cases) against every scenario's own expected value, then drives the
real, unmodified `HitmMatch` for 30 real frames from a real "D held"
Player 1 state and prints the fighter actually walking, followed by a
real Player 2 "Down held" state producing a real blocking stance. Ran
clean under AddressSanitizer alongside the existing, byte-identical
`hitm-match` demo.

**926/926 tests passing** (was 905 before this phase). Clean build, zero
warnings. Clean Release rebuild. AddressSanitizer+UndefinedBehaviorSanitizer
clean, 2 runs. Both live CLI demos re-verified clean under ASan.
Fresh-clone build + full suite verified before push.

## What this phase explicitly does not do

- No redesign of `HitmInputCommand` or `HitmMatch::AdvanceFrame()`.
- No Vulkan/GLFW glue, no window, no live keyboard reading — Phase 5D.
- No camera, no application loop, no rendering — unchanged.
- No new combat mechanics, no touching Ghost Dash, `FillTriangle`, bone-
  hierarchy FK, or any already-closed Track H combat module.

## Next checkpoint

Track H Phase 5D — the application/game loop: `Application::Tick()`
implementing poll input → advance simulation → build presentation →
update camera → render → present, with a real fixed simulation
timestep, and the real `glfwGetKey`/`glfwGetGamepadState` glue this
phase's `ReadRawInput` was built to receive.
