// tests/soul/test_cinematic_director.cpp
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CinematicDirector.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::RigBinder;
using dominus::combat::CinematicDirector;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatEventType;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionInput;
using dominus::core::DominusSerializer;

namespace {
std::filesystem::path FixtureDir() {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures"),
        std::filesystem::path("../tests/fixtures"),
        std::filesystem::path("../../tests/fixtures"),
    };
    for (auto& c : candidates)
        if (std::filesystem::exists(c)) return c;
    throw std::runtime_error("fixtures directory not found");
}
}  // namespace

DOMINUS_TEST(CinematicDirector_TriggerActivatesEvent) {
    CinematicDirector director;
    DOMINUS_EXPECT(!director.IsActive());
    auto def = director.Trigger(CombatEventType::kFinisher);
    DOMINUS_EXPECT(director.IsActive());
    DOMINUS_EXPECT(def.camera_trigger == "finisher_cam");
    DOMINUS_EXPECT(director.Current() != nullptr);
}

DOMINUS_TEST(CinematicDirector_UpdateEndsEventAfterDuration) {
    CinematicDirector director;
    auto def = director.Trigger(CombatEventType::kClash);  // duration 0.8s
    director.Update(0.5f);
    DOMINUS_EXPECT(director.IsActive());
    director.Update(0.5f);  // total 1.0s, past 0.8s duration
    DOMINUS_EXPECT(!director.IsActive());
    DOMINUS_EXPECT(director.Current() == nullptr);
    (void)def;
}

DOMINUS_TEST(CinematicDirector_NewTriggerOverridesActiveEvent) {
    CinematicDirector director;
    director.Trigger(CombatEventType::kClash);
    auto def2 = director.Trigger(CombatEventType::kTransformation);
    DOMINUS_EXPECT(director.Current()->type == CombatEventType::kTransformation);
    DOMINUS_EXPECT(def2.camera_trigger == "transform_cam");
}

DOMINUS_TEST(CinematicDirector_EveryEventTypeHasADistinctDef) {
    CombatEventType types[] = {CombatEventType::kFinisher, CombatEventType::kClash,
                                CombatEventType::kTransformation, CombatEventType::kWallImpact,
                                CombatEventType::kEnvironmentalDestruction};
    for (auto t : types) {
        auto def = CinematicDirector::LookupDef(t);
        DOMINUS_EXPECT(def.type == t);
        DOMINUS_EXPECT(!def.camera_trigger.empty());
        DOMINUS_EXPECT(def.duration_seconds > 0.0f);
    }
}

DOMINUS_TEST(CombatController_KnockdownAutoTriggersFinisherCinematic) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);

    CinematicDirector director;
    controller.SetCinematicDirector(&director);
    DOMINUS_EXPECT(controller.Cinematic() == &director);

    controller.StartMove("jab");
    DOMINUS_EXPECT(!director.IsActive());  // no cinematic just from starting a move

    controller.ApplyHit(ReactionInput{.hit_power = 50.0f}, 1.0f);  // knockdown-tier power
    DOMINUS_EXPECT(director.IsActive());
    DOMINUS_EXPECT(director.Current()->type == CombatEventType::kFinisher);
}

DOMINUS_TEST(CombatController_WithoutCinematicDirectorStillWorks) {
    // Phase 3 behavior must be untouched when no director is attached.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    auto& obj = *loadResult.value;
    RigBinder::Bind(obj, fixtureDir);
    CombatBinder::Bind(obj, fixtureDir);

    auto evaluator = MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<MoveSetComponent>();
    CombatController controller(*evaluator, *moveSet);
    DOMINUS_EXPECT(controller.Cinematic() == nullptr);

    controller.StartMove("jab");
    auto result = controller.ApplyHit(ReactionInput{.hit_power = 50.0f}, 1.0f);
    DOMINUS_EXPECT(result.type == dominus::combat::ReactionType::kKnockdown);  // no crash, no cinematic
}
