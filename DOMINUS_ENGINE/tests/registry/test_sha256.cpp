// tests/registry/test_sha256.cpp
// Correctness verified against digests independently computed via
// Python's hashlib (not memory, not asserted) -- see the bash session
// this was generated from: `python3 -c "import hashlib; ..."`.
#include "REGISTRY/Hash/Sha256.h"
#include "tests/TestFramework.h"

using dominus::registry::Sha256;

DOMINUS_TEST(Sha256_EmptyStringMatchesKnownVector) {
    DOMINUS_EXPECT(Sha256::Hash("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

DOMINUS_TEST(Sha256_AbcMatchesKnownVector) {
    DOMINUS_EXPECT(Sha256::Hash("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

DOMINUS_TEST(Sha256_MultiBlockInputMatchesPythonHashlibGroundTruth) {
    // 117 bytes -- exercises the multi-64-byte-block padding/chunking
    // path, not just the single-block case the two vectors above cover.
    std::string input =
        "style=psycho_drunken_martial_arts;range=close;pressure=relentless;counter=expert;mobility="
        "unpredictable;risk=medium";
    DOMINUS_EXPECT(Sha256::Hash(input) == "10bbf5ea81f2f87e2e3ad66a0d66da7c5eb28465209c0024f3d9baf8dbc616b5");
}

DOMINUS_TEST(Sha256_DifferentInputsProduceDifferentHashes) {
    DOMINUS_EXPECT(Sha256::Hash("a") != Sha256::Hash("b"));
}

DOMINUS_TEST(Sha256_SameInputAlwaysProducesSameHash) {
    DOMINUS_EXPECT(Sha256::Hash("deterministic") == Sha256::Hash("deterministic"));
}

DOMINUS_TEST(Sha256_OutputIsAlways64HexCharacters) {
    DOMINUS_EXPECT(Sha256::Hash("").size() == 64);
    DOMINUS_EXPECT(Sha256::Hash("a very long string used to check padding boundaries near 64 bytes exactly").size() ==
                    64);
}
