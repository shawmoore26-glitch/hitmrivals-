// tests/core/test_minijson.cpp
// Direct unit tests for CORE/Serialization/MiniJson.h. Before this file the
// parser had only indirect coverage through loaders exercising escape-free
// fixtures -- these tests exist because a real bug (silent \u mis-decoding,
// see ROADMAP.md Track H Module 0) shipped invisibly through that gap.
#include "CORE/Serialization/MiniJson.h"
#include "tests/TestFramework.h"

using dominus::core::json::Value;

DOMINUS_TEST(MiniJson_ParsesBasicEscapes) {
    Value v = Value::Parse(R"("a\nb\tc\rd\be\ff\"g\\h\/i")");
    DOMINUS_EXPECT(v.AsString() == "a\nb\tc\rd\be\ff\"g\\h/i");
}

DOMINUS_TEST(MiniJson_ParsesBmpUnicodeEscape) {
    // — is an em-dash (U+2014), UTF-8: 0xE2 0x80 0x94. This exact
    // escape appears throughout the real HITM Rivals identity data
    // (e.g. hitm-engine/data/identity/brooklyn/combat_genome.json's
    // "_law" and "_note" fields) -- the bug this test guards against
    // would have silently turned it into the literal text "u2014".
    Value v = Value::Parse(R"("read the room — or don't")");
    const std::string& s = v.AsString();
    std::string expected = "read the room \xE2\x80\x94 or don't";
    DOMINUS_EXPECT(s == expected);
}

DOMINUS_TEST(MiniJson_ParsesAsciiUnicodeEscape) {
    // A \u escape in the ASCII range should decode to the plain character,
    // not a multi-byte sequence.
    Value v = Value::Parse(R"("ABC")");
    DOMINUS_EXPECT(v.AsString() == "ABC");
}

DOMINUS_TEST(MiniJson_ParsesSurrogatePairEscape) {
    // U+1F600 (grinning face) requires a UTF-16 surrogate pair in JSON:
    // 😀. UTF-8 encoding: F0 9F 98 80.
    Value v = Value::Parse(R"("😀")");
    std::string expected = "\xF0\x9F\x98\x80";
    DOMINUS_EXPECT(v.AsString() == expected);
}

DOMINUS_TEST(MiniJson_RoundTripsUnicodeThroughDumpAndParse) {
    Value original = Value::Parse(R"("café — 😀")");
    std::string dumped = original.Dump();
    Value reparsed = Value::Parse(dumped);
    DOMINUS_EXPECT(reparsed.AsString() == original.AsString());
}

DOMINUS_TEST(MiniJson_ParsesNestedObjectWithUnicodeField) {
    // Mirrors the real shape: an object with an authored "_law" string
    // containing an em-dash escape, nested under a profile key.
    Value v = Value::Parse(R"({"defense_profile":{"_law":"VOLUME 18 — capped at 0.25"}})");
    const Value* law = v.Get("defense_profile");
    DOMINUS_EXPECT(law != nullptr);
    const Value* lawStr = law->Get("_law");
    DOMINUS_EXPECT(lawStr != nullptr);
    DOMINUS_EXPECT(lawStr->AsString().find("\xE2\x80\x94") != std::string::npos);
}

// --- Deliberate-break: malformed escapes must throw, never silently mangle ---

DOMINUS_TEST(MiniJson_Break_TruncatedUnicodeEscape_Throws) {
    bool threw = false;
    try {
        Value::Parse(R"("\u12")");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(MiniJson_Break_NonHexUnicodeEscape_Throws) {
    bool threw = false;
    try {
        Value::Parse(R"("\uZZZZ")");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(MiniJson_Break_DanglingHighSurrogate_Throws) {
    bool threw = false;
    try {
        // \uD83D is a high surrogate with no following low surrogate.
        Value::Parse(R"("\uD83D")");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(MiniJson_Break_LoneLowSurrogate_Throws) {
    bool threw = false;
    try {
        Value::Parse(R"("\uDE00")");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(MiniJson_Break_HighSurrogateFollowedByNonSurrogate_Throws) {
    bool threw = false;
    try {
        // High surrogate followed by a \u escape that isn't a low surrogate.
        Value::Parse(R"("\uD83DA")");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(MiniJson_Break_UnknownEscapeCharacter_Throws) {
    bool threw = false;
    try {
        // \q is not a valid JSON escape. Before this fix it would have
        // silently become the literal character 'q'.
        Value::Parse(R"("bad\qescape")");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}

DOMINUS_TEST(MiniJson_Break_TrailingBackslash_Throws) {
    bool threw = false;
    try {
        Value::Parse("\"trailing\\");
    } catch (const std::exception&) {
        threw = true;
    }
    DOMINUS_EXPECT(threw);
}
