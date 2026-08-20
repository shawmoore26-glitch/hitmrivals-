// CHARACTER/HitmBridge/HitmSceneBridge.h
// ROADMAP.md Track H Phase 5B -- the HITM Sprite Bridge. Closes the exact
// gap HITM_RENDER_INPUT_LOOP_AUDIT.md section A.3/A.4 named: "the two
// pipelines -- 'HITM real per-part draw data' and 'DOMINUS Scene->
// Frame->pixels' -- have never been connected, in either direction, for
// any fighter." This file is that connection, and only that connection:
//
//   HitmFighterRuntime -> HitmSpriteDrawData -> HitmPartDraw
//         -> (this file) -> GRAPHICS::SceneEntity -> FrameCompiler
//         -> GRAPHICS::DrawCommand -> GRAPHICS::TextureAtlas
//         -> GRAPHICS::RasterDevice -> actual HITM pixels
//
// A pure function, same discipline as HitmSpriteDrawData.h itself: no
// animation math, no clip/frame selection, no secondary-motion spring
// integration happens here -- all of that is Module 5B Phase 1's
// already-closed, already-tested job (HitmSpriteDrawData.h/.cpp,
// unmodified by this file). This file's only job is arithmetic: turn
// each already-computed real HitmPartDraw into a real, positioned,
// textured GRAPHICS::SceneEntity.
//
// THE PLACEMENT FORMULA, and why it is NOT tools/rig_render.py's literal
// arithmetic:
//
// hitm-engine's own real, working offline verification tool
// (tools/rig_render.py, already cited as the canonical placement
// reference by HitmRigPlacement.h and HitmSpriteDrawData.h) composes a
// part by pasting it at `(rect_x0 * canvasWidth, rect_y0 * canvasHeight)`
// as a top-left corner, where `canvasHeight = H` (an arbitrary debug
// default, 520) and `canvasWidth = H * 0.62`, and scales each part's
// pixel size by a further, undocumented `* 0.62` beyond `normW/normH *
// H`. Neither `0.62` constant is evidenced anywhere in real authored
// data -- `game.json` (HitmGameRules) has exactly one real, authored
// screen-scale value, `sprite.displayHeight`, and it is a single scalar,
// not a width/height pair. The two `0.62`s are also mutually
// inconsistent with the script's own root-bone placement
// (`0.5*H*0.42`, a THIRD, different, equally uncited constant) -- real
// evidence that these are debug-tool convenience fudges, not meaningful
// design data worth reverse-engineering byte for byte.
//
// This module therefore does NOT replicate rig_render.py's exact
// arithmetic. It uses the one real, authored, evidenced scale
// reference -- `HitmGameRules::Sprite().display_height` -- UNIFORMLY for
// both axes. This is not a guess: CHARACTER/HitmBridge/HitmAnimationSet.h's
// own header comment already documents that anim.json's real
// `offsetX`/`offsetY` keyframe values are "in the same
// normalized-to-display-height units as HitmPartsRig's normW/normH" --
// i.e. the real data itself already treats X and Y as sharing one scale
// reference. Everything else is a real, direct pass-through: `place_x`/
// `place_y` (rig.json's real rect corner, unmodified), `norm_w`/
// `norm_h` (parts.json's real normalized size, unmodified),
// `pose_rotation_deg`/`pose_offset_x/y` (anim.json's real sampled pose,
// including real secondary motion when the caller supplied it to
// BuildSpriteDrawData -- this file does not know or care whether
// secondary motion was involved, it just carries through whatever real
// pose value it was given), and `frame_x/y/w/h` (parts.json's real atlas
// pixel rect, carried byte for byte into DrawCommand's own
// `atlas_src_x/y/w/h`, exactly as Frame.h's own Phase 5A header comment
// already anticipated this bridge would do).
//
// ANCHORING: the fighter's own real (x, y) (HitmFighterSnapshot, Module
// 5A, unmodified) is HITM's real screen-pixel-space convention --
// evidenced directly in HitmFighterRuntime.cpp (`spatial->y >=
// phys.ground` clamps the fighter to the real, authored `ground` value
// from below, i.e. Y increases DOWNWARD, exactly PNG/rig_render.py's own
// convention, not a DOMINUS invention). A part's normalized local
// position (place_x + pose_offset_x, place_y + pose_offset_y) is
// anchored so (0.5, 1.0) -- rig.json's own implicit "character center,
// character base" convention, matching rig_render.py's real root-bone
// placement near the bottom-center of its canvas -- lands exactly on the
// fighter's own (x, y). Converted into DOMINUS's own real world
// convention (Y-UP, the same convention every other GRAPHICS::Camera/
// GRAPHICS::SceneEntity in this engine already uses -- see Camera.h,
// MeshTransform.h) by negating Y, the same axis flip
// GRAPHICS::RasterDevice already performs on every entity it draws, just
// applied once here instead of left implicit.
//
// ROTATION SIGN: `pose_rotation_deg` is real anim.json data, authored in
// the same convention rig_render.py's own `piece.rotate(-rot, ...)` call
// expects -- PIL's `rotate()` is a real, standard, visually-CCW-positive
// convention. DOMINUS's own `MeshTransform.h` applies `rotation_deg` as
// a standard math-CCW rotation in Y-UP world space; composed with this
// file's own Y-negation above (a reflection, which reverses rotational
// handedness once), the real, derived result is
// `transform.rotation_deg = -pose_rotation_deg` -- documented here so
// the one line of arithmetic below is traceable back to a real,
// reasoned derivation, not an arbitrary sign flip.
#pragma once

#include <string>
#include <vector>

#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"
#include "GRAPHICS/Renderer/Scene.h"

namespace dominus::character::hitm {

// Builds one real, positioned, textured GRAPHICS::SceneEntity per real
// HitmPartDraw in `drawData.parts`, in the same real drawOrder
// (`sort_layer` is assigned by list position, matching
// GRAPHICS::SceneFromEntities's own already-established, disclosed
// convention for "no real draw-layer property exists yet" -- see
// SceneFromEntities.h's own header comment). `fighterId` becomes both
// each entity's `entity_id` prefix (`"<fighterId>_<part_name>"`, unique
// per part) and the `atlas_id` every produced entity's `textured=true`
// DrawCommand will reference -- the caller is responsible for having
// decoded that fighter's real atlas PNG under exactly that same
// `atlas_id` (GRAPHICS::PngDecoder::DecodePngFile) and supplying it to
// FrameCompiler::Compile's own `atlases` parameter; this function does
// not decode or load anything itself, it only produces the reference.
//
// `displayHeight` is the real, authored `game.json` `sprite.displayHeight`
// (HitmGameRules::Sprite().display_height) -- required, not defaulted,
// because there is no real fallback value this file is willing to
// invent (see this file's own header comment for why rig_render.py's
// arbitrary debug default is not treated as real data). A `displayHeight`
// of exactly 0 fails rather than silently producing degenerate
// zero-size entities.
core::Result<std::vector<graphics::SceneEntity>> BuildHitmSceneEntities(const std::string& fighterId,
                                                                          const HitmFighterSnapshot& snapshot,
                                                                          const HitmSpriteDrawData& drawData,
                                                                          double displayHeight);

}  // namespace dominus::character::hitm
