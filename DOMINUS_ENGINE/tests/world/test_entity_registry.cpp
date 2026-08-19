// tests/world/test_entity_registry.cpp
#include "WORLD/Core/EntityRegistry.h"
#include "tests/TestFramework.h"

using dominus::core::MetaBinObject;
using dominus::world::EntityRegistry;

namespace {
struct TagComponent {
    std::string label;
};
}  // namespace

DOMINUS_TEST(EntityRegistry_CreateAndFindEntity) {
    EntityRegistry registry;
    MetaBinObject entity("tree_001", "0.1.0");
    registry.CreateEntity(std::move(entity));

    auto* found = registry.Find("tree_001");
    DOMINUS_EXPECT(found != nullptr);
    DOMINUS_EXPECT(found->Id() == "tree_001");
    DOMINUS_EXPECT(registry.Count() == 1);
}

DOMINUS_TEST(EntityRegistry_FindMissingReturnsNull) {
    EntityRegistry registry;
    DOMINUS_EXPECT(registry.Find("does_not_exist") == nullptr);
}

DOMINUS_TEST(EntityRegistry_RemoveEntity) {
    EntityRegistry registry;
    registry.CreateEntity(MetaBinObject("npc_204", "0.1.0"));
    DOMINUS_EXPECT(registry.Count() == 1);
    bool removed = registry.Remove("npc_204");
    DOMINUS_EXPECT(removed);
    DOMINUS_EXPECT(registry.Count() == 0);
    DOMINUS_EXPECT(!registry.Remove("npc_204"));  // already gone
}

DOMINUS_TEST(EntityRegistry_NoSpecialCasedEntityTypes) {
    // WORLD LAW 002: a "Tree", an "NPC", and a "Boss" are the exact same
    // container type with different components attached -- proven by
    // storing all three in the same registry with no type discrimination
    // anywhere in EntityRegistry's own code.
    EntityRegistry registry;

    MetaBinObject tree("tree_001", "0.1.0");
    tree.AddComponent<TagComponent>(TagComponent{"environment"});
    registry.CreateEntity(std::move(tree));

    MetaBinObject npc("citizen_204", "0.1.0");
    npc.AddComponent<TagComponent>(TagComponent{"npc"});
    registry.CreateEntity(std::move(npc));

    MetaBinObject boss("brooklyn", "0.1.0");
    boss.AddComponent<TagComponent>(TagComponent{"boss"});
    registry.CreateEntity(std::move(boss));

    DOMINUS_EXPECT(registry.Count() == 3);
    DOMINUS_EXPECT(registry.Find("tree_001")->GetComponent<TagComponent>()->label == "environment");
    DOMINUS_EXPECT(registry.Find("brooklyn")->GetComponent<TagComponent>()->label == "boss");
}

DOMINUS_TEST(EntityRegistry_WithComponentQueryFindsOnlyMatchingEntities) {
    EntityRegistry registry;
    MetaBinObject tagged("has_tag", "0.1.0");
    tagged.AddComponent<TagComponent>(TagComponent{"tagged"});
    registry.CreateEntity(std::move(tagged));
    registry.CreateEntity(MetaBinObject("no_tag", "0.1.0"));

    auto results = registry.WithComponent<TagComponent>();
    DOMINUS_EXPECT(results.size() == 1);
    DOMINUS_EXPECT(results[0]->Id() == "has_tag");
}

DOMINUS_TEST(EntityRegistry_AllIdsReturnsEveryEntity) {
    EntityRegistry registry;
    registry.CreateEntity(MetaBinObject("a", "0.1.0"));
    registry.CreateEntity(MetaBinObject("b", "0.1.0"));
    auto ids = registry.AllIds();
    DOMINUS_EXPECT(ids.size() == 2);
}
