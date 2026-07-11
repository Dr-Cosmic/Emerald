// value.cpp - Implementation of the runtime Value type.
#include "value.h"
#include "environment.h"
#include "errors.h"
#include <sstream>
#include <cmath>
#include <cstdio>

namespace emerald {

Value Value::makeNil() { Value v; v.type = ValueType::Nil; return v; }

Value Value::makeInt(long long x) { Value v; v.type = ValueType::Int; v.i = x; return v; }

Value Value::makeFloat(double x) { Value v; v.type = ValueType::Float; v.f = x; return v; }

Value Value::makeChar(char x) { Value v; v.type = ValueType::Char; v.c = x; return v; }

Value Value::makeBool(bool x) { Value v; v.type = ValueType::Bool; v.b = x; return v; }

Value Value::makeString(const std::string& x) {
    Value v; v.type = ValueType::String; v.str = std::make_shared<std::string>(x); return v;
}
Value Value::makeString(std::string&& x) {
    Value v; v.type = ValueType::String; v.str = std::make_shared<std::string>(std::move(x)); return v;
}

Value Value::makeArray(std::vector<Value> elems) {
    Value v; v.type = ValueType::Array;
    v.arr = std::make_shared<std::vector<Value>>(std::move(elems));
    return v;
}
Value Value::makeArrayShared(std::shared_ptr<std::vector<Value>> a) {
    Value v; v.type = ValueType::Array; v.arr = std::move(a); return v;
}

Value Value::makeObject(std::shared_ptr<ObjectData> o) {
    Value v; v.type = ValueType::Object; v.obj = std::move(o); return v;
}

Value Value::makeFunction(std::shared_ptr<FunctionData> f) {
    Value v; v.type = ValueType::Function; v.fn = std::move(f); return v;
}

Value Value::makeClass(std::shared_ptr<ClassData> c) {
    Value v; v.type = ValueType::Class; v.cls = std::move(c); return v;
}

Value Value::makeFile(std::shared_ptr<FileData> f) {
    Value v; v.type = ValueType::File; v.file = std::move(f); return v;
}

Value Value::makeTable(std::shared_ptr<TableData> t) {
    Value v; v.type = ValueType::Table; v.table = std::move(t); return v;
}

Value Value::makeModule(std::shared_ptr<ModuleData> m) {
    Value v; v.type = ValueType::Module; v.mod = std::move(m); return v;
}

Value Value::makeBuiltin(std::string name, BuiltinFn fn) {
    Value v; v.type = ValueType::Builtin; v.builtinName = std::move(name);
    v.builtin = std::move(fn); return v;
}

double Value::asDouble() const {
    switch (type) {
        case ValueType::Int: return static_cast<double>(i);
        case ValueType::Float: return f;
        case ValueType::Char: return static_cast<double>(static_cast<unsigned char>(c));
        case ValueType::Bool: return b ? 1.0 : 0.0;
        default:
            throw RuntimeError("Cannot use value of type '" + typeName() + "' as a number");
    }
}

long long Value::asInt() const {
    switch (type) {
        case ValueType::Int: return i;
        case ValueType::Float: return static_cast<long long>(f);
        case ValueType::Char: return static_cast<long long>(static_cast<unsigned char>(c));
        case ValueType::Bool: return b ? 1 : 0;
        default:
            throw RuntimeError("Cannot use value of type '" + typeName() + "' as an integer");
    }
}

bool Value::truthy() const {
    switch (type) {
        case ValueType::Nil: return false;
        case ValueType::Bool: return b;
        case ValueType::Int: return i != 0;
        case ValueType::Float: return f != 0.0;
        case ValueType::Char: return c != 0;
        case ValueType::String: return str && !str->empty();
        case ValueType::Array: return arr && !arr->empty();
        default: return true;
    }
}

std::string Value::typeName() const {
    switch (type) {
        case ValueType::Nil: return "nil";
        case ValueType::Int: return "int";
        case ValueType::Float: return "float";
        case ValueType::Char: return "char";
        case ValueType::Bool: return "bool";
        case ValueType::String: return "string";
        case ValueType::Array: return "array";
        case ValueType::Object: return "object";
        case ValueType::Function: return "function";
        case ValueType::Class: return "class";
        case ValueType::File: return "File";
        case ValueType::Table: return "table";
        case ValueType::Module: return "module";
        case ValueType::Builtin: return "builtin";
    }
    return "unknown";
}

// Format a double compactly: integral values print without a trailing ".0"? The
// Emerald spec keeps float and int distinct, so we always show a decimal point
// for floats to make the type visible.
static std::string formatDouble(double d) {
    if (std::isnan(d)) return "nan";
    if (std::isinf(d)) return d < 0 ? "-inf" : "inf";
    std::ostringstream oss;
    oss.precision(15);
    oss << d;
    std::string s = oss.str();
    // Ensure a decimal point / exponent is present so floats look like floats.
    if (s.find('.') == std::string::npos &&
        s.find('e') == std::string::npos &&
        s.find('E') == std::string::npos &&
        s.find("inf") == std::string::npos &&
        s.find("nan") == std::string::npos) {
        s += ".0";
    }
    return s;
}

std::string Value::toString() const {
    switch (type) {
        case ValueType::Nil: return "nil";
        case ValueType::Int: return std::to_string(i);
        case ValueType::Float: return formatDouble(f);
        case ValueType::Char: return std::string(1, c);
        case ValueType::Bool: return b ? "true" : "false";
        case ValueType::String: return str ? *str : std::string();
        case ValueType::Array: {
            std::string out = "[";
            if (arr) {
                for (size_t k = 0; k < arr->size(); ++k) {
                    if (k) out += ", ";
                    out += (*arr)[k].reprString();
                }
            }
            out += "]";
            return out;
        }
        case ValueType::Object: {
            std::string out;
            if (obj) {
                bool first = true;
                for (const auto& name : obj->fieldOrder) {
                    const Value& fv = obj->fields.at(name);
                    if (fv.type == ValueType::Function || fv.type == ValueType::Builtin)
                        continue; // methods are not data components
                    if (!first) out += "\n";
                    first = false;
                    out += name + ": " + fv.reprString();
                }
            }
            return out;
        }
        case ValueType::Function:
            return "<fn " + (fn ? fn->name : std::string("?")) + ">";
        case ValueType::Class:
            return "<class " + (cls ? cls->name : std::string("?")) + ">";
        case ValueType::File:
            return "<File " + (file ? file->path : std::string("?")) + ">";
        case ValueType::Table:
            return "<table " + (table ? table->name : std::string("?")) + " (" +
                   std::to_string(table ? table->rows.size() : 0) + " rows)>";
        case ValueType::Module:
            return "<module " + (mod ? mod->name : std::string("?")) + ">";
        case ValueType::Builtin:
            return "<builtin " + builtinName + ">";
    }
    return "";
}

std::string Value::reprString() const {
    switch (type) {
        case ValueType::String: return "\"" + (str ? *str : std::string()) + "\"";
        case ValueType::Char: return std::string("'") + c + "'";
        default: return toString();
    }
}

Value Value::clone() const {
    switch (type) {
        case ValueType::String:
            return Value::makeString(str ? *str : std::string());
        case ValueType::Array: {
            auto copy = std::make_shared<std::vector<Value>>();
            if (arr) {
                copy->reserve(arr->size());
                for (const auto& e : *arr) copy->push_back(e.clone());
            }
            return Value::makeArrayShared(copy);
        }
        case ValueType::Object: {
            if (!obj) return *this;
            auto copy = std::make_shared<ObjectData>();
            copy->cls = obj->cls;
            copy->fieldOrder = obj->fieldOrder;
            for (const auto& kv : obj->fields) {
                if (kv.second.type == ValueType::Function ||
                    kv.second.type == ValueType::Builtin) {
                    copy->fields[kv.first] = kv.second; // rebound just below
                } else {
                    copy->fields[kv.first] = kv.second.clone();
                }
            }
            // Rebind method closures so methods of the copy read and write the
            // copy's fields, not the original's (Emerald is pass-by-value).
            std::unordered_map<Environment*, std::shared_ptr<Environment>> remapped;
            std::function<std::shared_ptr<Environment>(const std::shared_ptr<Environment>&)>
            remap = [&](const std::shared_ptr<Environment>& env)
                    -> std::shared_ptr<Environment> {
                if (!env) return env;
                auto hit = remapped.find(env.get());
                if (hit != remapped.end()) return hit->second;
                if (env->object != obj) return env; // frames above the object
                auto renewed = env->cloneFrameFor(remap(env->parent()), copy);
                remapped[env.get()] = renewed;
                return renewed;
            };
            for (auto& kv : copy->fields) {
                Value& f = kv.second;
                if (f.type != ValueType::Function || !f.fn || !f.fn->closure)
                    continue;
                bool boundToThis = false;
                for (Environment* e = f.fn->closure.get(); e; e = e->parent().get()) {
                    if (e->object == obj) { boundToThis = true; break; }
                }
                if (!boundToThis) continue;
                auto rebound = std::make_shared<FunctionData>(*f.fn);
                rebound->closure = remap(f.fn->closure);
                f = Value::makeFunction(rebound);
            }
            return Value::makeObject(copy);
        }
        case ValueType::Table: {
            if (!table) return *this;
            auto copy = std::make_shared<TableData>();
            copy->name = table->name;
            copy->columns = table->columns;
            copy->rows.reserve(table->rows.size());
            for (const auto& row : table->rows) {
                std::vector<Value> r;
                r.reserve(row.size());
                for (const auto& cell : row) r.push_back(cell.clone());
                copy->rows.push_back(std::move(r));
            }
            return Value::makeTable(copy);
        }
        // Immutable / reference-like values are shared.
        default:
            return *this;
    }
}

} // namespace emerald
