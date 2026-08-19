// CHARACTER/HitmBridge/HitmFighterRuntime.cpp
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"

#include "PHYSICS/RigidBody.h"
#include "WORLD/Core/SpatialComponent.h"

namespace dominus::character::hitm {

using core::Result;

Result<HitmFighterRuntime> HitmFighterRuntime::Create(const HitmIdentityRecord& record, const HitmCombatGenome& genome,
                                                        const HitmGameRules& rules) {
    if (record.fighter_id != genome.FighterId()) {
        return Result<HitmFighterRuntime>::Fail("HitmFighterRuntime::Create: identity fighter_id (" + record.fighter_id +
                                                  ") does not match genome fighter_id (" + genome.FighterId() + ")");
    }

    auto moveResult = HitmMoveInstance::Extract(record, "special");
    if (!moveResult.ok) {
        return Result<HitmFighterRuntime>::Fail("HitmFighterRuntime::Create: " + moveResult.error);
    }

    const ReadEngine* readEngine = genome.GetReadEngine();
    if (!readEngine) {
        return Result<HitmFighterRuntime>::Fail(
            "HitmFighterRuntime::Create: fighter '" + record.fighter_id +
            "' has no read_engine in their real combat genome -- this vertical slice starts with Brooklyn "
            "specifically because he is the one real fighter who has one (see combat_genome.json across all "
            "three real fighters -- Rocket and Static genuinely do not)");
    }

    if (!genome.Defense().block_preference.has_value()) {
        return Result<HitmFighterRuntime>::Fail("HitmFighterRuntime::Create: fighter '" + record.fighter_id +
                                                  "' has no defense_profile.blockPreference in their real combat genome");
    }

    HitmFighterRuntime runtime(rules, HitmReadEngineState(*readEngine), *moveResult.value,
                                *genome.Defense().block_preference, record.fighter_id);
    return Result<HitmFighterRuntime>::Ok(std::move(runtime));
}

HitmFighterRuntime::HitmFighterRuntime(HitmGameRules rules, HitmReadEngineState readEngine, HitmMoveInstance specialMove,
                                         double defenseBlockPreference, std::string fighterId)
    : physics_(static_cast<float>(rules.Physics().gravity)),
      entityId_(fighterId),
      fighterId_(fighterId),
      rules_(std::move(rules)),
      readEngine_(std::move(readEngine)),
      specialMove_(std::move(specialMove)),
      defenseBlockPreference_(defenseBlockPreference) {
    // Real integration with WORLD's real entity/component substrate (WORLD
    // LAW 002) -- not a bespoke position struct. Start position: centered
    // between the real wallL/wallR, standing on the real ground level.
    core::MetaBinObject entity(entityId_, "0.1.0");
    entity.AddComponent<world::SpatialComponent>(world::SpatialComponent::Fighter2_5D(
        (static_cast<float>(rules_.Physics().wall_l) + static_cast<float>(rules_.Physics().wall_r)) / 2.0f,
        static_cast<float>(rules_.Physics().ground)));

    physics::RigidBody body;
    body.mass = 1.0f;
    body.affected_by_gravity = true;
    entity.AddComponent<physics::RigidBody>(body);

    world_.Entities().CreateEntity(std::move(entity));

    // No WorldTick registration here -- see this header's top comment
    // ("A REAL BUG FOUND AND FIXED") for why a `this`-capturing
    // WorldSystemFn is unsafe for a class that is itself moved (as every
    // Result<HitmFighterRuntime>-returning factory does). AdvanceFrame()
    // calls RunOneFrame() directly instead.
}

void HitmFighterRuntime::AdvanceFrame(HitmInputCommand input) {
    // dt fixed at 1.0: HITM's real game.json values (gravity=0.6,
    // walkSpeed=4.4, ...) are authored as flat per-frame deltas, not
    // accelerations to be scaled by a wall-clock fraction of a second.
    // PHYSICS::PhysicsSystem's Integrate() is convention-agnostic (it
    // just adds gravityY_*mass to force each call, then force/mass*dt to
    // velocity) -- treating one AdvanceFrame() as exactly one real HITM
    // frame (dt=1.0, RigidBody.mass=1.0) is what makes that generic
    // integrator reproduce HITM's real frame-based motion exactly,
    // without PHYSICS knowing anything about frames or fighting games.
    RunOneFrame(input);
}

void HitmFighterRuntime::RunOneFrame(HitmInputCommand input) {
    auto* entity = world_.Entities().Find(entityId_);
    auto* spatial = entity->GetComponent<world::SpatialComponent>();
    auto* body = entity->GetComponent<physics::RigidBody>();
    const auto& phys = rules_.Physics();

    // Hit-freeze convention: while hitstop is active, nothing else in the
    // simulation advances -- not movement, not the attack/reaction state
    // clock, not the read-engine decay clock. Only the hitstop counter
    // itself ticks down.
    if (hitstopFramesRemaining_ > 0) {
        --hitstopFramesRemaining_;
        ++frame_;
        return;
    }

    // 1. Input-driven velocity / state transitions, before physics moves
    // anything this frame.
    switch (state_) {
        case HitmFighterState::kIdle:
        case HitmFighterState::kWalking:
        case HitmFighterState::kBlockingStance:
            if (input == HitmInputCommand::kBlock) {
                body->velocity_x = 0.0f;
                state_ = HitmFighterState::kBlockingStance;
            } else if (input == HitmInputCommand::kLeft) {
                body->velocity_x = static_cast<float>(-phys.walk_speed);
                state_ = HitmFighterState::kWalking;
            } else if (input == HitmInputCommand::kRight) {
                body->velocity_x = static_cast<float>(phys.walk_speed);
                state_ = HitmFighterState::kWalking;
            } else if (input == HitmInputCommand::kJump && grounded_) {
                body->velocity_x = 0.0f;
                body->velocity_y = static_cast<float>(phys.jump_vel);
                grounded_ = false;
                state_ = HitmFighterState::kJumping;
            } else if (input == HitmInputCommand::kSpecial) {
                body->velocity_x = 0.0f;
                state_ = HitmFighterState::kAttackStartup;
                stateFramesRemaining_ = specialMove_.move_def.frames.startup;
            } else {
                body->velocity_x = 0.0f;
                state_ = HitmFighterState::kIdle;
            }
            break;
        case HitmFighterState::kJumping:
            // Real data has no air-attack or double-jump -- only
            // horizontal drift is honored while airborne; kJump/kSpecial
            // inputs are dropped, not buffered.
            if (input == HitmInputCommand::kLeft) {
                body->velocity_x = static_cast<float>(-phys.walk_speed);
            } else if (input == HitmInputCommand::kRight) {
                body->velocity_x = static_cast<float>(phys.walk_speed);
            } else {
                body->velocity_x = 0.0f;
            }
            break;
        case HitmFighterState::kAttackStartup:
        case HitmFighterState::kAttackActive:
        case HitmFighterState::kAttackRecovery:
        case HitmFighterState::kHitstun:
        case HitmFighterState::kBlockstun:
            // Locked: cannot walk, jump, or re-attack while committed to
            // an action or stunned. Input is dropped, not buffered --
            // real signature.json data describes no input-buffer system
            // (game.json's own combat.inputBufferFrames exists but this
            // vertical slice does not implement input buffering; a real,
            // documented gap, not a silent omission).
            body->velocity_x = 0.0f;
            break;
    }

    // 2. Real, existing, tested integration -- unmodified.
    physics_.Integrate(world_.Entities(), 1.0f);

    // 3. Ground/wall clamp using the real authored bounds.
    if (spatial->y >= static_cast<float>(phys.ground)) {
        spatial->y = static_cast<float>(phys.ground);
        body->velocity_y = 0.0f;
        if (!grounded_) {
            grounded_ = true;
            if (state_ == HitmFighterState::kJumping) {
                state_ = HitmFighterState::kIdle;
            }
        }
    }
    if (spatial->x < static_cast<float>(phys.wall_l)) spatial->x = static_cast<float>(phys.wall_l);
    if (spatial->x > static_cast<float>(phys.wall_r)) spatial->x = static_cast<float>(phys.wall_r);

    // 4. State-frame countdown, using the real per-move frame counts for
    // attack sub-states and the real per-hit hitstun/blockstun counts for
    // reaction states (set by TakeHit).
    if (stateFramesRemaining_ > 0) {
        --stateFramesRemaining_;
        if (stateFramesRemaining_ == 0) {
            switch (state_) {
                case HitmFighterState::kAttackStartup:
                    state_ = HitmFighterState::kAttackActive;
                    stateFramesRemaining_ = specialMove_.move_def.frames.active;
                    break;
                case HitmFighterState::kAttackActive:
                    state_ = HitmFighterState::kAttackRecovery;
                    stateFramesRemaining_ = specialMove_.move_def.frames.recovery;
                    break;
                case HitmFighterState::kAttackRecovery:
                case HitmFighterState::kHitstun:
                case HitmFighterState::kBlockstun:
                    state_ = grounded_ ? HitmFighterState::kIdle : HitmFighterState::kJumping;
                    break;
                default:
                    break;
            }
        }
    }

    // 5. Read-engine decay clock -- ticks every real frame regardless of
    // gameplay state, the literal reading of "stand still and the
    // knowledge goes stale" (nothing in the real data scopes decay to a
    // specific state).
    readEngine_.TickFrame();

    ++frame_;
}

void HitmFighterRuntime::TakeHit(const HitmMoveInstance& incoming, bool blocking) {
    combat::ReactionInput input;
    input.hit_power = static_cast<float>(incoming.move_def.power);
    input.defender_blocking = blocking;
    input.defender_already_staggered = (state_ == HitmFighterState::kHitstun);
    // Real, authored value -- this fighter's own defense_profile.
    // blockPreference stands in for COMBAT::ReactionSystem's
    // defense_bias concept (both describe "how much this fighter relies
    // on blocking vs. movement/deception"); GenomeDecoder's synthetic
    // DecisionWeights are deliberately not used here, per Track H's
    // standing rule against routing real data through the strawman.
    input.defense_bias = static_cast<float>(defenseBlockPreference_);
    // Real facing default -- no opponent entity exists in this vertical
    // slice to derive a real impact direction from, documented rather
    // than silently assumed elsewhere.
    lastReaction_ = combat::ReactionSystem::Determine(input, /*impactDirX=*/1.0f);

    hitstopFramesRemaining_ = HitstopFramesFor(incoming.hitstop_category);

    if (blocking) {
        meter_ = ClampMeter(meter_ + rules_.Meter().on_block_take);
        state_ = HitmFighterState::kBlockstun;
        stateFramesRemaining_ = incoming.blockstun_frames;
    } else {
        meter_ = ClampMeter(meter_ + rules_.Meter().on_hit_take);
        state_ = HitmFighterState::kHitstun;
        stateFramesRemaining_ = incoming.hitstun_frames;
    }
}

double HitmFighterRuntime::ResolveOutgoingDamage(const HitmMoveInstance& move) const {
    // The real authored law, implemented literally: "the read engine
    // multiplies OUTPUT, never the table."
    return move.move_def.power * readEngine_.CurrentDamageMultiplier();
}

void HitmFighterRuntime::ResolveOutgoingHitLanded(const HitmMoveInstance& move) {
    meter_ = ClampMeter(meter_ + move.meter_gain + rules_.Meter().on_hit_give);
}

double HitmFighterRuntime::ClampMeter(double value) const {
    double maxMeter = rules_.Meter().max;
    if (value < 0.0) return 0.0;
    if (value > maxMeter) return maxMeter;
    return value;
}

int HitmFighterRuntime::HitstopFramesFor(HitmHitstopCategory category) const {
    switch (category) {
        case HitmHitstopCategory::kLight:
            return static_cast<int>(rules_.Combat().hitstop_light);
        case HitmHitstopCategory::kHeavy:
            return static_cast<int>(rules_.Combat().hitstop_heavy);
        case HitmHitstopCategory::kCounter:
            return static_cast<int>(rules_.Combat().hitstop_counter);
    }
    return 0;  // unreachable -- HitmMoveInstance::Extract already validated the category
}

HitmFighterSnapshot HitmFighterRuntime::Snapshot() const {
    const auto* entity = world_.Entities().Find(entityId_);
    const auto* spatial = entity->GetComponent<world::SpatialComponent>();
    const auto* body = entity->GetComponent<physics::RigidBody>();

    HitmFighterSnapshot snap;
    snap.frame = frame_;
    snap.state = state_;
    snap.x = spatial->x;
    snap.y = spatial->y;
    snap.velocity_x = body->velocity_x;
    snap.velocity_y = body->velocity_y;
    snap.grounded = grounded_;
    snap.meter = meter_;
    snap.read_engine_reads = readEngine_.CurrentReads();
    snap.hitstop_frames_remaining = hitstopFramesRemaining_;
    snap.state_frames_remaining = stateFramesRemaining_;
    return snap;
}

}  // namespace dominus::character::hitm
