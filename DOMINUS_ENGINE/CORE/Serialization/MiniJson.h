// CORE/Serialization/MiniJson.h
// Minimal, dependency-free JSON value type used to (de)serialize .dominus
// files at v0.1. This is intentionally small -- enough to round-trip the
// object shape in schemas/dominus_object.schema.json. Swapping in a
// full-featured JSON library (nlohmann/json, simdjson) is an explicit,
// tracked upgrade for Phase 2+, not a v0.1 blocker (see Law 4: scalability
// before spectacle -- v0.1 just needs correctness and a clean seam).
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace dominus::core::json {

class Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value>;

class Value {
public:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Value() : data_(nullptr) {}
    Value(std::nullptr_t) : data_(nullptr) {}
    Value(bool b) : data_(b) {}
    Value(double d) : data_(d) {}
    Value(const char* s) : data_(std::string(s)) {}
    Value(std::string s) : data_(std::move(s)) {}
    Value(Array a) : data_(std::move(a)) {}
    Value(Object o) : data_(std::move(o)) {}

    bool IsNull() const { return std::holds_alternative<std::nullptr_t>(data_); }
    bool IsString() const { return std::holds_alternative<std::string>(data_); }
    bool IsObject() const { return std::holds_alternative<Object>(data_); }
    bool IsArray() const { return std::holds_alternative<Array>(data_); }
    bool IsNumber() const { return std::holds_alternative<double>(data_); }
    bool IsBool() const { return std::holds_alternative<bool>(data_); }

    const std::string& AsString() const { return std::get<std::string>(data_); }
    const Object& AsObject() const { return std::get<Object>(data_); }
    Object& AsObject() { return std::get<Object>(data_); }
    const Array& AsArray() const { return std::get<Array>(data_); }
    double AsNumber() const { return std::get<double>(data_); }
    bool AsBool() const { return std::get<bool>(data_); }

    // Object convenience accessors.
    bool Has(const std::string& key) const {
        return IsObject() && AsObject().count(key) > 0;
    }
    const Value* Get(const std::string& key) const {
        if (!IsObject()) return nullptr;
        auto it = AsObject().find(key);
        return it == AsObject().end() ? nullptr : &it->second;
    }
    Value& operator[](const std::string& key) {
        if (!IsObject()) data_ = Object{};
        return std::get<Object>(data_)[key];
    }

    std::string Dump(int indent = 0) const {
        std::ostringstream out;
        DumpTo(out, indent);
        return out.str();
    }

    static Value Parse(const std::string& text) {
        size_t pos = 0;
        SkipWhitespace(text, pos);
        Value v = ParseValue(text, pos);
        SkipWhitespace(text, pos);
        if (pos != text.size()) {
            throw std::runtime_error("Trailing content after JSON value at offset " + std::to_string(pos));
        }
        return v;
    }

private:
    Storage data_;

    void DumpTo(std::ostringstream& out, int indent) const {
        std::string pad(indent, ' ');
        std::string pad2(indent + 2, ' ');
        if (IsNull()) {
            out << "null";
        } else if (std::holds_alternative<bool>(data_)) {
            out << (std::get<bool>(data_) ? "true" : "false");
        } else if (IsNumber()) {
            double d = AsNumber();
            if (d == static_cast<int64_t>(d))
                out << static_cast<int64_t>(d);
            else
                out << d;
        } else if (IsString()) {
            out << '"' << EscapeString(AsString()) << '"';
        } else if (IsArray()) {
            const auto& arr = AsArray();
            if (arr.empty()) { out << "[]"; return; }
            out << "[\n";
            for (size_t i = 0; i < arr.size(); ++i) {
                out << pad2;
                arr[i].DumpTo(out, indent + 2);
                if (i + 1 < arr.size()) out << ",";
                out << "\n";
            }
            out << pad << "]";
        } else if (IsObject()) {
            const auto& obj = AsObject();
            if (obj.empty()) { out << "{}"; return; }
            out << "{\n";
            size_t i = 0;
            for (const auto& [key, val] : obj) {
                out << pad2 << '"' << EscapeString(key) << "\": ";
                val.DumpTo(out, indent + 2);
                if (++i < obj.size()) out << ",";
                out << "\n";
            }
            out << pad << "}";
        }
    }

    static std::string EscapeString(const std::string& s) {
        std::string out;
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                default: out += c;
            }
        }
        return out;
    }

    static void SkipWhitespace(const std::string& text, size_t& pos) {
        while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos]))) ++pos;
    }

    static Value ParseValue(const std::string& text, size_t& pos) {
        SkipWhitespace(text, pos);
        if (pos >= text.size()) throw std::runtime_error("Unexpected end of JSON input");
        char c = text[pos];
        if (c == '{') return ParseObject(text, pos);
        if (c == '[') return ParseArray(text, pos);
        if (c == '"') return Value(ParseString(text, pos));
        if (c == 't') { Expect(text, pos, "true"); return Value(true); }
        if (c == 'f') { Expect(text, pos, "false"); return Value(false); }
        if (c == 'n') { Expect(text, pos, "null"); return Value(nullptr); }
        return ParseNumber(text, pos);
    }

    static void Expect(const std::string& text, size_t& pos, const std::string& literal) {
        if (text.compare(pos, literal.size(), literal) != 0)
            throw std::runtime_error("Malformed literal at offset " + std::to_string(pos));
        pos += literal.size();
    }

    static Value ParseObject(const std::string& text, size_t& pos) {
        Object obj;
        ++pos;  // consume '{'
        SkipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == '}') { ++pos; return Value(obj); }
        while (true) {
            SkipWhitespace(text, pos);
            std::string key = ParseString(text, pos);
            SkipWhitespace(text, pos);
            if (text[pos] != ':') throw std::runtime_error("Expected ':' at offset " + std::to_string(pos));
            ++pos;
            Value val = ParseValue(text, pos);
            obj[key] = val;
            SkipWhitespace(text, pos);
            if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
            if (pos < text.size() && text[pos] == '}') { ++pos; break; }
            throw std::runtime_error("Expected ',' or '}' at offset " + std::to_string(pos));
        }
        return Value(obj);
    }

    static Value ParseArray(const std::string& text, size_t& pos) {
        Array arr;
        ++pos;  // consume '['
        SkipWhitespace(text, pos);
        if (pos < text.size() && text[pos] == ']') { ++pos; return Value(arr); }
        while (true) {
            Value val = ParseValue(text, pos);
            arr.push_back(val);
            SkipWhitespace(text, pos);
            if (pos < text.size() && text[pos] == ',') { ++pos; continue; }
            if (pos < text.size() && text[pos] == ']') { ++pos; break; }
            throw std::runtime_error("Expected ',' or ']' at offset " + std::to_string(pos));
        }
        return Value(arr);
    }

    // Appends the UTF-8 encoding of a Unicode code point to `out`. Code
    // points are produced only by ParseUnicodeEscape below, which already
    // validates the range (<= 0x10FFFF via surrogate-pair arithmetic), so
    // this never needs to reject a value itself.
    static void AppendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // Parses exactly 4 hex digits starting at text[pos]. Throws rather than
    // silently accepting a short/non-hex sequence -- a malformed \u escape
    // is corrupt data, not a best-effort guess.
    static unsigned ParseHex4(const std::string& text, size_t& pos) {
        if (pos + 4 > text.size()) throw std::runtime_error("Truncated \\u escape at offset " + std::to_string(pos));
        unsigned value = 0;
        for (int i = 0; i < 4; ++i) {
            char c = text[pos + static_cast<size_t>(i)];
            value <<= 4;
            if (c >= '0' && c <= '9') value |= static_cast<unsigned>(c - '0');
            else if (c >= 'a' && c <= 'f') value |= static_cast<unsigned>(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') value |= static_cast<unsigned>(c - 'A' + 10);
            else throw std::runtime_error("Invalid hex digit in \\u escape at offset " + std::to_string(pos));
        }
        pos += 4;
        return value;
    }

    // pos is positioned just after the 'u' of \u. Handles surrogate pairs
    // (\uD800-\uDBFF followed by \uDC00-\uDFFF) per the JSON/UTF-16 spec;
    // a lone/dangling surrogate is rejected rather than silently encoded
    // as an invalid code point.
    static void ParseUnicodeEscape(const std::string& text, size_t& pos, std::string& out) {
        unsigned first = ParseHex4(text, pos);
        if (first >= 0xD800 && first <= 0xDBFF) {
            if (pos + 1 >= text.size() || text[pos] != '\\' || text[pos + 1] != 'u') {
                throw std::runtime_error("Dangling high surrogate in \\u escape at offset " + std::to_string(pos));
            }
            pos += 2;  // consume the \u of the low surrogate
            unsigned second = ParseHex4(text, pos);
            if (second < 0xDC00 || second > 0xDFFF) {
                throw std::runtime_error("High surrogate not followed by a low surrogate at offset " + std::to_string(pos));
            }
            uint32_t cp = 0x10000u + ((first - 0xD800u) << 10) + (second - 0xDC00u);
            AppendUtf8(out, cp);
        } else if (first >= 0xDC00 && first <= 0xDFFF) {
            throw std::runtime_error("Lone low surrogate in \\u escape at offset " + std::to_string(pos));
        } else {
            AppendUtf8(out, first);
        }
    }

    static std::string ParseString(const std::string& text, size_t& pos) {
        if (text[pos] != '"') throw std::runtime_error("Expected string at offset " + std::to_string(pos));
        ++pos;
        std::string out;
        while (pos < text.size() && text[pos] != '"') {
            char c = text[pos];
            if (c == '\\') {
                if (pos + 1 >= text.size()) throw std::runtime_error("Unterminated escape at offset " + std::to_string(pos));
                char next = text[pos + 1];
                pos += 2;
                switch (next) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'u': ParseUnicodeEscape(text, pos, out); break;
                    default:
                        throw std::runtime_error("Unknown escape '\\" + std::string(1, next) + "' at offset " + std::to_string(pos - 2));
                }
            } else {
                out += c;
                ++pos;
            }
        }
        if (pos >= text.size()) throw std::runtime_error("Unterminated string");
        ++pos;  // consume closing quote
        return out;
    }

    static Value ParseNumber(const std::string& text, size_t& pos) {
        size_t start = pos;
        if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) ++pos;
        while (pos < text.size() && (std::isdigit(static_cast<unsigned char>(text[pos])) ||
                                      text[pos] == '.' || text[pos] == 'e' || text[pos] == 'E' ||
                                      text[pos] == '-' || text[pos] == '+')) {
            ++pos;
        }
        std::string numStr = text.substr(start, pos - start);
        if (numStr.empty()) throw std::runtime_error("Invalid number at offset " + std::to_string(start));
        return Value(std::stod(numStr));
    }
};

}  // namespace dominus::core::json
