// json.cpp - Implementation of the tiny JSON reader/writer.
#include "json.h"
#include <sstream>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace emerald {
namespace json {

// ----------------------- stringify -----------------------

static void escapeTo(std::string& out, const std::string& s) {
    out += '"';
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    out += '"';
}

static void indentTo(std::string& out, int depth) {
    for (int k = 0; k < depth; ++k) out += "  ";
}

static void stringifyRec(const Value& v, std::string& out, bool pretty, int depth) {
    switch (v.type) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += v.b ? "true" : "false"; break;
        case Type::Int: out += std::to_string(v.i); break;
        case Type::Double: {
            std::ostringstream oss;
            oss.precision(15);
            oss << v.d;
            std::string s = oss.str();
            out += s;
            break;
        }
        case Type::String: escapeTo(out, v.s); break;
        case Type::Array: {
            if (v.arr.empty()) { out += "[]"; break; }
            out += '[';
            for (size_t k = 0; k < v.arr.size(); ++k) {
                if (k) out += ',';
                if (pretty) { out += '\n'; indentTo(out, depth + 1); }
                stringifyRec(v.arr[k], out, pretty, depth + 1);
            }
            if (pretty) { out += '\n'; indentTo(out, depth); }
            out += ']';
            break;
        }
        case Type::Object: {
            if (v.obj.empty()) { out += "{}"; break; }
            out += '{';
            for (size_t k = 0; k < v.obj.size(); ++k) {
                if (k) out += ',';
                if (pretty) { out += '\n'; indentTo(out, depth + 1); }
                escapeTo(out, v.obj[k].first);
                out += pretty ? ": " : ":";
                stringifyRec(v.obj[k].second, out, pretty, depth + 1);
            }
            if (pretty) { out += '\n'; indentTo(out, depth); }
            out += '}';
            break;
        }
    }
}

std::string stringify(const Value& v, bool pretty) {
    std::string out;
    stringifyRec(v, out, pretty, 0);
    return out;
}

// ----------------------- parse -----------------------

namespace {
struct Parser {
    const std::string& src;
    size_t pos = 0;
    std::string error;

    explicit Parser(const std::string& s) : src(s) {}

    void skipWs() {
        while (pos < src.size()) {
            char c = src[pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') pos++;
            else break;
        }
    }

    bool fail(const std::string& msg) { if (error.empty()) error = msg; return false; }

    bool parseValue(Value& out) {
        skipWs();
        if (pos >= src.size()) return fail("Unexpected end of JSON");
        char c = src[pos];
        switch (c) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': return parseString(out);
            case 't': case 'f': return parseBool(out);
            case 'n': return parseNull(out);
            default:
                if (c == '-' || (c >= '0' && c <= '9')) return parseNumber(out);
                return fail("Unexpected character in JSON");
        }
    }

    bool parseObject(Value& out) {
        out = Value::makeObject();
        pos++; // {
        skipWs();
        if (pos < src.size() && src[pos] == '}') { pos++; return true; }
        while (true) {
            skipWs();
            if (pos >= src.size() || src[pos] != '"') return fail("Expected string key in object");
            Value key;
            if (!parseString(key)) return false;
            skipWs();
            if (pos >= src.size() || src[pos] != ':') return fail("Expected ':' in object");
            pos++;
            Value val;
            if (!parseValue(val)) return false;
            out.obj.emplace_back(key.s, std::move(val));
            skipWs();
            if (pos >= src.size()) return fail("Unterminated object");
            if (src[pos] == ',') { pos++; continue; }
            if (src[pos] == '}') { pos++; return true; }
            return fail("Expected ',' or '}' in object");
        }
    }

    bool parseArray(Value& out) {
        out = Value::makeArray();
        pos++; // [
        skipWs();
        if (pos < src.size() && src[pos] == ']') { pos++; return true; }
        while (true) {
            Value val;
            if (!parseValue(val)) return false;
            out.arr.push_back(std::move(val));
            skipWs();
            if (pos >= src.size()) return fail("Unterminated array");
            if (src[pos] == ',') { pos++; continue; }
            if (src[pos] == ']') { pos++; return true; }
            return fail("Expected ',' or ']' in array");
        }
    }

    bool parseString(Value& out) {
        pos++; // opening quote
        std::string result;
        while (pos < src.size()) {
            char c = src[pos++];
            if (c == '"') { out = Value::makeString(result); return true; }
            if (c == '\\') {
                if (pos >= src.size()) return fail("Bad escape in string");
                char esc = src[pos++];
                switch (esc) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'u': {
                        if (pos + 4 > src.size()) return fail("Bad \\u escape");
                        std::string hex = src.substr(pos, 4);
                        pos += 4;
                        unsigned int code = static_cast<unsigned int>(std::strtoul(hex.c_str(), nullptr, 16));
                        // Minimal UTF-8 encoding of the code point (BMP only).
                        if (code < 0x80) {
                            result += static_cast<char>(code);
                        } else if (code < 0x800) {
                            result += static_cast<char>(0xC0 | (code >> 6));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (code >> 12));
                            result += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (code & 0x3F));
                        }
                        break;
                    }
                    default: return fail("Unknown escape in string");
                }
            } else {
                result += c;
            }
        }
        return fail("Unterminated string");
    }

    bool parseBool(Value& out) {
        if (src.compare(pos, 4, "true") == 0) { out = Value::makeBool(true); pos += 4; return true; }
        if (src.compare(pos, 5, "false") == 0) { out = Value::makeBool(false); pos += 5; return true; }
        return fail("Invalid literal");
    }

    bool parseNull(Value& out) {
        if (src.compare(pos, 4, "null") == 0) { out = Value::makeNull(); pos += 4; return true; }
        return fail("Invalid literal");
    }

    bool parseNumber(Value& out) {
        size_t start = pos;
        bool isDouble = false;
        if (pos < src.size() && src[pos] == '-') pos++;
        while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        if (pos < src.size() && src[pos] == '.') {
            isDouble = true; pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E')) {
            isDouble = true; pos++;
            if (pos < src.size() && (src[pos] == '+' || src[pos] == '-')) pos++;
            while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos]))) pos++;
        }
        std::string num = src.substr(start, pos - start);
        if (num.empty()) return fail("Invalid number");
        if (isDouble) out = Value::makeDouble(std::strtod(num.c_str(), nullptr));
        else out = Value::makeInt(std::strtoll(num.c_str(), nullptr, 10));
        return true;
    }
};
} // anonymous namespace

bool parse(const std::string& text, Value& out, std::string& error) {
    Parser p(text);
    if (!p.parseValue(out)) { error = p.error; return false; }
    p.skipWs();
    if (p.pos != text.size()) { error = "Trailing characters after JSON value"; return false; }
    return true;
}

} // namespace json
} // namespace emerald
