// CORE/Serialization/DominusSerializer.h
// Load/Save/Validate for .dominus objects. This is the only module allowed
// to touch CORE/MetaBin's storage from outside MetaBinObject itself and the
// only module allowed to know the on-disk shape defined in
// schemas/dominus_object.schema.json.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "CORE/MetaBin/MetaBinObject.h"

namespace dominus::core {

template <typename T>
struct Result {
    bool ok;
    std::optional<T> value;
    std::string error;

    static Result<T> Ok(T v) { return Result<T>{true, std::move(v), ""}; }
    static Result<T> Fail(std::string err) { return Result<T>{false, std::nullopt, std::move(err)}; }
};

// void specialization for save-style calls.
struct VoidResult {
    bool ok;
    std::string error;
    static VoidResult Ok() { return {true, ""}; }
    static VoidResult Fail(std::string err) { return {false, std::move(err)}; }
};

class DominusSerializer {
public:
    static Result<MetaBinObject> Load(const std::filesystem::path& dominusFile);
    static VoidResult Save(const MetaBinObject& obj, const std::filesystem::path& outFile);

    // Validates a raw JSON text blob against the required-field contract in
    // schemas/dominus_object.schema.json (structural subset -- full JSON
    // Schema validation is a Phase 2+ upgrade; v0.1 checks required fields
    // and types by hand, which is sufficient to satisfy the Phase 1 exit
    // criteria's "rejects a fixture with a missing required field" test).
    static bool Validate(const std::string& jsonText, std::vector<std::string>* errorsOut);
};

}  // namespace dominus::core
