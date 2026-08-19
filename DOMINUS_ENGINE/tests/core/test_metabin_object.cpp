// tests/core/test_metabin_object.cpp
#include "CORE/MetaBin/MetaBinObject.h"
#include "tests/TestFramework.h"

using dominus::core::IdentityComponent;
using dominus::core::MetaBinObject;
using dominus::core::RawRefComponent;

DOMINUS_TEST(MetaBinObject_IdAndVersionRoundTrip) {
    MetaBinObject obj("brooklyn", "0.1.0");
    DOMINUS_EXPECT(obj.Id() == "brooklyn");
    DOMINUS_EXPECT(obj.Version() == "0.1.0");
}

DOMINUS_TEST(MetaBinObject_AddAndGetComponent) {
    MetaBinObject obj("brooklyn", "0.1.0");
    obj.AddComponent<IdentityComponent>(IdentityComponent{"Brooklyn", "Renegades"});

    auto* id = obj.GetComponent<IdentityComponent>();
    DOMINUS_EXPECT(id != nullptr);
    DOMINUS_EXPECT(id->display_name == "Brooklyn");
    DOMINUS_EXPECT(id->faction == "Renegades");
}

DOMINUS_TEST(MetaBinObject_GetMissingComponentReturnsNull) {
    MetaBinObject obj("brooklyn", "0.1.0");
    auto* ref = obj.GetComponent<RawRefComponent>();
    DOMINUS_EXPECT(ref == nullptr);
}

DOMINUS_TEST(MetaBinObject_SupportsMultipleDistinctComponentTypes) {
    MetaBinObject obj("brooklyn", "0.1.0");
    obj.AddComponent<IdentityComponent>(IdentityComponent{"Brooklyn", "Renegades"});
    obj.AddComponent<RawRefComponent>(RawRefComponent{"mesh/brooklyn.mesh"});

    DOMINUS_EXPECT(obj.ComponentCount() == 2);
    DOMINUS_EXPECT(obj.GetComponent<IdentityComponent>() != nullptr);
    DOMINUS_EXPECT(obj.GetComponent<RawRefComponent>() != nullptr);
}
