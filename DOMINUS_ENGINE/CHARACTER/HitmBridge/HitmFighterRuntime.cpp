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

    // Allocated ONCE, here, and never relocated for the rest of this
    // FrameState's life -- see this class's header comment for why that
    // is the actual fix for the move-safety bug this module found.
    auto state = std::make_unique<HitmFighterRuntime::FrameState>(rules, HitmReadEngineState(*readEngine),
                                                                    *moveResult.value,
                                                                    *genome.Defense().block_preference, record.fighter_id);

    // Real integration with WORLD's real entity/component substrate (WORLD
    // LAW 002) -- not a bespoke position struct. Start position: centered
    // between the real wallL/wallR, standing on the real ground level.
    core::MetaBinObject entity(state->entityId, "0.1.0");
    entity.AddComponent<world::SpatialComponent>(world::SpatialComponent::Fighter2_5D(
        (static_cast<float>(state->rules.Physics().wall_l) + static_cast<float>(state->rules.Physics().wall_r)) / 2.0f,
        static_cast<float>(state->rules.Physics().ground)));

    physics::RigidBody body;
    body.mass = 1.0f;
    body.affected_by_gravity = true;
    entity.AddComponent<physics::RigidBody>(body);

    state->world.Entities().CreateEntity(std::move(entity));

    // Real WORLD LAW 003 plugin registration. Safe under every future
    // move of the HitmFighterRuntime wrapper because `raw` points at the
    // heap-allocated FrameState itself, not at the wrapper -- obtained
    // once, right here, before the FrameState is ever handed to anything
    // that could move it.
    HitmFighterRuntime::FrameState* raw = state.get();
    raw->world.Systems().RegisterSystem("hitm_fighter_frame",
                                         [raw](world::EntityRegistry&, float) { RunOneFrame(*raw, raw->pendingInput); });

    return Result<HitmFighterRuntime>::Ok(HitmFighterRuntime(std::move(state)));
}

HitmFighterRuntime::HitmFighterRuntime(std::unique_ptr<FrameState> state) : state_(std::move(state)) {}

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
    state_->pendingInput = input;
    state_->world.Tick(1.0f);
}

void HitmFighterRuntime::RunOneFrame(FrameState& state, HitmInputCommand input) {
    auto* entity = state.world.Entities().Find(state.entityId);
    auto* spatial = entity->GetComponent<world::SpatialComponent>();
    auto* body = entity->GetComponent<physics::RigidBody>();
    const auto& phys = state.rules.Physics();

    // Hit-freeze convention: while hitstop is active, nothing else in the
    // simulation advances -- not movement, not the attack/reaction state
    // clock, not the read-engine decay clock. Only the hitstop counter
    // itself ticks down.
    if (state.hitstopFramesRemaining > 0) {
        --state.hitstopFramesRemaining;
        ++state.frame;
        return;
    }

    // 1. Input-driven velocity / state transitions, before physics moves
    // anything this frame.
    switch (state.state) {
        case HitmFighterState::kIdle:
        case HitmFighterState::kWalking:
        case HitmFighterState::kBlockingStance:
            if (input == HitmInputCommand::kBlock) {
                body->velocity_x = 0.0f;
                state.state = HitmFighterState::kBlockingStance;
            } else if (input == HitmInputCommand::kLeft) {
                body->velocity_x = static_cast<float>(-phys.walk_speed);
                state.state = HitmFighterState::kWalking;
            } else if (input == HitmInputCommand::kRight) {
                body->velocity_x = static_cast<float>(phys.walk_speed);
                state.state = HitmFighterState::kWalking;
            } else if (input == HitmInputCommand::kJump && state.grounded) {
                body->velocity_x = 0.0f;
                body->velocity_y = static_cast<float>(phys.jump_vel);
                state.grounded = false;
                state.state = HitmFighterState::kJumping;
            } else if (input == HitmInputCommand::kSpecial) {
                body->velocity_x = 0.0f;
                state.state = HitmFighterState::kAttackStartup;
                state.stateFramesRemaining = state.specialMove.move_def.frames.startup;
            } else {
                body->velocity_x = 0.0f;
                state.state = HitmFighterState::kIdle;
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
    state.physics.Integrate(state.world.Entities(), 1.0f);

    // 3. Ground/wall clamp using the real authored bounds.
    if (spatial->y >= static_cast<float>(phys.ground)) {
        spatial->y = static_cast<float>(phys.ground);
        body->velocity_y = 0.0f;
        if (!state.grounded) {
            state.grounded = true;
            if (state.state == HitmFighterState::kJumping) {
                state.state = HitmFighterState::kIdle;
            }
        }
    }
    if (spatial->x < static_cast<float>(phys.wall_l)) spatial->x = static_cast<float>(phys.wall_l);
    if (spatial->x > static_cast<float>(phys.wall_r)) spatial->x = static_cast<float>(phys.wall_r);

    // 4. State-frame countdown, using the real per-move frame counts for
    // attack sub-states and the real per-hit hitstun/blockstun counts for
    // reaction states (set by TakeHit).
    if (state.stateFramesRemaining > 0) {
        --state.stateFramesRemaining;
        if (state.stateFramesRemaining == 0) {
            switch (state.state) {
                case HitmFighterState::kAttackStartup:
                    state.state = HitmFighterState::kAttackActive;
                    state.stateFramesRemaining = state.specialMove.move_def.frames.active;
                    break;
                case HitmFighterState::kAttackActive:
                    state.state = HitmFighterState::kAttackRecovery;
                    state.stateFramesRemaining = state.specialMove.move_def.frames.recovery;
                    break;
                case HitmFighterState::kAttackRecovery:
                case HitmFighterState::kHitstun:
                case HitmFighterState::kBlockstun:
                    state.state = state.grounded ? HitmFighterState::kIdle : HitmFighterState::kJumping;
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
    state.readEngine.TickFrame();

    ++state.frame;
}

void HitmFighterRuntime::TakeHit(const HitmMoveInstance& incoming, bool blocking) {
    combat::ReactionInput input;
    input.hit_power = static_cast<float>(incoming.move_def.power);
    input.defender_blocking = blocking;
    input.defender_already_staggered = (state_->state == HitmFighterState::kHitstun);
    // Real, authored value -- this fighter's own defense_profile.
    // blockPreference stands in for COMBAT::ReactionSystem's
    // defense_bias concept (both describe "how much this fighter relies
    // on blocking vs. movement/deception"); GenomeDecoder's synthetic
    // DecisionWeights are deliberately not used here, per Track H's
    // standing rule against routing real data through the strawman.
    input.defense_bias = static_cast<float>(state_->defenseBlockPreference);
    // Real facing default -- no opponent entity exists in this vertical
    // slice to derive a real impact direction from, documented rather
    // than silently assumed elsewhere.
    state_->lastReaction = combat::ReactionSystem::Determine(input, /*impactDirX=*/1.0f);

    state_->hitstopFramesRemaining = HitstopFramesFor(*state_, incoming.hitstop_category);

    if (blocking) {
        state_->meter = ClampMeter(*state_, state_->meter + state_->rules.Meter().on_block_take);
        state_->state = HitmFighterState::kBlockstun;
        state_->stateFramesRemaining = incoming.blockstun_frames;
    } else {
        state_->meter = ClampMeter(*state_, state_->meter + state_->rules.Meter().on_hit_take);
        state_->state = HitmFighterState::kHitstun;
        state_->stateFramesRemaining = incoming.hitstun_frames;
    }
}

double HitmFighterRuntime::ResolveOutgoingDamage(const HitmMoveInstance& move) const {
    // The real authored law, implemented literally: "the read engine
    // multiplies OUTPUT, never the table."
    return move.move_def.power * state_->readEngine.CurrentDamageMultiplier();
}

void HitmFighterRuntime::ResolveOutgoingHitLanded(const HitmMoveInstance& move) {
    state_->meter = ClampMeter(*state_, state_->meter + move.meter_gain + state_->rules.Meter().on_hit_give);
}

void HitmFighterRuntime::GainRead() { state_->readEngine.GainRead(); }
void HitmFighterRuntime::LoseRead() { state_->readEngine.LoseRead(); }

double HitmFighterRuntime::ClampMeter(const FrameState& state, double value) {
    double maxMeter = state.rules.Meter().max;
    if (value < 0.0) return 0.0;
    if (value > maxMeter) return maxMeter;
    return value;
}

int HitmFighterRuntime::HitstopFramesFor(const FrameState& state, HitmHitstopCategory category) {
    switch (category) {
        case HitmHitstopCategory::kLight:
            return static_cast<int>(state.rules.Combat().hitstop_light);
        case HitmHitstopCategory::kHeavy:
            return static_cast<int>(state.rules.Combat().hitstop_heavy);
        case HitmHitstopCategory::kCounter:
            return static_cast<int>(state.rules.Combat().hitstop_counter);
    }
    return 0;  // unreachable -- HitmMoveInstance::Extract already validated the category
}

HitmFighterSnapshot HitmFighterRuntime::Snapshot() const {
    const auto* entity = state_->world.Entities().Find(state_->entityId);
    const auto* spatial = entity->GetComponent<world::SpatialComponent>();
    const auto* body = entity->GetComponent<physics::RigidBody>();

    HitmFighterSnapshot snap;
    snap.frame = state_->frame;
    snap.state = state_->state;
    snap.x = spatial->x;
    snap.y = spatial->y;
    snap.velocity_x = body->velocity_x;
    snap.velocity_y = body->velocity_y;
    snap.grounded = state_->grounded;
    snap.meter = state_->meter;
    snap.read_engine_reads = state_->readEngine.CurrentReads();
    snap.hitstop_frames_remaining = state_->hitstopFramesRemaining;
    snap.state_frames_remaining = state_->stateFramesRemaining;
    return snap;
}

HitmFighterState HitmFighterRuntime::State() const { return state_->state; }
const HitmReadEngineState& HitmFighterRuntime::ReadEngineState() const { return state_->readEngine; }
const std::string& HitmFighterRuntime::FighterId() const { return state_->fighterId; }
const combat::ReactionResult& HitmFighterRuntime::LastReaction() const { return state_->lastReaction; }

}  // namespace dominus::character::hitm
