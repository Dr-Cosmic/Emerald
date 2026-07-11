// value.h - Runtime value representation for the Emerald interpreter.
#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace emerald {

// Forward declarations of AST nodes.
struct Block;
struct ClassDecl;

// Forward declarations of runtime structures.
class Environment;
class Interpreter;
struct Value;
struct ObjectData;
struct FunctionData;
struct ClassData;
struct FileData;
struct TableData;
struct ModuleData;

using BuiltinFn = std::function<Value(Interpreter&, std::vector<Value>&)>;

enum class ValueType {
    Nil, Int, Float, Char, Bool, String,
    Array, Object, Function, Class, File, Table, Module, Builtin
};

struct Value {
    ValueType type = ValueType::Nil;

    long long i = 0;
    double f = 0.0;
    char c = 0;
    bool b = false;

    std::shared_ptr<std::string> str;
    std::shared_ptr<std::vector<Value>> arr;
    std::shared_ptr<ObjectData> obj;
    std::shared_ptr<FunctionData> fn;
    std::shared_ptr<ClassData> cls;
    std::shared_ptr<FileData> file;
    std::shared_ptr<TableData> table;
    std::shared_ptr<ModuleData> mod;
    BuiltinFn builtin;
    std::string builtinName;

    Value() = default;

    // --- constructors ---
    static Value makeNil();
    static Value makeInt(long long v);
    static Value makeFloat(double v);
    static Value makeChar(char v);
    static Value makeBool(bool v);
    static Value makeString(const std::string& v);
    static Value makeString(std::string&& v);
    static Value makeArray(std::vector<Value> elems);
    static Value makeArrayShared(std::shared_ptr<std::vector<Value>> a);
    static Value makeObject(std::shared_ptr<ObjectData> o);
    static Value makeFunction(std::shared_ptr<FunctionData> f);
    static Value makeClass(std::shared_ptr<ClassData> c);
    static Value makeFile(std::shared_ptr<FileData> f);
    static Value makeTable(std::shared_ptr<TableData> t);
    static Value makeModule(std::shared_ptr<ModuleData> m);
    static Value makeBuiltin(std::string name, BuiltinFn fn);

    // --- predicates ---
    bool isNil() const { return type == ValueType::Nil; }
    bool isNumeric() const {
        return type == ValueType::Int || type == ValueType::Float || type == ValueType::Char;
    }
    bool isCallable() const {
        return type == ValueType::Function || type == ValueType::Builtin ||
               type == ValueType::Class;
    }

    // --- conversions ---
    double asDouble() const;      // numeric coercion (int/float/char/bool)
    long long asInt() const;      // integer coercion
    bool truthy() const;          // truthiness rules

    // A human-readable representation used for printing and string interpolation.
    std::string toString() const;
    // A representation used when embedding inside larger structures (quotes strings).
    std::string reprString() const;

    // The name of the runtime type, for diagnostics.
    std::string typeName() const;

    // Deep copy (value semantics for arrays/objects/tables/strings).
    Value clone() const;
};

// A class instance.
struct ObjectData {
    std::shared_ptr<ClassData> cls;
    std::vector<std::string> fieldOrder;
    std::unordered_map<std::string, Value> fields;

    bool hasField(const std::string& name) const {
        return fields.find(name) != fields.end();
    }
    void setField(const std::string& name, const Value& v) {
        if (fields.find(name) == fields.end()) fieldOrder.push_back(name);
        fields[name] = v;
    }
    Value getField(const std::string& name) const {
        auto it = fields.find(name);
        return it == fields.end() ? Value::makeNil() : it->second;
    }
};

// A user-defined function or method.
struct FunctionData {
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; // (name, optionalType)
    Block* body = nullptr;                 // owned by the AST, alive for program duration
    std::shared_ptr<Environment> closure;  // lexical scope of definition
};

// A user-defined class.
struct ClassData {
    std::string name;
    std::vector<std::pair<std::string, std::string>> params; // constructor params
    ClassDecl* decl = nullptr;             // owned by the AST
    std::shared_ptr<Environment> closure;  // lexical scope of definition
    std::shared_ptr<ClassData> parent;     // resolved parent class (may be null)
};

// A handle to a file on disk.
struct FileData {
    std::string path;
};

// A table of data (backed by JSON on disk for SQL; also used for query results).
struct TableData {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<Value>> rows;
};

// An imported module: a namespace of top-level symbols.
struct ModuleData {
    std::string name;
    std::shared_ptr<Environment> env;
};

} // namespace emerald
