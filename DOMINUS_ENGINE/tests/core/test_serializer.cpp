// tests/core/test_serializer.cpp
#include "CORE/Serialization/DominusSerializer.h"
#include "tests/TestFramework.h"

#include <filesystem>
#include <fstream>
#include <sstream>

using dominus::core::DominusSerializer;
using dominus::core::IdentityComponent;

namespace {

// Fixtures live at tests/fixtures relative to the repo root. Tests are run
// via CTest with the working directory set to the build dir by default, so
// resolve relative to the source tree via a compile-time define set in
// CMakeLists (kept simple here: try a couple of relative paths).
std::filesystem::path FixturePath(const std::string& name) {
    std::vector<std::filesystem::path> candidates = {
        std::filesystem::path("tests/fixtures") / name,
        std::filesystem::path("../tests/fixtures") / name,
        std::filesystem::path("../../tests/fixtures") / name,
    };
    for (auto& c : candidates) {
        if (std::filesystem::exists(c)) return c;
    }
    throw std::runtime_error("Fixture not found: " + name);
}

std::string ReadWholeFile(const std::filesystem::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

DOMINUS_TEST(Serializer_LoadsValidFixture) {
    auto result = DominusSerializer::Load(FixturePath("brooklyn.dominus"));
    DOMINUS_EXPECT(result.ok);
    DOMINUS_EXPECT(result.value->Id() == "brooklyn");
    DOMINUS_EXPECT(result.value->Version() == "0.1.0");

    auto* identity = result.value->GetComponent<IdentityComponent>();
    DOMINUS_EXPECT(identity != nullptr);
    DOMINUS_EXPECT(identity->display_name == "Brooklyn");
    DOMINUS_EXPECT(identity->faction == "Renegades");
}

DOMINUS_TEST(Serializer_ValidateAcceptsValidFixture) {
    std::string text = ReadWholeFile(FixturePath("brooklyn.dominus"));
    std::vector<std::string> errors;
    bool valid = DominusSerializer::Validate(text, &errors);
    DOMINUS_EXPECT(valid);
    DOMINUS_EXPECT(errors.empty());
}

DOMINUS_TEST(Serializer_ValidateRejectsMissingRequiredField) {
    std::string text = ReadWholeFile(FixturePath("missing_identity.dominus"));
    std::vector<std::string> errors;
    bool valid = DominusSerializer::Validate(text, &errors);
    DOMINUS_EXPECT(!valid);
    DOMINUS_EXPECT(!errors.empty());
}

DOMINUS_TEST(Serializer_LoadFailsGracefullyOnMissingFile) {
    auto result = DominusSerializer::Load("tests/fixtures/does_not_exist.dominus");
    DOMINUS_EXPECT(!result.ok);
    DOMINUS_EXPECT(!result.error.empty());
}
