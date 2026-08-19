// REALITY/BrooklynEvidenceGraphBuilder.cpp
#include "REALITY/BrooklynEvidenceGraphBuilder.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"
#include "REALITY/BrooklynDomainCompilers.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::reality {

namespace {

using core::json::Value;

std::optional<std::string> ReadFileBytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// One discovered artifact reference: `key` is the .dominus JSON key
// (or key path) it was found under; `nodeSuffix` becomes the node id
// ("brooklyn." + nodeSuffix); `refFilenames` is one or more real files
// this reference names (more than one for the aggregated array refs --
// animations, moves).
struct DiscoveredRef {
    std::string key;
    std::string nodeSuffix;
    std::vector<std::string> refFilenames;
};

// Structural discovery over brooklyn_canonical.dominus's actual JSON
// shape -- generic over WHICH top-level keys exist (no hardcoded
// "visual_genome"/"material_genome" list), specific about the THREE
// concrete ref shapes this schema actually uses, verified by reading
// the real fixture file rather than assumed:
//   1. a top-level key whose value is an object with a "ref" string
//      field                                  -- e.g. "visual_genome": {"ref": "..."}
//   2. a top-level key whose value is an array of objects, each with
//      its own "ref" string field              -- e.g. "animations": [{"name":..,"ref":..}, ...]
//   3. a nested "<label>_ref" string field inside an otherwise
//      non-ref-shaped object                   -- e.g. "physics_rules": {"hurtbox_ref": "..."}
// A future top-level `{"ref": ...}` block added to the schema is
// picked up automatically by shape (1) without any code change here.
std::vector<DiscoveredRef> DiscoverRefs(const Value& root) {
    std::vector<DiscoveredRef> result;
    if (!root.IsObject()) return result;

    for (const auto& [key, value] : root.AsObject()) {
        if (value.IsObject()) {
            if (const Value* ref = value.Get("ref")) {
                if (ref->IsString()) {
                    result.push_back({key, key, {ref->AsString()}});
                    continue;
                }
            }
            // Shape 3: nested "<label>_ref" string fields.
            for (const auto& [subKey, subValue] : value.AsObject()) {
                const std::string suffix = "_ref";
                if (subKey.size() > suffix.size() &&
                    subKey.compare(subKey.size() - suffix.size(), suffix.size(), suffix) == 0 && subValue.IsString()) {
                    std::string label = subKey.substr(0, subKey.size() - suffix.size());
                    result.push_back({key + "." + subKey, label, {subValue.AsString()}});
                }
            }
        } else if (value.IsArray()) {
            // Shape 2: array of ref-bearing objects, aggregated into
            // ONE node for this key (matches how RIG's own
            // CharacterAcceptanceHarness treats e.g. all 13 animation
            // clips as one collective "animation" input).
            std::vector<std::string> filenames;
            bool allRefs = !value.AsArray().empty();
            for (const auto& entry : value.AsArray()) {
                const Value* ref = entry.IsObject() ? entry.Get("ref") : nullptr;
                if (!ref || !ref->IsString()) {
                    allRefs = false;
                    break;
                }
                filenames.push_back(ref->AsString());
            }
            if (allRefs && !filenames.empty()) {
                std::sort(filenames.begin(), filenames.end());  // deterministic regardless of file's own array order
                result.push_back({key, key, filenames});
            }
        }
    }

    std::sort(result.begin(), result.end(),
              [](const DiscoveredRef& a, const DiscoveredRef& b) { return a.key < b.key; });
    return result;
}

// Uniform raw-byte hashing for every discovered artifact reference --
// a deliberate, disclosed difference from the old REALITY/
// DependencyGraph.h (Milestone 3), which hashed genome nodes via their
// PARSED content (REGISTRY::VisualGenomeCompiler, etc.) instead of raw
// bytes. Raw-byte hashing is simpler, uniform across every ref shape
// this builder discovers (it doesn't need per-artifact-type parsing
// logic to know how to hash a moves[] entry vs a visual_genome ref),
// and more conservative: it changes on ANY byte difference, where a
// parsed-content hash can be silently blind to formatting-only edits.
// The trade-off, stated plainly: this graph's node hashes are NOT the
// same values Milestone 3's graph or VISUALFORGE's own certificates
// use for the same artifacts -- they answer "did the bytes change",
// not "did the parsed meaning change". Both are real, honest hashes;
// they just answer different questions.
std::string HashRefGroup(const std::filesystem::path& baseDir, const std::vector<std::string>& filenames) {
    std::ostringstream combined;
    for (const auto& filename : filenames) {
        auto bytes = ReadFileBytes(baseDir / filename);
        if (!bytes) return "";
        combined << "file=" << filename << ";bytes=" << *bytes << ";";
    }
    return registry::Sha256::Hash(combined.str());
}

}  // namespace

std::map<std::string, std::vector<std::string>> BrooklynEvidenceGraphBuilder::DiscoverArtifactFiles(
    const std::filesystem::path& fixtureDir) {
    std::map<std::string, std::vector<std::string>> result;

    auto dominusBytes = ReadFileBytes(fixtureDir / "brooklyn_canonical.dominus");
    if (!dominusBytes.has_value()) return result;

    Value root;
    try {
        root = Value::Parse(*dominusBytes);
    } catch (const std::exception&) {
        return result;
    }

    for (const auto& ref : DiscoverRefs(root)) {
        result["brooklyn." + ref.nodeSuffix] = ref.refFilenames;
    }
    return result;
}

namespace {

// Every citation below names the actual function and parameter list it
// was read from -- verified by reading the source, not assumed from
// the artifact's name. Shared, verbatim, between Build() and
// BuildStructure() so both ever cite the exact same evidence -- no
// risk of the two drifting apart over time.
void AddAllRealEdges(EvidenceGraph& graph) {
    auto addInput = [&](const std::string& from, const std::string& to, const std::string& reason,
                         const std::string& evidence) {
        if (graph.FindNode(from) == nullptr) {
            graph.undeclared.push_back(
                {from, to, "citation named a node that was never discovered: " + reason + " (" + evidence + ")"});
            return;
        }
        graph.AddEdge({from, to, EdgeType::kInput, reason, evidence});
    };

    addInput("brooklyn.skeleton", "brooklyn.rig_certificate",
              "CheckSkeleton/CheckAnimation/CheckCombat/CheckRuntime/CheckDeterminism all take a canonical skeleton "
              "path as a direct parameter",
              "RIG/CharacterAcceptanceHarness.h");
    addInput("brooklyn.animations", "brooklyn.rig_certificate", "CheckAnimation takes clipPairs directly",
              "RIG/CharacterAcceptanceHarness.h:CheckAnimation");
    addInput("brooklyn.hurtbox", "brooklyn.rig_certificate",
              "CheckCombat/CheckRuntime/CheckDeterminism take a hurtbox path directly",
              "RIG/CharacterAcceptanceHarness.h");
    // moves[] is aggregated as one node ("brooklyn.moves"); CheckCombat
    // only takes ONE move (jab) as a direct parameter -- everything
    // else in moves[] is a RUNTIME dependency, added below, not INPUT.
    addInput("brooklyn.moves", "brooklyn.rig_certificate",
              "CheckCombat/CheckRuntime/CheckDeterminism take moveName=\"jab\" directly; moves[] is aggregated here "
              "so this edge is coarser than the real per-move dependency",
              "RIG/CharacterAcceptanceHarness.h:CheckCombat");

    addInput("brooklyn.visual_genome", "brooklyn.visualforge_certificate",
              "CharacterBlueprintForge::Build requires a VisualGenome parameter",
              "VISUALFORGE/CharacterBlueprint.h:CharacterBlueprintForge::Build");
    addInput("brooklyn.material_genome", "brooklyn.visualforge_certificate",
              "CharacterBlueprintForge::Build takes an optional MaterialGenome pointer",
              "VISUALFORGE/CharacterBlueprint.h:CharacterBlueprintForge::Build");
    addInput("brooklyn.visual_style_genome", "brooklyn.visualforge_certificate",
              "CharacterBlueprintForge::Build takes an optional VisualStyleGenome pointer",
              "VISUALFORGE/CharacterBlueprint.h:CharacterBlueprintForge::Build");

    // --- RUNTIME edges: real, whole-object-bind evidence ---------------
    auto addRuntime = [&](const std::string& from, const std::string& to, const std::string& reason,
                           const std::string& evidence) {
        if (graph.FindNode(from) == nullptr) {
            graph.undeclared.push_back(
                {from, to, "citation named a node that was never discovered: " + reason + " (" + evidence + ")"});
            return;
        }
        graph.AddEdge({from, to, EdgeType::kRuntime, reason, evidence});
    };

    addRuntime("brooklyn.visual_genome", "brooklyn.rig_certificate",
                "CheckRuntime/CheckDeterminism call RigBinder::Bind on the full object, which resolves the "
                "visual_genome ref even though no CheckRuntime parameter names it directly -- a broken ref fails "
                "CheckRuntime, which fails rig_certificate",
                "CHARACTER/Rig/RigBinder.cpp (VisualGenomeRefComponent) + "
                "RIG/CharacterAcceptanceHarness.h:CheckRuntime");
    addRuntime("brooklyn.material_genome", "brooklyn.rig_certificate",
                "same RigBinder::Bind coupling as visual_genome, for MaterialGenomeRefComponent",
                "CHARACTER/Rig/RigBinder.cpp (MaterialGenomeRefComponent) + "
                "RIG/CharacterAcceptanceHarness.h:CheckRuntime");
    addRuntime("brooklyn.visual_style_genome", "brooklyn.rig_certificate",
                "same RigBinder::Bind coupling as visual_genome, for VisualStyleGenomeRefComponent",
                "CHARACTER/Rig/RigBinder.cpp (VisualStyleGenomeRefComponent) + "
                "RIG/CharacterAcceptanceHarness.h:CheckRuntime");
    addRuntime("brooklyn.motion_graph", "brooklyn.rig_certificate",
                "RigBinder::Bind resolves the motion_graph ref (MotionGraphRefComponent); no CheckRuntime parameter "
                "names it directly -- only reachable via the full bind",
                "CHARACTER/Rig/RigBinder.cpp (MotionGraphRefComponent) + "
                "RIG/CharacterAcceptanceHarness.h:CheckRuntime");
    addRuntime("brooklyn.combat_dna", "brooklyn.rig_certificate",
                "CombatBinder::Bind resolves the combat_dna ref (CombatDnaRefComponent); only reachable via the "
                "full bind CheckRuntime performs",
                "COMBAT/HitSystem/CombatBinder.cpp (CombatDnaRefComponent) + "
                "RIG/CharacterAcceptanceHarness.h:CheckRuntime");
    addRuntime("brooklyn.moves", "brooklyn.rig_certificate",
                "CombatBinder::Bind resolves EVERY entry in moves[] (MoveRefListComponent), not just jab -- the "
                "non-jab moves are only reachable via the full bind, not any direct CheckX parameter",
                "COMBAT/HitSystem/CombatBinder.cpp (MoveRefListComponent) + "
                "RIG/CharacterAcceptanceHarness.h:CheckRuntime");

    // --- DERIVED edges: certificate composition -------------------------
    graph.AddEdge({"brooklyn.rig_certificate", "brooklyn.reality_artifact", EdgeType::kDerived,
                    "RealityCompiler's artifact_hash formula includes rig_certificate_hash as a direct input",
                    "REALITY/BrooklynDomainCompilers.cpp:ComputeRealityArtifactHash"});
    graph.AddEdge({"brooklyn.visualforge_certificate", "brooklyn.reality_artifact", EdgeType::kDerived,
                    "RealityCompiler's artifact_hash formula includes visualforge_certificate_hash as a direct input",
                    "REALITY/BrooklynDomainCompilers.cpp:ComputeRealityArtifactHash"});
}

// Every source-file node (real hash) and every real edge -- shared by
// Build() and BuildStructure(). Certificate/artifact nodes are added
// PRESENT-but-empty here; Build() fills them in for real afterward,
// BuildStructure() leaves them exactly as built here.
EvidenceGraph BuildStructureCommon(const std::filesystem::path& fixtureDir) {
    EvidenceGraph graph;
    graph.subject = "brooklyn";

    auto dominusPath = fixtureDir / "brooklyn_canonical.dominus";
    auto dominusBytes = ReadFileBytes(dominusPath);
    if (!dominusBytes.has_value()) {
        graph.undeclared.push_back({"brooklyn_canonical.dominus", "*", "could not read '" + dominusPath.string() + "'"});
        return graph;
    }

    Value root;
    try {
        root = Value::Parse(*dominusBytes);
    } catch (const std::exception& e) {
        graph.undeclared.push_back({"brooklyn_canonical.dominus", "*", std::string("JSON parse failed: ") + e.what()});
        return graph;
    }

    for (const auto& ref : DiscoverRefs(root)) {
        std::string nodeId = "brooklyn." + ref.nodeSuffix;
        std::string hash = HashRefGroup(fixtureDir, ref.refFilenames);
        graph.AddNode({nodeId, ref.refFilenames.size() == 1 ? ref.refFilenames.front() : ("[" + ref.key + "]"), hash,
                        AuthorityType::kSourceFile, hash.empty() ? "MISSING" : "PRESENT"});
    }

    // Certificate/artifact nodes: present, hash empty until Build()
    // (if it's the caller) fills them in for real.
    graph.AddNode(
        {"brooklyn.rig_certificate", "RIG::AcceptanceCertificate", "", AuthorityType::kCertificate, "MISSING"});
    graph.AddNode({"brooklyn.visualforge_certificate", "VISUALFORGE::AcceptanceCertificate", "",
                    AuthorityType::kCertificate, "MISSING"});
    graph.AddNode({"brooklyn.reality_artifact", "REALITY::ExecutionCertificate.artifact_hash", "",
                    AuthorityType::kArtifact, "MISSING"});

    AddAllRealEdges(graph);
    return graph;
}

}  // namespace

EvidenceGraph BrooklynEvidenceGraphBuilder::BuildStructure(const std::filesystem::path& fixtureDir) {
    return BuildStructureCommon(fixtureDir);
}

EvidenceGraph BrooklynEvidenceGraphBuilder::Build(const std::filesystem::path& fixtureDir) {
    EvidenceGraph graph = BuildStructureCommon(fixtureDir);
    if (graph.nodes.empty()) return graph;  // a real refusal was already recorded in `undeclared`

    auto dominusPath = fixtureDir / "brooklyn_canonical.dominus";

    // --- Certificate and artifact nodes, real hashes via the same
    // compilers Milestone 4 already trusts ------------------------------
    auto rigCert = internal::CompileRig(fixtureDir);
    auto visualResult = internal::CompileVisualForge(dominusPath);
    std::string visualCertHash = visualResult.has_value() ? visualResult->certificate.certificate_hash : "";
    bool visualPass = visualResult.has_value() && visualResult->certificate.structurally_sound &&
                       visualResult->certificate.renderable;
    std::string artifactHash;
    if (!rigCert.certificate_hash.empty() && !visualCertHash.empty()) {
        artifactHash = internal::ComputeRealityArtifactHash("brooklyn", "DOMINUS_REALITY_COMPILER_V1",
                                                              rigCert.certificate_hash, visualCertHash);
    }

    for (auto& node : graph.nodes) {
        if (node.node_id == "brooklyn.rig_certificate") {
            node.artifact_hash = rigCert.certificate_hash;
            node.state = rigCert.overall_pass ? "PRESENT" : "MISSING";
        } else if (node.node_id == "brooklyn.visualforge_certificate") {
            node.artifact_hash = visualCertHash;
            node.state = visualPass ? "PRESENT" : "MISSING";
        } else if (node.node_id == "brooklyn.reality_artifact") {
            node.artifact_hash = artifactHash;
            node.state = artifactHash.empty() ? "MISSING" : "PRESENT";
        }
    }

    return graph;
}

}  // namespace dominus::reality
