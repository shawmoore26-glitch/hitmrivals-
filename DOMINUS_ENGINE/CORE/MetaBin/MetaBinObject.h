// CORE/MetaBin/MetaBinObject.h
// The atomic runtime unit of the engine: a component-carrying object graph
// node loaded from a single .dominus file. See docs/ARCHITECTURE_v0.1.md
// section 4 for the API surface this must satisfy, and section 5 for the
// on-disk schema this is built from.
#pragma once

#include <any>
#include <cstdint>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace dominus::core {

using ObjectId = std::string;
using ObjectVersion = std::string;  // semver, e.g. "0.1.0"
using ComponentTypeId = std::type_index;

// A MetaBinObject is a bag of components keyed by compile-time type --
// no dynamic_cast, no RTTI walk. This is the "character is not a model"
// principle made literal: a fully-loaded fighter has Mesh, Skeleton,
// AnimationSet, CombatGenome, AiBehavior, PhysicsRules, Audio, and Lore
// components all attached to one object, one id, one version.
class MetaBinObject {
public:
    MetaBinObject(ObjectId id, ObjectVersion version)
        : id_(std::move(id)), version_(std::move(version)) {}

    const ObjectId& Id() const { return id_; }
    const ObjectVersion& Version() const { return version_; }

    template <typename Component>
    Component& AddComponent(Component data) {
        auto key = std::type_index(typeid(Component));
        auto [it, inserted] = components_.emplace(key, std::move(data));
        return std::any_cast<Component&>(it->second);
    }

    template <typename Component>
    Component* GetComponent() {
        auto key = std::type_index(typeid(Component));
        auto it = components_.find(key);
        if (it == components_.end()) return nullptr;
        return std::any_cast<Component>(&it->second);
    }

    template <typename Component>
    const Component* GetComponent() const {
        auto key = std::type_index(typeid(Component));
        auto it = components_.find(key);
        if (it == components_.end()) return nullptr;
        return std::any_cast<Component>(&it->second);
    }

    bool HasComponent(ComponentTypeId type) const {
        return components_.find(type) != components_.end();
    }

    size_t ComponentCount() const { return components_.size(); }

private:
    ObjectId id_;
    ObjectVersion version_;
    std::unordered_map<ComponentTypeId, std::any> components_;
};

// Minimal component set for Phase 1 proof (identity + a stand-in payload
// component). Real Mesh/Skeleton/CombatGenome/etc. component types are
// defined by their owning modules (GRAPHICS, ANIMATION, COMBAT, ...) in
// Phase 2+, not here -- CORE/MetaBin never depends on a leaf module's
// component type, only on the generic AddComponent/GetComponent contract.
struct IdentityComponent {
    std::string display_name;
    std::string faction;
};

struct RawRefComponent {
    std::string ref_path;
};

// Generic path-only refs for skeleton/animation data. CORE deliberately does
// not know what a "bone" or a "clip" is -- it only carries the ref path
// through from the .dominus file. ANIMATION owns the actual Skeleton/
// AnimationClip types and their loaders (see ANIMATION/SkeletonSystem);
// CHARACTER/Rig is what resolves these refs into real components at bind
// time. This keeps the module boundary from docs/ARCHITECTURE_v0.1.md
// section 3 intact: CORE never depends on a leaf module's data types.
struct SkeletonRefComponent {
    std::string ref_path;
};

struct NamedAnimationRef {
    std::string name;
    std::string ref_path;
};

struct AnimationRefListComponent {
    std::vector<NamedAnimationRef> clips;
};

// Reuses the same {name, ref_path} shape for IK chain definitions and for
// the motion graph ref -- CORE only ever carries path strings for any of
// these, never solver-specific data.
struct IKChainRefListComponent {
    std::vector<NamedAnimationRef> chains;
};

struct MotionGraphRefComponent {
    std::string ref_path;
};

struct RetargetMapRefComponent {
    std::string ref_path;
};

// --- COMBAT GENOME LAWS C002/C005/C006: everything below is a path ref
// only -- CORE never knows what "frame data" or "hitbox" mean, same
// pattern as skeleton/animation/motion_graph refs above.
struct CombatDnaRefComponent {
    std::string ref_path;  // combat.dominus-style identity (style/range/pressure/...)
};

struct MoveRefListComponent {
    std::vector<NamedAnimationRef> moves;  // {name, ref} -- reused generic shape
};

struct HurtboxRefComponent {
    std::string ref_path;
};

struct TransformationRefListComponent {
    std::vector<NamedAnimationRef> transformations;  // {name, ref} -- reused generic shape
};

// --- Universal Entity Model additions: entity_type, provenance, and a
// ref to a social genome. All three are pure metadata/path-refs, same
// discipline as everything above -- CORE knows an entity CAN have a
// type tag, a birth certificate, and a social genome ref; it doesn't
// know what "organism" or "rival" mean.

// Free-form tag ("organism", "character", "npc", "economy", "city", ...)
// -- CORE doesn't own or validate this vocabulary. No entity_type
// implies any behavior by itself; it's metadata a query or an extension
// can filter on, nothing more, until something is actually built that
// interprets it (an Economy Engine reading entity_type=="economy", for
// instance -- not attempted here).
struct EntityTypeComponent {
    std::string entity_type;
};

// The "birth certificate": who/what created this entity, from what
// assets, under what method, descended from which parent entities.
// `creation_hash` is free-form -- it can be (and, once the Registry
// Prototype is wired in for a given entity, typically will be) a real
// SHA-256 digest from REGISTRY::ImmutableArtifact::Hash(), but
// Provenance itself has zero dependency on REGISTRY; it just carries
// the string.
struct ProvenanceComponent {
    std::string creator;
    std::string creation_method;
    std::vector<std::string> source_assets;
    std::vector<std::string> parent_entities;
    std::string creation_hash;
};

// Ref-only, same pattern as CombatDnaRefComponent -- CORE carries the
// path, CHARACTER::RigBinder resolves it into a real SocialGenome
// component (see CHARACTER/Genome/SocialGenome.h).
struct SocialGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern as SocialGenomeRefComponent -- CORE carries the
// path, CHARACTER::RigBinder resolves it into a real CreatureGenome
// component (see CHARACTER/Genome/CreatureGenome.h).
struct CreatureGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern as SocialGenomeRefComponent/
// CreatureGenomeRefComponent -- CORE carries the path,
// CHARACTER::RigBinder resolves it into a real VisualGenome component
// (see CHARACTER/Genome/VisualGenome.h). Closes the "not bound yet" gap
// flagged when VisualGenome was first built (DORRE).
struct VisualGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern as SocialGenomeRefComponent/CreatureGenomeRefComponent/
// VisualGenomeRefComponent -- CORE carries the path, CHARACTER::RigBinder
// resolves it into a real CombatStyleGenome component (see
// CHARACTER/Genome/CombatStyleGenome.h). Closes the "not bound yet" gap
// flagged at the end of REALITY DESCRIPTION FOUNDATION.
struct CombatStyleGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern -- resolves into a real CombatPhysicsGenome
// component (see CHARACTER/Genome/CombatPhysicsGenome.h).
struct CombatPhysicsGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern -- resolves into a real GameDesignGenome
// component (see CHARACTER/Genome/GameDesignGenome.h).
struct GameDesignGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern -- resolves into a real MaterialGenome
// component (see CHARACTER/Genome/MaterialGenome.h).
struct MaterialGenomeRefComponent {
    std::string ref_path;
};

// Ref-only, same pattern -- resolves into a real VisualStyleGenome
// component (see CHARACTER/Genome/VisualStyleGenome.h).
struct VisualStyleGenomeRefComponent {
    std::string ref_path;
};

}  // namespace dominus::core
