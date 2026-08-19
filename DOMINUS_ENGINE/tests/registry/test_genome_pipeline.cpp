// tests/registry/test_genome_pipeline.cpp
// The actual prototype proof requested: CombatGenome Source -> Validator
// -> Canonical Serializer -> SHA-256 Hash -> Immutable Artifact ->
// Registry Entry -> Runtime Snapshot, using Brooklyn's REAL combat_dna
// source data, not synthetic stand-ins. Each claim from the directive is
// its own test: deterministic compilation, stable hashes, registry
// lookup, version lineage, and runtime never touching authored data.
#include "CHARACTER/Genome/CombatIdentityLoader.h"
#include "CHARACTER/Genome/GenomeDecoder.h"
#include "CHARACTER/Genome/MaterialGenomeLoader.h"
#include "REGISTRY/CanonicalSerializer.h"
#include "REGISTRY/GenomeCompiler.h"
#include "REGISTRY/GenomeRegistry.h"
#include "REGISTRY/RuntimeSnapshot.h"
#include "tests/TestFramework.h"

#include <filesystem>

using dominus::character::CombatIdentity;
using dominus::character::CombatIdentityLoader;
using dominus::character::GenomeDecoder;
using dominus::character::MaterialGenomeLoader;
using dominus::registry::CanonicalSerializer;
using dominus::registry::GenomeCompiler;
using dominus::registry::GenomeKind;
using dominus::registry::GenomeRegistry;
using dominus::registry::SnapshotBuilder;

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

// --- CanonicalSerializer -----------------------------------------------------

DOMINUS_TEST(CanonicalSerializer_SameValuesProduceSameBytesRegardlessOfSourceFormatting) {
    // Two CombatIdentity instances with identical field values -- as if
    // loaded from two differently-whitespaced/differently-key-ordered
    // source files -- must serialize to byte-identical canonical output.
    CombatIdentity a{"style_x", "close", "relentless", "expert", "unpredictable", "medium"};
    CombatIdentity b{"style_x", "close", "relentless", "expert", "unpredictable", "medium"};
    DOMINUS_EXPECT(CanonicalSerializer::SerializeCombatGenome(a) == CanonicalSerializer::SerializeCombatGenome(b));
}

DOMINUS_TEST(CanonicalSerializer_DifferentValuesProduceDifferentBytes) {
    CombatIdentity a{"style_x", "close", "relentless", "expert", "unpredictable", "medium"};
    CombatIdentity b{"style_y", "close", "relentless", "expert", "unpredictable", "medium"};
    DOMINUS_EXPECT(CanonicalSerializer::SerializeCombatGenome(a) != CanonicalSerializer::SerializeCombatGenome(b));
}

// --- GenomeCompiler -----------------------------------------------------------

DOMINUS_TEST(GenomeCompiler_RejectsEmptyStyleAtValidatorStage) {
    CombatIdentity invalid{"", "close", "relentless", "expert", "unpredictable", "medium"};
    auto result = GenomeCompiler::CompileCombatGenome("test_entity", invalid, std::nullopt, 1, "2026-08-01T00:00:00Z");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.errors.empty());
    DOMINUS_EXPECT(!result.artifact.has_value());
}

DOMINUS_TEST(GenomeCompiler_CompilesRealBrooklynCombatGenome) {
    auto loadResult = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    DOMINUS_EXPECT(loadResult.ok);

    auto compileResult =
        GenomeCompiler::CompileCombatGenome("brooklyn", *loadResult.value, std::nullopt, 1, "2026-08-01T00:00:00Z");
    DOMINUS_EXPECT(compileResult.ok);
    DOMINUS_EXPECT(compileResult.artifact.has_value());
    DOMINUS_EXPECT(compileResult.artifact->Hash().size() == 64);  // real SHA-256 hex digest
    DOMINUS_EXPECT(compileResult.artifact->EntityId() == "brooklyn");
    DOMINUS_EXPECT(compileResult.artifact->VersionNumber() == 1);
    DOMINUS_EXPECT(!compileResult.artifact->ParentHash().has_value());  // v1 has no parent
}

// --- Claim 1: Deterministic compilation --------------------------------------

DOMINUS_TEST(Claim_DeterministicCompilation_SameSourceAlwaysProducesSameHash) {
    auto loadResult = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto compile1 = GenomeCompiler::CompileCombatGenome("brooklyn", *loadResult.value, std::nullopt, 1, "t1");
    auto compile2 = GenomeCompiler::CompileCombatGenome("brooklyn", *loadResult.value, std::nullopt, 1, "t2");
    // Same identity data compiled twice (even with a different
    // "compiled_at" timestamp) produces the identical hash -- the hash is
    // a function of CONTENT, not of when/how many times it was compiled.
    DOMINUS_EXPECT(compile1.artifact->Hash() == compile2.artifact->Hash());
}

DOMINUS_TEST(Claim_DeterministicCompilation_ReloadingFromDiskProducesSameHash) {
    // Loading the SAME file from disk twice (simulating two independent
    // build runs) and compiling both must agree.
    auto load1 = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto load2 = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto compile1 = GenomeCompiler::CompileCombatGenome("brooklyn", *load1.value, std::nullopt, 1, "t1");
    auto compile2 = GenomeCompiler::CompileCombatGenome("brooklyn", *load2.value, std::nullopt, 1, "t2");
    DOMINUS_EXPECT(compile1.artifact->Hash() == compile2.artifact->Hash());
}

// --- Claim 2: Hashes are stable / content-addressed --------------------------

DOMINUS_TEST(Claim_HashesAreStable_DifferentGenomesProduceDifferentHashes) {
    auto brooklyn = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto beastMode = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_beast_combat.json");
    auto compileA = GenomeCompiler::CompileCombatGenome("brooklyn", *brooklyn.value, std::nullopt, 1, "t");
    auto compileB = GenomeCompiler::CompileCombatGenome("brooklyn", *beastMode.value, std::nullopt, 2, "t");
    DOMINUS_EXPECT(compileA.artifact->Hash() != compileB.artifact->Hash());
}

// --- Claim 3: Registry lookup works ------------------------------------------

DOMINUS_TEST(Claim_RegistryLookup_FindReturnsExactArtifactByHash) {
    auto loadResult = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto compileResult = GenomeCompiler::CompileCombatGenome("brooklyn", *loadResult.value, std::nullopt, 1, "t");

    GenomeRegistry registry;
    registry.Register(*compileResult.artifact);

    auto* found = registry.Find(compileResult.artifact->Hash());
    DOMINUS_EXPECT(found != nullptr);
    DOMINUS_EXPECT(found->EntityId() == "brooklyn");
    DOMINUS_EXPECT(found->Hash() == compileResult.artifact->Hash());
}

DOMINUS_TEST(Claim_RegistryLookup_MissingHashReturnsNull) {
    GenomeRegistry registry;
    DOMINUS_EXPECT(registry.Find("0000000000000000000000000000000000000000000000000000000000000000") == nullptr);
}

DOMINUS_TEST(Claim_RegistryLookup_ReRegisteringIdenticalContentIsIdempotent) {
    auto loadResult = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto compileResult = GenomeCompiler::CompileCombatGenome("brooklyn", *loadResult.value, std::nullopt, 1, "t");

    GenomeRegistry registry;
    registry.Register(*compileResult.artifact);
    registry.Register(*compileResult.artifact);  // same content, same hash, registered again
    DOMINUS_EXPECT(registry.ArtifactCount() == 1);  // not duplicated
}

// --- Claim 4: Version lineage works ------------------------------------------

DOMINUS_TEST(Claim_VersionLineage_TracksOrderedParentChildChain) {
    auto v1Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto v1Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v1Load.value, std::nullopt, 1, "t1");

    GenomeRegistry registry;
    registry.Register(*v1Compile.artifact);

    auto v2Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_beast_combat.json");
    auto v2Compile =
        GenomeCompiler::CompileCombatGenome("brooklyn", *v2Load.value, v1Compile.artifact->Hash(), 2, "t2");
    registry.Register(*v2Compile.artifact);

    auto lineage = registry.Lineage("brooklyn");
    DOMINUS_EXPECT(lineage.size() == 2);
    DOMINUS_EXPECT(lineage[0] == v1Compile.artifact->Hash());
    DOMINUS_EXPECT(lineage[1] == v2Compile.artifact->Hash());
    DOMINUS_EXPECT(v2Compile.artifact->ParentHash().has_value());
    DOMINUS_EXPECT(*v2Compile.artifact->ParentHash() == v1Compile.artifact->Hash());
}

DOMINUS_TEST(Claim_VersionLineage_OlderVersionsRemainRetrievableAndUnchanged) {
    auto v1Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto v1Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v1Load.value, std::nullopt, 1, "t1");
    std::string v1Hash = v1Compile.artifact->Hash();
    std::string v1CanonicalBefore = v1Compile.artifact->CanonicalBytes();

    GenomeRegistry registry;
    registry.Register(*v1Compile.artifact);

    auto v2Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_beast_combat.json");
    auto v2Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v2Load.value, v1Hash, 2, "t2");
    registry.Register(*v2Compile.artifact);

    // v1 is still there, byte-for-byte, after v2 was registered -- nothing
    // was overwritten in place.
    auto* v1StillThere = registry.Find(v1Hash);
    DOMINUS_EXPECT(v1StillThere != nullptr);
    DOMINUS_EXPECT(v1StillThere->CanonicalBytes() == v1CanonicalBefore);

    auto* latest = registry.Latest("brooklyn");
    DOMINUS_EXPECT(latest != nullptr);
    DOMINUS_EXPECT(latest->Hash() == v2Compile.artifact->Hash());  // latest is v2, not v1
}

// --- Claim 5: Runtime never touches authored data ----------------------------

DOMINUS_TEST(Claim_RuntimeSnapshot_BuiltPurelyFromArtifactMatchesDirectDecode) {
    auto loadResult = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto compileResult = GenomeCompiler::CompileCombatGenome("brooklyn", *loadResult.value, std::nullopt, 1, "t");

    GenomeRegistry registry;
    registry.Register(*compileResult.artifact);

    // The snapshot is built from ONLY the registry-stored artifact -- no
    // access to loadResult.value (the original source) at this point.
    auto* stored = registry.Find(compileResult.artifact->Hash());
    auto snapshot = SnapshotBuilder::Build(*stored);

    // Its weights must still match what directly decoding the original
    // source would produce -- proving the bake-in at compile time was
    // correct, not just that SOME weights got produced.
    auto directWeights = GenomeDecoder::Decode(*loadResult.value);
    DOMINUS_EXPECT(snapshot.Weights().aggression == directWeights.aggression);
    DOMINUS_EXPECT(snapshot.Weights().counter_bias == directWeights.counter_bias);
    DOMINUS_EXPECT(snapshot.SourceHash() == compileResult.artifact->Hash());
}

DOMINUS_TEST(Claim_RuntimeSnapshot_TracesBackToCorrectVersionInLineage) {
    auto v1Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto v1Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v1Load.value, std::nullopt, 1, "t1");
    auto v2Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_beast_combat.json");
    auto v2Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v2Load.value, v1Compile.artifact->Hash(), 2, "t2");

    GenomeRegistry registry;
    registry.Register(*v1Compile.artifact);
    registry.Register(*v2Compile.artifact);

    // A snapshot built from v1's artifact must NOT reflect v2's (very
    // different, "reckless"/"high risk") genome values -- proving
    // snapshots are pinned to a specific immutable version, not "whatever
    // the latest happens to be".
    auto v1Snapshot = SnapshotBuilder::Build(*registry.Find(v1Compile.artifact->Hash()));
    auto v2Snapshot = SnapshotBuilder::Build(*registry.Find(v2Compile.artifact->Hash()));
    DOMINUS_EXPECT(v1Snapshot.Weights().aggression != v2Snapshot.Weights().aggression);
    DOMINUS_EXPECT(v1Snapshot.SourceHash() == v1Compile.artifact->Hash());
    DOMINUS_EXPECT(v2Snapshot.SourceHash() == v2Compile.artifact->Hash());
}

// =====================================================================
// MaterialGenome Authority Registration
//
// MaterialGenome already had a real, correct hash (MaterialGenomeCompiler
// -> CanonicalSerializer::SerializeMaterialGenome -> Sha256::Hash) --
// but GenomeRegistry previously registered/served CombatGenome only.
// This section proves MaterialGenome is now a first-class registered
// genome type through the SAME GenomeRegistry/ImmutableArtifact
// boundary CombatGenome already uses, with real, explicit type safety
// (GenomeKind) rather than reliance on hash-collision improbability.
// =====================================================================

DOMINUS_TEST(Material_ValidGenome_RegistersThroughGenomeRegistry) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    auto compileResult = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compileResult.ok);
    DOMINUS_EXPECT(compileResult.artifact.has_value());
    DOMINUS_EXPECT(compileResult.artifact->Kind() == GenomeKind::kMaterial);

    GenomeRegistry registry;
    bool registered = registry.Register(*compileResult.artifact);
    DOMINUS_EXPECT(registered);
    DOMINUS_EXPECT(registry.ArtifactCount() == 1);
}

DOMINUS_TEST(Material_RegisteredGenome_CanBeRetrievedByCanonicalLookup) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto compileResult = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(compileResult.ok);

    GenomeRegistry registry;
    registry.Register(*compileResult.artifact);

    // The registry's own canonical lookup mechanism -- Find(hash, kind)
    // -- must return the exact registered artifact, real content
    // intact, not a copy that dropped or corrupted the genome.
    const auto* found = registry.Find(compileResult.artifact->Hash(), GenomeKind::kMaterial);
    DOMINUS_EXPECT(found != nullptr);
    DOMINUS_EXPECT(found->Kind() == GenomeKind::kMaterial);
    DOMINUS_EXPECT(found->Material().material_id == load.value->material_id);
    DOMINUS_EXPECT(found->Material().identity.type == load.value->identity.type);
    DOMINUS_EXPECT(found->Material().properties.wear_state == load.value->properties.wear_state);
    DOMINUS_EXPECT(found->CanonicalBytes() == compileResult.artifact->CanonicalBytes());
}

DOMINUS_TEST(Material_EquivalentGenomeData_ProducesDeterministicIdentity) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);

    // Two independent compiles of the SAME real genome data -- real
    // determinism, not assumed.
    auto compileA = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t1");
    auto compileB = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", *load.value, std::nullopt, 1, "t2");
    DOMINUS_EXPECT(compileA.ok && compileB.ok);
    // compiledAt differs (t1 vs t2) -- proving identity is derived from
    // the genome's OWN canonical content, not incidentally from
    // whatever metadata happens to be passed alongside it (the same
    // discipline CombatGenome's own compile-twice test already
    // established).
    DOMINUS_EXPECT(compileA.artifact->Hash() == compileB.artifact->Hash());

    GenomeRegistry registry;
    registry.Register(*compileA.artifact);
    registry.Register(*compileB.artifact);
    // Idempotent, content-addressed: registering equivalent content
    // twice never creates a second entry.
    DOMINUS_EXPECT(registry.ArtifactCount() == 1);
}

DOMINUS_TEST(Material_MutatedField_ProducesDifferentIdentity_AndRegistryDoesNotConfuseThem) {
    auto load = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    DOMINUS_EXPECT(load.ok);
    auto original = *load.value;
    auto compileOriginal = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", original, std::nullopt, 1, "t1");

    // Change one real, meaningful field -- not cosmetic.
    auto modified = original;
    modified.properties.wear_state = original.properties.wear_state + 0.5f;
    auto compileModified = GenomeCompiler::CompileMaterialGenome("brooklyn_jacket", modified, std::nullopt, 1, "t1");

    DOMINUS_EXPECT(compileOriginal.ok && compileModified.ok);
    DOMINUS_EXPECT(compileOriginal.artifact->Hash() != compileModified.artifact->Hash());

    GenomeRegistry registry;
    registry.Register(*compileOriginal.artifact);
    registry.Register(*compileModified.artifact);
    // Real, distinct identities -- two real registry entries, not one
    // silently overwriting the other.
    DOMINUS_EXPECT(registry.ArtifactCount() == 2);
    const auto* foundOriginal = registry.Find(compileOriginal.artifact->Hash(), GenomeKind::kMaterial);
    const auto* foundModified = registry.Find(compileModified.artifact->Hash(), GenomeKind::kMaterial);
    DOMINUS_EXPECT(foundOriginal != nullptr && foundModified != nullptr);
    DOMINUS_EXPECT(foundOriginal->Material().properties.wear_state != foundModified->Material().properties.wear_state);
}

DOMINUS_TEST(Material_CombatGenomeRegistrationAndLookup_RemainUnchanged) {
    // Regression: every pre-existing CombatGenome call site (no
    // explicit GenomeKind argument at all) must behave exactly as it
    // did before MaterialGenome registration was added.
    auto v1Load = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto v1Compile = GenomeCompiler::CompileCombatGenome("brooklyn", *v1Load.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(v1Compile.ok);
    DOMINUS_EXPECT(v1Compile.artifact->Kind() == GenomeKind::kCombat);

    GenomeRegistry registry;
    registry.Register(*v1Compile.artifact);
    DOMINUS_EXPECT(registry.ArtifactCount() == 1);

    // Old, unqualified call syntax -- no GenomeKind argument -- must
    // still resolve to the real CombatGenome artifact.
    const auto* found = registry.Find(v1Compile.artifact->Hash());
    DOMINUS_EXPECT(found != nullptr);
    DOMINUS_EXPECT(found->Kind() == GenomeKind::kCombat);
    DOMINUS_EXPECT(found->Weights().aggression == v1Compile.artifact->Weights().aggression);

    auto lineage = registry.Lineage("brooklyn");
    DOMINUS_EXPECT(lineage.size() == 1);
    DOMINUS_EXPECT(lineage[0] == v1Compile.artifact->Hash());

    const auto* latest = registry.Latest("brooklyn");
    DOMINUS_EXPECT(latest != nullptr && latest->Hash() == v1Compile.artifact->Hash());
}

DOMINUS_TEST(Material_CrossTypeAuthority_NeverResolvesAsCombatGenome) {
    // The real, explicit type-safety mechanism this phase added:
    // composite (GenomeKind, hash) keys, not bare hash. Registers a
    // real CombatGenome and a real MaterialGenome for the SAME
    // entityId ("brooklyn" has both, legitimately) and proves neither
    // Find nor Lineage nor Latest ever cross-resolves one as the
    // other, even though both share an entity id and both live in the
    // same registry instance.
    auto combatLoad = CombatIdentityLoader::LoadFromFile(FixtureDir() / "brooklyn_combat.json");
    auto combatCompile = GenomeCompiler::CompileCombatGenome("brooklyn", *combatLoad.value, std::nullopt, 1, "t1");
    auto materialLoad = MaterialGenomeLoader::LoadFromFile(FixtureDir() / "brooklyn_jacket_material.json");
    auto materialCompile =
        GenomeCompiler::CompileMaterialGenome("brooklyn", *materialLoad.value, std::nullopt, 1, "t1");
    DOMINUS_EXPECT(combatCompile.ok && materialCompile.ok);

    GenomeRegistry registry;
    registry.Register(*combatCompile.artifact);
    registry.Register(*materialCompile.artifact);
    DOMINUS_EXPECT(registry.ArtifactCount() == 2);

    // Explicit-kind lookups resolve to the correct, real kind only.
    const auto* combatFound = registry.Find(combatCompile.artifact->Hash(), GenomeKind::kCombat);
    const auto* materialFound = registry.Find(materialCompile.artifact->Hash(), GenomeKind::kMaterial);
    DOMINUS_EXPECT(combatFound != nullptr && combatFound->Kind() == GenomeKind::kCombat);
    DOMINUS_EXPECT(materialFound != nullptr && materialFound->Kind() == GenomeKind::kMaterial);

    // A material hash looked up under kCombat must not resolve (unless
    // the two genuinely happen to share a hash, which real, distinct
    // canonical content makes practically impossible -- confirmed
    // directly: the two hashes are not equal).
    DOMINUS_EXPECT(combatCompile.artifact->Hash() != materialCompile.artifact->Hash());
    DOMINUS_EXPECT(registry.Find(materialCompile.artifact->Hash(), GenomeKind::kCombat) == nullptr);
    DOMINUS_EXPECT(registry.Find(combatCompile.artifact->Hash(), GenomeKind::kMaterial) == nullptr);

    // Lineage/Latest are ALSO kind-scoped -- "brooklyn" has one real
    // combat lineage entry and one real, completely separate material
    // lineage entry, never merged into one mixed-type list.
    auto combatLineage = registry.Lineage("brooklyn", GenomeKind::kCombat);
    auto materialLineage = registry.Lineage("brooklyn", GenomeKind::kMaterial);
    DOMINUS_EXPECT(combatLineage.size() == 1 && combatLineage[0] == combatCompile.artifact->Hash());
    DOMINUS_EXPECT(materialLineage.size() == 1 && materialLineage[0] == materialCompile.artifact->Hash());

    const auto* latestCombat = registry.Latest("brooklyn", GenomeKind::kCombat);
    const auto* latestMaterial = registry.Latest("brooklyn", GenomeKind::kMaterial);
    DOMINUS_EXPECT(latestCombat != nullptr && latestCombat->Kind() == GenomeKind::kCombat);
    DOMINUS_EXPECT(latestMaterial != nullptr && latestMaterial->Kind() == GenomeKind::kMaterial);
}
