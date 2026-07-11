// json.h - A small, dependency-free JSON reader/writer used to persist SQL
// tables to disk (Emerald stores SQL data as JSON).
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace emerald {
namespace json {

enum class Type { Null, Bool, Int, Double, String, Array, Object };

struct Value {
    Type type = Type::Null;
    bool b = false;
    long long i = 0;
    double d = 0.0;
    std::string s;
    std::vector<Value> arr;
    std::vector<std::pair<std::string, Value>> obj; // insertion-ordered

    static Value makeNull() { return Value{}; }
    static Value makeBool(bool v) { Value x; x.type = Type::Bool; x.b = v; return x; }
    static Value makeInt(long long v) { Value x; x.type = Type::Int; x.i = v; return x; }
    static Value makeDouble(double v) { Value x; x.type = Type::Double; x.d = v; return x; }
    static Value makeString(std::string v) { Value x; x.type = Type::String; x.s = std::move(v); return x; }
    static Value makeArray() { Value x; x.type = Type::Array; return x; }
    static Value makeObject() { Value x; x.type = Type::Object; return x; }

    void set(const std::string& key, Value v) {
        for (auto& kv : obj) {
            if (kv.first == key) { kv.second = std::move(v); return; }
        }
        obj.emplace_back(key, std::move(v));
    }
    const Value* find(const std::string& key) const {
        for (const auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
};

// Serialize a JSON value. `pretty` adds indentation.
std::string stringify(const Value& v, bool pretty = true);

// Parse JSON text. Returns false on failure (with an error message).
bool parse(const std::string& text, Value& out, std::string& error);

} // namespace json
} // namespace emerald
