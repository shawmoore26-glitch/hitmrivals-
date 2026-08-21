// CHARACTER/HitmBridge/HitmApplicationLoop.cpp
#include "CHARACTER/HitmBridge/HitmApplicationLoop.h"

#include <utility>

#include "CHARACTER/HitmBridge/HitmSceneBridge.h"
#include "GRAPHICS/Renderer/FrameCompiler.h"
#include "GRAPHICS/Renderer/Scene.h"

namespace dominus::character::hitm {

namespace {
// Matches every prior Track H live CLI demo's own `currentMoveForState`
// lambda (see TOOLS/Editor/dominus_cli.cpp) -- the real move currently
// driving an attack sub-state, nullptr otherwise. Duplicated here rather
// than reused because it is three lines of real, already-established
// logic, not a shared abstraction worth a new header for.
const HitmMoveInstance* CurrentMoveForState(HitmFighterState state, const HitmMoveInstance& special) {
    switch (state) {
        case HitmFighterState::kAttackStartup:
        case HitmFighterState::kAttackActive:
        case HitmFighterState::kAttackRecovery:
            return &special;
        default:
            return nullptr;
    }
}
}  // namespace

core::Result<HitmApplicationLoop> HitmApplicationLoop::Create(const HitmIdentityRecord& identityA,
                                                                 const HitmCombatGenome& genomeA,
                                                                 const HitmAssetBundle& bundleA,
                                                                 graphics::TextureAtlas atlasA,
                                                                 const std::string& fighterIdA,
                                                                 const HitmIdentityRecord& identityB,
                                                                 const HitmCombatGenome& genomeB,
                                                                 const HitmAssetBundle& bundleB,
                                                                 graphics::TextureAtlas atlasB,
                                                                 const std::string& fighterIdB,
                                                                 const HitmGameRules& rules) {
    auto matchResult = HitmMatch::Create(identityA, genomeA, identityB, genomeB, rules);
    if (!matchResult.ok) {
        return core::Result<HitmApplicationLoop>::Fail("HitmApplicationLoop: match creation failed: " + matchResult.error);
    }
    auto specialA = HitmMoveInstance::Extract(identityA, "special");
    if (!specialA.ok) {
        return core::Result<HitmApplicationLoop>::Fail("HitmApplicationLoop: fighter A special extraction failed: " +
                                                          specialA.error);
    }
    auto specialB = HitmMoveInstance::Extract(identityB, "special");
    if (!specialB.ok) {
        return core::Result<HitmApplicationLoop>::Fail("HitmApplicationLoop: fighter B special extraction failed: " +
                                                          specialB.error);
    }
    if (rules.Sprite().display_height <= 0.0) {
        return core::Result<HitmApplicationLoop>::Fail(
            "HitmApplicationLoop: real game.json sprite.displayHeight must be positive");
    }

    HitmApplicationLoop loop(std::move(*matchResult.value), bundleA, std::move(*specialA.value), std::move(atlasA),
                              fighterIdA, bundleB, std::move(*specialB.value), std::move(atlasB), fighterIdB,
                              rules.Sprite().display_height, static_cast<int>(rules.View().w),
                              static_cast<int>(rules.View().h));
    loop.SimulationStep();  // real frame-0 state, so CachedFrame()/Present() are meaningful before any real input
    return core::Result<HitmApplicationLoop>::Ok(std::move(loop));
}

HitmApplicationLoop::HitmApplicationLoop(HitmMatch match, HitmAssetBundle bundleA, HitmMoveInstance specialA,
                                           graphics::TextureAtlas atlasA, std::string fighterIdA,
                                           HitmAssetBundle bundleB, HitmMoveInstance specialB,
                                           graphics::TextureAtlas atlasB, std::string fighterIdB,
                                           double displayHeight, int viewWidth, int viewHeight)
    : match_(std::move(match)),
      bundleA_(std::move(bundleA)),
      specialA_(std::move(specialA)),
      atlasA_(std::move(atlasA)),
      fighterIdA_(std::move(fighterIdA)),
      bundleB_(std::move(bundleB)),
      specialB_(std::move(specialB)),
      atlasB_(std::move(atlasB)),
      fighterIdB_(std::move(fighterIdB)),
      displayHeight_(displayHeight),
      viewWidth_(viewWidth),
      viewHeight_(viewHeight) {}

void HitmApplicationLoop::UpdatePresentationCamera(const HitmMatchSnapshot& snapshot) {
    // Deliberately minimal placeholder -- see this file's own top
    // comment. Real camera work is Track H Phase 5E.
    //
    // Y is negated to match HitmSceneBridge's own real HITM-Y-down ->
    // DOMINUS-Y-up conversion (see HitmSceneBridge.h's header comment) --
    // without this, the camera would sit at world y=0 while every real
    // fighter part renders around world y=-300..-540 (HITM's real
    // ground=538 pushed through that same negation), putting both
    // fighters entirely outside any reasonably-sized viewport. Midpoint
    // of the two fighters' real (x, y) keeps both fighters framed as
    // they move, the same real value this file already computes for X.
    camera_.x = (snapshot.fighter_a.x + snapshot.fighter_b.x) / 2.0f;
    camera_.y = -(snapshot.fighter_a.y + snapshot.fighter_b.y) / 2.0f;
    camera_.zoom = 1.0f;
    camera_.rotation_deg = 0.0f;
}

void HitmApplicationLoop::SimulationStep() {
    HitmInputCommand commandA = TranslateRawInput(ReadRawInput(kPlayerOneKeyboard, isPressedA_));
    HitmInputCommand commandB = TranslateRawInput(ReadRawInput(kPlayerTwoKeyboard, isPressedB_));
    match_.AdvanceFrame(commandA, commandB);

    HitmMatchSnapshot snapshot = match_.Snapshot();

    const HitmMoveInstance* currentMoveA = CurrentMoveForState(snapshot.fighter_a.state, specialA_);
    auto drawA = BuildSpriteDrawData(snapshot.fighter_a, currentMoveA, bundleA_, &secondaryMotionA_);
    if (!drawA.ok) {
        lastError_ = "HitmApplicationLoop: fighter A sprite draw data failed: " + drawA.error;
        return;  // real, disclosed: keep the previous cached frame rather than corrupt presentation
    }
    const HitmMoveInstance* currentMoveB = CurrentMoveForState(snapshot.fighter_b.state, specialB_);
    auto drawB = BuildSpriteDrawData(snapshot.fighter_b, currentMoveB, bundleB_, &secondaryMotionB_);
    if (!drawB.ok) {
        lastError_ = "HitmApplicationLoop: fighter B sprite draw data failed: " + drawB.error;
        return;
    }

    auto entitiesA = BuildHitmSceneEntities(fighterIdA_, snapshot.fighter_a, *drawA.value, displayHeight_);
    if (!entitiesA.ok) {
        lastError_ = "HitmApplicationLoop: fighter A scene bridge failed: " + entitiesA.error;
        return;
    }
    auto entitiesB = BuildHitmSceneEntities(fighterIdB_, snapshot.fighter_b, *drawB.value, displayHeight_);
    if (!entitiesB.ok) {
        lastError_ = "HitmApplicationLoop: fighter B scene bridge failed: " + entitiesB.error;
        return;
    }

    graphics::Scene scene;
    scene.entities.reserve(entitiesA.value->size() + entitiesB.value->size());
    for (auto& e : *entitiesA.value) scene.entities.push_back(std::move(e));
    for (auto& e : *entitiesB.value) scene.entities.push_back(std::move(e));

    UpdatePresentationCamera(snapshot);

    cachedFrame_ = graphics::FrameCompiler::Compile(scene, camera_, graphics::Viewport{viewWidth_, viewHeight_},
                                                       {atlasA_, atlasB_});
    lastError_.clear();
}

graphics::PixelBuffer HitmApplicationLoop::Present(graphics::RasterOptions options) const {
    return graphics::RasterDevice::Rasterize(cachedFrame_, options);
}

void HitmApplicationLoop::RegisterInto(core::Application& app, graphics::RasterOptions options,
                                         std::function<void(const graphics::PixelBuffer&)> sink) {
    app.SetSimulationStepCallback([this] { SimulationStep(); });
    app.SetPresentCallback([this, options, sink] {
        graphics::PixelBuffer buffer = Present(options);
        if (sink) sink(buffer);
    });
}

}  // namespace dominus::character::hitm
