// CHARACTER/HitmBridge/HitmFighterRuntime.cpp
#include "CHARACTER/HitmBridge/HitmFighterRuntime.h"

#include <cmath>

#include "CORE/Serialization/MiniJson.h"
#include "PHYSICS/RigidBody.h"
#include "WORLD/Core/SpatialComponent.h"

namespace dominus::character::hitm {

using core::Result;

namespace {

// Real, hardcoded, uniform-across-every-fighter engine constant -- NOT
// authored per-fighter data (hitm-engine's own `Fighter.js:23`: "const hp
// = Math.round(1000 * def.stats.healthMult)"). Same category as
// COMBAT::kFramesPerSecond: a real literal from the real engine's own
// source, safe to port directly.
constexpr double kBaseHp = 1000.0;

// Real, per-fighter data: character_dna.json's own real
// `frames.healthMult` (confirmed present, and genuinely different, for
// all three real fighters -- Brooklyn 0.94, Rocket 1.09, Static 0.96 --
// see HITM_BROOKLYN_VS_ROCKET_PLAYABILITY_AUDIT.md's "HP/KO" finding).
// Already imported losslessly by Module 1 as
// HitmIdentityRecord::character_dna, but never extracted into a typed
// field before this change -- extracted here, locally to Module 5A,
// rather than reopening Module 2's HitmCombatGenome (per that audit's own
// "which modules should not be reopened" finding).
Result<double> ExtractHealthMult(const HitmIdentityRecord& record) {
    std::string context = "HitmFighterRuntime::Create: fighter '" + record.fighter_id +
                           "' character_dna.frames.healthMult";
    const core::json::Value* framesObj = record.character_dna.Get("frames");
    if (!framesObj || !framesObj->IsObject()) {
        return Result<double>::Fail(context + " -- character_dna.json has no 'frames' object");
    }
    const core::json::Value* v = framesObj->Get("healthMult");
    if (!v || !v->IsNumber()) {
        return Result<double>::Fail(context + " -- missing or not a number");
    }
    return Result<double>::Ok(v->AsNumber());
}

}  // namespace

Result<HitmFighterRuntime> HitmFighterRuntime::Create(const HitmIdentityRecord& record, const HitmCombatGenome& genome,
                                                        const HitmGameRules& rules) {
    if (record.fighter_id != genome.FighterId()) {
        return Result<HitmFighterRuntime>::Fail("HitmFighterRuntime::Create: identity fighter_id (" + record.fighter_id +
                                                  ") does not match genome fighter_id (" + genome.FighterId() + ")");
    }

    // Real data: not every fighter's real special extracts with today's
    // schema (Rocket's real "Ghost Dash" -- see HitmMoveInstance.h's own
    // top comment). No longer a Create()-blocking failure -- see this
    // header's top comment ("PHASE 3"). SpecialMove()/HasSpecialMove()
    // let a caller ask; kSpecial input is a real, documented no-op for a
    // fighter with none.
    auto moveResult = HitmMoveInstance::Extract(record, "special");
    std::optional<HitmMoveInstance> specialMove;
    if (moveResult.ok) {
        specialMove = std::move(*moveResult.value);
    }

    // Real data: not every fighter has a read engine -- Rocket and Static
    // genuinely do not (see combat_genome.json across all three real
    // fighters). That is real, verified information about them, not a
    // gap, so it is no longer a Create()-blocking failure -- see this
    // header's top comment ("PHASE 1"). GainRead()/LoseRead()/
    // ReadEngineState()/ResolveOutgoingDamage() below all degrade to
    // real, documented no-ops/defaults for a fighter with none.
    std::optional<HitmReadEngineState> readEngineState;
    if (const ReadEngine* readEngine = genome.GetReadEngine()) {
        readEngineState = HitmReadEngineState(*readEngine);
    }

    if (!genome.Defense().block_preference.has_value()) {
        return Result<HitmFighterRuntime>::Fail("HitmFighterRuntime::Create: fighter '" + record.fighter_id +
                                                  "' has no defense_profile.blockPreference in their real combat genome");
    }

    auto healthMultResult = ExtractHealthMult(record);
    if (!healthMultResult.ok) {
        return Result<HitmFighterRuntime>::Fail(healthMultResult.error);
    }
    // Real hardcoded engine constant x real per-fighter multiplier -- see
    // ExtractHealthMult/kBaseHp above. Phase 1 only: hp starts at maxHp
    // and stays there; nothing here reduces it yet.
    int maxHp = static_cast<int>(std::lround(kBaseHp * (*healthMultResult.value)));

    // Allocated ONCE, here, and never relocated for the rest of this
    // FrameState's life -- see this class's header comment for why that
    // is the actual fix for the move-safety bug this module found.
    auto state = std::make_unique<HitmFighterRuntime::FrameState>(
        rules, std::move(readEngineState), std::move(specialMove), *genome.Defense().block_preference, maxHp,
        record.fighter_id);

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

    // Captured before any of this frame's transitions run, compared again
    // at the bottom -- see the header's "A DELIBERATELY SCOPED EXTENSION"
    // comment. Deliberately a single before/after comparison, not a reset
    // at each of the (several) individual places `state.state` can change
    // below -- correct regardless of how many times state changes within
    // one real frame (e.g. a hypothetical 1-frame startup could transition
    // twice in a single call), and touches nothing else about how those
    // transitions already work.
    const HitmFighterState stateAtFrameStart = state.state;

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
            } else if (input == HitmInputCommand::kSpecial && state.specialMove.has_value()) {
                body->velocity_x = 0.0f;
                state.state = HitmFighterState::kAttackStartup;
                state.stateFramesRemaining = state.specialMove->move_def.frames.startup;
            } else {
                // Real, documented no-op for kSpecial on a fighter with
                // no working special (see header "PHASE 3") -- falls
                // through to the same "no input recognized" path as
                // kNeutral, exactly as real signature.json data
                // describes no buffered-input fallback for an
                // unavailable move.
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
        case HitmFighterState::kKO:
            // Locked: cannot walk, jump, or re-attack while committed to
            // an action, stunned, or KO'd. Input is dropped, not
            // buffered -- real signature.json data describes no
            // input-buffer system (game.json's own
            // combat.inputBufferFrames exists but this vertical slice
            // does not implement input buffering; a real, documented
            // gap, not a silent omission). A KO'd fighter still falls
            // via the real, unmodified gravity integration below (step
            // 2) and settles on the real ground exactly like any other
            // airborne fighter -- real behavior, not fabricated; only
            // the real engine's own extra `vy=-9.5` knockback pop is
            // deliberately not applied (see this class's header comment
            // "PHASE 2" / the kKO enumerator's own comment for why).
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
                    // Safe to dereference unchecked: only reachable via
                    // the kSpecial branch above, which already requires
                    // state.specialMove.has_value() to enter
                    // kAttackStartup in the first place.
                    state.state = HitmFighterState::kAttackActive;
                    state.stateFramesRemaining = state.specialMove->move_def.frames.active;
                    break;
                case HitmFighterState::kAttackActive:
                    state.state = HitmFighterState::kAttackRecovery;
                    state.stateFramesRemaining = state.specialMove->move_def.frames.recovery;
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
    // specific state). Real no-op for a fighter with no real read engine
    // (Rocket/Static) -- there is no decay clock to tick.
    if (state.readEngine) state.readEngine->TickFrame();

    // Net state change across this whole frame (see the comment above
    // `stateAtFrameStart`) -- 0 on the frame `state` lands on a new
    // value, otherwise one more frame in the same state.
    if (state.state != stateAtFrameStart) {
        state.stateFrame = 0;
    } else {
        ++state.stateFrame;
    }

    ++state.frame;
}

void HitmFighterRuntime::TakeHit(const HitmMoveInstance& incoming, bool blocking) {
    // Real no-op: a KO'd fighter cannot be hit again
    // (CombatSystem.js:385, `if(d.state===STATE.KO ...) return false;`).
    if (state_->state == HitmFighterState::kKO) return;

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

    // Real damage -> real hp. See this header's top comment ("PHASE 2")
    // for exactly which real pieces of the full formula this includes
    // (the move's own real power, real block-chip multiplier) and which
    // are deliberately excluded (attacker's read-engine multiplier, real
    // combo scaling, the real `atk.power` stat -- each a found,
    // documented gap, not a silent 1.0x). A direct port of
    // `CombatSystem.js:424,436`: `final=Math.round(damage*(blocking?
    // chipMult:1))`, `hp=Math.max(0,hp-final)`.
    double damage = incoming.move_def.power * (blocking ? state_->rules.Combat().chip_mult : 1.0);
    int finalDamage = static_cast<int>(std::lround(damage));
    state_->hp = state_->hp > finalDamage ? state_->hp - finalDamage : 0;

    // Real, unconditional -- CombatSystem.js:455 checks `hp<=0` right
    // after the hp reduction with no exemption for a blocked hit (see
    // this header's top comment for why that's the real engine's own
    // behavior, ported exactly).
    if (state_->hp <= 0) {
        state_->state = HitmFighterState::kKO;
    }

    // TakeHit mutates `state` synchronously, outside RunOneFrame's own
    // before/after transition tracking -- reset unconditionally (not
    // "only if the enum value actually changed"), because a fresh hit is
    // always a new reaction, even when it lands on a fighter already in
    // kHitstun (`input.defender_already_staggered` above) -- getting hit
    // again mid-hitstun restarts the hurt reaction from its own frame 0,
    // the same way a real fighting game's hit reaction always does.
    state_->stateFrame = 0;
}

double HitmFighterRuntime::ResolveOutgoingDamage(const HitmMoveInstance& move) const {
    // The real authored law, implemented literally: "the read engine
    // multiplies OUTPUT, never the table." A fighter with no real read
    // engine (Rocket/Static) has no table to multiply by -- real 1.0x,
    // not a guessed value.
    double multiplier = state_->readEngine ? state_->readEngine->CurrentDamageMultiplier() : 1.0;
    return move.move_def.power * multiplier;
}

void HitmFighterRuntime::ResolveOutgoingHitLanded(const HitmMoveInstance& move) {
    state_->meter = ClampMeter(*state_, state_->meter + move.meter_gain + state_->rules.Meter().on_hit_give);
}

void HitmFighterRuntime::GainRead() {
    // Real no-op for a fighter with no real read engine (Rocket/Static) --
    // not an error: there is nothing to gain.
    if (state_->readEngine) state_->readEngine->GainRead();
}
void HitmFighterRuntime::LoseRead() {
    if (state_->readEngine) state_->readEngine->LoseRead();
}

void HitmFighterRuntime::SetFacing(int facing) {
    // A direct port of the real engine's own rule (CombatSystem.js:488,
    // 509: `if (f.state!==ATTACK) f.facing = ...`) -- facing does not
    // change while this fighter is committed to any attack sub-state.
    // Real no-op, not a caller error, matching "facing locks for the
    // attack's duration." See this header's top comment ("PHASE 1") for
    // why every OTHER frame's real value is the caller's (a future
    // two-fighter match driver's) responsibility, not computed here.
    if (state_->state == HitmFighterState::kAttackStartup || state_->state == HitmFighterState::kAttackActive ||
        state_->state == HitmFighterState::kAttackRecovery) {
        return;
    }
    state_->facing = facing;
}

const HitmMoveInstance* HitmFighterRuntime::SpecialMove() const {
    return state_->specialMove ? &(*state_->specialMove) : nullptr;
}
bool HitmFighterRuntime::HasSpecialMove() const { return state_->specialMove.has_value(); }

void HitmFighterRuntime::ResetForNewRound(float x, float y, int facing) {
    auto* entity = state_->world.Entities().Find(state_->entityId);
    auto* spatial = entity->GetComponent<world::SpatialComponent>();
    auto* body = entity->GetComponent<physics::RigidBody>();

    spatial->x = x;
    spatial->y = y;
    body->velocity_x = 0.0f;
    body->velocity_y = 0.0f;

    // Real: hp back to max_hp, state back to idle, every countdown
    // cleared -- a direct port of the real engine's own resetRound()
    // (see this header's top comment "PHASE 3"). Deliberately does NOT
    // touch meter or the read engine's reads -- both genuinely persist
    // across real rounds within a real match (confirmed by direct read
    // of the real resetRound() body: neither field appears in it).
    state_->hp = state_->maxHp;
    state_->state = HitmFighterState::kIdle;
    state_->stateFramesRemaining = 0;
    // Mutates `state` synchronously, outside RunOneFrame's own
    // before/after transition tracking -- reset explicitly, same
    // discipline TakeHit() already uses for the identical reason.
    state_->stateFrame = 0;
    state_->hitstopFramesRemaining = 0;
    state_->grounded = true;
    state_->facing = facing;
}

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
    // 0 for a fighter with no real read engine (Rocket/Static) -- the same
    // real baseline value Brooklyn himself starts at, not a special case.
    snap.read_engine_reads = state_->readEngine ? state_->readEngine->CurrentReads() : 0;
    snap.hitstop_frames_remaining = state_->hitstopFramesRemaining;
    snap.state_frames_remaining = state_->stateFramesRemaining;
    snap.state_frame = state_->stateFrame;
    snap.max_hp = state_->maxHp;
    snap.hp = state_->hp;
    snap.facing = state_->facing;
    return snap;
}

HitmFighterState HitmFighterRuntime::State() const { return state_->state; }
const HitmReadEngineState* HitmFighterRuntime::ReadEngineState() const {
    return state_->readEngine ? &(*state_->readEngine) : nullptr;
}
bool HitmFighterRuntime::HasReadEngine() const { return state_->readEngine.has_value(); }
const std::string& HitmFighterRuntime::FighterId() const { return state_->fighterId; }
const combat::ReactionResult& HitmFighterRuntime::LastReaction() const { return state_->lastReaction; }

}  // namespace dominus::character::hitm
