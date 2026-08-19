// tests/combat/test_impact_provenance.cpp
// Phase 4.1.6: Record -> Serialize -> Reload -> Replay. If the same
// ImpactResult (by hash) comes back out after a full JSON round-trip,
// DOMINUS proves deterministic combat history -- not narrated, checked.
#include "CHARACTER/Genome/CombatPhysicsGenomeLoader.h"
#include "COMBAT/PhysicsCombat/ImpactSolver.h"
#include "COMBAT/Provenance/ImpactCanonicalSerializer.h"
#include "COMBAT/Provenance/ImpactEvent.h"
#include "COMBAT/Provenance/ImpactEventCompiler.h"
#include "COMBAT/Provenance/ImpactEventLog.h"
#include "COMBAT/Provenance/ImpactEventWorldHistoryHook.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CombatPhysicsGenomeLoader;
using dominus::combat::ImpactCanonicalSerializer;
using dominus::combat::ImpactContext;
using dominus::combat::ImpactEvent;
using dominus::combat::ImpactEventCompiler;
using dominus::combat::ImpactEventLog;
using dominus::combat::ImpactResult;
using dominus::combat::ImpactSolver;
using dominus::combat::RecordImpactEvent;
using dominus::world::WorldHistory;

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

ImpactContext RealBrooklynContext() {
    auto physics = CombatPhysicsGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_combat_physics.json");
    ImpactContext ctx;
    ctx.attacker_mass_kg = 90.0f;
    ctx.attacker_velocity = 8.0f;
    ctx.defender_durability = physics.value->impact.durability;
    ctx.defender_armor = physics.value->body.armor;
    ctx.struck_body_part = "torso";
    return ctx;
}
}  // namespace

// --- ImpactCanonicalSerializer: deterministic, same discipline as REGISTRY --

DOMINUS_TEST(ImpactCanonicalSerializer_SameContextProducesSameBytes) {
    ImpactContext a = RealBrooklynContext();
    ImpactContext b = RealBrooklynContext();
    DOMINUS_EXPECT(ImpactCanonicalSerializer::SerializeContext(a) == ImpactCanonicalSerializer::SerializeContext(b));
}

DOMINUS_TEST(ImpactCanonicalSerializer_DifferentContextProducesDifferentBytes) {
    ImpactContext a = RealBrooklynContext();
    ImpactContext b = RealBrooklynContext();
    b.attacker_velocity = 99.0f;
    DOMINUS_EXPECT(ImpactCanonicalSerializer::SerializeContext(a) != ImpactCanonicalSerializer::SerializeContext(b));
}

// --- ImpactEventCompiler: hash stage --------------------------------------

DOMINUS_TEST(ImpactEventCompiler_CompilesDeterministicHashes) {
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);

    auto eventA = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 100);
    auto eventB = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 100);

    DOMINUS_EXPECT(eventA.context_hash.size() == 64);  // SHA-256 hex digest
    DOMINUS_EXPECT(eventA.result_hash.size() == 64);
    DOMINUS_EXPECT(eventA.context_hash == eventB.context_hash);
    DOMINUS_EXPECT(eventA.result_hash == eventB.result_hash);
}

DOMINUS_TEST(ImpactEventCompiler_DifferentResultsProduceDifferentResultHash) {
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult resultA = ImpactSolver::Solve(ctx);

    ImpactContext harderHit = ctx;
    harderHit.attacker_velocity = 20.0f;
    ImpactResult resultB = ImpactSolver::Solve(harderHit);

    auto eventA = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, resultA, 100);
    auto eventB = ImpactEventCompiler::Compile("attacker_001", "brooklyn", harderHit, resultB, 100);

    DOMINUS_EXPECT(eventA.context_hash != eventB.context_hash);
    DOMINUS_EXPECT(eventA.result_hash != eventB.result_hash);
}

// --- Record -> Serialize -> Reload ------------------------------------------

DOMINUS_TEST(ImpactEventLog_RecordThenSerializeThenReload_RoundTripsExactly) {
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);
    auto event = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 48291);

    ImpactEventLog log;
    log.Record(event);
    DOMINUS_EXPECT(log.Count() == 1);

    std::string serialized = log.Serialize();
    DOMINUS_EXPECT(serialized.find("IMPACT") != std::string::npos);
    DOMINUS_EXPECT(serialized.find(event.context_hash) != std::string::npos);

    ImpactEventLog reloaded = ImpactEventLog::Deserialize(serialized);
    DOMINUS_EXPECT(reloaded.Count() == 1);

    const auto& r = reloaded.Events()[0];
    DOMINUS_EXPECT(r.event == event.event);
    DOMINUS_EXPECT(r.attacker == event.attacker);
    DOMINUS_EXPECT(r.target == event.target);
    DOMINUS_EXPECT(r.context_hash == event.context_hash);
    DOMINUS_EXPECT(r.result_hash == event.result_hash);
    DOMINUS_EXPECT(r.tick == event.tick);
}

DOMINUS_TEST(ImpactEventLog_ReloadOfMultipleEventsPreservesOrderAndCount) {
    ImpactEventLog log;
    ImpactContext ctx = RealBrooklynContext();
    for (std::uint64_t tick = 0; tick < 5; ++tick) {
        ImpactResult result = ImpactSolver::Solve(ctx);
        log.Record(ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, tick));
    }

    auto reloaded = ImpactEventLog::Deserialize(log.Serialize());
    DOMINUS_EXPECT(reloaded.Count() == 5);
    for (std::uint64_t tick = 0; tick < 5; ++tick) {
        DOMINUS_EXPECT(reloaded.Events()[tick].tick == tick);
    }
}

DOMINUS_TEST(ImpactEventLog_EventsForEntity_FindsBothAttackerAndTarget) {
    ImpactEventLog log;
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);
    log.Record(ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 1));
    log.Record(ImpactEventCompiler::Compile("brooklyn", "attacker_001", ctx, result, 2));  // brooklyn counters
    log.Record(ImpactEventCompiler::Compile("someone_else", "another_entity", ctx, result, 3));

    auto brooklynEvents = log.EventsForEntity("brooklyn");
    DOMINUS_EXPECT(brooklynEvents.size() == 2);  // once as target, once as attacker -- not the unrelated third event
}

// --- Replay: the actual determinism proof -----------------------------------

DOMINUS_TEST(ImpactEventCompiler_VerifyMatches_ReturnsTrueWhenReplayedFromSameContext) {
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult originalResult = ImpactSolver::Solve(ctx);
    auto event = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, originalResult, 48291);

    // Replay: solve the SAME context fresh, independently of the
    // original call above.
    ImpactResult replayedResult = ImpactSolver::Solve(ctx);
    DOMINUS_EXPECT(ImpactEventCompiler::VerifyMatches(event, ctx, replayedResult));
}

DOMINUS_TEST(ImpactEventCompiler_VerifyMatches_ReturnsFalseWhenContextDiffers) {
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);
    auto event = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 48291);

    ImpactContext differentCtx = ctx;
    differentCtx.attacker_velocity = 99.0f;
    ImpactResult differentResult = ImpactSolver::Solve(differentCtx);

    DOMINUS_EXPECT(!ImpactEventCompiler::VerifyMatches(event, differentCtx, differentResult));
}

DOMINUS_TEST(ImpactProvenance_FullCycle_RecordSerializeReloadReplayAllAgree) {
    // The complete chain the directive asked for, in one test: an
    // impact happens, gets recorded, the log is serialized to a JSON
    // string, a brand-new log is reloaded from that string (simulating
    // a process restart -- no shared state with the original log), and
    // the reloaded event's hashes are checked against a FRESH replay of
    // the original context through ImpactSolver::Solve(). If they
    // agree, deterministic combat history is proven, not asserted.
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);
    auto event = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 48291);

    ImpactEventLog originalLog;
    originalLog.Record(event);
    std::string serialized = originalLog.Serialize();

    ImpactEventLog reloadedLog = ImpactEventLog::Deserialize(serialized);
    DOMINUS_EXPECT(reloadedLog.Count() == 1);
    const ImpactEvent& reloadedEvent = reloadedLog.Events()[0];

    ImpactResult replayedResult = ImpactSolver::Solve(ctx);  // completely fresh call
    DOMINUS_EXPECT(ImpactEventCompiler::VerifyMatches(reloadedEvent, ctx, replayedResult));
}

// --- WorldHistory hook -------------------------------------------------------

DOMINUS_TEST(RecordImpactEvent_WritesARealEntryIntoWorldHistory) {
    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);
    auto event = ImpactEventCompiler::Compile("attacker_001", "brooklyn", ctx, result, 48291);

    WorldHistory history;
    DOMINUS_EXPECT(history.Count() == 0);
    RecordImpactEvent(history, event, /*tickTime=*/48291.0f);
    DOMINUS_EXPECT(history.Count() == 1);

    const auto& recorded = history.Events()[0];
    DOMINUS_EXPECT(recorded.event_type == "IMPACT");
    DOMINUS_EXPECT(recorded.entity_id == "attacker_001");
    DOMINUS_EXPECT(recorded.description.find("brooklyn") != std::string::npos);

    // The hashes must be genuinely recoverable from WorldHistory's own
    // generic consequences field -- not just narratively present.
    bool foundContextHash = false, foundResultHash = false, foundTarget = false;
    for (const auto& c : recorded.consequences) {
        if (c == "context_hash=" + event.context_hash) foundContextHash = true;
        if (c == "result_hash=" + event.result_hash) foundResultHash = true;
        if (c == "target=brooklyn") foundTarget = true;
    }
    DOMINUS_EXPECT(foundContextHash);
    DOMINUS_EXPECT(foundResultHash);
    DOMINUS_EXPECT(foundTarget);
}

DOMINUS_TEST(RecordImpactEvent_IsQueryableViaWorldHistorysExistingEventsForEntity) {
    // Proves the connection is real, not cosmetic: WorldHistory's own
    // pre-existing EventsForEntity query (Society Phase 1) finds the
    // impact with zero changes to WorldHistory itself.
    WorldHistory history;
    history.Record(0.0f, "entity_created", "brooklyn", "Brooklyn enters the city");

    ImpactContext ctx = RealBrooklynContext();
    ImpactResult result = ImpactSolver::Solve(ctx);
    auto event = ImpactEventCompiler::Compile("static", "brooklyn", ctx, result, 500);
    RecordImpactEvent(history, event, 500.0f);

    auto brooklynEvents = history.EventsForEntity("brooklyn");
    // brooklyn is the TARGET here, not the entity_id (attacker "static"
    // is) -- EventsForEntity only matches entity_id, so brooklyn's own
    // count stays at just his entity_created event. This is an honest
    // limitation shared with WorldHistory's pre-existing design (see
    // ImpactEventLog::EventsForEntity above for the richer
    // attacker-OR-target version this hook does NOT replace).
    DOMINUS_EXPECT(brooklynEvents.size() == 1);

    auto staticEvents = history.EventsForEntity("static");
    DOMINUS_EXPECT(staticEvents.size() == 1);
    DOMINUS_EXPECT(staticEvents[0].event_type == "IMPACT");
}
