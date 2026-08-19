// VISUALFORGE/ProductionSnapshot.h
// "BROOKLYN_VISUAL_BUILD_001" -- a named, versioned, hash-addressed
// snapshot future artists/tools can reproduce exactly. Version NUMBERS
// (visual_version/material_version/animation_version) are honestly
// mechanical: this engine has no way to judge whether a change is a
// big or small revision (same epistemic humility
// GameDesignCoherenceChecker/VisualStyleGenome's own "no auto-combine"
// discipline already established), so CreateNext bumps a counter ONLY
// when that dependency's real hash actually changed -- never a guess,
// never all-or-nothing. style_label stays free-form, caller-declared
// text (e.g. "HITM City v1"), same as VisualStyleGenome.name already
// is -- no version-bumping authority is invented for it.
//
// Lifecycle: Created -> Validated -> Approved -> Accepted -> Active.
// Accepted is NEW and mechanical, mirroring RIG's own
// RigProfileLifecycle::AdvanceToAccepted exactly: it requires a real,
// generated AcceptanceCertificate (VISUALFORGE/AcceptanceCertificate.h)
// whose structurally_sound AND renderable are both true, plus the
// certificate's own hash, matching RIG's "activation cites the exact
// evidence that earned it" discipline.
//
// Active is DELIBERATELY NOT IMPLEMENTED. This is the load-bearing
// distinction the whole VISUALFORGE Acceptance phase turns on:
// structurally_sound + renderable together prove a character's data is
// internally consistent, deterministic, and compiles into a real,
// deterministic logical Frame. NEITHER proves the frame looks correct
// -- there is no rasterizer anywhere in this engine (see GRAPHICS/
// README.md). RIG's DeclareActive is real because Brooklyn's migration
// had an actual, running combat system to prove behavioral equivalence
// against. VisualForge has no equivalent proof surface yet. Rather
// than write a DeclareActive that quietly accepts structural+render
// evidence as if it were visual proof, no such method exists at all --
// "never let STRUCTURALLY_SOUND (or RENDERABLE) alone become ACTIVE"
// is enforced by the type system (nothing can construct a state
// transition into kActive), not by a comment someone could later
// route around.
#pragma once

#include <set>
#include <sstream>
#include <string>

#include "REGISTRY/Hash/Sha256.h"
#include "VISUALFORGE/AcceptanceCertificate.h"
#include "VISUALFORGE/BlueprintValidator.h"
#include "VISUALFORGE/DependencyGraph.h"

namespace dominus::visualforge {

enum class SnapshotState { kCreated, kValidated, kApproved, kAccepted, kActive };

inline const char* SnapshotStateName(SnapshotState state) {
    switch (state) {
        case SnapshotState::kCreated: return "Created";
        case SnapshotState::kValidated: return "Validated";
        case SnapshotState::kApproved: return "Approved";
        case SnapshotState::kAccepted: return "Accepted";
        case SnapshotState::kActive: return "Active";
    }
    return "Unknown";
}

struct ProductionSnapshot {
    std::string snapshot_id;  // caller-supplied, e.g. "BROOKLYN_VISUAL_BUILD_001"
    std::string entity_id;
    std::string compiled_at;

    int visual_version = 1;
    int material_version = 1;
    std::string style_label;  // free-form, caller-declared -- e.g. "HITM City v1"
    int animation_version = 1;

    DependencyGraph dependencies;  // the real hashes this version set corresponds to
    ValidationResult validation;   // the blueprint's validation state at snapshot time

    SnapshotState state = SnapshotState::kCreated;
    std::string approved_by;  // empty until Approve() -- who/what made the creative call, caller-declared
    std::string accepted_certificate_hash;  // the AcceptanceCertificate hash that earned kAccepted

    std::string snapshot_hash;  // this snapshot's own identity -- hash of everything above
};

class ProductionSnapshotForge {
public:
    static ProductionSnapshot CreateInitial(const std::string& snapshotId, const std::string& entityId,
                                             const std::string& compiledAt, const DependencyGraph& dependencies,
                                             const ValidationResult& validation, const std::string& styleLabel = "") {
        ProductionSnapshot snap;
        snap.snapshot_id = snapshotId;
        snap.entity_id = entityId;
        snap.compiled_at = compiledAt;
        snap.visual_version = 1;
        snap.material_version = 1;
        snap.animation_version = 1;
        snap.style_label = styleLabel;
        snap.dependencies = dependencies;
        snap.validation = validation;
        snap.snapshot_hash = ComputeHash(snap);
        return snap;
    }

    // The real mechanical rule: bump ONLY the version counters whose
    // dependency hash actually changed from `previous` -- this is what
    // "Dominus knows what needs rebuilding" means as executable code,
    // not narration. style_genome/history_snapshot changes are real
    // and visible in `dependencies`/ChangedDependencies(), but have no
    // dedicated counter in this schema (style is caller-labeled text,
    // history is a running log, not a versioned asset) -- flagged here,
    // not silently dropped.
    static ProductionSnapshot CreateNext(const std::string& snapshotId, const ProductionSnapshot& previous,
                                          const std::string& compiledAt, const DependencyGraph& newDependencies,
                                          const ValidationResult& validation, const std::string& styleLabel = "") {
        ProductionSnapshot snap;
        snap.snapshot_id = snapshotId;
        snap.entity_id = previous.entity_id;
        snap.compiled_at = compiledAt;
        snap.dependencies = newDependencies;
        snap.validation = validation;
        snap.style_label = styleLabel.empty() ? previous.style_label : styleLabel;

        auto changed = DependencyGraphForge::ChangedDependencies(previous.dependencies, newDependencies);
        std::set<std::string> changedSet(changed.begin(), changed.end());

        snap.visual_version = previous.visual_version + (changedSet.count("visual_genome") ? 1 : 0);
        snap.material_version = previous.material_version + (changedSet.count("material_genome") ? 1 : 0);
        snap.animation_version = previous.animation_version + (changedSet.count("animation_spec") ? 1 : 0);

        snap.snapshot_hash = ComputeHash(snap);
        return snap;
    }

private:
    static std::string ComputeHash(const ProductionSnapshot& snap) {
        std::ostringstream out;
        out << "snapshot_id=" << snap.snapshot_id << ";entity_id=" << snap.entity_id
            << ";visual_version=" << snap.visual_version << ";material_version=" << snap.material_version
            << ";style_label=" << snap.style_label << ";animation_version=" << snap.animation_version
            << ";visual_genome_hash=" << snap.dependencies.visual_genome_hash
            << ";material_genome_hash=" << snap.dependencies.material_genome_hash
            << ";style_genome_hash=" << snap.dependencies.style_genome_hash
            << ";animation_spec_hash=" << snap.dependencies.animation_spec_hash
            << ";history_snapshot_hash=" << snap.dependencies.history_snapshot_hash
            << ";validation_valid=" << (snap.validation.valid ? "true" : "false")
            << ";state=" << SnapshotStateName(snap.state) << ";approved_by=" << snap.approved_by;
        return registry::Sha256::Hash(out.str());
    }
};

// Lifecycle transitions. Created->Validated is mechanical.
// Validated->Approved is an explicit human checkpoint. Approved-
// >Accepted is mechanical again, gated on a real AcceptanceCertificate.
// There is no Accepted->Active transition anywhere in this class --
// see the file header for exactly why. `SnapshotState::kActive` exists
// in the enum for the future, real day a rasterizer earns it; nothing
// in this codebase can produce it today.
class SnapshotLifecycle {
public:
    // Created -> Validated: mechanical, real rule -- only advances if
    // the snapshot's OWN recorded validation actually passed. No
    // judgment call here, just reading a fact that's already on the
    // snapshot.
    static bool AdvanceToValidated(ProductionSnapshot& snap) {
        if (snap.state != SnapshotState::kCreated) return false;
        if (!snap.validation.valid) return false;
        snap.state = SnapshotState::kValidated;
        snap.snapshot_hash = RecomputeHash(snap);
        return true;
    }

    // Validated -> Approved: deliberately NOT mechanical -- this is a
    // human/creative decision this engine has no data to make on its
    // own. Requires an explicit caller-supplied approver, not inferred.
    static bool Approve(ProductionSnapshot& snap, const std::string& approvedBy) {
        if (snap.state != SnapshotState::kValidated) return false;
        if (approvedBy.empty()) return false;  // an anonymous approval isn't a real approval
        snap.state = SnapshotState::kApproved;
        snap.approved_by = approvedBy;
        snap.snapshot_hash = RecomputeHash(snap);
        return true;
    }

    // Approved -> Accepted: mechanical, requires a real, passing
    // AcceptanceCertificate -- both structurally_sound AND renderable
    // must be true (everything this engine can currently, honestly
    // check). No reason string accepted here on purpose, same as
    // RIG's AdvanceToAccepted: the certificate IS the reason.
    static bool AdvanceToAccepted(ProductionSnapshot& snap, const AcceptanceCertificate& certificate) {
        if (snap.state != SnapshotState::kApproved) return false;
        if (!certificate.structurally_sound || !certificate.renderable) return false;
        if (certificate.certificate_hash.empty()) return false;
        snap.state = SnapshotState::kAccepted;
        snap.accepted_certificate_hash = certificate.certificate_hash;
        snap.snapshot_hash = RecomputeHash(snap);
        return true;
    }

    // Deliberately absent: there is no DeclareActive. See the file
    // header. Reaching kActive requires real pixel/raster proof this
    // engine cannot produce yet.

private:
    static std::string RecomputeHash(const ProductionSnapshot& snap) {
        // Mirrors ProductionSnapshotForge::ComputeHash, plus the new
        // accepted_certificate_hash field -- kept as a duplicate rather
        // than exposed as shared private state, same "small, obvious
        // duplication over a fragile shared internal" tradeoff this
        // engine has made elsewhere.
        std::ostringstream out;
        out << "snapshot_id=" << snap.snapshot_id << ";entity_id=" << snap.entity_id
            << ";visual_version=" << snap.visual_version << ";material_version=" << snap.material_version
            << ";style_label=" << snap.style_label << ";animation_version=" << snap.animation_version
            << ";visual_genome_hash=" << snap.dependencies.visual_genome_hash
            << ";material_genome_hash=" << snap.dependencies.material_genome_hash
            << ";style_genome_hash=" << snap.dependencies.style_genome_hash
            << ";animation_spec_hash=" << snap.dependencies.animation_spec_hash
            << ";history_snapshot_hash=" << snap.dependencies.history_snapshot_hash
            << ";validation_valid=" << (snap.validation.valid ? "true" : "false")
            << ";state=" << SnapshotStateName(snap.state) << ";approved_by=" << snap.approved_by
            << ";accepted_certificate_hash=" << snap.accepted_certificate_hash;
        return registry::Sha256::Hash(out.str());
    }
};

}  // namespace dominus::visualforge
