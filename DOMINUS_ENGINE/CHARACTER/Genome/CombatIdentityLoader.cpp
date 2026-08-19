// CHARACTER/Genome/CombatIdentityLoader.cpp
#include "CHARACTER/Genome/CombatIdentityLoader.h"

#include <fstream>
#include <sstream>

#include "CORE/Serialization/MiniJson.h"

namespace dominus::character {

using core::Result;
using core::json::Value;

namespace {
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open file: " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
std::string StrOr(const Value& root, const char* key, const std::string& def) {
    auto* v = root.Get(key);
    return v && v->IsString() ? v->AsString() : def;
}
}  // namespace

Result<CombatIdentity> CombatIdentityLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<CombatIdentity>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<CombatIdentity>::Fail(std::string("Parse error: ") + e.what());
    }

    if (!root.IsObject() || !root.Has("style")) {
        return Result<CombatIdentity>::Fail("combat identity file missing required 'style' field: " + path.string());
    }

    CombatIdentity id;
    id.style = StrOr(root, "style", "");
    id.range = StrOr(root, "range", "mid");
    id.pressure = StrOr(root, "pressure", "reactive");
    id.counter = StrOr(root, "counter", "average");
    id.mobility = StrOr(root, "mobility", "grounded");
    id.risk = StrOr(root, "risk", "medium");
    return Result<CombatIdentity>::Ok(std::move(id));
}

}  // namespace dominus::character
