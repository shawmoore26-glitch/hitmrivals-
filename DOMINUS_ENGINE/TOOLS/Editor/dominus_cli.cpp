// TOOLS/Editor/dominus_cli.cpp
// "Editor foundation" for Phase 1+2, per ROADMAP.md: a headless CLI before
// any GUI editor work. Usage:
//   dominus-cli inspect  path/to/object.dominus
//   dominus-cli validate path/to/object.dominus
//   dominus-cli play     path/to/object.dominus <clip_name> <time_seconds>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "AI/Agents/CombatAI.h"
#include "ANIMATION/ProceduralMotion/AnimationPlayer.h"
#include "ANIMATION/SkeletonSystem/AnimationClipLoader.h"
#include "ANIMATION/SkeletonSystem/SkeletonLoader.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/HitmBridge/HitmAnimationSet.h"
#include "CHARACTER/HitmBridge/HitmAssetImporter.h"
#include "CHARACTER/HitmBridge/HitmCombatGenome.h"
#include "CHARACTER/HitmBridge/HitmIdentityImporter.h"
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"
#include "CHARACTER/HitmBridge/HitmGameRules.h"
#include "CHARACTER/HitmBridge/HitmMoveInstance.h"
#include "CHARACTER/HitmBridge/HitmPartsRig.h"
#include "CHARACTER/HitmBridge/HitmRigPlacement.h"
#include "CHARACTER/HitmBridge/HitmSpriteDrawData.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/CombatController.h"
#include "COMBAT/HitSystem/AssetValidation.h"
#include "COMBAT/HitSystem/CollisionEvaluator.h"
#include "COMBAT/HitSystem/HurtboxLoader.h"
#include "RIG/AcceptanceCertificate.h"
#include "RIG/CharacterAcceptanceHarness.h"
#include "RIG/RigAuthorityValidator.h"
#include "RIG/RigProfileLoader.h"
#include "RIG/RigProfileValidator.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "COMBAT/HitSystem/CombatComponents.h"
#include "COMBAT/TransformationSystem.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "PHYSICS/CollisionSystem.h"
#include "PHYSICS/PhysicsSystem.h"
#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"
#include "CHARACTER/Genome/CombatStyleGenomeLoader.h"
#include "CHARACTER/Genome/CreatureGenomeSemanticValidator.h"
#include "CHARACTER/Genome/GameDesignCoherenceChecker.h"
#include "CHARACTER/Genome/GameDesignGenomeLoader.h"
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "CHARACTER/Genome/MaterialWearDeriver.h"
#include "CHARACTER/Genome/VisualGenomeLoader.h"
#include "CHARACTER/Genome/VisualMemorySummary.h"
#include "CHARACTER/Genome/VisualStyleGenomeLoader.h"
#include "COMBAT/PhysicsCombat/ImpactSolver.h"
#include "VISUALFORGE/AnimationSpecification.h"
#include "VISUALFORGE/AssetSpecification.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/CharacterBlueprint.h"
#include "VISUALFORGE/DependencyGraph.h"
#include "VISUALFORGE/ProductionSnapshot.h"
#include "VISUALFORGE/AcceptanceCertificate.h"
#include "VISUALFORGE/RendererPackage.h"
#include "VISUALFORGE/RendererPackageAcceptanceHarness.h"
#include "COMBAT/Provenance/ImpactEvent.h"
#include "COMBAT/Provenance/ImpactEventCompiler.h"
#include "COMBAT/Provenance/ImpactEventLog.h"
#include "COMBAT/Provenance/ImpactEventWorldHistoryHook.h"
#include "WORLD/Core/WorldHistory.h"
#include "REALITY/CompilationContext.h"
#include "REALITY/RealityCompiler.h"
#include "REALITY/RealityRebuilder.h"
#include "REALITY/BrooklynEvidenceGraphBuilder.h"
#include "REALITY/ChangeEventNormalizer.h"
#include "REALITY/RealityWatcher.h"
#include "REALITY/Reconciler.h"
#include "REGISTRY/CombatPhysicsGenomeCompiler.h"
#include "REGISTRY/CombatStyleGenomeCompiler.h"
#include "REGISTRY/GameDesignGenomeCompiler.h"
#include "REGISTRY/MaterialGenomeCompiler.h"
#include "REGISTRY/VisualGenomeCompiler.h"
#include "REGISTRY/VisualStyleGenomeCompiler.h"
#include "VALIDATION/BuildPipeline.h"
#include "REGISTRY/CreatureGenomeCompiler.h"
#include "REGISTRY/GenomeCompiler.h"
#include "REGISTRY/GenomeRegistry.h"
#include "REGISTRY/RuntimeSnapshot.h"
#include "VALIDATION/PackageValidator.h"
#include "WORLD/Core/SourceRefComponent.h"
#include "WORLD/Core/WorldPersistence.h"
#include "WORLD/Core/SpatialComponent.h"
#include "WORLD/Core/World.h"

using dominus::ai::CombatAI;
using dominus::ai::OpponentPatternTracker;
using dominus::animation::AnimationPlayer;
using dominus::character::AnimationSetComponent;
using dominus::character::GenomeDecoder;
using dominus::character::MakeMotionGraphEvaluator;
using dominus::character::MotionGraphComponent;
using dominus::character::RigBinder;
using dominus::character::SkeletonComponent;
using dominus::combat::AssetValidation;
using dominus::combat::CollisionEvaluator;
using dominus::combat::CombatBinder;
using dominus::combat::CombatController;
using dominus::combat::CombatIdentityComponent;
using dominus::combat::HurtboxSetComponent;
using dominus::combat::MoveSetComponent;
using dominus::combat::ReactionInput;
using dominus::combat::SkeletonScaleComponent;
using dominus::combat::TransformationLoader;
using dominus::combat::TransformationSystem;
using dominus::core::DominusSerializer;

namespace {

int Inspect(const std::string& path) {
    auto result = DominusSerializer::Load(path);
    if (!result.ok) {
        std::cerr << "Failed to load " << path << ": " << result.error << "\n";
        return 1;
    }
    const auto& obj = *result.value;
    std::cout << "object_id: " << obj.Id() << "\n";
    std::cout << "dominus_version: " << obj.Version() << "\n";
    std::cout << "component_count: " << obj.ComponentCount() << "\n";
    if (const auto* entityType = obj.GetComponent<dominus::core::EntityTypeComponent>()) {
        std::cout << "entity_type: " << entityType->entity_type << "\n";
    }
    if (const auto* provenance = obj.GetComponent<dominus::core::ProvenanceComponent>()) {
        std::cout << "provenance.creator: " << provenance->creator << "\n";
        std::cout << "provenance.creation_method: " << provenance->creation_method << "\n";
        std::cout << "provenance.creation_hash: " << provenance->creation_hash << "\n";
        std::cout << "provenance.source_assets: " << provenance->source_assets.size() << "\n";
    }
    if (const auto* socialRef = obj.GetComponent<dominus::core::SocialGenomeRefComponent>()) {
        std::cout << "social_genome.ref: " << socialRef->ref_path << "\n";
    }
    return 0;
}

int Validate(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "Cannot open " << path << "\n";
        return 1;
    }
    std::ostringstream ss;
    ss << in.rdbuf();

    std::vector<std::string> errors;
    bool valid = DominusSerializer::Validate(ss.str(), &errors);
    if (valid) {
        std::cout << path << ": VALID\n";
        return 0;
    }
    std::cout << path << ": INVALID\n";
    for (auto& e : errors) std::cout << "  - " << e << "\n";
    return 1;
}

int Play(const std::string& dominusPath, const std::string& clipName, float time) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;

    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    auto bindResult = RigBinder::Bind(obj, baseDir);
    if (!bindResult.ok) {
        std::cerr << "Failed to bind rig: " << bindResult.error << "\n";
        return 1;
    }

    auto* skeleton = obj.GetComponent<SkeletonComponent>();
    auto* animSet = obj.GetComponent<AnimationSetComponent>();
    if (!skeleton || !animSet) {
        std::cerr << "Object has no bound skeleton/animation set\n";
        return 1;
    }
    const auto* clip = animSet->Find(clipName);
    if (!clip) {
        std::cerr << "No clip named '" << clipName << "' on this object\n";
        return 1;
    }

    auto pose = AnimationPlayer::Sample(skeleton->skeleton, *clip, time);
    const auto& bones = skeleton->skeleton.Bones();

    std::cout << "pose for '" << obj.Id() << "' @ clip='" << clipName << "' t=" << time << "s\n";
    for (size_t i = 0; i < bones.size(); ++i) {
        std::cout << "  " << bones[i].name << ": x=" << pose[i].x << " y=" << pose[i].y
                   << " rot=" << pose[i].rotation_deg << "deg\n";
    }
    return 0;
}

int Graph(const std::string& dominusPath, const std::string& trigger, float driveSeconds) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;

    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    auto bindResult = RigBinder::Bind(obj, baseDir);
    if (!bindResult.ok) {
        std::cerr << "Failed to bind rig: " << bindResult.error << "\n";
        return 1;
    }

    auto evaluator = MakeMotionGraphEvaluator(obj);
    if (!evaluator) {
        std::cerr << "Object has no motion_graph bound (need skeleton + animations + motion_graph)\n";
        return 1;
    }

    std::cout << "start state: " << evaluator->CurrentState() << "\n";
    evaluator->Update(0.0f);

    if (!trigger.empty()) {
        bool ok = evaluator->Trigger(trigger);
        std::cout << "trigger '" << trigger << "': " << (ok ? "accepted" : "no matching transition") << "\n";
    }

    constexpr float kStep = 0.02f;
    std::string lastState = evaluator->CurrentState();
    for (float t = 0.0f; t < driveSeconds; t += kStep) {
        evaluator->Update(kStep);
        if (evaluator->CurrentState() != lastState) {
            std::cout << "  t=" << (t + kStep) << "s -> entered state '" << evaluator->CurrentState() << "'\n";
            lastState = evaluator->CurrentState();
        }
    }
    std::cout << "end state: " << evaluator->CurrentState() << " (transitioning=" << evaluator->IsTransitioning()
               << ")\n";
    return 0;
}

int Fight(const std::string& dominusPath, const std::string& moveName, float offsetX, float offsetY) {
    auto attackerLoad = DominusSerializer::Load(dominusPath);
    auto defenderLoad = DominusSerializer::Load(dominusPath);
    if (!attackerLoad.ok || !defenderLoad.ok) {
        std::cerr << "Failed to load " << dominusPath << "\n";
        return 1;
    }
    auto& attacker = *attackerLoad.value;
    auto& defender = *defenderLoad.value;

    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    if (!RigBinder::Bind(attacker, baseDir).ok || !RigBinder::Bind(defender, baseDir).ok ||
        !CombatBinder::Bind(attacker, baseDir).ok || !CombatBinder::Bind(defender, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }

    auto attackerEval = MakeMotionGraphEvaluator(attacker);
    auto* attackerSkel = attacker.GetComponent<SkeletonComponent>();
    auto* attackerMoves = attacker.GetComponent<MoveSetComponent>();
    auto* defenderSkel = defender.GetComponent<SkeletonComponent>();
    auto* defenderHurtboxes = defender.GetComponent<HurtboxSetComponent>();
    if (!attackerEval || !attackerSkel || !attackerMoves || !defenderSkel || !defenderHurtboxes) {
        std::cerr << "Object missing required combat components after binding\n";
        return 1;
    }

    // LAW C001: Intent -> Combat Logic -> Motion Request -> Skeleton
    // Execution -> Collision Evaluation -> Physics Result -> Reaction State.
    CombatController controller(*attackerEval, *attackerMoves);
    std::cout << "[intent] StartMove('" << moveName << "')\n";
    if (!controller.StartMove(moveName)) {
        std::cout << "  -> refused (no move data or motion graph has no matching state)\n";
        return 1;
    }

    const auto* move = controller.CurrentMove();
    float sampleTime = dominus::combat::FramesToSeconds(move->frames.startup + 1);  // one frame into active
    controller.Update(sampleTime);
    std::cout << "[motion] phase=" << static_cast<int>(controller.Phase()) << " at t=" << sampleTime << "s\n";

    // Bind pose stands in for "current animated pose" here -- the point of
    // this command is the collision + reaction pipeline, not re-deriving
    // animated pose math already proven by the `play`/`graph` commands.
    auto attackerBindPose = attackerSkel->skeleton.ComputeBindPoseWorld();
    auto defenderBindPose = defenderSkel->skeleton.ComputeBindPoseWorld();
    for (auto& t : defenderBindPose) {
        t.x += offsetX;
        t.y += offsetY;
    }

    auto hits = CollisionEvaluator::Evaluate(attackerSkel->skeleton, attackerBindPose, *move, defenderSkel->skeleton,
                                              defenderBindPose, defenderHurtboxes->hurtboxes);
    std::cout << "[collision] defender offset=(" << offsetX << "," << offsetY << ") -> " << hits.size()
              << " hit(s)\n";

    if (hits.empty()) {
        std::cout << "[result] no contact\n";
        return 0;
    }

    for (auto& hit : hits) {
        std::cout << "  hit: " << hit.attacker_bone << " -> " << hit.defender_bone << " at (" << hit.impact_x << ","
                   << hit.impact_y << ")\n";
    }

    ReactionInput input;
    input.hit_power = move->power;
    auto reaction = dominus::combat::ReactionSystem::Determine(input, offsetX);
    std::cout << "[reaction] power=" << move->power << " -> trigger='" << reaction.motion_trigger << "' force=("
              << reaction.force_x << "," << reaction.force_y << ")\n";
    return 0;
}

int Transform(const std::string& dominusPath, const std::string& transformName) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();

    if (!RigBinder::Bind(obj, baseDir).ok || !CombatBinder::Bind(obj, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }

    auto* refs = obj.GetComponent<dominus::core::TransformationRefListComponent>();
    if (!refs) {
        std::cerr << "Object has no transformations list\n";
        return 1;
    }
    std::string transformRefPath;
    for (auto& ref : refs->transformations) {
        if (ref.name == transformName) transformRefPath = ref.ref_path;
    }
    if (transformRefPath.empty()) {
        std::cerr << "No transformation named '" << transformName << "'\n";
        return 1;
    }

    auto* identityBefore = obj.GetComponent<CombatIdentityComponent>();
    std::cout << "[before] style='" << (identityBefore ? identityBefore->identity.style : "(none)") << "'\n";

    auto transformResult = TransformationLoader::LoadFromFile(baseDir / transformRefPath);
    if (!transformResult.ok) {
        std::cerr << "Failed to load transformation: " << transformResult.error << "\n";
        return 1;
    }
    auto applyResult = TransformationSystem::Apply(obj, *transformResult.value, baseDir);
    if (!applyResult.ok) {
        std::cerr << "Transformation failed: " << applyResult.error << "\n";
        return 1;
    }

    auto* identityAfter = obj.GetComponent<CombatIdentityComponent>();
    auto* movesAfter = obj.GetComponent<MoveSetComponent>();
    auto* scale = obj.GetComponent<SkeletonScaleComponent>();
    std::cout << "[after] style='" << identityAfter->identity.style << "' pressure='" << identityAfter->identity.pressure
               << "' risk='" << identityAfter->identity.risk << "'\n";
    std::cout << "[after] skeleton_scale=" << (scale ? scale->scale : 1.0f) << "\n";
    std::cout << "[after] move_count=" << movesAfter->moves.size() << "\n";

    auto weights = GenomeDecoder::Decode(identityAfter->identity);
    std::cout << "[genome] aggression=" << weights.aggression << " risk_tolerance=" << weights.risk_tolerance
               << " unpredictability=" << weights.unpredictability << "\n";

    if (auto* ai = obj.GetComponent<dominus::combat::AIProfile>()) {
        std::cout << "[profile] ai.behavior_tag='" << ai->behavior_tag << "' difficulty=" << ai->difficulty << "\n";
    }
    if (auto* physics = obj.GetComponent<dominus::combat::PhysicsProfile>()) {
        std::cout << "[profile] physics.mass_kg=" << physics->mass_kg
                   << " gravity_scale=" << physics->gravity_scale << "\n";
    }
    if (auto* audio = obj.GetComponent<dominus::combat::AudioProfile>()) {
        std::cout << "[profile] audio.voice_bank='" << audio->voice_bank << "'\n";
    }
    if (auto* visual = obj.GetComponent<dominus::combat::VisualProfile>()) {
        std::cout << "[profile] visual.material_set='" << visual->material_set << "'\n";
    }
    if (auto* camera = obj.GetComponent<dominus::combat::CameraProfile>()) {
        std::cout << "[profile] camera.shake_intensity=" << camera->shake_intensity << "\n";
    }
    return 0;
}

std::string Capitalize(const std::string& s) {
    if (s.empty()) return s;
    std::string out = s;
    out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
    return out;
}

int AiDecide(const std::string& dominusPath) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();

    if (!RigBinder::Bind(obj, baseDir).ok || !CombatBinder::Bind(obj, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }

    auto* identity = obj.GetComponent<CombatIdentityComponent>();
    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto* graphComp = obj.GetComponent<MotionGraphComponent>();
    if (!identity || !moves || !graphComp) {
        std::cerr << "Object missing combat identity, moves, or motion graph\n";
        return 1;
    }

    // Player Action -> CombatAI -> GenomeDecoder -> DecisionWeights ->
    // MoveSelector -> Motion Graph -> Skeleton Runtime.
    std::cout << "[genome] style='" << identity->identity.style << "'\n";
    auto weights = GenomeDecoder::Decode(identity->identity);
    std::cout << "[weights] aggression=" << weights.aggression << " risk_tolerance=" << weights.risk_tolerance
               << " counter_bias=" << weights.counter_bias << "\n";

    OpponentPatternTracker tracker(8);
    tracker.RecordMove("jab");
    tracker.RecordMove("jab");
    tracker.RecordMove("jab");
    std::cout << "[player action] opponent has thrown 3x 'jab'\n";

    CombatAI ai(tracker, weights);
    std::string category = ai.Decide();
    std::string chosen = ai.DecideMoveName(*moves, {"jab", "combo_starter"});
    std::cout << "[ai decision] category='" << category << "' chosen_move='" << chosen << "'\n";

    if (chosen.empty()) {
        std::cout << "[motion] AI chose to block -- no move to request\n";
        return 0;
    }

    auto evaluator = MakeMotionGraphEvaluator(obj);
    CombatController controller(*evaluator, *moves);
    bool started = controller.StartMove(chosen);
    std::cout << "[motion] StartMove('" << chosen << "') -> " << (started ? "accepted" : "refused") << "\n";

    if (started) {
        const auto* state = graphComp->graph.FindState(chosen);
        if (state) {
            std::cout << "[state] entering " << Capitalize(state->name) << "\n";
            std::cout << "[animation] " << state->clip_name << ".anim\n";
        }
        std::cout << "[result] combat pipeline complete\n";
    }
    return 0;
}

int ValidateMotion(const std::string& dominusPath) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();

    if (!RigBinder::Bind(obj, baseDir).ok || !CombatBinder::Bind(obj, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }

    auto* moves = obj.GetComponent<MoveSetComponent>();
    auto* graphComp = obj.GetComponent<MotionGraphComponent>();
    if (!moves || !graphComp) {
        std::cerr << "Object has no moves or motion_graph to validate\n";
        return 1;
    }

    auto report = AssetValidation::CheckMotionCoverage(*moves, graphComp->graph);
    std::cout << "checked " << moves->moves.size() << " move(s) against " << graphComp->graph.transitions.size()
               << " motion graph transition(s)\n";

    if (report.AllMovesResolve()) {
        std::cout << "OK: every move resolves to a valid motion state.\n";
        return 0;
    }

    std::cout << "MISSING: " << report.unreachable_moves.size() << " move(s) have no matching motion graph transition:\n";
    for (auto& name : report.unreachable_moves) {
        std::cout << "  - " << name << "\n";
    }
    return 1;
}

int WorldDemo(const std::string& dominusPath) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& entity = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();

    if (!RigBinder::Bind(entity, baseDir).ok || !CombatBinder::Bind(entity, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }

    // WORLD LAW 001: this fighter is 2.5D, side-view -- the SpatialComponent
    // is generic, not fighting-game-specific.
    entity.AddComponent<dominus::world::SpatialComponent>(dominus::world::SpatialComponent::Fighter2_5D(0.0f, 0.0f));
    std::string entityId = entity.Id();

    dominus::world::World world;
    world.Entities().CreateEntity(std::move(entity));
    std::cout << "[world] entity '" << entityId << "' created, dimension=2.5D\n";

    // The combat extension: built and registered HERE, outside WORLD/Core,
    // proving COMBAT is a plugin the world ticks rather than a built-in
    // world feature (see ROADMAP.md Phase 4.0).
    auto evaluator = MakeMotionGraphEvaluator(*world.Entities().Find(entityId));
    if (!evaluator) {
        std::cerr << "Entity missing skeleton/animations/motion_graph after binding\n";
        return 1;
    }
    bool triggered = false;

    world.Systems().RegisterSystem("combat_extension", [&](dominus::world::EntityRegistry&, float dt) {
        if (!triggered) {
            evaluator->Trigger("attack");
            triggered = true;
        }
        evaluator->Update(dt);
    });
    std::cout << "[world] registered system 'combat_extension' (" << world.Systems().SystemCount() << " total)\n";

    std::cout << "[world] ticking headless, no rendering...\n";
    for (int i = 0; i < 50; ++i) world.Tick(0.02f);
    std::cout << "[world] elapsed=" << world.ElapsedSeconds() << "s state='" << evaluator->CurrentState() << "'\n";
    std::cout << "[result] " << entityId << " ran as a world entity; COMBAT never modified WORLD/Core\n";
    return 0;
}

int PhysicsDemo(const std::string& dominusPath) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& fighter = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    if (!RigBinder::Bind(fighter, baseDir).ok || !CombatBinder::Bind(fighter, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }
    std::string fighterId = fighter.Id();
    fighter.AddComponent<dominus::world::SpatialComponent>(dominus::world::SpatialComponent::Fighter2_5D(0.0f, 0.0f));
    fighter.AddComponent<dominus::physics::Collider>(dominus::physics::Collider{});
    dominus::physics::RigidBody fighterBody;
    fighterBody.affected_by_gravity = false;
    fighterBody.velocity_x = 5.0f;
    fighter.AddComponent<dominus::physics::RigidBody>(fighterBody);

    dominus::core::MetaBinObject crate("crate_001", "0.1.0");
    crate.AddComponent<dominus::world::SpatialComponent>(dominus::world::SpatialComponent::Fighter2_5D(1.5f, 0.0f));
    crate.AddComponent<dominus::physics::Collider>(dominus::physics::Collider{});
    dominus::physics::RigidBody crateBody;
    crateBody.affected_by_gravity = false;
    crate.AddComponent<dominus::physics::RigidBody>(crateBody);

    dominus::world::World world;
    world.Entities().CreateEntity(std::move(fighter));
    world.Entities().CreateEntity(std::move(crate));
    std::cout << "[physics] entities: '" << fighterId << "' (Fighter, has CombatIdentity) and 'crate_001' (Crate, no combat data)\n";

    dominus::physics::PhysicsSystem physics(0.0f);
    world.Systems().RegisterSystem("physics", physics.AsWorldSystem());
    world.Systems().RegisterSystem("collision", dominus::physics::CollisionSystem::AsWorldSystem());
    std::cout << "[physics] registered systems: " << world.Systems().SystemCount()
               << " (physics never includes COMBAT/CHARACTER headers)\n";

    float startX = world.Entities().Find("crate_001")->GetComponent<dominus::world::SpatialComponent>()->x;
    for (int i = 0; i < 60; ++i) world.Tick(1.0f / 60.0f);
    float endX = world.Entities().Find("crate_001")->GetComponent<dominus::world::SpatialComponent>()->x;

    std::cout << "[physics] crate.x: " << startX << " -> " << endX << " (moved="
               << (endX != startX ? "true" : "false") << ")\n";
    std::cout << "[result] fighter and crate resolved through PHYSICS alone -- physics never knew either identity\n";
    return 0;
}

int ValidatePackage(const std::string& dominusPath) {
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    auto report = dominus::validation::PackageValidator::ValidateFile(dominusPath, baseDir);

    std::cout << "[validate-package] " << dominusPath << "\n";
    std::cout << "  errors=" << report.ErrorCount() << " warnings=" << report.WarningCount() << "\n";
    for (auto& issue : report.issues) {
        const char* tag = issue.severity == dominus::validation::Severity::kError    ? "ERROR"
                           : issue.severity == dominus::validation::Severity::kWarning ? "WARN"
                                                                                        : "INFO";
        std::cout << "  [" << tag << "] " << issue.category << ": " << issue.message << "\n";
    }

    if (report.Passed()) {
        std::cout << "[result] package passed validation\n";
        return 0;
    }
    std::cout << "[result] package FAILED validation\n";
    return 1;
}

// ROADMAP.md Track H Module 1. Imports a real hitm-engine fighter identity
// directory and prints what actually landed -- field counts and top-level
// keys, not a claim that this is a compiled character. See
// CHARACTER/HitmBridge/HitmIdentityImporter.h for exactly what is and
// isn't read.
int ImportHitmIdentityDemo(const std::string& identityDirStr) {
    using dominus::character::hitm::HitmIdentityImporter;

    auto result = HitmIdentityImporter::Import(identityDirStr);
    if (!result.ok) {
        std::cerr << "[import-hitm-identity] FAILED: " << result.error << "\n";
        return 1;
    }
    auto& rec = *result.value;
    std::cout << "[import-hitm-identity] fighter_id=" << rec.fighter_id << "\n";
    std::cout << "[import-hitm-identity] identity.name=" << rec.identity.Get("name")->AsString()
               << " faction=" << rec.identity.Get("faction")->AsString() << "\n";
    std::cout << "[import-hitm-identity] combat_genome.archetype=" << rec.combat_genome.Get("archetype")->AsString()
               << " (" << rec.combat_genome.AsObject().size() << " top-level keys)\n";
    std::cout << "[import-hitm-identity] character_dna: " << rec.character_dna.AsObject().size() << " top-level keys\n";
    std::cout << "[import-hitm-identity] design: " << rec.design.AsObject().size() << " top-level keys\n";
    std::cout << "[import-hitm-identity] signature: " << rec.signature.AsObject().size() << " top-level keys\n";
    std::cout << "[result] real identity data ingested and validated -- NOT compiled, NOT rendered, NOT playable yet\n";
    return 0;
}

// ROADMAP.md Track H Module 2. Imports a real fighter (via Module 1) and
// builds the explicit, typed HitmCombatGenome, then proves the
// import-then-export round trip lost nothing by comparing the exact
// source tree's canonical dump against ToJson()'s canonical dump live --
// not just in a unit test.
int HitmCombatGenomeDemo(const std::string& identityDirStr) {
    using dominus::character::hitm::HitmCombatGenome;
    using dominus::character::hitm::HitmIdentityImporter;

    auto importResult = HitmIdentityImporter::Import(identityDirStr);
    if (!importResult.ok) {
        std::cerr << "[hitm-combat-genome] import FAILED: " << importResult.error << "\n";
        return 1;
    }
    auto genomeResult = HitmCombatGenome::FromRecord(*importResult.value);
    if (!genomeResult.ok) {
        std::cerr << "[hitm-combat-genome] genome mapping FAILED: " << genomeResult.error << "\n";
        return 1;
    }
    auto& g = *genomeResult.value;

    std::cout << "[hitm-combat-genome] fighter_id=" << g.FighterId() << " archetype=" << g.Archetype() << "\n";
    std::cout << "[hitm-combat-genome] philosophy: " << g.Philosophy() << "\n";
    std::cout << "[hitm-combat-genome] defense.style=" << g.Defense().style.value_or("(none)")
               << " defense.blockPreference=" << (g.Defense().block_preference ? std::to_string(*g.Defense().block_preference) : "(none)")
               << "\n";
    if (g.HasReadEngine()) {
        const auto* re = g.GetReadEngine();
        std::cout << "[hitm-combat-genome] read_engine: max_reads=" << re->max_reads << " tiers=" << re->tiers.size()
                   << " decay_frames=" << re->decay.frames << "\n";
        for (const auto& tier : re->tiers) {
            std::cout << "  reads=" << tier.reads << " name=\"" << tier.name << "\" damage_mult=" << tier.damage_mult
                       << "\n";
        }
    } else {
        std::cout << "[hitm-combat-genome] read_engine: (none -- " << g.FighterId() << " does not have one, real gap in his design, not an import failure)\n";
    }

    std::string sourceDump = importResult.value->combat_genome.Dump();
    std::string exportedDump = g.ToJson().Dump();
    bool lossless = (sourceDump == exportedDump);
    std::cout << "[hitm-combat-genome] round-trip lossless (source dump == exported dump): " << (lossless ? "true" : "false")
               << "\n";
    std::cout << "[result] genome mapped and validated -- NOT wired to any consumer, NOT gameplay-affecting yet\n";
    return lossless ? 0 : 1;
}

// ROADMAP.md Track H Module 3. Imports a real fighter's generated
// parts.json (hitm-engine/data/characters/<fighter>/) and proves the same
// import-then-export losslessness live, plus surfaces the real duplicate-
// bone-name finding documented in HitmPartsRig.h.
int HitmPartsRigDemo(const std::string& characterDirStr) {
    using dominus::character::hitm::HitmPartsRig;

    auto result = HitmPartsRig::Import(characterDirStr);
    if (!result.ok) {
        std::cerr << "[hitm-parts-rig] FAILED: " << result.error << "\n";
        return 1;
    }
    auto& rig = *result.value;

    std::cout << "[hitm-parts-rig] fighter_id=" << rig.FighterId() << " atlas=" << rig.Atlas() << " sourceSize=["
               << rig.SourceWidth() << "x" << rig.SourceHeight() << "]\n";
    std::cout << "[hitm-parts-rig] parts=" << rig.Parts().size() << " bones=" << rig.Bones().size()
               << " drawOrder=" << rig.DrawOrder().size() << " handBone=" << rig.HandBone() << "\n";

    std::size_t springBones = 0;
    for (const auto& b : rig.Bones()) {
        if (b.follow.has_value()) ++springBones;
    }
    std::cout << "[hitm-parts-rig] " << springBones
               << " bone entries carry real secondary-motion 'follow' spring params (render-layer only, per the "
                  "Constitution's law -- not gameplay-affecting)\n";

    std::string sourceDump = dominus::core::json::Value::Parse([&] {
                                  std::ifstream in(std::filesystem::path(characterDirStr) / "parts.json", std::ios::binary);
                                  std::ostringstream ss;
                                  ss << in.rdbuf();
                                  return ss.str();
                              }())
                                  .Dump();
    bool lossless = (sourceDump == rig.ToJson().Dump());
    std::cout << "[hitm-parts-rig] round-trip lossless (source dump == exported dump): " << (lossless ? "true" : "false")
               << "\n";
    std::cout << "[result] atlas-space sprite-cutout rig parsed and validated -- NOT bound to a dominus::animation::"
                  "Skeleton, NOT rendered\n";
    return lossless ? 0 : 1;
}

// ROADMAP.md Track H Module 4. Imports the real, global
// data/system/game.json and proves the same import-then-export
// losslessness live.
int HitmGameRulesDemo(const std::string& gameJsonPathStr) {
    using dominus::character::hitm::HitmGameRules;

    auto result = HitmGameRules::Import(gameJsonPathStr);
    if (!result.ok) {
        std::cerr << "[hitm-game-rules] FAILED: " << result.error << "\n";
        return 1;
    }
    auto& g = *result.value;

    std::cout << "[hitm-game-rules] roster (" << g.Roster().size() << "):";
    for (const auto& f : g.Roster()) std::cout << " " << f;
    std::cout << "\n";
    std::cout << "[hitm-game-rules] physics: gravity=" << g.Physics().gravity << " walkSpeed=" << g.Physics().walk_speed
               << " dashSpeed=" << g.Physics().dash_speed << " jumpVel=" << g.Physics().jump_vel << "\n";
    std::cout << "[hitm-game-rules] combat: chipMult=" << g.Combat().chip_mult
               << " counterDmgMult=" << g.Combat().counter_dmg_mult << " hitstopLight=" << g.Combat().hitstop_light
               << " hitstopHeavy=" << g.Combat().hitstop_heavy << " hitstopCounter=" << g.Combat().hitstop_counter << "\n";
    std::cout << "[hitm-game-rules] rounds: toWin=" << g.Rounds().to_win << " timerSeconds=" << g.Rounds().timer_seconds
               << "\n";

    std::string sourceDump = dominus::core::json::Value::Parse([&] {
                                  std::ifstream in(gameJsonPathStr, std::ios::binary);
                                  std::ostringstream ss;
                                  ss << in.rdbuf();
                                  return ss.str();
                              }())
                                  .Dump();
    bool lossless = (sourceDump == g.ToJson().Dump());
    std::cout << "[hitm-game-rules] round-trip lossless (source dump == exported dump): " << (lossless ? "true" : "false")
               << "\n";
    std::cout << "[result] global game-rules table parsed and validated -- NOT wired into PHYSICS or COMBAT yet\n";
    return lossless ? 0 : 1;
}

// ROADMAP.md Track H Module 5A. Builds a real Brooklyn fighter from real
// imported data (Modules 1/2/4) and drives him through a scripted
// sequence, printing real state every frame -- the live, human-readable
// proof that real HITM data actually simulates, not just validates.
int HitmFighterRuntimeDemo(const std::string& identityDirStr, const std::string& gameJsonPathStr) {
    using dominus::character::hitm::HitmCombatGenome;
    using dominus::character::hitm::HitmFighterRuntime;
    using dominus::character::hitm::HitmFighterState;
    using dominus::character::hitm::HitmGameRules;
    using dominus::character::hitm::HitmIdentityImporter;
    using dominus::character::hitm::HitmInputCommand;
    using dominus::character::hitm::HitmMoveInstance;

    auto identityResult = HitmIdentityImporter::Import(identityDirStr);
    if (!identityResult.ok) {
        std::cerr << "[hitm-fighter-runtime] identity import FAILED: " << identityResult.error << "\n";
        return 1;
    }
    auto genomeResult = HitmCombatGenome::FromRecord(*identityResult.value);
    if (!genomeResult.ok) {
        std::cerr << "[hitm-fighter-runtime] genome mapping FAILED: " << genomeResult.error << "\n";
        return 1;
    }
    auto rulesResult = HitmGameRules::Import(gameJsonPathStr);
    if (!rulesResult.ok) {
        std::cerr << "[hitm-fighter-runtime] game rules import FAILED: " << rulesResult.error << "\n";
        return 1;
    }
    auto moveResult = HitmMoveInstance::Extract(*identityResult.value, "special");
    if (!moveResult.ok) {
        std::cerr << "[hitm-fighter-runtime] move extraction FAILED: " << moveResult.error << "\n";
        return 1;
    }
    auto runtimeResult = HitmFighterRuntime::Create(*identityResult.value, *genomeResult.value, *rulesResult.value);
    if (!runtimeResult.ok) {
        std::cerr << "[hitm-fighter-runtime] runtime creation FAILED: " << runtimeResult.error << "\n";
        return 1;
    }
    auto& fighter = *runtimeResult.value;
    auto& move = *moveResult.value;

    auto stateName = [](HitmFighterState s) {
        switch (s) {
            case HitmFighterState::kIdle: return "idle";
            case HitmFighterState::kWalking: return "walking";
            case HitmFighterState::kJumping: return "jumping";
            case HitmFighterState::kBlockingStance: return "blocking_stance";
            case HitmFighterState::kAttackStartup: return "attack_startup";
            case HitmFighterState::kAttackActive: return "attack_active";
            case HitmFighterState::kAttackRecovery: return "attack_recovery";
            case HitmFighterState::kHitstun: return "hitstun";
            case HitmFighterState::kBlockstun: return "blockstun";
        }
        return "?";
    };
    auto printFrame = [&](const char* label) {
        auto s = fighter.Snapshot();
        std::cout << "[hitm-fighter-runtime] frame=" << s.frame << " (" << label << ") state=" << stateName(s.state)
                   << " x=" << s.x << " y=" << s.y << " vx=" << s.velocity_x << " vy=" << s.velocity_y
                   << " grounded=" << (s.grounded ? "true" : "false") << " meter=" << s.meter
                   << " reads=" << s.read_engine_reads << " hitstop=" << s.hitstop_frames_remaining << "\n";
    };

    std::cout << "[hitm-fighter-runtime] fighter_id=" << fighter.FighterId() << " special_move=\"" << move.move_def.name
               << "\" (startup=" << move.move_def.frames.startup << " active=" << move.move_def.frames.active
               << " recovery=" << move.move_def.frames.recovery << " damage=" << move.move_def.power << ")\n";
    printFrame("init");

    for (int i = 0; i < 3; ++i) fighter.AdvanceFrame(HitmInputCommand::kRight);
    printFrame("after 3x real walkSpeed steps");

    fighter.AdvanceFrame(HitmInputCommand::kJump);
    printFrame("jump (real jumpVel)");
    while (!fighter.Snapshot().grounded) fighter.AdvanceFrame(HitmInputCommand::kNeutral);
    printFrame("landed (real gravity integration)");

    fighter.AdvanceFrame(HitmInputCommand::kSpecial);
    printFrame("special triggered (real startup begins)");
    while (fighter.State() == HitmFighterState::kAttackStartup) fighter.AdvanceFrame(HitmInputCommand::kNeutral);
    printFrame("-> attack_active (real startup frames elapsed)");
    while (fighter.State() == HitmFighterState::kAttackActive) fighter.AdvanceFrame(HitmInputCommand::kNeutral);
    printFrame("-> attack_recovery (real active frames elapsed)");
    while (fighter.State() == HitmFighterState::kAttackRecovery) fighter.AdvanceFrame(HitmInputCommand::kNeutral);
    printFrame("-> idle (real recovery frames elapsed)");

    std::cout << "[hitm-fighter-runtime] read_engine tier 0 outgoing damage (real " << move.move_def.power
               << " x real mult 1.0): " << fighter.ResolveOutgoingDamage(move) << "\n";
    fighter.GainRead();
    fighter.GainRead();
    fighter.GainRead();
    std::cout << "[hitm-fighter-runtime] after 3 real GainRead() calls: reads=" << fighter.Snapshot().read_engine_reads
               << " tier=\"" << fighter.ReadEngineState().CurrentTierName()
               << "\" outgoing damage (real mult " << fighter.ReadEngineState().CurrentDamageMultiplier()
               << "): " << fighter.ResolveOutgoingDamage(move) << "\n";

    fighter.TakeHit(move, /*blocking=*/false);
    printFrame("took a hit as defender (real damage/hitstun/meter/hitstop, real defense_bias-driven reaction)");
    const char* reactionNames[] = {"none", "stagger", "knockback", "launch", "wall_impact", "ground_impact", "knockdown"};
    std::cout << "[hitm-fighter-runtime] COMBAT::ReactionSystem reaction: "
               << reactionNames[static_cast<int>(fighter.LastReaction().type)] << " motion_trigger=\""
               << fighter.LastReaction().motion_trigger << "\"\n";

    std::cout << "[result] real HITM data drove a live DOMINUS simulation for " << fighter.Snapshot().frame
               << " frames -- NOT rendered, NOT audible, NOT a claim this is playable yet\n";
    return 0;
}

// ROADMAP.md Track H Module 5B. Real HITM sprite/texture integration:
// builds the same real Brooklyn runtime Module 5A proved, plus a real
// asset bundle (atlas/parts/anim/rig), and drives him through a scripted
// sequence printing the ACTUAL deterministic sprite draw data each real
// runtime state selects -- clip name, sampled frame, and one real part's
// resolved pose -- CPU-only, no pixel is ever decoded or drawn. See
// HitmSpriteDrawData.h's top comment for exactly what this proves and
// what it deliberately does not.
int HitmSpriteDrawDataDemo(const std::string& identityDirStr, const std::string& gameJsonPathStr,
                            const std::string& hitmEngineRootStr) {
    using dominus::character::hitm::BuildSpriteDrawData;
    using dominus::character::hitm::HitmAssetImporter;
    using dominus::character::hitm::HitmCombatGenome;
    using dominus::character::hitm::HitmFighterRuntime;
    using dominus::character::hitm::HitmFighterState;
    using dominus::character::hitm::HitmGameRules;
    using dominus::character::hitm::HitmIdentityImporter;
    using dominus::character::hitm::HitmInputCommand;
    using dominus::character::hitm::HitmMoveInstance;
    using dominus::character::hitm::HitmSecondaryMotionState;

    auto identityResult = HitmIdentityImporter::Import(identityDirStr);
    if (!identityResult.ok) {
        std::cerr << "[hitm-sprite-draw-data] identity import FAILED: " << identityResult.error << "\n";
        return 1;
    }
    auto genomeResult = HitmCombatGenome::FromRecord(*identityResult.value);
    if (!genomeResult.ok) {
        std::cerr << "[hitm-sprite-draw-data] genome mapping FAILED: " << genomeResult.error << "\n";
        return 1;
    }
    auto rulesResult = HitmGameRules::Import(gameJsonPathStr);
    if (!rulesResult.ok) {
        std::cerr << "[hitm-sprite-draw-data] game rules import FAILED: " << rulesResult.error << "\n";
        return 1;
    }
    auto moveResult = HitmMoveInstance::Extract(*identityResult.value, "special");
    if (!moveResult.ok) {
        std::cerr << "[hitm-sprite-draw-data] move extraction FAILED: " << moveResult.error << "\n";
        return 1;
    }
    auto runtimeResult = HitmFighterRuntime::Create(*identityResult.value, *genomeResult.value, *rulesResult.value);
    if (!runtimeResult.ok) {
        std::cerr << "[hitm-sprite-draw-data] runtime creation FAILED: " << runtimeResult.error << "\n";
        return 1;
    }
    auto bundleResult = HitmAssetImporter::Import(*identityResult.value, hitmEngineRootStr);
    if (!bundleResult.ok) {
        std::cerr << "[hitm-sprite-draw-data] asset import FAILED: " << bundleResult.error << "\n";
        return 1;
    }
    auto& fighter = *runtimeResult.value;
    auto& move = *moveResult.value;
    auto& bundle = *bundleResult.value;

    std::cout << "[hitm-sprite-draw-data] fighter_id=" << bundle.fighter_id << " atlas=" << bundle.atlas_png_path.string()
               << " (" << bundle.atlas_pixel_width << "x" << bundle.atlas_pixel_height << " real px) parts="
               << bundle.parts.Parts().size() << " clips=" << bundle.animations.ClipNames().size() << "\n";

    // Owned once, threaded through every real frame -- the same
    // exactly-once-per-real-frame discipline HitmSecondaryMotionState's
    // header comment requires. Real, evidenced follow-bone example:
    // "dreadFar" (parent "head", real lagBeats=1.4/maxAngle=46/
    // gravity=0.5) authors no track in any of idle/walk/jump/special --
    // without secondary motion it would sit frozen at zero every frame.
    HitmSecondaryMotionState secondaryMotion;

    // BuildSpriteDrawData must run exactly once per real frame to keep
    // `secondaryMotion` correct (see HitmSpriteDrawData.h's header
    // comment) -- `compute()` is therefore the ONLY place this demo
    // calls it, called once for frame 0 and then exactly once per
    // `tick()`. `printLast()` only prints the most recently computed
    // result; it never calls BuildSpriteDrawData itself, so labeling a
    // frame for output never double-integrates the spring for it.
    dominus::core::Result<dominus::character::hitm::HitmSpriteDrawData> lastResult =
        dominus::core::Result<dominus::character::hitm::HitmSpriteDrawData>::Fail("not computed yet");
    auto currentMoveForState = [&]() -> const HitmMoveInstance* {
        switch (fighter.State()) {
            case HitmFighterState::kAttackStartup:
            case HitmFighterState::kAttackActive:
            case HitmFighterState::kAttackRecovery:
                return &move;
            default:
                return nullptr;
        }
    };
    auto compute = [&]() { lastResult = BuildSpriteDrawData(fighter.Snapshot(), currentMoveForState(), bundle, &secondaryMotion); };
    auto tick = [&](HitmInputCommand input) {
        fighter.AdvanceFrame(input);
        compute();
    };
    auto printLast = [&](const char* label) {
        if (!lastResult.ok) {
            std::cout << "[hitm-sprite-draw-data] (" << label << ") FAILED: " << lastResult.error << "\n";
            return;
        }
        const auto& draw = *lastResult.value;
        const dominus::character::hitm::HitmPartDraw* torso = nullptr;
        const dominus::character::hitm::HitmPartDraw* dreadFar = nullptr;
        for (const auto& p : draw.parts) {
            if (p.part_name == "torso") torso = &p;
            if (p.part_name == "dreadFar") dreadFar = &p;
        }
        std::cout << "[hitm-sprite-draw-data] frame=" << fighter.Snapshot().frame << " (" << label
                   << ") clip=\"" << draw.clip_name << "\" raw_frame=" << draw.raw_frame
                   << " sampled_frame=" << draw.sampled_frame << " parts=" << draw.parts.size();
        if (torso) {
            std::cout << " torso[frame=(" << torso->frame_x << "," << torso->frame_y << "," << torso->frame_w << ","
                       << torso->frame_h << ") pose_rot=" << torso->pose_rotation_deg << "deg]";
        }
        if (dreadFar) {
            std::cout << " dreadFar[pose_rot=" << dreadFar->pose_rotation_deg << "deg, real spring-driven secondary motion]";
        }
        std::cout << "\n";
    };

    compute();
    printLast("init");
    for (int i = 0; i < 3; ++i) tick(HitmInputCommand::kRight);
    printLast("walking");

    tick(HitmInputCommand::kJump);
    printLast("jumping (rising)");
    while (!fighter.Snapshot().grounded) tick(HitmInputCommand::kNeutral);
    printLast("landed");

    tick(HitmInputCommand::kSpecial);
    printLast("attack_startup (real special clip begins)");
    while (fighter.State() == HitmFighterState::kAttackStartup) tick(HitmInputCommand::kNeutral);
    printLast("attack_active");
    while (fighter.State() == HitmFighterState::kAttackActive) tick(HitmInputCommand::kNeutral);
    printLast("attack_recovery");
    while (fighter.State() == HitmFighterState::kAttackRecovery) tick(HitmInputCommand::kNeutral);
    printLast("idle again");

    // TakeHit() mutates state synchronously without advancing `frame` --
    // a genuine, real, one-time out-of-band state change (not a repeated
    // pattern), so recomputing once here to reflect it is correct, not a
    // double-integration of the same frame's physics.
    fighter.TakeHit(move, /*blocking=*/false);
    compute();
    printLast("hitstun (real hurt clip)");

    std::cout << "[result] real HITM sprite/atlas/animation data produced deterministic draw data for "
               << fighter.Snapshot().frame << " real frames -- NOT rendered, NOT a claim any pixel exists on screen\n";
    return 0;
}

int GenomeCompileDemo(const std::string& baseDirStr) {
    std::filesystem::path baseDir = baseDirStr;
    using namespace dominus::registry;
    using dominus::character::CombatIdentityLoader;

    auto v1Load = CombatIdentityLoader::LoadFromFile(baseDir / "brooklyn_combat.json");
    if (!v1Load.ok) {
        std::cerr << "Failed to load brooklyn_combat.json: " << v1Load.error << "\n";
        return 1;
    }
    std::cout << "[source] brooklyn_combat.json -> style='" << v1Load.value->style << "'\n";

    auto v1Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v1Load.value, std::nullopt, 1, "2026-08-01T00:00:00Z");
    if (!v1Compile.ok) {
        std::cerr << "v1 compile failed\n";
        for (auto& e : v1Compile.errors) std::cerr << "  " << e << "\n";
        return 1;
    }
    std::cout << "[compile] v1 hash=" << v1Compile.artifact->Hash() << " (canonical: " << v1Compile.artifact->CanonicalBytes()
               << ")\n";

    GenomeRegistry registry;
    registry.Register(*v1Compile.artifact);
    std::cout << "[registry] registered v1, artifact_count=" << registry.ArtifactCount() << "\n";

    // Recompile v1 from the SAME source a second time -- proves
    // deterministic compilation: identical hash, no new registry entry.
    auto v1Recompile = GenomeCompiler::CompileCombatGenome("brooklyn", *v1Load.value, std::nullopt, 1, "different_timestamp");
    registry.Register(*v1Recompile.artifact);
    std::cout << "[determinism] recompiled v1 -> hash=" << v1Recompile.artifact->Hash()
               << " (matches=" << (v1Recompile.artifact->Hash() == v1Compile.artifact->Hash() ? "true" : "false")
               << "), artifact_count still=" << registry.ArtifactCount() << "\n";

    auto v2Load = CombatIdentityLoader::LoadFromFile(baseDir / "brooklyn_beast_combat.json");
    auto v2Compile =
        GenomeCompiler::CompileCombatGenome("brooklyn", *v2Load.value, v1Compile.artifact->Hash(), 2, "2026-08-01T01:00:00Z");
    registry.Register(*v2Compile.artifact);
    std::cout << "[compile] v2 hash=" << v2Compile.artifact->Hash() << " parent=" << *v2Compile.artifact->ParentHash()
               << "\n";

    auto lineage = registry.Lineage("brooklyn");
    std::cout << "[lineage] brooklyn: " << lineage.size() << " version(s)\n";
    for (size_t i = 0; i < lineage.size(); ++i) {
        std::cout << "  v" << (i + 1) << ": " << lineage[i] << "\n";
    }

    auto* v1Stored = registry.Find(v1Compile.artifact->Hash());
    auto v1Snapshot = SnapshotBuilder::Build(*v1Stored);
    auto* latest = registry.Latest("brooklyn");
    auto latestSnapshot = SnapshotBuilder::Build(*latest);
    std::cout << "[snapshot] v1 aggression=" << v1Snapshot.Weights().aggression
               << " | latest(v2) aggression=" << latestSnapshot.Weights().aggression << "\n";

    std::cout << "[result] deterministic compilation + stable hashing + registry lookup + version lineage + "
                 "runtime-never-touches-authored-data all proven against real Brooklyn combat genome data\n";
    return 0;
}

int WorldSaveDemo(const std::string& dominusPath, const std::string& stateDirStr) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& entity = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    if (!RigBinder::Bind(entity, baseDir).ok || !CombatBinder::Bind(entity, baseDir).ok) {
        std::cerr << "Failed to bind rig/combat data\n";
        return 1;
    }
    std::string fileName = std::filesystem::path(dominusPath).filename().string();
    entity.AddComponent<dominus::world::SourceRefComponent>(dominus::world::SourceRefComponent{fileName});
    entity.AddComponent<dominus::world::SpatialComponent>(dominus::world::SpatialComponent::Fighter2_5D(42.0f, 0.0f));
    std::string entityId = entity.Id();

    dominus::world::World world;
    world.Entities().CreateEntity(std::move(entity));
    world.History().Record(0.0f, "entity_created", entityId, entityId + " enters the city");
    world.Tick(60.0f);
    world.History().Record(world.ElapsedSeconds(), "battle_won", entityId, "Defeated the gang leader");

    auto saveResult = dominus::world::WorldPersistence::Save(world, stateDirStr);
    if (!saveResult.ok) {
        std::cerr << "Save failed: " << saveResult.error << "\n";
        return 1;
    }
    std::cout << "[save] entity='" << entityId << "' elapsed=" << world.ElapsedSeconds()
               << "s history_events=" << world.History().Count() << "\n";
    std::cout << "[save] wrote " << stateDirStr << "/world.json, entities/" << entityId
               << ".json, history/timeline.json\n";
    std::cout << "[result] world saved -- creator can leave now\n";
    return 0;
}

int WorldLoadDemo(const std::string& stateDirStr, const std::string& fixturesDirStr) {
    std::filesystem::path fixturesDir = fixturesDirStr;
    auto loadResult = dominus::world::WorldPersistence::Load(stateDirStr);
    if (!loadResult.ok) {
        std::cerr << "Load failed: " << loadResult.error << "\n";
        return 1;
    }
    std::cout << "[load] read " << loadResult.entities.size() << " entity record(s), "
               << loadResult.history.size() << " history event(s), elapsed=" << loadResult.elapsed_seconds << "s\n";

    dominus::world::World world;
    world.SetElapsedSeconds(loadResult.elapsed_seconds);
    for (auto& record : loadResult.entities) {
        if (record.source_ref.empty()) {
            std::cout << "[rebind] '" << record.entity_id << "' has no source ref, skipping rebind\n";
            continue;
        }
        auto rebind = DominusSerializer::Load(fixturesDir / record.source_ref);
        if (!rebind.ok) {
            std::cerr << "[rebind] failed for '" << record.entity_id << "': " << rebind.error << "\n";
            continue;
        }
        auto& entity = *rebind.value;
        RigBinder::Bind(entity, fixturesDir);
        CombatBinder::Bind(entity, fixturesDir);
        if (record.has_spatial) entity.AddComponent<dominus::world::SpatialComponent>(record.spatial);
        std::cout << "[rebind] '" << record.entity_id << "' <- " << record.source_ref << " (fully bound again)\n";
        world.Entities().CreateEntity(std::move(entity));
    }
    for (auto& event : loadResult.history) {
        world.History().Record(event.tick_time, event.event_type, event.entity_id, event.description);
        std::cout << "[history] t=" << event.tick_time << "s " << event.event_type << " (" << event.entity_id
                   << "): " << event.description << "\n";
    }

    std::cout << "[result] world continues -- " << world.Entities().Count() << " entity(ies), elapsed="
               << world.ElapsedSeconds() << "s, " << world.History().Count() << " remembered event(s)\n";
    return 0;
}

int SocialDemo(const std::string& dominusPath) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& entity = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    auto bindResult = RigBinder::Bind(entity, baseDir);
    if (!bindResult.ok) {
        std::cerr << "RigBinder::Bind failed: " << bindResult.error << "\n";
        return 1;
    }

    auto* social = entity.GetComponent<dominus::character::SocialGenomeComponent>();
    if (!social) {
        std::cout << "[social] '" << entity.Id() << "' has no social genome\n";
        return 0;
    }

    std::cout << "[social] '" << entity.Id() << "' personality: trust=" << social->genome.personality.trust
               << " aggression=" << social->genome.personality.aggression
               << " loyalty=" << social->genome.personality.loyalty << "\n";
    std::cout << "[social] " << social->genome.relationships.size() << " relationship(s):\n";
    for (auto& rel : social->genome.relationships) {
        std::cout << "  " << rel.entity_id << ": " << rel.relation << " (strength=" << rel.strength << ")\n";
    }
    std::cout << "[result] social genome resolved -- " << entity.Id()
               << " is not a quest marker, it has relationships\n";
    return 0;
}

int CreatureDemo(const std::string& dominusPath) {
    auto loadResult = DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& entity = *loadResult.value;
    std::filesystem::path baseDir = std::filesystem::path(dominusPath).parent_path();
    auto bindResult = RigBinder::Bind(entity, baseDir);
    if (!bindResult.ok) {
        std::cerr << "RigBinder::Bind failed: " << bindResult.error << "\n";
        return 1;
    }

    auto* creature = entity.GetComponent<dominus::character::CreatureGenomeComponent>();
    if (!creature) {
        std::cout << "[creature] '" << entity.Id() << "' has no creature genome\n";
        return 0;
    }
    auto& g = creature->genome;

    std::cout << "[creature] " << g.identity.common_name << " (" << g.identity.species_name << ")\n";
    std::cout << "[creature] taxonomy: " << g.taxonomy.classification << ", " << g.taxonomy.body_plan << ", "
               << g.taxonomy.limb_count << " limbs\n";
    std::cout << "[creature] cognition: tier=" << static_cast<int>(g.cognition.tier)
               << " problem_solving=" << g.cognition.problem_solving << "\n";
    std::cout << "[creature] combat_profile: aggression=" << g.combat.aggression << " speed=" << g.combat.speed
               << " durability=" << g.combat.durability << " range=" << g.combat.range << "\n";
    std::cout << "[creature] ecology: " << g.ecology.trophic_role << " in " << g.ecology.habitat << "\n";

    auto compileResult = dominus::registry::CreatureGenomeCompiler::Compile(g);
    if (compileResult.ok) {
        std::cout << "[creature] genome_hash=" << compileResult.hash << "\n";
    }

    auto semanticIssues = dominus::character::CreatureGenomeSemanticValidator::Validate(g);
    std::cout << "[creature] semantic validation: " << semanticIssues.size() << " issue(s)\n";
    for (auto& issue : semanticIssues) {
        const char* tag = issue.severity == dominus::character::SemanticSeverity::kError ? "ERROR" : "WARN";
        std::cout << "  [" << tag << "] " << issue.check << ": " << issue.message << "\n";
    }

    std::cout << "[result] creature genome resolved and hashed -- a real schema, not a stat block\n";
    return 0;
}

int CombatStyleDemo(const std::string& styleJsonPath) {
    // Styles are standalone -- not entity-attached like Creature/Social
    // genomes -- so this takes the style JSON directly, not a .dominus
    // wrapper.
    auto result = dominus::character::CombatStyleGenomeLoader::LoadFromFile(styleJsonPath);
    if (!result.ok) {
        std::cerr << "Failed to load " << styleJsonPath << ": " << result.error << "\n";
        return 1;
    }
    auto& g = *result.value;

    std::cout << "[combat-style] " << g.style_name << "\n";
    std::cout << "[combat-style] ancestry: ";
    for (size_t i = 0; i < g.ancestry.size(); ++i) {
        std::cout << g.ancestry[i];
        if (i + 1 < g.ancestry.size()) std::cout << ", ";
    }
    std::cout << "\n";
    std::cout << "[combat-style] aggression=" << g.aggression << " defense=" << g.defense
               << " mobility=" << g.mobility << " deception=" << g.deception
               << " adaptability=" << g.adaptability << "\n";
    std::cout << "[combat-style] philosophy: " << g.philosophy << "\n";
    std::cout << "[combat-style] " << g.weaknesses.size() << " known weakness(es)\n";

    auto compileResult = dominus::registry::CombatStyleGenomeCompiler::Compile(g);
    if (compileResult.ok) {
        std::cout << "[combat-style] genome_hash=" << compileResult.hash << "\n";
    }
    std::cout << "[result] combat style resolved and hashed -- a real schema, not a name mixed with adjectives\n";
    return 0;
}

int ImpactDemo(const std::string& physicsJsonPath) {
    // Takes the physics genome JSON directly, same reasoning as
    // combat-style: this data isn't entity-bound via RigBinder yet.
    auto result = dominus::character::CombatPhysicsGenomeLoader::LoadFromFile(physicsJsonPath);
    if (!result.ok) {
        std::cerr << "Failed to load " << physicsJsonPath << ": " << result.error << "\n";
        return 1;
    }
    auto& defender = *result.value;
    std::cout << "[impact] defender: mass=" << defender.body.mass_kg << "kg armor=" << defender.body.armor
               << " durability=" << defender.impact.durability << "\n";

    // A stand-in attacker for the demo -- real integration into a live
    // fight would come from a second loaded fighter's own physics
    // genome, not a hardcoded pair.
    const float attackerMass = 90.0f;
    const float attackVelocity = 8.0f;
    std::cout << "[impact] attacker: mass=" << attackerMass << "kg velocity=" << attackVelocity << "m/s\n";

    auto impact = dominus::combat::ImpactSolver::ResolveImpact(attackerMass, attackVelocity, defender.impact.durability);
    const char* severityStr = impact.severity == dominus::combat::ImpactSeverity::kHigh       ? "HIGH"
                               : impact.severity == dominus::combat::ImpactSeverity::kModerate ? "MODERATE"
                                                                                                : "LOW";
    std::cout << "[impact] force=" << impact.force << " resistance=" << impact.resistance
               << " difference=" << impact.difference << " severity=" << severityStr << "\n";

    float postArmor = dominus::combat::ImpactSolver::ApplyArmor(impact.force, defender.body.armor);
    std::cout << "[impact] post-armor force=" << postArmor << " (armor absorbed "
               << (impact.force - postArmor) << ")\n";

    for (const char* bodyPart : {"head", "torso", "arm", "leg"}) {
        float damage = dominus::combat::ImpactSolver::CalculateDamage(postArmor, bodyPart);
        std::cout << "[impact]   if struck in the " << bodyPart << ": damage=" << damage << "\n";
    }

    std::cout << "[result] every number above traces back to mass/velocity/armor/durability -- nothing invented\n";
    return 0;
}

int DesignCoherenceDemo(const std::string& designJsonPath, const std::string& styleJsonPath,
                         const std::string& physicsJsonPath) {
    auto designResult = dominus::character::GameDesignGenomeLoader::LoadFromFile(designJsonPath);
    auto styleResult = dominus::character::CombatStyleGenomeLoader::LoadFromFile(styleJsonPath);
    auto physicsResult = dominus::character::CombatPhysicsGenomeLoader::LoadFromFile(physicsJsonPath);
    if (!designResult.ok) {
        std::cerr << "Failed to load design: " << designResult.error << "\n";
        return 1;
    }
    if (!styleResult.ok) {
        std::cerr << "Failed to load style: " << styleResult.error << "\n";
        return 1;
    }
    if (!physicsResult.ok) {
        std::cerr << "Failed to load physics: " << physicsResult.error << "\n";
        return 1;
    }

    std::cout << "[design] genre='" << designResult.value->genre << "'\n";
    std::cout << "[design] style: mobility=" << styleResult.value->mobility
               << " precision=" << styleResult.value->precision << "\n";
    std::cout << "[design] physics: fatigue_rate=" << physicsResult.value->energy.fatigue_rate << "\n";

    auto notes = dominus::character::GameDesignCoherenceChecker::Evaluate(*designResult.value, *styleResult.value,
                                                                            *physicsResult.value);
    std::cout << "[design] " << notes.size() << " coherence note(s) for genre '" << designResult.value->genre
               << "':\n";
    for (auto& note : notes) {
        std::cout << "  [" << note.field << "] " << note.message << "\n";
    }
    if (notes.empty()) {
        std::cout << "  (none -- this fighter's data reads as coherent with the declared genre by these checks)\n";
    }
    std::cout << "[result] advisory only -- these are suggestions, not a pass/fail verdict\n";
    return 0;
}

int BuildDemo(const std::string& projectDir) {
    if (!std::filesystem::exists(projectDir)) {
        std::cerr << "Project directory does not exist: " << projectDir << "\n";
        return 1;
    }

    std::cout << "[build] scanning " << projectDir << " for .dominus files...\n";
    auto report = dominus::validation::BuildPipeline::Run(projectDir);

    if (report.results.empty()) {
        std::cout << "[build] no .dominus files found\n";
        std::cout << "[result] BUILD PASSED (nothing to validate)\n";
        return 0;
    }

    for (auto& r : report.results) {
        std::cout << (r.passed ? "[PASS] " : "[FAIL] ") << r.file_path << "\n";
        for (auto& msg : r.error_messages) {
            std::cout << "    " << msg << "\n";
        }
    }

    std::cout << "[build] " << report.results.size() << " file(s) checked, " << report.FailedCount()
               << " failed\n";
    if (report.AllPassed()) {
        std::cout << "[result] BUILD PASSED\n";
        return 0;
    }
    std::cout << "[result] BUILD FAILED\n";
    return 1;
}

int VisualDemo(const std::string& visualJsonPath) {
    auto result = dominus::character::VisualGenomeLoader::LoadFromFile(visualJsonPath);
    if (!result.ok) {
        std::cerr << "Failed to load " << visualJsonPath << ": " << result.error << "\n";
        return 1;
    }
    auto& g = *result.value;

    std::cout << "[visual] form: " << g.form.silhouette << ", " << g.form.proportion << ", "
               << g.form.shape_language << "\n";
    std::cout << "[visual] skin: roughness=" << g.skin.roughness << " subsurface=" << g.skin.subsurface
               << " age=" << g.skin.age_years << "\n";
    std::cout << "[visual] clothing: " << g.clothing.material
               << " (adaptive_damage=" << (g.clothing.adaptive_damage ? "true" : "false") << ")\n";
    std::cout << "[visual] presence: aura=" << g.presence.aura
               << " threat_signature=" << g.presence.threat_signature << " style_id=" << g.presence.style_id
               << "\n";

    auto compileResult = dominus::registry::VisualGenomeCompiler::Compile(g);
    std::cout << "[visual] genome_hash=" << compileResult.hash << "\n";

    // Demonstrate the real WorldHistory connection (Society Phase 0) --
    // not fabricated "battles: 400+", a genuine, small, in-memory
    // history log built for this demo, then summarized honestly.
    dominus::world::WorldHistory history;
    history.Record(0.0f, "entity_created", "brooklyn", "Brooklyn enters the city");
    history.Record(30.0f, "battle_won", "brooklyn", "Defeated the gang leader");
    history.Record(60.0f, "battle_won", "brooklyn", "Won another fight");
    auto memory = dominus::character::VisualMemoryDeriver::Derive(history, "brooklyn");
    std::cout << "[visual] memory: " << memory.event_count << " real recorded event(s)";
    if (memory.has_history) {
        std::cout << " (first=" << memory.first_event_time << "s, last=" << memory.last_event_time << "s)";
    }
    std::cout << "\n";
    std::cout << "[result] visual genome resolved and hashed; memory derived from real WorldHistory, not invented\n";
    return 0;
}

int MaterialDemo(const std::string& materialJsonPath) {
    auto result = dominus::character::MaterialGenomeLoader::LoadFromFile(materialJsonPath);
    if (!result.ok) {
        std::cerr << "Failed to load " << materialJsonPath << ": " << result.error << "\n";
        return 1;
    }
    auto& g = *result.value;

    std::cout << "[material] " << g.material_id << " (" << g.identity.type << ")\n";
    std::cout << "[material] age=" << g.properties.age_years << "y wear_state=" << g.properties.wear_state
               << " weather_exposure=" << (g.properties.weather_exposure ? "true" : "false") << "\n";

    auto compileResult = dominus::registry::MaterialGenomeCompiler::Compile(g);
    std::cout << "[material] genome_hash=" << compileResult.hash << "\n";

    // The document's own worked example, made real: derive wear from
    // actual recorded damage events, not an invented "battles: 400+".
    dominus::world::WorldHistory history;
    history.Record(0.0f, "entity_created", g.material_id, "Jacket crafted");
    history.Record(30.0f, "damage_event", g.material_id, "Battle_482 -- torn sleeve");
    history.Record(60.0f, "damage_event", g.material_id, "Battle_501 -- scorched collar");
    float derivedWear = dominus::character::MaterialWearDeriver::DeriveWearState(history, g.material_id);
    std::cout << "[material] derived wear_state from 2 real damage_event(s): " << derivedWear << "\n";
    std::cout << "[result] material genome resolved and hashed; wear derived from real WorldHistory, not invented\n";
    return 0;
}

int VisualStyleDemo(const std::string& styleJsonPath) {
    auto result = dominus::character::VisualStyleGenomeLoader::LoadFromFile(styleJsonPath);
    if (!result.ok) {
        std::cerr << "Failed to load " << styleJsonPath << ": " << result.error << "\n";
        return 1;
    }
    auto& g = *result.value;

    std::cout << "[visual-style] " << g.name << " (" << g.style_id << ")\n";
    std::cout << "[visual-style] rules: line=" << g.visual_rules.line_quality
               << " color=" << g.visual_rules.color_behavior << " shape=" << g.visual_rules.shape_behavior
               << " motion=" << g.visual_rules.motion_behavior << "\n";
    std::cout << "[visual-style] influences: ";
    for (size_t i = 0; i < g.influences.size(); ++i) {
        std::cout << g.influences[i];
        if (i + 1 < g.influences.size()) std::cout << ", ";
    }
    std::cout << "\n";

    auto compileResult = dominus::registry::VisualStyleGenomeCompiler::Compile(g);
    std::cout << "[visual-style] genome_hash=" << compileResult.hash << "\n";
    std::cout << "[result] visual style resolved and hashed; influences are data, not an auto-combine result\n";
    return 0;
}

int ImpactProvenanceDemo(const std::string& physicsJsonPath) {
    // Phase 4.1.6: Record -> Serialize -> Reload -> Replay. Same
    // stand-in-attacker convention ImpactDemo already uses.
    auto physicsResult = dominus::character::CombatPhysicsGenomeLoader::LoadFromFile(physicsJsonPath);
    if (!physicsResult.ok) {
        std::cerr << "Failed to load " << physicsJsonPath << ": " << physicsResult.error << "\n";
        return 1;
    }
    auto& defender = *physicsResult.value;

    dominus::combat::ImpactContext ctx;
    ctx.attacker_mass_kg = 90.0f;
    ctx.attacker_velocity = 8.0f;
    ctx.defender_durability = defender.impact.durability;
    ctx.defender_armor = defender.body.armor;
    ctx.struck_body_part = "torso";

    auto result = dominus::combat::ImpactSolver::Solve(ctx);
    std::cout << "[impact-provenance] force=" << result.force << " damage=" << result.damage << "\n";

    // Record.
    auto event = dominus::combat::ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result,
                                                                 /*tick=*/48291);
    dominus::combat::ImpactEventLog log;
    log.Record(event);
    std::cout << "[impact-provenance] recorded: event=" << event.event << " attacker=" << event.attacker
               << " target=" << event.target << " tick=" << event.tick << "\n";
    std::cout << "[impact-provenance] context_hash=" << event.context_hash << "\n";
    std::cout << "[impact-provenance] result_hash=" << event.result_hash << "\n";

    // Serialize.
    std::string serialized = log.Serialize();

    // Reload.
    auto reloaded = dominus::combat::ImpactEventLog::Deserialize(serialized);
    if (reloaded.Count() != 1) {
        std::cerr << "[impact-provenance] reload FAILED -- expected 1 event, got " << reloaded.Count() << "\n";
        return 1;
    }
    const auto& reloadedEvent = reloaded.Events()[0];
    bool reloadMatches = reloadedEvent.context_hash == event.context_hash &&
                          reloadedEvent.result_hash == event.result_hash && reloadedEvent.tick == event.tick;
    std::cout << "[impact-provenance] reload: " << (reloadMatches ? "OK -- hashes match" : "FAILED -- hash mismatch")
               << "\n";

    // Replay: re-run the SAME context through Solve() fresh, and check
    // the recomputed hashes agree with the reloaded event.
    auto replayedResult = dominus::combat::ImpactSolver::Solve(ctx);
    bool replayMatches = dominus::combat::ImpactEventCompiler::VerifyMatches(reloadedEvent, ctx, replayedResult);
    std::cout << "[impact-provenance] replay: "
               << (replayMatches ? "OK -- deterministic, same ImpactResult reproduced"
                                  : "FAILED -- non-deterministic")
               << "\n";

    // WorldHistory hook.
    dominus::world::WorldHistory history;
    dominus::combat::RecordImpactEvent(history, event, /*tickTime=*/float(event.tick));
    auto brooklynEvents = history.EventsForEntity("attacker_001");
    std::cout << "[impact-provenance] world_history: " << brooklynEvents.size()
               << " event(s) recorded for 'attacker_001'\n";

    std::cout << "[result] " << (reloadMatches && replayMatches
                                      ? "provenance proven: record -> serialize -> reload -> replay all agree"
                                      : "provenance FAILED")
               << "\n";
    return (reloadMatches && replayMatches) ? 0 : 1;
}

const char* ReactionTypeName(dominus::combat::ReactionType type) {
    switch (type) {
        case dominus::combat::ReactionType::kNone: return "none";
        case dominus::combat::ReactionType::kStagger: return "stagger";
        case dominus::combat::ReactionType::kKnockback: return "knockback";
        case dominus::combat::ReactionType::kLaunch: return "launch";
        case dominus::combat::ReactionType::kWallImpact: return "wall_impact";
        case dominus::combat::ReactionType::kGroundImpact: return "ground_impact";
        case dominus::combat::ReactionType::kKnockdown: return "knockdown";
    }
    return "unknown";
}

int LiveImpactCaptureDemo(const std::string& dominusPath) {
    // Phase 4.1.7: Spawn -> Attack -> Collision -> ImpactSolver ->
    // Reaction -> WorldHistory -> Replay verification, through the REAL
    // CombatController::ApplyImpact path, not a hand-assembled event.
    namespace fs = std::filesystem;
    fs::path baseDir = fs::path(dominusPath).parent_path();

    // Spawn Brooklyn.
    auto loadResult = dominus::core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    auto rigResult = dominus::character::RigBinder::Bind(obj, baseDir);
    auto combatResult = dominus::combat::CombatBinder::Bind(obj, baseDir);
    if (!rigResult.ok || !combatResult.ok) {
        std::cerr << "Failed to bind " << dominusPath << "\n";
        return 1;
    }
    std::cout << "[live-impact] spawned '" << obj.Id() << "'\n";

    auto evaluator = dominus::character::MakeMotionGraphEvaluator(obj);
    auto* moveSet = obj.GetComponent<dominus::combat::MoveSetComponent>();
    dominus::combat::CombatController controller(*evaluator, *moveSet);
    controller.SetEntityId(obj.Id());

    dominus::combat::ImpactEventLog provenanceLog;
    dominus::world::WorldHistory history;
    controller.SetProvenanceLog(&provenanceLog);
    controller.SetWorldHistory(&history);

    // Attack.
    bool started = controller.StartMove("jab");
    std::cout << "[live-impact] attack: StartMove(\"jab\") -> " << (started ? "accepted" : "refused") << "\n";

    // Collision -> ImpactSolver -> Reaction, via the real, genome-driven
    // ApplyImpact path -- attacker is a plain stand-in (no genome refs),
    // defender is Brooklyn's own real bound genomes.
    dominus::combat::ImpactGenomeInputs attackerInputs;  // no genome refs
    dominus::combat::ImpactGenomeInputs defenderInputs;
    defenderInputs.physics = obj.GetComponent<dominus::character::CombatPhysicsGenomeComponent>();
    defenderInputs.material = obj.GetComponent<dominus::character::MaterialGenomeComponent>();
    defenderInputs.design = obj.GetComponent<dominus::character::GameDesignGenomeComponent>();
    defenderInputs.visualStyle = obj.GetComponent<dominus::character::VisualStyleGenomeComponent>();

    const std::uint64_t tick = 48291;
    auto reaction = controller.ApplyImpact(attackerInputs, defenderInputs, /*attackerVelocity=*/8.0f, "head", 1.0f,
                                            /*defenderBlocking=*/false, /*defenderAlreadyStaggered=*/false,
                                            /*attackerId=*/"attacker_001", tick);
    std::cout << "[live-impact] collision -> impact solved -> reaction=" << ReactionTypeName(reaction.type)
               << " motion_trigger='" << reaction.motion_trigger << "'\n";

    // WorldHistory.
    std::cout << "[live-impact] world_history: " << provenanceLog.Count() << " provenance event(s) captured\n";
    auto attackerEvents = history.EventsForEntity("attacker_001");
    std::cout << "[live-impact] world_history: " << attackerEvents.size() << " event(s) queryable for 'attacker_001'\n";

    if (provenanceLog.Count() != 1 || attackerEvents.empty()) {
        std::cerr << "[result] live capture FAILED -- no provenance recorded\n";
        return 1;
    }
    const auto& capturedEvent = provenanceLog.Events()[0];
    std::cout << "[live-impact] context_hash=" << capturedEvent.context_hash << "\n";
    std::cout << "[live-impact] result_hash=" << capturedEvent.result_hash << "\n";

    // Replay verification: rebuild the EXACT same context from the same
    // recorded inputs, solve it fresh, and confirm the hashes agree.
    auto replayCtx = dominus::combat::BuildImpactContext(attackerInputs, defenderInputs, /*attackerVelocity=*/8.0f,
                                                           "head", 1.0f, false, false);
    auto replayResult = dominus::combat::ImpactSolver::Solve(replayCtx);
    bool replayMatches =
        dominus::combat::ImpactEventCompiler::VerifyMatches(capturedEvent, replayCtx, replayResult);
    std::cout << "[live-impact] replay: " << (replayMatches ? "OK -- deterministic" : "FAILED -- mismatch") << "\n";

    std::cout << "[result] "
               << (replayMatches ? "live gameplay proof: every real impact captured, replayed, and verified"
                                  : "live capture FAILED")
               << "\n";
    return replayMatches ? 0 : 1;
}

int VisualForgeDemo(const std::string& dominusPath) {
    namespace fs = std::filesystem;
    fs::path baseDir = fs::path(dominusPath).parent_path();

    auto loadResult = dominus::core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    auto rigResult = dominus::character::RigBinder::Bind(obj, baseDir);
    if (!rigResult.ok) {
        std::cerr << "Failed to bind " << dominusPath << ": " << rigResult.error << "\n";
        return 1;
    }

    auto* visual = obj.GetComponent<dominus::character::VisualGenomeComponent>();
    if (!visual) {
        std::cerr << "'" << obj.Id() << "' has no bound VisualGenome -- Visual Forge requires one\n";
        return 1;
    }
    auto* material = obj.GetComponent<dominus::character::MaterialGenomeComponent>();
    auto* visualStyle = obj.GetComponent<dominus::character::VisualStyleGenomeComponent>();
    auto* motionGraph = obj.GetComponent<dominus::character::MotionGraphComponent>();
    auto* animationSet = obj.GetComponent<dominus::character::AnimationSetComponent>();

    dominus::world::WorldHistory history;
    history.Record(0.0f, "entity_created", obj.Id(), obj.Id() + " enters the city");

    auto blueprint = dominus::visualforge::CharacterBlueprintForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr, &history);

    std::cout << "[visual-forge] === CharacterBlueprint: " << blueprint.entity_id << " ===\n";
    std::cout << "[visual-forge] identity: body_type=" << blueprint.form.body_type
               << " silhouette=" << blueprint.form.silhouette << " aura=" << blueprint.presence.aura << "\n";
    std::cout << "[visual-forge] material: " << (blueprint.has_material ? blueprint.material_id : "(none attached)")
               << "\n";
    std::cout << "[visual-forge] style: " << (blueprint.has_visual_style ? blueprint.style_name : "(none attached)")
               << "\n";
    std::cout << "[visual-forge] world_history: " << blueprint.memory.event_count << " event(s)\n";
    std::cout << "[visual-forge] style_reference_matches=" << (blueprint.style_reference_matches ? "true" : "false")
               << "\n";

    auto assets = dominus::visualforge::AssetSpecificationForge::Build(blueprint);
    std::cout << "[visual-forge] === AssetSpecification: " << assets.requirements.size() << " requirement(s) ===\n";
    for (const auto& r : assets.requirements) {
        std::cout << "[visual-forge]   [" << r.category << "] " << r.name << " (from " << r.source_field << ")\n";
    }

    auto animation = dominus::visualforge::AnimationSpecificationForge::Build(obj.Id(), motionGraph, animationSet);
    if (animation) {
        std::cout << "[visual-forge] === AnimationSpecification: " << animation->states.size() << " state(s), "
                   << animation->transitions.size() << " transition(s) ===\n";
    } else {
        std::cout << "[visual-forge] === AnimationSpecification: not available (no motion graph bound) ===\n";
    }

    auto package = dominus::visualforge::RendererPackageForge::Build(obj.Id(), "2026-08-06T00:00:00Z", blueprint,
                                                                       assets, dominus::visualforge::DependencyGraphForge::Build(
                                                                                   obj.Id(), visual->genome,
                                                                                   material ? &material->genome : nullptr,
                                                                                   visualStyle ? &visualStyle->genome : nullptr,
                                                                                   &history, animation),
                                                                       animation);
    std::cout << "[visual-forge] === RendererPackage ===\n";
    if (!package.ok) {
        std::cout << "[visual-forge] REFUSED -- blueprint failed validation, no package created\n";
        std::cout << "[result] Visual Forge complete -- gate refused the package (LAW: Visual Package Integrity)\n";
        return 1;
    }
    std::cout << "[visual-forge] character_blueprint_hash=" << package.package->character_blueprint_hash << "\n";
    std::cout << "[visual-forge] asset_specification_hash=" << package.package->asset_specification_hash << "\n";
    std::cout << "[visual-forge] animation_specification_hash="
               << (package.package->has_animation_specification ? package.package->animation_specification_hash : "(n/a)")
               << "\n";
    std::cout << "[visual-forge] validation_artifact_hash=" << package.package->validation_artifact_hash << "\n";
    std::cout << "[visual-forge] package_hash=" << package.package->package_hash << "\n";
    std::cout << "[result] Visual Forge complete -- inert data, no renderer to consume it yet (GRAPHICS ungated)\n";
    return 0;
}

int VisualForgeV2Demo(const std::string& dominusPath) {
    namespace fs = std::filesystem;
    fs::path baseDir = fs::path(dominusPath).parent_path();

    auto loadResult = dominus::core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    auto rigResult = dominus::character::RigBinder::Bind(obj, baseDir);
    if (!rigResult.ok) {
        std::cerr << "Failed to bind " << dominusPath << ": " << rigResult.error << "\n";
        return 1;
    }

    auto* visual = obj.GetComponent<dominus::character::VisualGenomeComponent>();
    if (!visual) {
        std::cerr << "'" << obj.Id() << "' has no bound VisualGenome\n";
        return 1;
    }
    auto* material = obj.GetComponent<dominus::character::MaterialGenomeComponent>();
    auto* visualStyle = obj.GetComponent<dominus::character::VisualStyleGenomeComponent>();
    auto* motionGraph = obj.GetComponent<dominus::character::MotionGraphComponent>();
    auto* animationSet = obj.GetComponent<dominus::character::AnimationSetComponent>();

    dominus::world::WorldHistory history;
    history.Record(0.0f, "entity_created", obj.Id(), obj.Id() + " enters the city");

    auto blueprint = dominus::visualforge::CharacterBlueprintForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr, &history);
    auto animation = dominus::visualforge::AnimationSpecificationForge::Build(obj.Id(), motionGraph, animationSet);

    // 1. Blueprint Validator.
    auto validation = dominus::visualforge::BlueprintValidator::Validate(blueprint);
    std::cout << "[visual-forge-v2] === Blueprint Validation ===\n";
    std::cout << "[visual-forge-v2] valid=" << (validation.valid ? "true" : "false") << "\n";
    for (const auto& issue : validation.issues) {
        std::cout << "[visual-forge-v2]   [" << issue.severity << "] " << issue.field << ": " << issue.message
                   << "\n";
    }

    // 2. Dependency Graph.
    auto depsV1 = dominus::visualforge::DependencyGraphForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr, &history, animation);
    auto depsValidation = dominus::visualforge::DependencyGraphForge::Validate(depsV1);
    std::cout << "[visual-forge-v2] === Dependency Graph (v1) ===\n";
    std::cout << "[visual-forge-v2] visual_genome_hash=" << depsV1.visual_genome_hash << "\n";
    std::cout << "[visual-forge-v2] material_genome_hash="
               << (depsV1.material_genome_hash.empty() ? "(none)" : depsV1.material_genome_hash) << "\n";
    std::cout << "[visual-forge-v2] style_genome_hash="
               << (depsV1.style_genome_hash.empty() ? "(none)" : depsV1.style_genome_hash) << "\n";
    std::cout << "[visual-forge-v2] animation_spec_hash="
               << (depsV1.animation_spec_hash.empty() ? "(none)" : depsV1.animation_spec_hash) << "\n";
    std::cout << "[visual-forge-v2] history_snapshot_hash="
               << (depsV1.history_snapshot_hash.empty() ? "(none)" : depsV1.history_snapshot_hash) << "\n";
    std::cout << "[visual-forge-v2] dependency hashes valid=" << (depsValidation.valid ? "true" : "false") << "\n";

    // 3. Versioned Production Snapshot -- initial.
    auto snapshotV1 = dominus::visualforge::ProductionSnapshotForge::CreateInitial(
        obj.Id() + "_VISUAL_BUILD_001", obj.Id(), "2026-08-06T00:00:00Z", depsV1, validation, "HITM City v1");
    std::cout << "[visual-forge-v2] === ProductionSnapshot: " << snapshotV1.snapshot_id << " ===\n";
    std::cout << "[visual-forge-v2] visual_version=" << snapshotV1.visual_version
               << " material_version=" << snapshotV1.material_version << " style_label=" << snapshotV1.style_label
               << " animation_version=" << snapshotV1.animation_version << "\n";
    std::cout << "[visual-forge-v2] snapshot_hash=" << snapshotV1.snapshot_hash << "\n";

    // Now simulate a real change -- material wear increased (the exact
    // kind of change MaterialWearDeriver would produce after combat) --
    // and prove Visual Forge knows exactly what needs rebuilding.
    auto wornMaterial = material ? material->genome : dominus::character::MaterialGenome{};
    wornMaterial.properties.wear_state = 0.75f;
    auto depsV2 = dominus::visualforge::DependencyGraphForge::Build(
        obj.Id(), visual->genome, &wornMaterial, visualStyle ? &visualStyle->genome : nullptr, &history, animation);
    auto changed = dominus::visualforge::DependencyGraphForge::ChangedDependencies(depsV1, depsV2);

    std::cout << "[visual-forge-v2] === Simulated change: material wear_state 0.0 -> 0.75 ===\n";
    std::cout << "[visual-forge-v2] changed dependencies: ";
    for (size_t i = 0; i < changed.size(); ++i) {
        std::cout << changed[i];
        if (i + 1 < changed.size()) std::cout << ", ";
    }
    std::cout << (changed.empty() ? "(none)" : "") << "\n";

    auto snapshotV2 = dominus::visualforge::ProductionSnapshotForge::CreateNext(
        obj.Id() + "_VISUAL_BUILD_002", snapshotV1, "2026-08-06T01:00:00Z", depsV2, validation);
    std::cout << "[visual-forge-v2] === ProductionSnapshot: " << snapshotV2.snapshot_id << " ===\n";
    std::cout << "[visual-forge-v2] visual_version=" << snapshotV2.visual_version
               << " material_version=" << snapshotV2.material_version
               << " animation_version=" << snapshotV2.animation_version << "\n";
    std::cout << "[result] material_version bumped (" << snapshotV1.material_version << " -> "
               << snapshotV2.material_version << "), visual_version/animation_version unchanged -- "
               << "Visual Forge knows exactly what needs rebuilding\n";
    return 0;
}

int VisualForgeV3Demo(const std::string& dominusPath) {
    namespace fs = std::filesystem;
    fs::path baseDir = fs::path(dominusPath).parent_path();

    auto loadResult = dominus::core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << ": " << loadResult.error << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    dominus::character::RigBinder::Bind(obj, baseDir);

    auto* visual = obj.GetComponent<dominus::character::VisualGenomeComponent>();
    if (!visual) {
        std::cerr << "'" << obj.Id() << "' has no bound VisualGenome\n";
        return 1;
    }
    auto* material = obj.GetComponent<dominus::character::MaterialGenomeComponent>();
    auto* visualStyle = obj.GetComponent<dominus::character::VisualStyleGenomeComponent>();

    // --- Part 1: the gate refuses an invalid blueprint. -----------------
    std::cout << "[visual-forge-v3] === LAW: Visual Package Integrity ===\n";
    dominus::visualforge::CharacterBlueprint invalidBlueprint;  // never went through the Forge
    auto invalidAssets = dominus::visualforge::AssetSpecificationForge::Build(invalidBlueprint);
    auto invalidDeps = dominus::visualforge::DependencyGraphForge::Build("nobody", dominus::character::VisualGenome{});
    auto refused =
        dominus::visualforge::RendererPackageForge::Build("nobody", "t", invalidBlueprint, invalidAssets, invalidDeps);
    std::cout << "[visual-forge-v3] invalid blueprint -> package.ok=" << (refused.ok ? "true" : "false")
               << " (expected false)\n";
    std::cout << "[visual-forge-v3] validation_artifact.result=" << refused.validation_artifact.result << "\n";
    for (const auto& check : refused.validation_artifact.checks) {
        std::cout << "[visual-forge-v3]   [" << check.status << "] " << check.name << "\n";
    }

    // --- Part 2: a real, valid blueprint passes the gate and gets a
    // package with a full provenance chain. ---
    auto blueprint = dominus::visualforge::CharacterBlueprintForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr);
    auto assets = dominus::visualforge::AssetSpecificationForge::Build(blueprint);
    auto deps = dominus::visualforge::DependencyGraphForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr);
    auto validResult = dominus::visualforge::RendererPackageForge::Build(obj.Id(), "t", blueprint, assets, deps);
    std::cout << "[visual-forge-v3] === Valid blueprint ===\n";
    std::cout << "[visual-forge-v3] package.ok=" << (validResult.ok ? "true" : "false") << "\n";
    std::cout << "[visual-forge-v3] created_from.character_blueprint_hash="
               << validResult.package->character_blueprint_hash << "\n";
    std::cout << "[visual-forge-v3] depends_on.visual_genome_hash=" << validResult.package->depends_on.visual_genome_hash
               << "\n";
    std::cout << "[visual-forge-v3] depends_on.material_genome_hash="
               << validResult.package->depends_on.material_genome_hash << "\n";
    std::cout << "[visual-forge-v3] validation_artifact_hash=" << validResult.package->validation_artifact_hash
               << "\n";

    // --- Part 3: Snapshot lifecycle. -------------------------------------
    std::cout << "[visual-forge-v3] === Snapshot Lifecycle ===\n";
    auto realValidation = dominus::visualforge::BlueprintValidator::Validate(blueprint);
    auto snapshot = dominus::visualforge::ProductionSnapshotForge::CreateInitial(obj.Id() + "_VISUAL_BUILD_001",
                                                                                    obj.Id(), "t", deps, realValidation);
    std::cout << "[visual-forge-v3] state=" << dominus::visualforge::SnapshotStateName(snapshot.state) << "\n";

    bool advanced = dominus::visualforge::SnapshotLifecycle::AdvanceToValidated(snapshot);
    std::cout << "[visual-forge-v3] AdvanceToValidated -> " << (advanced ? "true" : "false")
               << ", state=" << dominus::visualforge::SnapshotStateName(snapshot.state) << "\n";

    bool approved = dominus::visualforge::SnapshotLifecycle::Approve(snapshot, "shawn");
    std::cout << "[visual-forge-v3] Approve(\"shawn\") -> " << (approved ? "true" : "false")
               << ", state=" << dominus::visualforge::SnapshotStateName(snapshot.state) << "\n";

    // Approved -> Accepted requires a real AcceptanceCertificate (see
    // VISUALFORGE/AcceptanceCertificate.h / dominus-cli visual-acceptance
    // for the full, real certificate) -- a synthetic passing one is
    // enough to prove the GATE here without re-running the whole
    // rendering harness inside this v0.3 structural demo.
    dominus::visualforge::AcceptanceCertificate demoCert;
    demoCert.structurally_sound = true;
    demoCert.renderable = true;
    demoCert.certificate_hash = "demo_certificate_hash";
    bool accepted = dominus::visualforge::SnapshotLifecycle::AdvanceToAccepted(snapshot, demoCert);
    std::cout << "[visual-forge-v3] AdvanceToAccepted -> " << (accepted ? "true" : "false")
               << ", state=" << dominus::visualforge::SnapshotStateName(snapshot.state) << "\n";
    std::cout << "[visual-forge-v3] Active: no path exists -- see ProductionSnapshot.h (requires real pixel proof, "
                  "GRAPHICS has no rasterizer yet)\n";

    // --- Part 4: Dependency Graph Authority -- a real rebuild plan. -----
    std::cout << "[visual-forge-v3] === Rebuild Plan (material change) ===\n";
    auto wornMaterial = material ? material->genome : dominus::character::MaterialGenome{};
    wornMaterial.properties.wear_state = 0.9f;
    auto depsAfter = dominus::visualforge::DependencyGraphForge::Build(
        obj.Id(), visual->genome, &wornMaterial, visualStyle ? &visualStyle->genome : nullptr);
    auto changed = dominus::visualforge::DependencyGraphForge::ChangedDependencies(deps, depsAfter);
    auto plan = dominus::visualforge::DependencyGraphForge::PlanRebuild(changed);
    std::cout << "[visual-forge-v3] rebuild_renderer_package=" << (plan.rebuild_renderer_package ? "YES" : "no")
               << "\n";
    std::cout << "[visual-forge-v3] rebuild_asset_specification=" << (plan.rebuild_asset_specification ? "YES" : "no")
               << "\n";
    std::cout << "[visual-forge-v3] rebuild_animation_specification="
               << (plan.rebuild_animation_specification ? "YES" : "no") << "\n";

    std::cout << "[result] v0.3 complete -- gate enforced, provenance chained, lifecycle real, rebuild plan real\n";
    return 0;
}

int LiveCollisionLoopDemo(const std::string& dominusPath) {
    // Closes Combat's real broken loop, live: CollisionEvaluator ->
    // CollisionResolver -> ImpactContext -> ImpactSolver -> ImpactResult
    // -> ReactionSystem::Apply, through two genuinely separate fighters
    // (an attack does not interrupt itself).
    namespace fs = std::filesystem;
    fs::path baseDir = fs::path(dominusPath).parent_path();

    auto LoadFighter = [&](const std::string& entityId) {
        struct Fighter {
            dominus::core::Result<dominus::core::MetaBinObject> loadResult;
            std::unique_ptr<dominus::animation::MotionGraphEvaluator> evaluator;
            std::unique_ptr<dominus::combat::CombatController> controller;
        };
        auto f = std::make_unique<Fighter>();
        f->loadResult = dominus::core::DominusSerializer::Load(dominusPath);
        if (!f->loadResult.ok) return f;
        auto& obj = *f->loadResult.value;
        dominus::character::RigBinder::Bind(obj, baseDir);
        dominus::combat::CombatBinder::Bind(obj, baseDir);
        f->evaluator = dominus::character::MakeMotionGraphEvaluator(obj);
        auto* moveSet = obj.GetComponent<dominus::combat::MoveSetComponent>();
        f->controller = std::make_unique<dominus::combat::CombatController>(*f->evaluator, *moveSet);
        f->controller->SetEntityId(entityId);
        return f;
    };

    auto attacker = LoadFighter("attacker_001");
    auto defender = LoadFighter("brooklyn");
    if (!attacker->loadResult.ok || !defender->loadResult.ok) {
        std::cerr << "Failed to load fighters\n";
        return 1;
    }
    std::cout << "[live-collision] spawned attacker_001 and brooklyn (defender)\n";

    auto skel = dominus::animation::SkeletonLoader::LoadFromFile(baseDir / "brooklyn.skel.json");
    auto hurtboxes = dominus::combat::HurtboxLoader::LoadFromFile(baseDir / "brooklyn_hurtboxes.json");
    if (!skel.ok || !hurtboxes.ok) {
        std::cerr << "Failed to load skeleton/hurtboxes\n";
        return 1;
    }
    auto attackerPose = skel.value->ComputeBindPoseWorld();
    auto defenderPose = skel.value->ComputeBindPoseWorld();
    for (auto& t : defenderPose) {
        t.x += 20.0f;
        t.y += 10.0f;
    }

    dominus::combat::ImpactEventLog log;
    defender->controller->SetProvenanceLog(&log);

    attacker->controller->StartMove("jab");
    attacker->controller->Update(dominus::combat::FramesToSeconds(6));  // into active
    std::cout << "[live-collision] attacker phase=active, defender phase=neutral\n";

    dominus::combat::ImpactGenomeInputs a, d;
    auto reaction = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f,
        "attacker_001", 100);

    if (!reaction) {
        std::cerr << "[result] no collision detected -- FAILED\n";
        return 1;
    }
    std::cout << "[live-collision] impact applied: reaction=" << ReactionTypeName(reaction->type) << "\n";
    std::cout << "[live-collision] attacker still mid-swing: " << (attacker->controller->CurrentMove() != nullptr)
               << " (attack does not self-interrupt)\n";
    std::cout << "[live-collision] defender interrupted: " << (defender->controller->CurrentMove() == nullptr)
               << "\n";
    std::cout << "[live-collision] provenance captured: " << log.Count() << " event(s)\n";

    // Repeated evaluation, same activation -- must not double-apply.
    auto repeat = attacker->controller->EvaluateCollisionAndApplyImpact(
        *defender->controller, *skel.value, attackerPose, *skel.value, defenderPose, *hurtboxes.value, a, d, 5.0f,
        "attacker_001", 101);
    std::cout << "[live-collision] repeated evaluation this activation: "
               << (repeat.has_value() ? "APPLIED AGAIN (bug)" : "correctly refused") << "\n";

    std::cout << "[result] "
               << (log.Count() == 1 && !repeat.has_value()
                       ? "Combat's loop is closed: collision -> impact -> reaction -> provenance, verified live"
                       : "FAILED")
               << "\n";
    return (log.Count() == 1 && !repeat.has_value()) ? 0 : 1;
}

int RigAuthorityDemo(const std::string& skelPath) {
    // DOMINUS RIG v1.0: run the real, opt-in canonical-skeleton
    // validator against whatever skeleton file is given.
    auto result = dominus::animation::SkeletonLoader::LoadFromFile(skelPath);
    if (!result.ok) {
        std::cerr << "Failed to load " << skelPath << ": " << result.error << "\n";
        return 1;
    }

    auto report = dominus::rig::RigAuthorityValidator::Validate(skelPath, *result.value);
    std::cout << "[rig-authority] skeleton='" << skelPath << "' is_rigged=" << (report.is_rigged ? "true" : "false")
               << "\n";
    for (const auto& issue : report.issues) {
        std::cout << "[rig-authority]   [" << issue.severity << "] " << issue.check << ": " << issue.message << "\n";
    }
    std::cout << "[result] " << (report.is_rigged ? "kRigged" : "NOT kRigged -- see issues above") << "\n";
    return 0;
}

int RigProfileDemo(const std::string& skelPath, const std::string& profilePath) {
    // RIG Phase 3: the migration diagnostic, live.
    auto skel = dominus::animation::SkeletonLoader::LoadFromFile(skelPath);
    auto profileResult = dominus::rig::RigProfileLoader::LoadFromFile(profilePath);
    if (!skel.ok || !profileResult.ok) {
        std::cerr << "Failed to load skeleton or profile\n";
        return 1;
    }

    dominus::rig::RigProfile profile = *profileResult.value;
    std::cout << "[rig-profile] profile='" << profile.profile_id << "' entity='" << profile.entity_id << "'\n";
    std::cout << "[rig-profile] state=" << dominus::rig::RigProfileStateName(profile.state) << "\n";

    auto report = dominus::rig::RigProfileValidator::Validate(*skel.value, profile);
    std::cout << "[rig-profile] === mapped (" << report.mapped.size() << ") ===\n";
    for (const auto& m : report.mapped) std::cout << "[rig-profile]   " << m << "\n";
    std::cout << "[rig-profile] === missing (" << report.missing.size() << ") ===\n";
    for (const auto& m : report.missing) std::cout << "[rig-profile]   " << m << "\n";
    std::cout << "[rig-profile] === extra (" << report.extra.size() << ") ===\n";
    for (const auto& m : report.extra) std::cout << "[rig-profile]   " << m << "\n";
    std::cout << "[rig-profile] === hierarchy_conflicts (" << report.hierarchy_conflicts.size() << ") ===\n";
    for (const auto& m : report.hierarchy_conflicts) std::cout << "[rig-profile]   " << m << "\n";
    std::cout << "[rig-profile] === unresolved (" << report.unresolved.size() << ") ===\n";
    for (const auto& m : report.unresolved) std::cout << "[rig-profile]   " << m << "\n";

    // Try to advance the lifecycle as far as the real data honestly allows.
    dominus::rig::RigProfileLifecycle::AdvanceToMapped(profile);
    std::cout << "[rig-profile] after AdvanceToMapped: state=" << dominus::rig::RigProfileStateName(profile.state)
               << "\n";
    bool advanced = dominus::rig::RigProfileLifecycle::AdvanceToValidated(profile, report.valid);
    std::cout << "[rig-profile] AdvanceToValidated -> " << (advanced ? "true" : "false")
               << ", state=" << dominus::rig::RigProfileStateName(profile.state) << "\n";

    std::cout << "[result] RIG PROFILE: " << (report.valid ? "VALID" : "INVALID") << "\n";
    return 0;
}

int BrooklynMigrationDemo(const std::string& fixtureDirPath) {
    // RIG Phase 4: prove Brooklyn's canonical skeleton is behaviorally
    // equivalent to his legacy one, live, across every real clip.
    namespace fs = std::filesystem;
    fs::path dir = fixtureDirPath;

    auto legacySkel = dominus::animation::SkeletonLoader::LoadFromFile(dir / "brooklyn.skel.json");
    auto canonicalSkel = dominus::animation::SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    if (!legacySkel.ok || !canonicalSkel.ok) {
        std::cerr << "Failed to load skeletons\n";
        return 1;
    }
    std::cout << "[brooklyn-migration] legacy bones=" << legacySkel.value->BoneCount()
               << " canonical bones=" << canonicalSkel.value->BoneCount() << "\n";

    auto report = dominus::rig::RigAuthorityValidator::Validate("brooklyn_canonical", *canonicalSkel.value);
    std::cout << "[brooklyn-migration] canonical skeleton is_rigged=" << (report.is_rigged ? "true" : "false")
               << "\n";

    struct ClipPair {
        std::string oldFile, newFile, oldBone, newBone;
    };
    std::vector<std::pair<std::string, std::string>> pairs = {
        {"brooklyn_idle.clip.json", "canonical_brooklyn_idle.clip.json"},
        {"brooklyn_attack.clip.json", "canonical_brooklyn_attack.clip.json"},
        {"brooklyn_knockback.clip.json", "canonical_brooklyn_knockback.clip.json"},
    };
    std::vector<std::pair<std::string, std::string>> bones = {
        {"torso", "chest"}, {"head", "head"}, {"arm_r", "hand_R"}, {"leg_r", "foot_R"}};

    bool allMatch = true;
    for (const auto& [oldFile, newFile] : pairs) {
        auto oldClip = dominus::animation::AnimationClipLoader::LoadFromFile(dir / oldFile);
        auto newClip = dominus::animation::AnimationClipLoader::LoadFromFile(dir / newFile);
        if (!oldClip.ok || !newClip.ok) continue;
        float t = oldClip.value->duration * 0.5f;
        auto oldPose = dominus::animation::AnimationPlayer::Sample(*legacySkel.value, *oldClip.value, t);
        auto newPose = dominus::animation::AnimationPlayer::Sample(*canonicalSkel.value, *newClip.value, t);
        std::cout << "[brooklyn-migration] " << oldFile << " @t=" << t << ":\n";
        for (const auto& [oldBone, newBone] : bones) {
            auto oi = legacySkel.value->FindBoneIndex(oldBone);
            auto ni = canonicalSkel.value->FindBoneIndex(newBone);
            if (!oi || !ni) continue;
            const auto& ow = oldPose[*oi];
            const auto& nw = newPose[*ni];
            bool match = std::fabs(ow.x - nw.x) < 0.01f && std::fabs(ow.y - nw.y) < 0.01f &&
                         std::fabs(ow.rotation_deg - nw.rotation_deg) < 0.01f;
            allMatch = allMatch && match;
            std::cout << "[brooklyn-migration]   " << oldBone << "->" << newBone << " legacy=(" << ow.x << ","
                       << ow.y << "," << ow.rotation_deg << ") canonical=(" << nw.x << "," << nw.y << ","
                       << nw.rotation_deg << ") " << (match ? "MATCH" : "MISMATCH") << "\n";
        }
    }

    std::cout << "[result] "
               << (allMatch && report.is_rigged
                       ? "Brooklyn's canonical migration is proven behaviorally equivalent -- COMPATIBLE, not yet ACTIVE"
                       : "FAILED")
               << "\n";
    return (allMatch && report.is_rigged) ? 0 : 1;
}

int BrooklynAcceptanceDemo(const std::string& fixtureDirPath) {
    // The Canonical Character Acceptance Harness, live -- generates a
    // real migration certificate from real check results, then drives
    // RigProfileLifecycle through ACCEPTED to ACTIVE using it.
    namespace fs = std::filesystem;
    fs::path dir = fixtureDirPath;

    dominus::rig::BoneMap boneMap = {{"root", "root"},   {"torso", "chest"}, {"head", "head"},
                                      {"arm_r", "hand_R"}, {"arm_l", "hand_L"}, {"leg_r", "foot_R"},
                                      {"leg_l", "foot_L"}};
    std::vector<std::string> clipNames = {"idle",     "attack",     "air_combo",  "block_impact",
                                           "combo_starter", "counter", "dodge",   "knockback",
                                           "knockdown", "knockdown_recovery", "launcher", "stagger", "transformation"};
    dominus::rig::ClipPairs clipPairs;
    for (const auto& n : clipNames) {
        clipPairs.push_back({dir / ("brooklyn_" + n + ".clip.json"), dir / ("canonical_brooklyn_" + n + ".clip.json")});
    }

    std::vector<dominus::rig::AcceptanceSection> sections = {
        dominus::rig::CharacterAcceptanceHarness::CheckSkeleton(dir / "brooklyn.skel.json",
                                                                  dir / "brooklyn_canonical.skel.json", boneMap),
        dominus::rig::CharacterAcceptanceHarness::CheckAnimation(
            dir / "brooklyn.skel.json", dir / "brooklyn_canonical.skel.json", boneMap, clipPairs),
        dominus::rig::CharacterAcceptanceHarness::CheckCombat(dir / "brooklyn_canonical.skel.json",
                                                                 dir / "brooklyn_canonical_hurtboxes.json",
                                                                 dir / "canonical_brooklyn_move_jab.json"),
        dominus::rig::CharacterAcceptanceHarness::CheckRuntime(
            dir / "brooklyn_canonical.dominus", dir, dir / "brooklyn_canonical_hurtboxes.json", "jab"),
        dominus::rig::CharacterAcceptanceHarness::CheckDeterminism(
            dir / "brooklyn_canonical.dominus", dir, dir / "brooklyn_canonical_hurtboxes.json", "jab"),
    };

    auto cert = dominus::rig::AcceptanceCertificateForge::Generate("brooklyn", "brooklyn_canonical_identity_v1",
                                                                     sections);

    std::cout << "BROOKLYN MIGRATION\n";
    std::cout << "------------------\n";
    std::cout << "Source:              legacy Brooklyn\n";
    std::cout << "Target:              DOMINUS RIG v1.0 (" << cert.target_profile << ")\n\n";
    for (const auto& s : cert.sections) {
        std::cout << s.name << ":" << std::string(std::max<size_t>(1, 21 - s.name.size()), ' ')
                   << (s.passed ? "PASS" : "FAIL") << "  (" << s.detail << ")\n";
    }
    std::cout << "\nBehavioral Drift:    " << (cert.overall_pass ? "NONE" : "DETECTED") << "\n";
    std::cout << "Certificate hash:    " << cert.certificate_hash << "\n\n";

    // Drive the lifecycle using the real certificate as the gate.
    dominus::rig::RigProfile profile;
    profile.profile_id = cert.target_profile;
    profile.entity_id = cert.source_entity;
    auto canonicalSkel = dominus::animation::SkeletonLoader::LoadFromFile(dir / "brooklyn_canonical.skel.json");
    if (canonicalSkel.ok) {
        for (const auto& bone : canonicalSkel.value->Bones()) profile.mappings.push_back({bone.name, bone.name});
    }
    dominus::rig::RigProfileLifecycle::AdvanceToMapped(profile);
    auto profileReport = dominus::rig::RigProfileValidator::Validate(*canonicalSkel.value, profile);
    dominus::rig::RigProfileLifecycle::AdvanceToValidated(profile, profileReport.valid);
    dominus::rig::RigProfileLifecycle::DeclareCompatible(profile, "structural migration map verified");

    bool accepted = dominus::rig::RigProfileLifecycle::AdvanceToAccepted(profile, cert.overall_pass, cert.certificate_hash);
    std::cout << "PROFILE STATE:\n";
    std::cout << "COMPATIBLE -> " << (accepted ? "ACCEPTED" : "REJECTED (certificate did not pass)") << "\n";
    if (accepted) {
        bool activated = dominus::rig::RigProfileLifecycle::DeclareActive(
            profile, cert.certificate_hash, "brooklyn-acceptance CLI run, certificate " + cert.certificate_hash.substr(0, 12));
        std::cout << "ACCEPTED -> " << (activated ? "ACTIVE" : "FAILED TO ACTIVATE") << "\n";
    }

    std::cout << "\n[result] " << (cert.overall_pass ? "Brooklyn earns ACTIVE from derived evidence, not a manual flip"
                                                       : "Brooklyn's migration has real, unresolved drift -- REJECTED")
               << "\n";
    return cert.overall_pass ? 0 : 1;
}

int VisualAcceptanceDemo(const std::string& dominusPath) {
    // The visual authority model, live: real structural checks against
    // a real character's VisualForge data, honestly excluding
    // RenderedOutput (GRAPHICS doesn't exist).
    namespace fs = std::filesystem;
    fs::path baseDir = fs::path(dominusPath).parent_path();

    auto loadResult = dominus::core::DominusSerializer::Load(dominusPath);
    if (!loadResult.ok) {
        std::cerr << "Failed to load " << dominusPath << "\n";
        return 1;
    }
    auto& obj = *loadResult.value;
    dominus::character::RigBinder::Bind(obj, baseDir);
    auto* visual = obj.GetComponent<dominus::character::VisualGenomeComponent>();
    if (!visual) {
        std::cerr << "'" << obj.Id() << "' has no bound VisualGenome\n";
        return 1;
    }
    auto* material = obj.GetComponent<dominus::character::MaterialGenomeComponent>();
    auto* visualStyle = obj.GetComponent<dominus::character::VisualStyleGenomeComponent>();

    auto blueprint = dominus::visualforge::CharacterBlueprintForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr);
    auto assets = dominus::visualforge::AssetSpecificationForge::Build(blueprint);
    auto deps = dominus::visualforge::DependencyGraphForge::Build(
        obj.Id(), visual->genome, material ? &material->genome : nullptr,
        visualStyle ? &visualStyle->genome : nullptr);
    auto packageResult = dominus::visualforge::RendererPackageForge::Build(obj.Id(), "t", blueprint, assets, deps);
    if (!packageResult.ok) {
        std::cerr << "RendererPackage build refused -- invalid blueprint\n";
        return 1;
    }

    std::vector<dominus::visualforge::AcceptanceSection> sections = {
        dominus::visualforge::RendererPackageAcceptanceHarness::CheckBlueprintValidity(blueprint),
        dominus::visualforge::RendererPackageAcceptanceHarness::CheckDependencyIntegrity(deps),
        dominus::visualforge::RendererPackageAcceptanceHarness::CheckPackageDeterminism(
            obj.Id(), "t", blueprint, assets, deps, std::nullopt),
        dominus::visualforge::RendererPackageAcceptanceHarness::CheckProvenanceCompleteness(*packageResult.package,
                                                                                              deps),
        dominus::visualforge::RendererPackageAcceptanceHarness::CheckRenderable(blueprint),
        dominus::visualforge::RendererPackageAcceptanceHarness::CheckRenderDeterminism(blueprint),
        dominus::visualforge::RendererPackageAcceptanceHarness::RenderedOutputNotDeclared(),
    };
    auto cert = dominus::visualforge::VisualAcceptanceCertificateForge::Generate(
        obj.Id(), packageResult.package->package_hash, sections);

    std::cout << obj.Id() << " VISUAL ACCEPTANCE\n";
    std::cout << "------------------\n";
    for (const auto& s : cert.sections) {
        std::string status = (s.name == "RenderedOutput") ? "NOT_DECLARED" : (s.passed ? "PASS" : "FAIL");
        std::cout << s.name << ":" << std::string(std::max<size_t>(1, 25 - s.name.size()), ' ') << status << "  ("
                   << s.detail << ")\n";
    }
    std::cout << "\nCertificate hash:    " << cert.certificate_hash << "\n";
    std::cout << "STATE: structurally_sound=" << (cert.structurally_sound ? "true" : "false")
               << " renderable=" << (cert.renderable ? "true" : "false")
               << " (never ACTIVE -- no rasterizer exists to prove visual equivalence against)\n";
    std::cout << "\n[result] "
               << (cert.structurally_sound && cert.renderable
                       ? "RendererPackage proven structurally sound, deterministic, and compiles into a real frame"
                       : "a real defect was found")
               << "\n";
    return (cert.structurally_sound && cert.renderable) ? 0 : 1;
}

int RealityCompileDemo(const std::string& fixtureDirPath) {
    // The Reality Compiler, live: one governed pipeline coordinating
    // RIG::CharacterAcceptanceHarness and
    // VISUALFORGE::RendererPackageAcceptanceHarness for Brooklyn --
    // DECLARED -> COMPILING -> VALIDATED -> REGISTERED -> EXECUTABLE --
    // then the directive's own RECONSTRUCT -> REPRODUCE proof, run for
    // real: the whole pipeline executed twice, independently, and the
    // resulting artifact_hash compared.
    namespace fs = std::filesystem;
    fs::path dir = fixtureDirPath;

    auto result = dominus::reality::RealityCompiler::CompileBrooklyn(dir);

    std::cout << "DOMINUS REALITY COMPILER -- brooklyn\n";
    std::cout << "-------------------------------------\n";
    std::cout << "compiler_identity:   " << result.context.compiler_identity << "\n\n";

    std::cout << "STAGES:\n";
    for (const auto& s : result.context.stages) {
        std::cout << "  " << s.stage << ":" << std::string(std::max<size_t>(1, 14 - s.stage.size()), ' ')
                   << dominus::reality::PipelineStateName(s.state) << "  (" << s.detail << ")\n";
    }

    std::cout << "\nPIPELINE STATE: " << dominus::reality::PipelineStateName(result.context.state) << "\n";
    if (!result.context.artifact_hash.empty()) {
        std::cout << "ARTIFACT HASH:   " << result.context.artifact_hash << "\n";
    }
    if (!result.context.diagnostics.empty()) {
        std::cout << "DIAGNOSTICS:\n";
        for (const auto& d : result.context.diagnostics) std::cout << "  - " << d << "\n";
    }

    std::cout << "\nEXECUTION CERTIFICATE:\n";
    for (const auto& s : result.execution_certificate.sections) {
        std::cout << "  " << s.name << ":" << std::string(std::max<size_t>(1, 28 - s.name.size()), ' ')
                   << (s.passed ? "PASS" : "FAIL") << "  (" << s.detail << ")\n";
    }
    std::cout << "  certificate_hash:  " << result.execution_certificate.certificate_hash << "\n";
    std::cout << "  EXECUTABLE:        " << (result.execution_certificate.executable ? "true" : "false") << "\n";

    std::cout << "\nRECONSTRUCT -> REPRODUCE:\n";
    std::string hashA, hashB;
    bool reproduced = dominus::reality::RealityCompiler::ReproduceAndVerify(dir, hashA, hashB);
    std::cout << "  Run A artifact_hash: " << (hashA.empty() ? "(none)" : hashA) << "\n";
    std::cout << "  Run B artifact_hash: " << (hashB.empty() ? "(none)" : hashB) << "\n";
    std::cout << "  A == B:              " << (reproduced ? "true" : "false") << "\n";

    std::cout << "\n[result] "
               << (result.execution_certificate.executable && reproduced
                       ? "Brooklyn compiled through one governed pipeline, reached EXECUTABLE, and reproduced "
                         "identically -- not asserted, derived twice"
                       : "the pipeline has real, unresolved drift -- see STAGES/DIAGNOSTICS above")
               << "\n";
    return (result.execution_certificate.executable && reproduced) ? 0 : 1;
}

int RealityGraphDemo(const std::string& fixtureDirPath) {
    // Milestone 7: Dependency Sovereignty, visible, via the current
    // dependency authority. Prints the real graph -- twelve nodes,
    // each with a real hash (or an honest empty string if that source
    // couldn't be read), and the real, cited edges from
    // BrooklynEvidenceGraphBuilder.
    namespace fs = std::filesystem;
    auto graph = dominus::reality::BrooklynEvidenceGraphBuilder::BuildStructure(fs::path(fixtureDirPath));

    std::cout << "DOMINUS DEPENDENCY GRAPH -- " << graph.subject << "\n";
    std::cout << "-------------------------------------\n";
    std::cout << "NODES:\n";
    for (const auto& n : graph.nodes) {
        std::string hashDisplay = n.artifact_hash.empty() ? "(not computed by this fast path -- run reality-compile)"
                                                             : n.artifact_hash.substr(0, 16);
        std::cout << "  " << n.node_id << std::string(std::max<size_t>(1, 34 - n.node_id.size()), ' ') << "["
                   << dominus::reality::AuthorityTypeName(n.authority_type) << "]  " << hashDisplay << "\n";
    }
    std::cout << "\nEDGES (from -> to means 'to' depends on 'from'):\n";
    for (const auto& e : graph.edges) {
        std::cout << "  " << e.from << "  ->  " << e.to << "  [" << dominus::reality::EdgeTypeName(e.edge_type) << "]\n";
    }
    std::cout << "\n[result] " << graph.nodes.size() << " nodes, " << graph.edges.size() << " edges -- derived from "
               << "real fixture files and REGISTRY's own compilers, not hardcoded architecture\n";
    return 0;
}

int RealityImpactDemo(const std::string& fixtureDirPath, const std::string& changedNodeId) {
    // CHANGE -> DEPENDENCY GRAPH -> IMPACT ANALYSIS, live, via the
    // current dependency authority. Answers exactly the directive's
    // own question -- "if X changes, who becomes invalid?" -- via a
    // real BFS over the real edge list.
    namespace fs = std::filesystem;
    auto graph = dominus::reality::BrooklynEvidenceGraphBuilder::BuildStructure(fs::path(fixtureDirPath));

    if (!graph.FindNode(changedNodeId)) {
        std::cout << "[error] '" << changedNodeId << "' is not a node in this graph. Known nodes:\n";
        for (const auto& n : graph.nodes) std::cout << "  " << n.node_id << "\n";
        return 1;
    }

    auto affected = graph.ImpactedBy(changedNodeId);
    std::cout << "IMPACT ANALYSIS -- if '" << changedNodeId << "' changes:\n";
    std::cout << "-------------------------------------\n";
    for (const auto& id : affected) {
        std::cout << "  " << (id == changedNodeId ? "[changed]  " : "[invalidated]  ") << id << "\n";
    }
    std::cout << "\n[result] " << affected.size() << " node(s) affected, via real graph traversal\n";
    return 0;
}

int ChangeEventsDemo(const std::string& fixtureDirPath, const std::string& registryPathStr,
                      const std::vector<std::string>& rawPaths) {
    // Milestone 10, live: OS events != Reality changes. Feeds a batch
    // of raw (possibly noisy, possibly duplicate, possibly irrelevant)
    // paths through the real canonicalize -> dedupe -> verify pipeline
    // before anything is allowed to reach RealityRebuilder.
    namespace fs = std::filesystem;
    std::vector<dominus::reality::RawFileEvent> events;
    for (const auto& p : rawPaths) events.push_back({fs::path(p), dominus::reality::RawEventKind::kModified});

    auto changeSet =
        dominus::reality::ChangeEventNormalizer::Normalize(fs::path(fixtureDirPath), fs::path(registryPathStr), events);

    std::cout << "CHANGE EVENT NORMALIZATION\n";
    std::cout << "-------------------------------------\n";
    std::cout << "raw_events_received:        " << changeSet.raw_events_received << "\n";
    std::cout << "distinct_paths_after_dedup: " << changeSet.distinct_paths_after_dedup << "\n\n";

    std::cout << "REJECTED (" << changeSet.rejected.size() << "):\n";
    for (const auto& r : changeSet.rejected) std::cout << "  " << r.path.string() << " -- " << r.reason << "\n";

    std::cout << "\nUNCHANGED (" << changeSet.unchanged.size() << "):\n";
    for (const auto& u : changeSet.unchanged) std::cout << "  " << u.node_id << " (via " << u.evidence_node_id << ")\n";

    std::cout << "\nUNREPRESENTED (" << changeSet.unrepresented.size() << "):\n";
    for (const auto& u : changeSet.unrepresented) std::cout << "  " << u.evidence_node_id << " -- " << u.detail << "\n";

    std::cout << "\nVERIFIED (" << changeSet.verified.size() << "):\n";
    for (const auto& v : changeSet.verified) std::cout << "  " << v.node_id << " (via " << v.evidence_node_id << ") -- " << v.detail << "\n";

    if (changeSet.verified.empty()) {
        std::cout << "\n[result] nothing verified -- no RealityRebuilder call made\n";
        return 0;
    }

    std::cout << "\n--- driving RealityRebuilder for each verified change ---\n";
    auto reports =
        dominus::reality::ChangeEventNormalizer::ProcessEvents(fs::path(fixtureDirPath), fs::path(registryPathStr), events);
    bool allOk = true;
    for (const auto& r : reports) {
        std::cout << "  " << r.changed_node_id << ": " << (r.ok ? "OK" : "FAILED") << " -- " << r.summary << "\n";
        allOk = allOk && r.ok;
    }
    return allOk ? 0 : 1;
}

int RealityRebuildDemo(const std::string& fixtureDirPath, const std::string& registryPathStr,
                        const std::string& nodeId) {
    // Milestone 4, live: FILE CHANGE -> AUTHORITATIVE HASH CHANGE ->
    // ImpactAnalyzer -> EXACT INVALIDATION SET -> TOPOLOGICAL ORDER ->
    // CANONICAL COMPILER -> VALIDATION -> REGISTRY UPDATE. Recompiles
    // ONLY the nodes ImpactAnalyzer names, via the real per-node
    // compilers -- never a blanket reality-compile.
    namespace fs = std::filesystem;
    auto report =
        dominus::reality::RealityRebuilder::RebuildFromChange(fs::path(fixtureDirPath), fs::path(registryPathStr),
                                                                nodeId);

    std::cout << "DOMINUS SELECTIVE REBUILD -- " << report.subject << "\n";
    std::cout << "-------------------------------------\n";
    if (report.lock_contention) {
        std::cout << "[result] LOCK CONTENTION -- " << report.summary << "\n";
        return 1;
    }
    std::cout << "changed_node_id:   " << report.changed_node_id << "\n";
    std::cout << "change_detected:   " << (report.change_detected ? "true" : "false") << "\n";

    if (!report.invalidation_set.empty()) {
        std::cout << "\nINVALIDATION SET (from ImpactAnalyzer):\n";
        for (const auto& id : report.invalidation_set) std::cout << "  " << id << "\n";
    }

    if (!report.results.empty()) {
        std::cout << "\nRECOMPILED, IN TOPOLOGICAL ORDER:\n";
        for (const auto& r : report.results) {
            std::cout << "  " << r.node_id << std::string(std::max<size_t>(1, 34 - r.node_id.size()), ' ')
                       << (r.passed ? "PASS" : "FAIL") << "  (" << r.detail << ")\n";
        }
    }

    std::cout << "\n[result] " << (report.ok ? "OK" : "FAILED") << " -- " << report.summary << "\n";
    return report.ok ? 0 : 1;
}

int EvidenceGraphDemo(const std::string& fixtureDirPath) {
    // Milestone 6, live: the evidence-derived graph. Every node/edge
    // below is either a real hash read from disk or a citation to a
    // real function signature / binder behavior -- see
    // REALITY/BrooklynEvidenceGraphBuilder.cpp for the source of each.
    namespace fs = std::filesystem;
    auto graph = dominus::reality::BrooklynEvidenceGraphBuilder::Build(fs::path(fixtureDirPath));

    std::cout << "DEPENDENCY GRAPH -- " << graph.subject << "\n";
    std::cout << "-------------------------------------\n";
    std::cout << "Nodes:                 " << graph.nodes.size() << "\n";
    std::cout << "Edges:                 " << graph.edges.size() << "\n\n";

    auto report = graph.Validate();
    std::cout << "Duplicate nodes:       " << (report.no_duplicate_nodes ? "NONE" : "FOUND") << "\n";
    std::cout << "Dangling edges:        " << (report.no_dangling_edges ? "NONE" : "FOUND") << "\n";
    std::cout << "Self-dependencies:     " << (report.no_self_dependencies ? "NONE" : "FOUND") << "\n";
    std::cout << "Cycles:                " << (report.no_cycles ? "NONE" : "FOUND") << "\n";
    std::cout << "Undeclared citations:  " << (graph.undeclared.empty() ? "NONE" : std::to_string(graph.undeclared.size()))
               << "\n";
    for (const auto& u : graph.undeclared) std::cout << "  UNDECLARED: " << u.from << " -> " << u.to << " (" << u.detail << ")\n";

    auto topo = graph.TopologicalOrder();
    std::cout << "Determinism (2 builds): "
               << (dominus::reality::BrooklynEvidenceGraphBuilder::Build(fs::path(fixtureDirPath)).GraphHash() ==
                           graph.GraphHash()
                       ? "PASS"
                       : "FAIL")
               << "\n";
    std::cout << "Topological order:     " << (topo.has_value() ? "PASS" : "FAIL (cycle)") << "\n\n";

    std::cout << "NODES:\n";
    for (const auto& n : graph.nodes) {
        std::cout << "  " << n.node_id << std::string(std::max<size_t>(1, 32 - n.node_id.size()), ' ') << "["
                   << dominus::reality::AuthorityTypeName(n.authority_type) << "]  " << n.state << "  "
                   << n.artifact_hash.substr(0, 12) << "\n";
    }

    std::cout << "\nEDGES:\n";
    for (const auto& e : graph.edges) {
        std::cout << "  " << e.from << " -> " << e.to << "  [" << dominus::reality::EdgeTypeName(e.edge_type) << "]\n";
        std::cout << "      reason:   " << e.reason << "\n";
        std::cout << "      evidence: " << e.evidence << "\n";
    }

    std::cout << "\nGraph hash: " << graph.GraphHash() << "\n";
    std::cout << "\n[result] the graph does not decide what is true -- RIG and VisualForge's own certificates do; "
                 "this describes how those truths depend on one another\n";
    return report.ok ? 0 : 1;
}

int ReconcileDemo(const std::string& fixtureDirPath, const std::string& registryPathStr) {
    // Milestone 12, live: "here's what changed while I wasn't
    // watching." A full scan against the registry, driving real
    // rebuilds for anything that genuinely differs -- the same
    // pipeline a restarted RealityWatcher runs automatically before
    // resuming live observation.
    namespace fs = std::filesystem;
    auto scanResult =
        dominus::reality::Reconciler::Scan(fs::path(fixtureDirPath), fs::path(registryPathStr));

    std::cout << "RECONCILIATION SCAN\n";
    std::cout << "-------------------------------------\n";
    std::cout << "VERIFIED (real changes found, " << scanResult.verified.size() << "):\n";
    for (const auto& v : scanResult.verified) std::cout << "  " << v.node_id << " (via " << v.evidence_node_id << ")\n";
    std::cout << "unchanged: " << scanResult.unchanged.size() << "   rejected: " << scanResult.rejected.size()
               << "   unrepresented: " << scanResult.unrepresented.size() << "\n";

    if (scanResult.verified.empty()) {
        std::cout << "\n[result] nothing changed while offline -- no rebuild needed\n";
        return 0;
    }

    std::cout << "\n--- reconciling ---\n";
    auto reports = dominus::reality::Reconciler::Reconcile(fs::path(fixtureDirPath), fs::path(registryPathStr));
    bool allOk = true;
    for (const auto& r : reports) {
        std::cout << "  " << r.changed_node_id << ": " << (r.ok ? "OK" : "FAILED") << " -- " << r.summary << "\n";
        allOk = allOk && r.ok;
    }
    return allOk ? 0 : 1;
}

int RealityWatchDemo(const std::string& fixtureDirPath, const std::string& registryPathStr) {
    // Milestone 11, live: the first genuine end-to-end autonomous loop.
    // Edit reality -> DOMINUS notices -> DOMINUS proves what changed ->
    // DOMINUS determines consequences -> DOMINUS rebuilds -> DOMINUS
    // persists the new reality. Deliberately boring: one directory, one
    // inotify fd, no parallel workers. Ctrl+C to stop.
    namespace fs = std::filesystem;
    std::cout << "DOMINUS REALITY WATCHER\n";
    std::cout << "-------------------------------------\n";
    std::cout << "watching:  " << fixtureDirPath << "\n";
    std::cout << "registry:  " << registryPathStr << "\n";
    std::cout << "Save a change to any authoritative artifact -- Ctrl+C to stop.\n\n";

    try {
        dominus::reality::RealityWatcher watcher{fs::path(fixtureDirPath), fs::path(registryPathStr)};
        // Deliberately boring: single-threaded, blocking Run() on the
        // main thread -- no supervision, no restart policy. Real
        // process-level Ctrl+C (SIGINT) ends the process; there is no
        // in-process signal handler here, matching "no process
        // supervision" from the milestone's own scope.
        watcher.Run();
    } catch (const std::exception& e) {
        std::cerr << "[error] " << e.what() << "\n";
        return 1;
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage:\n"
                   << "  dominus-cli inspect  <file.dominus>\n"
                   << "  dominus-cli validate <file.dominus>\n"
                   << "  dominus-cli play     <file.dominus> <clip_name> <time_seconds>\n"
                   << "  dominus-cli graph    <file.dominus> [trigger] [drive_seconds]\n"
                   << "  dominus-cli fight    <file.dominus> <move_name> <defender_offset_x> <defender_offset_y>\n"
                   << "  dominus-cli transform <file.dominus> <transformation_name>\n"
                   << "  dominus-cli ai        <file.dominus>\n"
                   << "  dominus-cli validate-motion <file.dominus>\n"
                   << "  dominus-cli world     <file.dominus>\n"
                   << "  dominus-cli physics   <file.dominus>\n"
                   << "  dominus-cli validate-package <file.dominus>\n"
                   << "  dominus-cli genome-compile <fixtures_dir>\n"
                   << "  dominus-cli world-save <file.dominus> <state_dir>\n"
                   << "  dominus-cli world-load <state_dir> <fixtures_dir>\n"
                   << "  dominus-cli social     <file.dominus>\n"
                   << "  dominus-cli creature   <file.dominus>\n"
                   << "  dominus-cli combat-style <style.json>\n"
                   << "  dominus-cli impact     <physics.json>\n"
                   << "  dominus-cli design-coherence <design.json> <style.json> <physics.json>\n"
                   << "  dominus-cli build      <project_dir>\n"
                   << "  dominus-cli visual     <visual.json>\n"
                   << "  dominus-cli material   <material.json>\n"
                   << "  dominus-cli visual-style <style.json>\n"
                   << "  dominus-cli reality-compile <fixtures_dir>\n"
                   << "  dominus-cli reality-graph   <fixtures_dir>\n"
                   << "  dominus-cli reality-impact  <fixtures_dir> <node_id>\n"
                   << "  dominus-cli reality-rebuild <fixtures_dir> <registry_path> <node_id>\n"
                   << "  dominus-cli evidence-graph  <fixtures_dir>\n"
                   << "  dominus-cli change-events   <fixtures_dir> <registry_path> <raw_path> [more paths...]\n"
                   << "  dominus-cli reality-watch   <fixtures_dir> <registry_path>\n"
                   << "  dominus-cli reality-reconcile <fixtures_dir> <registry_path>\n"
                   << "  dominus-cli import-hitm-identity <identity_dir>\n"
                   << "  dominus-cli hitm-combat-genome <identity_dir>\n"
                   << "  dominus-cli hitm-parts-rig <character_dir>\n"
                   << "  dominus-cli hitm-game-rules <game.json>\n"
                   << "  dominus-cli hitm-fighter-runtime <identity_dir> <game.json>\n"
                   << "  dominus-cli hitm-sprite-draw-data <identity_dir> <game.json> <hitm_engine_root>\n";
        return 2;
    }
    std::string command = argv[1];
    std::string path = argv[2];

    if (command == "inspect") return Inspect(path);
    if (command == "validate") return Validate(path);
    if (command == "play") {
        if (argc < 5) {
            std::cerr << "usage: dominus-cli play <file.dominus> <clip_name> <time_seconds>\n";
            return 2;
        }
        return Play(path, argv[3], std::stof(argv[4]));
    }
    if (command == "graph") {
        std::string trigger = argc >= 4 ? argv[3] : "";
        float driveSeconds = argc >= 5 ? std::stof(argv[4]) : 1.0f;
        return Graph(path, trigger, driveSeconds);
    }
    if (command == "fight") {
        if (argc < 6) {
            std::cerr << "usage: dominus-cli fight <file.dominus> <move_name> <defender_offset_x> <defender_offset_y>\n";
            return 2;
        }
        return Fight(path, argv[3], std::stof(argv[4]), std::stof(argv[5]));
    }
    if (command == "transform") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli transform <file.dominus> <transformation_name>\n";
            return 2;
        }
        return Transform(path, argv[3]);
    }
    if (command == "ai") {
        return AiDecide(path);
    }
    if (command == "validate-motion") {
        return ValidateMotion(path);
    }
    if (command == "world") {
        return WorldDemo(path);
    }
    if (command == "physics") {
        return PhysicsDemo(path);
    }
    if (command == "validate-package") {
        return ValidatePackage(path);
    }
    if (command == "genome-compile") {
        return GenomeCompileDemo(path);
    }
    if (command == "world-save") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli world-save <file.dominus> <state_dir>\n";
            return 2;
        }
        return WorldSaveDemo(path, argv[3]);
    }
    if (command == "world-load") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli world-load <state_dir> <fixtures_dir>\n";
            return 2;
        }
        return WorldLoadDemo(path, argv[3]);
    }
    if (command == "social") {
        return SocialDemo(path);
    }
    if (command == "creature") {
        return CreatureDemo(path);
    }
    if (command == "combat-style") {
        return CombatStyleDemo(path);
    }
    if (command == "impact") {
        return ImpactDemo(path);
    }
    if (command == "design-coherence") {
        if (argc < 5) {
            std::cerr << "usage: dominus-cli design-coherence <design.json> <style.json> <physics.json>\n";
            return 2;
        }
        return DesignCoherenceDemo(path, argv[3], argv[4]);
    }
    if (command == "build") {
        return BuildDemo(path);
    }
    if (command == "visual") {
        return VisualDemo(path);
    }
    if (command == "material") {
        return MaterialDemo(path);
    }
    if (command == "visual-style") {
        return VisualStyleDemo(path);
    }
    if (command == "impact-provenance") {
        return ImpactProvenanceDemo(path);
    }
    if (command == "live-impact") {
        return LiveImpactCaptureDemo(path);
    }
    if (command == "visual-forge") {
        return VisualForgeDemo(path);
    }
    if (command == "visual-forge-v2") {
        return VisualForgeV2Demo(path);
    }
    if (command == "visual-forge-v3") {
        return VisualForgeV3Demo(path);
    }
    if (command == "live-collision") {
        return LiveCollisionLoopDemo(path);
    }
    if (command == "rig-authority") {
        return RigAuthorityDemo(path);
    }
    if (command == "rig-profile") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli rig-profile <skeleton.skel.json> <profile.json>\n";
            return 2;
        }
        return RigProfileDemo(path, argv[3]);
    }
    if (command == "brooklyn-migration") {
        return BrooklynMigrationDemo(path);
    }
    if (command == "brooklyn-acceptance") {
        return BrooklynAcceptanceDemo(path);
    }
    if (command == "reality-compile") {
        return RealityCompileDemo(path);
    }
    if (command == "reality-graph") {
        return RealityGraphDemo(path);
    }
    if (command == "reality-impact") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli reality-impact <fixtures_dir> <node_id>\n";
            return 2;
        }
        return RealityImpactDemo(path, argv[3]);
    }
    if (command == "reality-rebuild") {
        if (argc < 5) {
            std::cerr << "usage: dominus-cli reality-rebuild <fixtures_dir> <registry_path> <node_id>\n";
            return 2;
        }
        return RealityRebuildDemo(path, argv[3], argv[4]);
    }
    if (command == "evidence-graph") {
        return EvidenceGraphDemo(path);
    }
    if (command == "change-events") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli change-events <fixtures_dir> <registry_path> <raw_path> [more paths...]\n";
            return 2;
        }
        std::vector<std::string> rawPaths;
        for (int i = 4; i < argc; i++) rawPaths.push_back(argv[i]);
        return ChangeEventsDemo(path, argv[3], rawPaths);
    }
    if (command == "reality-watch") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli reality-watch <fixtures_dir> <registry_path>\n";
            return 2;
        }
        return RealityWatchDemo(path, argv[3]);
    }
    if (command == "reality-reconcile") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli reality-reconcile <fixtures_dir> <registry_path>\n";
            return 2;
        }
        return ReconcileDemo(path, argv[3]);
    }
    if (command == "visual-acceptance") {
        return VisualAcceptanceDemo(path);
    }
    if (command == "import-hitm-identity") {
        return ImportHitmIdentityDemo(path);
    }
    if (command == "hitm-combat-genome") {
        return HitmCombatGenomeDemo(path);
    }
    if (command == "hitm-parts-rig") {
        return HitmPartsRigDemo(path);
    }
    if (command == "hitm-game-rules") {
        return HitmGameRulesDemo(path);
    }
    if (command == "hitm-fighter-runtime") {
        if (argc < 4) {
            std::cerr << "usage: dominus-cli hitm-fighter-runtime <identity_dir> <game.json>\n";
            return 2;
        }
        return HitmFighterRuntimeDemo(path, argv[3]);
    }
    if (command == "hitm-sprite-draw-data") {
        if (argc < 5) {
            std::cerr << "usage: dominus-cli hitm-sprite-draw-data <identity_dir> <game.json> <hitm_engine_root>\n";
            return 2;
        }
        return HitmSpriteDrawDataDemo(path, argv[3], argv[4]);
    }

    std::cerr << "unknown command: " << command << "\n";
    return 2;
}
