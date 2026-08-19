// CHARACTER/Genome/GameDesignGenomeLoader.cpp
#include "CHARACTER/Genome/GameDesignGenomeLoader.h"

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

float NumOr(const Value& root, const char* key, float def) {
    auto* v = root.Get(key);
    return v && v->IsNumber() ? static_cast<float>(v->AsNumber()) : def;
}

std::string StrOr(const Value& root, const char* key, const std::string& def = "") {
    auto* v = root.Get(key);
    return v && v->IsString() ? v->AsString() : def;
}

std::vector<std::string> Validate(const GameDesignGenome& g) {
    std::vector<std::string> errors;
    if (g.genre.empty()) {
        errors.push_back("genre is required and must be non-empty");
    }
    auto checkUnit = [&errors](float value, const char* field) {
        if (value < 0.0f || value > 1.0f) {
            errors.push_back(std::string(field) + " must be within [0, 1], got " + std::to_string(value));
        }
    };
    checkUnit(g.difficulty, "difficulty");
    checkUnit(g.risk_reward_balance, "risk_reward_balance");
    return errors;
}

}  // namespace

Result<GameDesignGenome> GameDesignGenomeLoader::LoadFromFile(const std::filesystem::path& path) {
    std::string text;
    try {
        text = ReadFile(path);
    } catch (const std::exception& e) {
        return Result<GameDesignGenome>::Fail(e.what());
    }

    Value root;
    try {
        root = Value::Parse(text);
    } catch (const std::exception& e) {
        return Result<GameDesignGenome>::Fail(std::string("Parse error: ") + e.what());
    }

    GameDesignGenome g;
    g.genre = StrOr(root, "genre");
    g.core_loop = StrOr(root, "core_loop");
    g.difficulty = NumOr(root, "difficulty", 0.5f);
    g.risk_reward_balance = NumOr(root, "risk_reward_balance", 0.5f);

    auto errors = Validate(g);
    if (!errors.empty()) {
        std::string combined = "GameDesignGenome validation failed (" + std::to_string(errors.size()) + " error(s)): ";
        for (size_t i = 0; i < errors.size(); ++i) {
            combined += errors[i];
            if (i + 1 < errors.size()) combined += "; ";
        }
        return Result<GameDesignGenome>::Fail(combined);
    }

    return Result<GameDesignGenome>::Ok(std::move(g));
}

}  // namespace dominus::character
