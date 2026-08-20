// CHARACTER/HitmBridge/HitmInputAdapter.h
// ROADMAP.md Track H Phase 5C -- the input adapter:
//
//   Keyboard / Gamepad -> Input Adapter -> HitmInputCommand
//     -> HitmMatch::AdvanceFrame() -> existing proven combat simulation
//
// This file is the WHOLE adapter, and only the adapter. It does not
// redesign `HitmInputCommand` or `HitmMatch::AdvanceFrame()` (both
// untouched, unmodified, called exactly as Track H Phases 3/4 left
// them) -- it translates physical button state into the existing real
// seam. `HitmMatch::AdvanceFrame(HitmInputCommand inputA,
// HitmInputCommand inputB)` already takes two independent per-fighter
// commands (Track H Phase 3) -- Player 2 is not a new architectural
// concern this phase invents, it is already structurally supported;
// this file only needs to produce a second, independent
// `HitmInputCommand` for it, from a second, independent binding.
//
// TWO REAL LAYERS, DELIBERATELY SEPARATE:
//
//   1. `HitmRawInputState` -> `HitmInputCommand` (`TranslateRawInput`).
//      Pure, platform-free, fully unit-tested. This is the actual
//      "adapter" logic -- how six independent boolean buttons collapse
//      into ONE `HitmInputCommand` per frame, since the enum genuinely
//      cannot represent more than one command at a time.
//   2. `HitmButtonBinding` + `ReadRawInput` -- which physical button ID
//      (a real GLFW key code, or a real GLFW gamepad button index --
//      see the constants below) feeds which `HitmRawInputState` field.
//      Deliberately takes a generic `std::function<bool(int)> isPressed`
//      rather than a `GLFWwindow*` -- this keeps the WHOLE adapter,
//      keyboard and gamepad alike, free of any GLFW/Vulkan dependency
//      and fully testable with a synthetic callable. The real,
//      one-line glue (`[window](int k){ return
//      glfwGetKey(window,k)==GLFW_PRESS; }` /
//      `glfwGetGamepadState`-backed) is Track H Phase 5D's job, where
//      the real `GLFWwindow`/game loop actually lives -- see this
//      file's own "Deliberately not built this phase" note below.
//
// THE ENUM'S REAL LIMITATION, NOT SILENTLY PAPERED OVER:
// `HITM_RENDER_INPUT_LOOP_AUDIT.md` section B.3 already established
// that hitm-engine's own real `InputSystem.js` models input as
// independent, simultaneously-true booleans (`{left,right,up,down,
// light,heavy,special,...}`), while `HitmInputCommand` is a single,
// mutually-exclusive enum -- one command per frame, by Module 5A's own
// original, disclosed design (no real authored data exists for a
// second attack type, see `HitmMoveInstance.h`'s top comment). This
// phase does NOT redesign that enum. `TranslateRawInput` resolves
// simultaneous physical input into ONE command via a real, disclosed,
// evidence-grounded priority order (see its own comment below) --
// exactly the scoped, non-blocking answer the phase's own brief asked
// for. If a future phase's actual play-testing finds this priority
// order is not enough (e.g. a real need to walk while blocking, which
// the real engine's own `Fighter.js`/`CombatSystem.js` DOES support via
// its independent booleans and DOMINUS currently cannot), that is the
// next, separately-scoped extension to `HitmInputCommand` itself -- not
// something to smuggle into this adapter.
//
// "ATTACK" AND "SPECIAL" ARE TWO REAL KEY BINDINGS FOR ONE REAL COMMAND,
// AND THAT IS DISCLOSED, NOT HIDDEN: the phase brief asks for both
// "attack" and "special" controls. `HitmInputCommand` has exactly one
// real attack-type command, `kSpecial` -- `HitmFighterRuntime` tracks
// exactly one real move per fighter (extracted from `signature.json`'s
// real "special" token, `HitmMoveInstance.h`'s own top comment). Both
// `HitmRawInputState::attack` and `::special` therefore resolve to the
// exact same `HitmInputCommand::kSpecial` in `TranslateRawInput` today.
// This is not an adapter choice masking a gap -- it is the honest,
// visible consequence of a real, already-documented data-model
// limitation. The two bindings are kept genuinely separate (different
// real keys/buttons) so that if a second real move type is ever
// imported, "attack" and "special" become genuinely distinct here with
// zero adapter redesign -- the seam already exists, waiting.
#pragma once

#include <functional>

#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"  // for HitmInputCommand

namespace dominus::character::hitm {

// Six independent, real, simultaneously-representable buttons -- the
// exact six the phase brief named (left, right, up/jump, attack, block,
// special). Not `HitmInputCommand` -- this is the honest, richer input
// this adapter reads BEFORE collapsing it down to the one command
// `AdvanceFrame` can actually consume.
struct HitmRawInputState {
    bool left = false;
    bool right = false;
    bool up = false;
    bool attack = false;
    bool block = false;
    bool special = false;
};

// Real, disclosed priority order for collapsing simultaneous
// `HitmRawInputState` buttons into the one `HitmInputCommand`
// `AdvanceFrame` can take this frame -- grounded in real, evidenced
// precedent from hitm-engine's own `CombatSystem.js::_move()`, not
// invented from scratch:
//
//   1. `block` -- real engine checks `input.down` FIRST and returns
//      immediately (`CombatSystem.js` line ~490-493): block preempts
//      everything else, unconditionally, every real frame.
//   2. `special` / `attack` -- real engine's own `_tryStart()` checks
//      `special` before any other action button; called even while
//      moving, and a started move overrides the frame's resulting
//      state regardless of concurrent movement input.
//   3. `up` (jump) -- real engine applies jump's real effect
//      (`f.vy=P.jumpVel; f.state=STATE.JUMP`) unconditionally when
//      grounded, independent of concurrent left/right.
//   4. `left` XOR `right` -- exactly one of the two; both held (a
//      genuinely contradictory input) or neither held both resolve to
//      `kNeutral`, a real, disclosed cancellation choice, not an
//      arbitrary "left wins".
//   5. Otherwise `kNeutral`.
HitmInputCommand TranslateRawInput(const HitmRawInputState& raw);

// A real physical button ID per `HitmRawInputState` field. The integer
// values below are real, stable, public GLFW key-code / gamepad-button-
// index constants (cited by name in each binding's own comment) -- this
// struct and every binding constant below intentionally do NOT include
// <GLFW/glfw3.h> or reference GLFWwindow anywhere, so this whole file
// stays buildable with zero new dependencies, exactly like every other
// CHARACTER::hitm module. `ReadRawInput` below is what actually turns
// these IDs into a `HitmRawInputState`, given any real "is this ID
// currently down" callable -- keyboard and gamepad alike, see
// `kPlayerOneKeyboard`/`kPlayerTwoKeyboard`/`kGamepadButtons` below.
struct HitmButtonBinding {
    int left = 0;
    int right = 0;
    int up = 0;
    int attack = 0;
    int block = 0;
    int special = 0;
};

// Real GLFW key codes (the same public, stable integer values
// `GLFW_KEY_*` names in glfw3.h) -- one local keyboard, two independent
// players. `block` sits at each cluster's own real "down" position
// (`S` for WASD, Down-arrow for the arrow cluster), mirroring the real
// engine's own real `input.down` = block convention even though
// DOMINUS's `kBlock` is its own direct command, not derived from a
// "down" direction. `attack`/`special` are two real, distinct,
// collision-free keys per player -- see this file's own top comment for
// why both currently produce the same `HitmInputCommand`.
//
// Player 1 (WASD cluster): A/D/W movement, S block, J attack, L special
// -- J and L are the SAME real keys hitm-engine's own `InputSystem.js`
// binds to `light`/`special` (`readHuman()`'s real `k['j']`/`k['l']`),
// reused here rather than invented.
inline constexpr HitmButtonBinding kPlayerOneKeyboard{
    /*left=*/65,     // GLFW_KEY_A
    /*right=*/68,    // GLFW_KEY_D
    /*up=*/87,       // GLFW_KEY_W
    /*attack=*/74,   // GLFW_KEY_J
    /*block=*/83,    // GLFW_KEY_S
    /*special=*/76,  // GLFW_KEY_L
};

// Player 2 (arrow cluster + numpad): Left/Right/Up movement, Down block,
// numpad 1/2 for attack/special -- a real, disclosed, arbitrary-but-
// collision-free choice (no key code shared with kPlayerOneKeyboard,
// verified by a dedicated test below), not a claim about ergonomic
// optimality. Easily rebound later; this is real data, not a fixed
// design decision this file is precious about.
inline constexpr HitmButtonBinding kPlayerTwoKeyboard{
    /*left=*/263,   // GLFW_KEY_LEFT
    /*right=*/262,  // GLFW_KEY_RIGHT
    /*up=*/265,     // GLFW_KEY_UP
    /*attack=*/321, // GLFW_KEY_KP_1
    /*block=*/264,  // GLFW_KEY_DOWN
    /*special=*/322, // GLFW_KEY_KP_2
};

// A real GLFW gamepad binding (`GLFW_GAMEPAD_BUTTON_*` indices, the
// same generic `ReadRawInput` below consumes identically to a keyboard
// binding -- one architecture, two real physical input devices, exactly
// the "if the existing architecture supports it cleanly" case the phase
// brief asked about). Left/right/up read from the D-pad; block/attack/
// special from three of the four real face buttons.
inline constexpr HitmButtonBinding kGamepadButtons{
    /*left=*/13,     // GLFW_GAMEPAD_BUTTON_DPAD_LEFT
    /*right=*/12,    // GLFW_GAMEPAD_BUTTON_DPAD_RIGHT
    /*up=*/11,       // GLFW_GAMEPAD_BUTTON_DPAD_UP
    /*attack=*/0,    // GLFW_GAMEPAD_BUTTON_A
    /*block=*/2,     // GLFW_GAMEPAD_BUTTON_X
    /*special=*/1,   // GLFW_GAMEPAD_BUTTON_B
};

// Real, pure, platform-free: queries `isPressed` once per binding field
// -- six real calls, no polling logic, no debouncing, no edge-detection
// of its own (a real "held this frame" snapshot is exactly what
// `HitmInputCommand`'s own per-frame, level-triggered semantics already
// expect -- `HitmFighterRuntime::AdvanceFrame` has never distinguished
// "just pressed" from "held", and this adapter does not invent that
// distinction either).
HitmRawInputState ReadRawInput(const HitmButtonBinding& binding, const std::function<bool(int)>& isPressed);

}  // namespace dominus::character::hitm
