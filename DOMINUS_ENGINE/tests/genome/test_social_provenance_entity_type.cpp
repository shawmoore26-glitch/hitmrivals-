// tests/genome/test_social_provenance_entity_type.cpp
// The Universal Entity Model additions: entity_type tag, provenance
// (the "birth certificate"), social genome (relationships + personality),
// and WorldHistory's consequences/EventsForEntity extension. Each tested
// both in isolation (synthetic data) and against Brooklyn's real fixture.
#include "CHARACTER/Genome/SocialGenomeLoader.h"
#include "CHARACTER/Rig/RigBinder.h"
#include "COMBAT/HitSystem/CombatBinder.h"
#include "CORE/Serialization/DominusSerializer.h"
#include "WORLD/Core/WorldHistory.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>

using dominus::character::RigBinder;
using dominus::character::SocialGenomeComponent;
using dominus::character::SocialGenomeLoader;
using dominus::combat::CombatBinder;
using dominus::core::DominusSerializer;
using dominus::core::EntityTypeComponent;
using dominus::core::ProvenanceComponent;
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
}  // namespace

// --- SocialGenomeLoader -------------------------------------------------------

DOMINUS_TEST(SocialGenomeLoader_LoadsRealBrooklynSocialGenome) {
    auto result = SocialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_social.json");
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->personality.trust > 0.29f && result.value->personality.trust < 0.31f);
    DOMINUS_EXPECT(result.value->personality.aggression > 0.79f);
    DOMINUS_EXPECT(result.value->relationships.size() == 2);
    DOMINUS_EXPECT(result.value->relationships[0].entity_id == "iron_wolves_leader");
    DOMINUS_EXPECT(result.value->relationships[0].relation == "rival");
    DOMINUS_EXPECT(result.value->relationships[0].strength < 0.0f);  // signed -- rival reads as negative
    DOMINUS_EXPECT(result.value->relationships[1].relation == "ally");
    DOMINUS_EXPECT(result.value->relationships[1].strength > 0.0f);
}

DOMINUS_TEST(SocialGenomeLoader_MissingFileFailsGracefully) {
    auto result = SocialGenomeLoader::LoadFromFile(FixtureDir() / "does_not_exist_social.json");
    DOMINUS_EXPECT(!result.ok);
}

DOMINUS_TEST(SocialGenomeLoader_DefaultsApplyWhenFieldsMissing) {
    // A minimal/empty social genome file should still load with sane
    // defaults, not fail -- social data is optional richness, not a
    // hard requirement the way CombatIdentity's 'style' is.
    auto path = std::filesystem::temp_directory_path() / "dominus_minimal_social_test.json";
    {
        std::ofstream out(path);
        out << "{}";
    }
    auto result = SocialGenomeLoader::LoadFromFile(path);
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->personality.trust > 0.49f && result.value->personality.trust < 0.51f);
    DOMINUS_EXPECT(result.value->relationships.empty());
    std::filesystem::remove(path);
}

// --- Full pipeline: entity_type + provenance + social genome, real Brooklyn --

DOMINUS_TEST(UniversalEntityModel_BrooklynCarriesEntityTypeProvenanceAndSocialGenome) {
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "brooklyn.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    auto& brooklyn = *loadResult.value;

    // entity_type and provenance are parsed by DominusSerializer directly
    // -- no binding required, same as IdentityComponent.
    auto* entityType = brooklyn.GetComponent<EntityTypeComponent>();
    DOMINUS_EXPECT(entityType != nullptr);
    DOMINUS_EXPECT(entityType->entity_type == "character");

    auto* provenance = brooklyn.GetComponent<ProvenanceComponent>();
    DOMINUS_EXPECT(provenance != nullptr);
    DOMINUS_EXPECT(provenance->creator == "Shawn");
    DOMINUS_EXPECT(provenance->creation_method == "hitm-character-forge");
    DOMINUS_EXPECT(provenance->source_assets.size() == 2);
    DOMINUS_EXPECT(provenance->source_assets[0] == "brooklyn_art_v3");
    DOMINUS_EXPECT(provenance->parent_entities.empty());
    // The creation_hash is the REAL, independently-verified SHA-256
    // digest of Brooklyn's canonical combat genome (see the Registry
    // Prototype) -- not a placeholder string.
    DOMINUS_EXPECT(provenance->creation_hash.size() == 64);

    // social_genome requires binding (it's a ref, resolved by RigBinder).
    DOMINUS_EXPECT(RigBinder::Bind(brooklyn, fixtureDir).ok);
    DOMINUS_EXPECT(CombatBinder::Bind(brooklyn, fixtureDir).ok);

    auto* social = brooklyn.GetComponent<SocialGenomeComponent>();
    DOMINUS_EXPECT(social != nullptr);
    DOMINUS_EXPECT(social->genome.relationships.size() == 2);
    DOMINUS_EXPECT(social->genome.relationships[0].relation == "rival");
}

DOMINUS_TEST(UniversalEntityModel_EntityWithoutTheseFieldsStillLoadsFine) {
    // Backward compatibility: an entity with none of the new fields
    // (e.g. ik_test_rig.dominus, unchanged) must still load with zero
    // errors and simply lack the new components.
    auto fixtureDir = FixtureDir();
    auto loadResult = DominusSerializer::Load(fixtureDir / "ik_test_rig.dominus");
    DOMINUS_EXPECT(loadResult.ok);
    DOMINUS_EXPECT(loadResult.value->GetComponent<EntityTypeComponent>() == nullptr);
    DOMINUS_EXPECT(loadResult.value->GetComponent<ProvenanceComponent>() == nullptr);
}

// --- WorldHistory extensions --------------------------------------------------

DOMINUS_TEST(WorldHistory_RecordWithConsequences) {
    WorldHistory history;
    history.Record(5.0f, "combat", "brooklyn", "Brooklyn defeated the Iron Wolves",
                    {"reputation +20", "enemy faction created"});
    DOMINUS_EXPECT(history.Count() == 1);
    DOMINUS_EXPECT(history.Events()[0].consequences.size() == 2);
    DOMINUS_EXPECT(history.Events()[0].consequences[0] == "reputation +20");
}

DOMINUS_TEST(WorldHistory_RecordWithoutConsequencesDefaultsToEmpty) {
    // Backward compatibility with the Society Phase 0 call shape (4 args,
    // no consequences) -- must still compile and produce an empty vector.
    WorldHistory history;
    history.Record(1.0f, "entity_created", "brooklyn", "Brooklyn enters the city");
    DOMINUS_EXPECT(history.Events()[0].consequences.empty());
}

DOMINUS_TEST(WorldHistory_EventsForEntityFiltersCorrectly) {
    WorldHistory history;
    history.Record(1.0f, "entity_created", "brooklyn", "Brooklyn enters the city");
    history.Record(2.0f, "entity_created", "gang_member_04", "A rival appears");
    history.Record(5.0f, "combat", "brooklyn", "Brooklyn defeated the Iron Wolves", {"reputation +20"});

    auto brooklynEvents = history.EventsForEntity("brooklyn");
    DOMINUS_EXPECT(brooklynEvents.size() == 2);
    DOMINUS_EXPECT(brooklynEvents[0].description == "Brooklyn enters the city");
    DOMINUS_EXPECT(brooklynEvents[1].consequences.size() == 1);

    auto noEvents = history.EventsForEntity("nobody");
    DOMINUS_EXPECT(noEvents.empty());
}
