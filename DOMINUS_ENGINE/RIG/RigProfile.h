// RIG/RigProfile.h
// The migration boundary between a legacy skeleton and the canonical
// contract (RIG/CanonicalSkeleton.h). A RigProfile is explicit,
// authored data -- a real list of legacy_bone -> canonical_bone pairs
// -- never inferred or auto-generated. If a canonical bone has no
// mapping, RigProfileValidator reports it as genuinely MISSING; this
// file never invents one to make a report look better.
//
// Lifecycle: AUTHORED -> MAPPED -> VALIDATED -> COMPATIBLE -> ACCEPTED
// -> ACTIVE.
//
// AUTHORED/MAPPED/VALIDATED are mechanical (RigProfileLifecycle reads
// real facts already on the profile/report). COMPATIBLE is still a
// human checkpoint -- a real, non-empty, caller-supplied reason for
// believing the migration is worth the cost of full acceptance
// testing (same epistemic-humility pattern VisualForge's
// SnapshotLifecycle::Approve already established).
//
// ACCEPTED and ACTIVE are DIFFERENT from the prior version of this
// lifecycle: they are now MECHANICAL, not a discretionary human
// decision. AdvanceToAccepted requires a real, passing
// AcceptanceCertificate (RIG/AcceptanceCertificate.h) -- generated
// FROM test/harness results, never hand-typed. DeclareActive requires
// the profile to already be ACCEPTED and the SAME certificate hash
// that earned that acceptance -- activation is a deployment action on
// already-proven evidence, not a new judgment call. "ACTIVE must be
// derived from the acceptance evidence, not manually flipped because
// somebody thinks the character is ready" -- enforced here, not just
// stated in a comment.
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace dominus::rig {

enum class RigProfileState { kAuthored, kMapped, kValidated, kCompatible, kAccepted, kActive };

inline const char* RigProfileStateName(RigProfileState state) {
    switch (state) {
        case RigProfileState::kAuthored: return "AUTHORED";
        case RigProfileState::kMapped: return "MAPPED";
        case RigProfileState::kValidated: return "VALIDATED";
        case RigProfileState::kCompatible: return "COMPATIBLE";
        case RigProfileState::kAccepted: return "ACCEPTED";
        case RigProfileState::kActive: return "ACTIVE";
    }
    return "UNKNOWN";
}

struct RigBoneMapping {
    std::string legacy_bone;
    std::string canonical_bone;
};

struct RigProfile {
    std::string profile_id;
    std::string entity_id;
    std::vector<RigBoneMapping> mappings;

    RigProfileState state = RigProfileState::kAuthored;
    std::string compatible_reason;      // non-empty only once state >= kCompatible
    std::string accepted_certificate_hash;  // the AcceptanceCertificate hash that earned kAccepted
    std::string active_note;            // audit-trail note, non-empty only once state == kActive

    std::optional<std::string> CanonicalFor(const std::string& legacyBone) const {
        for (const auto& m : mappings) {
            if (m.legacy_bone == legacyBone) return m.canonical_bone;
        }
        return std::nullopt;
    }

    std::optional<std::string> LegacyFor(const std::string& canonicalBone) const {
        for (const auto& m : mappings) {
            if (m.canonical_bone == canonicalBone) return m.legacy_bone;
        }
        return std::nullopt;
    }
};

class RigProfileLifecycle {
public:
    static bool AdvanceToMapped(RigProfile& profile) {
        if (profile.state != RigProfileState::kAuthored) return false;
        if (profile.mappings.empty()) return false;  // "mapped" requires at least one real mapping
        profile.state = RigProfileState::kMapped;
        return true;
    }

    // Mechanical: only advances if a real RigProfileReport (see
    // RigProfileValidator.h) says every canonical bone is covered, every
    // mapping resolves, and no hierarchy conflict exists. Caller passes
    // report.valid rather than this header depending on the validator
    // header, keeping RigProfile.h dependency-free.
    static bool AdvanceToValidated(RigProfile& profile, bool reportIsValid) {
        if (profile.state != RigProfileState::kMapped) return false;
        if (!reportIsValid) return false;
        profile.state = RigProfileState::kValidated;
        return true;
    }

    static bool DeclareCompatible(RigProfile& profile, const std::string& reason) {
        if (profile.state != RigProfileState::kValidated) return false;
        if (reason.empty()) return false;  // an unexplained "compatible" isn't a real decision
        profile.state = RigProfileState::kCompatible;
        profile.compatible_reason = reason;
        return true;
    }

    // Mechanical: requires a real AcceptanceCertificate whose own
    // overall_pass is true (itself only ever set by
    // CharacterAcceptanceHarness from genuine check results -- see
    // RIG/AcceptanceCertificate.h). No reason string accepted here on
    // purpose: the certificate IS the reason, and a caller cannot
    // substitute an opinion for it.
    static bool AdvanceToAccepted(RigProfile& profile, bool certificateOverallPass,
                                   const std::string& certificateHash) {
        if (profile.state != RigProfileState::kCompatible) return false;
        if (!certificateOverallPass) return false;
        if (certificateHash.empty()) return false;
        profile.state = RigProfileState::kAccepted;
        profile.accepted_certificate_hash = certificateHash;
        return true;
    }

    // Mechanical: requires kAccepted, and requires the SAME certificate
    // hash that earned it -- prevents "accepted under certificate A,
    // activated citing unrelated certificate B." note is an audit-trail
    // string (who/what triggered the cutover), not a judgment call --
    // the judgment already happened, in the certificate.
    static bool DeclareActive(RigProfile& profile, const std::string& certificateHash, const std::string& note) {
        if (profile.state != RigProfileState::kAccepted) return false;
        if (certificateHash != profile.accepted_certificate_hash) return false;
        if (note.empty()) return false;
        profile.state = RigProfileState::kActive;
        profile.active_note = note;
        return true;
    }
};

}  // namespace dominus::rig
