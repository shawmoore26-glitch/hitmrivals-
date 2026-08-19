// CORE/Serialization/DominusSerializer.cpp
#include "CORE/Serialization/DominusSerializer.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::core {

using json::Value;

namespace {

std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

Result<MetaBinObject> DominusSerializer::Load(const std::filesystem::path& dominusFile) {
    std::string text;
    try {
        text = ReadFile(dominusFile);
    } catch (const std::exception& e) {
        return Result<MetaBinObject>::Fail(e.what());
    }

    std::vector<std::string> errors;
    if (!Validate(text, &errors)) {
        std::string joined;
        for (auto& e : errors) joined += e + "; ";
        return Result<MetaBinObject>::Fail("Validation failed: " + joined);
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<MetaBinObject>::Fail(std::string("Parse error: ") + e.what());
    }

    std::string objectId = root.Get("object_id")->AsString();
    std::string version = root.Get("dominus_version")->AsString();

    MetaBinObject obj(objectId, version);

    const Value* identity = root.Get("identity");
    if (identity && identity->IsObject()) {
        IdentityComponent id{};
        if (auto* name = identity->Get("display_name")) id.display_name = name->AsString();
        if (auto* faction = identity->Get("faction")) id.faction = faction->AsString();
        obj.AddComponent<IdentityComponent>(std::move(id));
    }

    if (const Value* entityType = root.Get("entity_type")) {
        if (entityType->IsString()) {
            obj.AddComponent<EntityTypeComponent>(EntityTypeComponent{entityType->AsString()});
        }
    }

    if (const Value* provenance = root.Get("provenance")) {
        if (provenance->IsObject()) {
            ProvenanceComponent prov{};
            if (auto* creator = provenance->Get("creator")) prov.creator = creator->AsString();
            if (auto* method = provenance->Get("creation_method")) prov.creation_method = method->AsString();
            if (auto* hash = provenance->Get("creation_hash")) prov.creation_hash = hash->AsString();
            if (auto* assets = provenance->Get("source_assets")) {
                if (assets->IsArray()) {
                    for (const Value& entry : assets->AsArray()) {
                        if (entry.IsString()) prov.source_assets.push_back(entry.AsString());
                    }
                }
            }
            if (auto* parents = provenance->Get("parent_entities")) {
                if (parents->IsArray()) {
                    for (const Value& entry : parents->AsArray()) {
                        if (entry.IsString()) prov.parent_entities.push_back(entry.AsString());
                    }
                }
            }
            obj.AddComponent<ProvenanceComponent>(std::move(prov));
        }
    }

    if (const Value* socialGenome = root.Get("social_genome")) {
        if (auto* ref = socialGenome->Get("ref")) {
            obj.AddComponent<SocialGenomeRefComponent>(SocialGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* creatureGenome = root.Get("creature_genome")) {
        if (auto* ref = creatureGenome->Get("ref")) {
            obj.AddComponent<CreatureGenomeRefComponent>(CreatureGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* visualGenome = root.Get("visual_genome")) {
        if (auto* ref = visualGenome->Get("ref")) {
            obj.AddComponent<VisualGenomeRefComponent>(VisualGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* combatStyleGenome = root.Get("combat_style_genome")) {
        if (auto* ref = combatStyleGenome->Get("ref")) {
            obj.AddComponent<CombatStyleGenomeRefComponent>(CombatStyleGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* combatPhysicsGenome = root.Get("combat_physics_genome")) {
        if (auto* ref = combatPhysicsGenome->Get("ref")) {
            obj.AddComponent<CombatPhysicsGenomeRefComponent>(CombatPhysicsGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* gameDesignGenome = root.Get("game_design_genome")) {
        if (auto* ref = gameDesignGenome->Get("ref")) {
            obj.AddComponent<GameDesignGenomeRefComponent>(GameDesignGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* materialGenome = root.Get("material_genome")) {
        if (auto* ref = materialGenome->Get("ref")) {
            obj.AddComponent<MaterialGenomeRefComponent>(MaterialGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* visualStyleGenome = root.Get("visual_style_genome")) {
        if (auto* ref = visualStyleGenome->Get("ref")) {
            obj.AddComponent<VisualStyleGenomeRefComponent>(VisualStyleGenomeRefComponent{ref->AsString()});
        }
    }

    if (const Value* mesh = root.Get("mesh")) {
        if (auto* ref = mesh->Get("ref")) {
            obj.AddComponent<RawRefComponent>(RawRefComponent{ref->AsString()});
        }
    }

    if (const Value* skeleton = root.Get("skeleton")) {
        if (auto* ref = skeleton->Get("ref")) {
            obj.AddComponent<SkeletonRefComponent>(SkeletonRefComponent{ref->AsString()});
        }
    }

    if (const Value* animations = root.Get("animations")) {
        if (animations->IsArray()) {
            AnimationRefListComponent list;
            for (const Value& entry : animations->AsArray()) {
                auto* name = entry.Get("name");
                auto* ref = entry.Get("ref");
                if (name && ref) {
                    list.clips.push_back(NamedAnimationRef{name->AsString(), ref->AsString()});
                }
            }
            obj.AddComponent<AnimationRefListComponent>(std::move(list));
        }
    }

    if (const Value* ikChains = root.Get("ik_chains")) {
        if (ikChains->IsArray()) {
            IKChainRefListComponent list;
            for (const Value& entry : ikChains->AsArray()) {
                auto* name = entry.Get("name");
                auto* ref = entry.Get("ref");
                if (name && ref) {
                    list.chains.push_back(NamedAnimationRef{name->AsString(), ref->AsString()});
                }
            }
            obj.AddComponent<IKChainRefListComponent>(std::move(list));
        }
    }

    if (const Value* motionGraph = root.Get("motion_graph")) {
        if (auto* ref = motionGraph->Get("ref")) {
            obj.AddComponent<MotionGraphRefComponent>(MotionGraphRefComponent{ref->AsString()});
        }
    }

    if (const Value* retargetMap = root.Get("retarget_map")) {
        if (auto* ref = retargetMap->Get("ref")) {
            obj.AddComponent<RetargetMapRefComponent>(RetargetMapRefComponent{ref->AsString()});
        }
    }

    if (const Value* combatDna = root.Get("combat_dna")) {
        if (auto* ref = combatDna->Get("ref")) {
            obj.AddComponent<CombatDnaRefComponent>(CombatDnaRefComponent{ref->AsString()});
        }
    }

    if (const Value* moves = root.Get("moves")) {
        if (moves->IsArray()) {
            MoveRefListComponent list;
            for (const Value& entry : moves->AsArray()) {
                auto* name = entry.Get("name");
                auto* ref = entry.Get("ref");
                if (name && ref) list.moves.push_back(NamedAnimationRef{name->AsString(), ref->AsString()});
            }
            obj.AddComponent<MoveRefListComponent>(std::move(list));
        }
    }

    if (const Value* physicsRules = root.Get("physics_rules")) {
        if (auto* ref = physicsRules->Get("hurtbox_ref")) {
            obj.AddComponent<HurtboxRefComponent>(HurtboxRefComponent{ref->AsString()});
        }
    }

    if (const Value* transformations = root.Get("transformations")) {
        if (transformations->IsArray()) {
            TransformationRefListComponent list;
            for (const Value& entry : transformations->AsArray()) {
                auto* name = entry.Get("name");
                auto* ref = entry.Get("ref");
                if (name && ref) list.transformations.push_back(NamedAnimationRef{name->AsString(), ref->AsString()});
            }
            obj.AddComponent<TransformationRefListComponent>(std::move(list));
        }
    }

    return Result<MetaBinObject>::Ok(std::move(obj));
}

VoidResult DominusSerializer::Save(const MetaBinObject& obj, const std::filesystem::path& outFile) {
    json::Object root;
    root["dominus_version"] = Value(obj.Version());
    root["object_id"] = Value(obj.Id());

    // NOTE: this is intentionally re-deriving object_id from the strongly
    // typed MetaBinObject rather than round-tripping the original raw JSON
    // text, matching the "single source of truth is the object graph, not
    // the file" principle in the Constitution. Component -> JSON mapping is
    // extended here as new component types are added in Phase 2+.
    root["identity"] = json::Object{
        {"display_name", Value(std::string(""))},
    };

    Value out(root);
    std::ofstream file(outFile, std::ios::binary);
    if (!file) return VoidResult::Fail("Cannot open output file: " + outFile.string());
    file << out.Dump();
    return VoidResult::Ok();
}

bool DominusSerializer::Validate(const std::string& jsonText, std::vector<std::string>* errorsOut) {
    Value root;
    try {
        root = Value::Parse(jsonText);
    } catch (const std::exception& e) {
        if (errorsOut) errorsOut->push_back(std::string("Parse error: ") + e.what());
        return false;
    }

    bool valid = true;
    auto require = [&](const char* field) {
        if (!root.Has(field)) {
            valid = false;
            if (errorsOut) errorsOut->push_back(std::string("Missing required field: ") + field);
        }
    };

    require("dominus_version");
    require("object_id");
    require("identity");

    if (root.Has("identity")) {
        const Value* identity = root.Get("identity");
        if (!identity->IsObject() || !identity->Has("display_name")) {
            valid = false;
            if (errorsOut) errorsOut->push_back("identity.display_name is required");
        }
    }

    return valid;
}

}  // namespace dominus::core
