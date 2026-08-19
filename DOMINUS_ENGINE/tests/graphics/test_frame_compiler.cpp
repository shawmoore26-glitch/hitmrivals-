// tests/graphics/test_frame_compiler.cpp
// GRAPHICS's one real question: can DOMINUS take a scene and produce
// an actual deterministic frame? Verified directly -- hand-computed
// camera math, real determinism across independent calls, real
// deterministic ordering regardless of insertion order.
#include "GRAPHICS/Renderer/Camera.h"
#include "GRAPHICS/Renderer/Frame.h"
#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "GRAPHICS/Renderer/Scene.h"
#include "tests/TestFramework.h"

#include <cmath>

using dominus::animation::Transform2D;
using dominus::graphics::Camera;
using dominus::graphics::Frame;
using dominus::graphics::FrameCompiler;
using dominus::graphics::Scene;
using dominus::graphics::SceneEntity;
using dominus::graphics::ToCameraSpace;
using dominus::graphics::Viewport;

namespace {
bool NearlyEqual(float a, float b, float eps = 0.001f) { return std::fabs(a - b) < eps; }
}  // namespace

// --- Camera math, hand-verified ----------------------------------------

DOMINUS_TEST(ToCameraSpace_IdentityCameraLeavesWorldTransformUnchanged) {
    Camera camera;  // at origin, zoom=1, rotation=0
    Transform2D world;
    world.x = 10.0f;
    world.y = 20.0f;
    world.rotation_deg = 15.0f;

    auto screen = ToCameraSpace(camera, world);
    DOMINUS_EXPECT(NearlyEqual(screen.x, 10.0f));
    DOMINUS_EXPECT(NearlyEqual(screen.y, 20.0f));
    DOMINUS_EXPECT(NearlyEqual(screen.rotation_deg, 15.0f));
}

DOMINUS_TEST(ToCameraSpace_TranslatesRelativeToCameraPosition) {
    Camera camera;
    camera.x = 5.0f;
    camera.y = 5.0f;
    Transform2D world;
    world.x = 15.0f;
    world.y = 25.0f;

    auto screen = ToCameraSpace(camera, world);
    DOMINUS_EXPECT(NearlyEqual(screen.x, 10.0f));  // 15 - 5
    DOMINUS_EXPECT(NearlyEqual(screen.y, 20.0f));  // 25 - 5
}

DOMINUS_TEST(ToCameraSpace_AppliesZoomAsAUniformScale) {
    Camera camera;
    camera.zoom = 2.0f;
    Transform2D world;
    world.x = 10.0f;
    world.y = 10.0f;
    world.scale_x = 1.0f;
    world.scale_y = 1.0f;

    auto screen = ToCameraSpace(camera, world);
    DOMINUS_EXPECT(NearlyEqual(screen.x, 20.0f));
    DOMINUS_EXPECT(NearlyEqual(screen.y, 20.0f));
    DOMINUS_EXPECT(NearlyEqual(screen.scale_x, 2.0f));
    DOMINUS_EXPECT(NearlyEqual(screen.scale_y, 2.0f));
}

DOMINUS_TEST(ToCameraSpace_RotatesAroundCameraByItsNegativeRotation) {
    // A 90-degree camera rotation should rotate a world point at
    // (1,0) relative to the camera to (0,-1) in screen space -- hand-
    // computed, not just "the code ran."
    Camera camera;
    camera.rotation_deg = 90.0f;
    Transform2D world;
    world.x = 1.0f;
    world.y = 0.0f;

    auto screen = ToCameraSpace(camera, world);
    DOMINUS_EXPECT(NearlyEqual(screen.x, 0.0f, 0.01f));
    DOMINUS_EXPECT(NearlyEqual(screen.y, -1.0f, 0.01f));
    DOMINUS_EXPECT(NearlyEqual(screen.rotation_deg, -90.0f));
}

// --- FrameCompiler: real determinism -------------------------------------

DOMINUS_TEST(FrameCompiler_SameSceneProducesSameFrameHash) {
    Scene scene;
    scene.entities.push_back({"brooklyn", Transform2D{10, 20, 0, 1, 1}, "mat_jacket", "", 0});
    scene.entities.push_back({"static", Transform2D{-5, 15, 30, 1, 1}, "mat_static", "", 1});
    Camera camera;

    auto frameA = FrameCompiler::Compile(scene, camera);
    auto frameB = FrameCompiler::Compile(scene, camera);
    DOMINUS_EXPECT(frameA.frame_hash == frameB.frame_hash);
    DOMINUS_EXPECT(frameA.frame_hash.size() == 64);
}

DOMINUS_TEST(FrameCompiler_DifferentSceneProducesDifferentFrameHash) {
    Scene sceneA;
    sceneA.entities.push_back({"brooklyn", Transform2D{10, 20, 0, 1, 1}, "mat_jacket", "", 0});
    Scene sceneB;
    sceneB.entities.push_back({"brooklyn", Transform2D{11, 20, 0, 1, 1}, "mat_jacket", "", 0});  // real, meaningful change
    Camera camera;

    auto frameA = FrameCompiler::Compile(sceneA, camera);
    auto frameB = FrameCompiler::Compile(sceneB, camera);
    DOMINUS_EXPECT(frameA.frame_hash != frameB.frame_hash);
}

DOMINUS_TEST(FrameCompiler_OrderIsDeterministicRegardlessOfInsertionOrder) {
    Scene sceneA;
    sceneA.entities.push_back({"zebra", Transform2D{}, "", "", 0});
    sceneA.entities.push_back({"apple", Transform2D{}, "", "", 0});

    Scene sceneB;
    sceneB.entities.push_back({"apple", Transform2D{}, "", "", 0});
    sceneB.entities.push_back({"zebra", Transform2D{}, "", "", 0});

    Camera camera;
    auto frameA = FrameCompiler::Compile(sceneA, camera);
    auto frameB = FrameCompiler::Compile(sceneB, camera);

    DOMINUS_EXPECT(frameA.frame_hash == frameB.frame_hash);  // insertion order shouldn't matter
    DOMINUS_EXPECT(frameA.commands[0].entity_id == "apple");  // real, fixed tie-break order
    DOMINUS_EXPECT(frameA.commands[1].entity_id == "zebra");
}

DOMINUS_TEST(FrameCompiler_SortLayerTakesPriorityOverEntityId) {
    Scene scene;
    scene.entities.push_back({"zebra", Transform2D{}, "", "", /*sort_layer=*/0});
    scene.entities.push_back({"apple", Transform2D{}, "", "", /*sort_layer=*/1});
    Camera camera;

    auto frame = FrameCompiler::Compile(scene, camera);
    DOMINUS_EXPECT(frame.commands[0].entity_id == "zebra");  // layer 0 first, despite alphabetical order
    DOMINUS_EXPECT(frame.commands[1].entity_id == "apple");
}

DOMINUS_TEST(FrameCompiler_EmptySceneProducesAValidEmptyFrame) {
    Scene scene;  // zero entities
    Camera camera;
    auto frame = FrameCompiler::Compile(scene, camera);
    DOMINUS_EXPECT(frame.commands.empty());
    DOMINUS_EXPECT(frame.frame_hash.size() == 64);  // still a real, well-formed hash
}

DOMINUS_TEST(FrameCompiler_UnresolvedMaterialAndMeshRefsAreCarriedThroughNotFabricated) {
    Scene scene;
    scene.entities.push_back({"brooklyn", Transform2D{}, "MAT-JACKET-001", "brooklyn_mesh.obj", 0});
    Camera camera;
    auto frame = FrameCompiler::Compile(scene, camera);
    DOMINUS_EXPECT(frame.commands[0].material_ref == "MAT-JACKET-001");
    DOMINUS_EXPECT(frame.commands[0].mesh_ref == "brooklyn_mesh.obj");
}

// =====================================================================
// RenderFrame authority: Frame is the ONLY channel a renderer consumes.
// Confirmed structurally (RasterDevice/VulkanFrameRenderer contain zero
// references to EntityRegistry/MetaBinObject/Scene/SceneEntity -- see
// Frame.h's own header comment) and proven behaviorally here: a Frame
// is a complete, deterministic snapshot -- same inputs always produce
// an identical Frame, and a targeted mutation to the underlying Scene
// changes ONLY the affected RenderItem (DrawCommand), never any other.
// =====================================================================

bool SameDrawCommand(const dominus::graphics::DrawCommand& a, const dominus::graphics::DrawCommand& b) {
    return a.entity_id == b.entity_id && a.screen_transform.x == b.screen_transform.x &&
           a.screen_transform.y == b.screen_transform.y &&
           a.screen_transform.rotation_deg == b.screen_transform.rotation_deg &&
           a.screen_transform.scale_x == b.screen_transform.scale_x &&
           a.screen_transform.scale_y == b.screen_transform.scale_y && a.material_ref == b.material_ref &&
           a.mesh_ref == b.mesh_ref && a.sort_layer == b.sort_layer &&
           a.material_wear_state == b.material_wear_state;
}

DOMINUS_TEST(RenderFrame_IsComplete_CarriesItsOwnCameraAndViewport) {
    Scene scene;
    scene.entities.push_back({"brooklyn", {5, 5, 0, 1, 1}, "MAT-A", "", 0});
    Camera camera;
    camera.x = 12.0f;
    camera.zoom = 2.0f;
    Viewport viewport{512, 288};

    Frame frame = FrameCompiler::Compile(scene, camera, viewport);

    // The Frame carries its own real provenance -- not just the baked-
    // in EFFECT of the camera on screen_transform, the camera itself.
    DOMINUS_EXPECT(frame.camera.x == 12.0f);
    DOMINUS_EXPECT(frame.camera.zoom == 2.0f);
    DOMINUS_EXPECT(frame.viewport.width == 512);
    DOMINUS_EXPECT(frame.viewport.height == 288);
}

DOMINUS_TEST(RenderFrame_SameSceneCameraViewport_ProducesIdenticalFrame_AB) {
    // Scene -> FrameCompiler -> RenderFrame A
    // same Scene -> FrameCompiler -> RenderFrame B
    // A == B -- checked as real, direct data equality (every field of
    // every DrawCommand, plus camera and viewport), not merely hash
    // equality standing in for it.
    Scene scene;
    scene.entities.push_back({"alpha", {10, 20, 15, 1, 1}, "MAT-A", "meshA", 0});
    scene.entities.push_back({"beta", {-5, 8, 0, 2, 1}, "MAT-B", "meshB", 1});
    Camera camera;
    camera.x = 3.0f;
    camera.zoom = 1.5f;
    Viewport viewport{256, 256};

    Frame frameA = FrameCompiler::Compile(scene, camera, viewport);
    Frame frameB = FrameCompiler::Compile(scene, camera, viewport);

    DOMINUS_EXPECT(frameA.frame_hash == frameB.frame_hash);
    DOMINUS_EXPECT(frameA.camera.x == frameB.camera.x && frameA.camera.zoom == frameB.camera.zoom);
    DOMINUS_EXPECT(frameA.viewport.width == frameB.viewport.width &&
                   frameA.viewport.height == frameB.viewport.height);
    DOMINUS_EXPECT(frameA.commands.size() == frameB.commands.size());
    for (std::size_t i = 0; i < frameA.commands.size(); i++) {
        DOMINUS_EXPECT(SameDrawCommand(frameA.commands[i], frameB.commands[i]));
    }
}

DOMINUS_TEST(RenderFrame_MutatingOneEntityTransform_ChangesOnlyThatRenderItem) {
    // Entity B transform mutated -> RenderFrame changes ONLY B. Checked
    // at the DATA level (the unaffected entity's DrawCommand fields),
    // not merely "the frame_hash differs somewhere".
    Scene sceneBefore;
    sceneBefore.entities.push_back({"entity_a", {0, 0, 0, 1, 1}, "MAT-A", "", 0});
    sceneBefore.entities.push_back({"entity_b", {50, 50, 0, 1, 1}, "MAT-B", "", 0});
    Camera camera;

    Frame frameBefore = FrameCompiler::Compile(sceneBefore, camera);

    Scene sceneAfter = sceneBefore;
    sceneAfter.entities[1].world_transform.x = 999.0f;  // mutate ONLY entity_b's transform
    Frame frameAfter = FrameCompiler::Compile(sceneAfter, camera);

    DOMINUS_EXPECT(frameBefore.frame_hash != frameAfter.frame_hash);  // a real change occurred

    // entity_a's real RenderItem is untouched -- every field, not just
    // "probably fine".
    const auto* aBefore = &frameBefore.commands[0];
    const auto* aAfter = &frameAfter.commands[0];
    DOMINUS_EXPECT(aBefore->entity_id == "entity_a" && aAfter->entity_id == "entity_a");
    DOMINUS_EXPECT(SameDrawCommand(*aBefore, *aAfter));

    // entity_b's real RenderItem DID change, specifically in x.
    const auto* bBefore = &frameBefore.commands[1];
    const auto* bAfter = &frameAfter.commands[1];
    DOMINUS_EXPECT(bBefore->entity_id == "entity_b" && bAfter->entity_id == "entity_b");
    DOMINUS_EXPECT(bBefore->screen_transform.x != bAfter->screen_transform.x);
    DOMINUS_EXPECT(bAfter->screen_transform.x == 999.0f);
}

DOMINUS_TEST(RenderFrame_RemovingOneEntity_FrameContainsOnlyRemaining) {
    // Entity B removed -> RenderFrame contains A, not B -- and A's own
    // RenderItem is byte-identical to what it was before the removal
    // (removal never perturbs a survivor's own real data).
    Scene sceneBefore;
    sceneBefore.entities.push_back({"entity_a", {7, 3, 10, 1, 1}, "MAT-A", "meshA", 0});
    sceneBefore.entities.push_back({"entity_b", {50, 50, 0, 1, 1}, "MAT-B", "meshB", 0});
    Camera camera;
    Frame frameBefore = FrameCompiler::Compile(sceneBefore, camera);
    DOMINUS_EXPECT(frameBefore.commands.size() == 2);

    Scene sceneAfter;
    sceneAfter.entities.push_back(sceneBefore.entities[0]);  // only entity_a remains
    Frame frameAfter = FrameCompiler::Compile(sceneAfter, camera);

    DOMINUS_EXPECT(frameAfter.commands.size() == 1);
    DOMINUS_EXPECT(frameAfter.commands[0].entity_id == "entity_a");
    for (const auto& cmd : frameAfter.commands) DOMINUS_EXPECT(cmd.entity_id != "entity_b");

    // The survivor's RenderItem is exactly what it was in the
    // 2-entity Frame -- removal of an unrelated entity changed nothing
    // about it.
    DOMINUS_EXPECT(SameDrawCommand(frameBefore.commands[0], frameAfter.commands[0]));
}

// =====================================================================
// Scene Lifecycle / Reconciliation: Render output is a pure consequence
// of CURRENT DOMINUS scene state, never accumulated renderer/compiler
// history. Proven at the GPU-buffer level already (GPU Frame Lifecycle
// milestone); this proves it at the scene/RenderFrame level.
//
//   Scene A (entity_a + entity_b) -> Compile -> RenderFrame A
//   Remove entity_b -> Compile -> RenderFrame A'
//   Re-add entity_b with IDENTICAL authoritative state -> Compile
//     -> RenderFrame A_restored
//   A_restored == A, byte-for-byte, field-by-field
//
// Catches, specifically: stale DrawCommands, stale mesh/material
// references, stale entity IDs, ordering drift, FrameCompiler
// retaining old scene state -- FrameCompiler::Compile is a pure,
// static function with no members and no statics, so "retaining
// state" is structurally impossible, but this is proven directly
// below rather than merely asserted from reading the class
// declaration.
// =====================================================================

DOMINUS_TEST(SceneLifecycle_CreateRemoveReAdd_RenderFrameRestoredExactly) {
    SceneEntity entityA{"entity_a", {10, 20, 5, 1.0f, 1.0f}, "MAT-A", "meshA", 0};
    SceneEntity entityB{"entity_b", {-30, 15, 90, 2.0f, 1.5f}, "MAT-B", "meshB", 1};
    Camera camera;
    camera.x = 5.0f;
    camera.zoom = 1.2f;

    // Create: Scene A -> Compile -> RenderFrame A.
    Scene sceneA;
    sceneA.entities.push_back(entityA);
    sceneA.entities.push_back(entityB);
    Frame frameA = FrameCompiler::Compile(sceneA, camera);
    DOMINUS_EXPECT(frameA.commands.size() == 2);

    // Remove: entity_b removed -> Compile -> RenderFrame A'.
    Scene sceneAPrime;
    sceneAPrime.entities.push_back(entityA);
    Frame framePrime = FrameCompiler::Compile(sceneAPrime, camera);
    DOMINUS_EXPECT(framePrime.commands.size() == 1);
    DOMINUS_EXPECT(framePrime.commands[0].entity_id == "entity_a");
    for (const auto& cmd : framePrime.commands) DOMINUS_EXPECT(cmd.entity_id != "entity_b");  // no stale entity id
    // No stale DrawCommand survives from Frame A -- entity_a's own
    // data is untouched by entity_b's removal.
    DOMINUS_EXPECT(SameDrawCommand(frameA.commands[0], framePrime.commands[0]));

    // Re-add: entity_b restored with IDENTICAL authoritative state ->
    // Compile -> RenderFrame A (restored).
    Scene sceneARestored;
    sceneARestored.entities.push_back(entityA);
    sceneARestored.entities.push_back(entityB);  // same struct value as originally used
    Frame frameARestored = FrameCompiler::Compile(sceneARestored, camera);

    // The strongest claim: A_restored == A, byte-for-byte.
    DOMINUS_EXPECT(frameARestored.frame_hash == frameA.frame_hash);
    DOMINUS_EXPECT(frameARestored.commands.size() == frameA.commands.size());
    for (std::size_t i = 0; i < frameA.commands.size(); i++) {
        DOMINUS_EXPECT(SameDrawCommand(frameA.commands[i], frameARestored.commands[i]));
    }
    // No stale mesh/material reference: the re-added entity_b's real
    // mesh_ref/material_ref are exactly what they were before removal.
    DOMINUS_EXPECT(frameARestored.commands[1].mesh_ref == frameA.commands[1].mesh_ref);
    DOMINUS_EXPECT(frameARestored.commands[1].material_ref == frameA.commands[1].material_ref);
    // No ordering drift: entity_a still precedes entity_b in both
    // Frames (sort_layer 0 < 1), the same relative order.
    DOMINUS_EXPECT(frameA.commands[0].entity_id == "entity_a" && frameA.commands[1].entity_id == "entity_b");
    DOMINUS_EXPECT(frameARestored.commands[0].entity_id == "entity_a" &&
                   frameARestored.commands[1].entity_id == "entity_b");
}

DOMINUS_TEST(SceneLifecycle_FrameCompilerIsStateless_InterleavedCallsNeverCrossContaminate) {
    // FrameCompiler::Compile is a static, member-less function -- no
    // instance state, no static/global state -- so "retaining old
    // scene state across calls" is structurally impossible. Proven
    // directly, not just read from the class declaration: compile
    // scene X, then many OTHER, unrelated scenes, then scene X again
    // -- X's result must be identical every time, regardless of what
    // was compiled in between.
    Scene sceneX;
    sceneX.entities.push_back({"entity_x", {1, 2, 3, 1, 1}, "MAT-X", "meshX", 0});
    Camera camera;
    Frame frameX1 = FrameCompiler::Compile(sceneX, camera);

    for (int i = 0; i < 20; i++) {
        Scene otherScene;
        otherScene.entities.push_back(
            {"entity_other_" + std::to_string(i), {static_cast<float>(i), 0, 0, 1, 1}, "MAT-OTHER", "", 0});
        FrameCompiler::Compile(otherScene, camera);  // interleaved, unrelated compiles
    }

    Frame frameX2 = FrameCompiler::Compile(sceneX, camera);
    DOMINUS_EXPECT(frameX1.frame_hash == frameX2.frame_hash);
    DOMINUS_EXPECT(SameDrawCommand(frameX1.commands[0], frameX2.commands[0]));
}

DOMINUS_TEST(SceneLifecycle_RemovalThenDifferentEntityAdded_NeverConfusedWithOriginal) {
    // A real, targeted check against entity-id confusion: remove
    // entity_b, add a DIFFERENT entity_c at entity_b's exact former
    // screen position -- the Frame must reflect entity_c's real
    // identity, never silently treat it as "entity_b came back".
    SceneEntity entityA{"entity_a", {0, 0, 0, 1, 1}, "MAT-A", "", 0};
    SceneEntity entityB{"entity_b", {40, 40, 0, 1, 1}, "MAT-B", "", 1};
    Camera camera;

    Scene sceneWithB;
    sceneWithB.entities.push_back(entityA);
    sceneWithB.entities.push_back(entityB);
    Frame frameWithB = FrameCompiler::Compile(sceneWithB, camera);

    SceneEntity entityC{"entity_c", {40, 40, 0, 1, 1}, "MAT-C", "", 1};  // same position, real different identity
    Scene sceneWithC;
    sceneWithC.entities.push_back(entityA);
    sceneWithC.entities.push_back(entityC);
    Frame frameWithC = FrameCompiler::Compile(sceneWithC, camera);

    DOMINUS_EXPECT(frameWithC.commands[1].entity_id == "entity_c");
    DOMINUS_EXPECT(frameWithC.commands[1].material_ref == "MAT-C");
    DOMINUS_EXPECT(frameWithB.frame_hash != frameWithC.frame_hash);  // real, distinct identities, real distinct frames
}
