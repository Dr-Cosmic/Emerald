// environment.cpp - Implementation of lexical scoping.
#include "environment.h"
#include "errors.h"

namespace emerald {

void Environment::setLocal(const std::string& name, const Value& value) {
    if (vars_.find(name) == vars_.end()) order_.push_back(name);
    vars_[name] = value;
}

void Environment::define(const std::string& name, const Value& value) {
    setLocal(name, value);
}

void Environment::declare(const std::string& name, const Value& value) {
    if (constructing && object) {
        object->setField(name, value);
    } else {
        setLocal(name, value);
    }
}

Value Environment::get(const std::string& name, int line, int col) {
    Environment* env = this;
    while (env) {
        auto it = env->vars_.find(name);
        if (it != env->vars_.end()) return it->second;
        if (env->object && env->object->hasField(name)) {
            return env->object->getField(name);
        }
        env = env->parent_.get();
    }
    throw RuntimeError("Undefined name '" + name + "'", line, col);
}

bool Environment::has(const std::string& name) const {
    const Environment* env = this;
    while (env) {
        if (env->vars_.find(name) != env->vars_.end()) return true;
        if (env->object && env->object->hasField(name)) return true;
        env = env->parent_.get();
    }
    return false;
}

bool Environment::getOwn(const std::string& name, Value& out) const {
    auto it = vars_.find(name);
    if (it != vars_.end()) { out = it->second; return true; }
    if (object && object->hasField(name)) { out = object->getField(name); return true; }
    return false;
}

bool Environment::tryAssign(const std::string& name, const Value& value) {
    Environment* env = this;
    while (env) {
        auto it = env->vars_.find(name);
        if (it != env->vars_.end()) { it->second = value; return true; }
        if (env->object && env->object->hasField(name)) {
            env->object->setField(name, value);
            return true;
        }
        env = env->parent_.get();
    }
    return false;
}

void Environment::assign(const std::string& name, const Value& value) {
    if (tryAssign(name, value)) return;
    // Not found: create it following Emerald's rules.
    if (constructing && object) {
        object->setField(name, value);
    } else {
        setLocal(name, value);
    }
}

std::shared_ptr<Environment> Environment::cloneFrameFor(
        std::shared_ptr<Environment> newParent,
        std::shared_ptr<ObjectData> newObject) const {
    auto copy = std::make_shared<Environment>(std::move(newParent));
    copy->object = std::move(newObject);
    copy->constructing = false;
    copy->order_ = order_;
    for (const auto& kv : vars_) {
        // Locals are copied by value; function identities are preserved.
        if (kv.second.type == ValueType::Function ||
            kv.second.type == ValueType::Builtin) {
            copy->vars_[kv.first] = kv.second;
        } else {
            copy->vars_[kv.first] = kv.second.clone();
        }
    }
    return copy;
}

} // namespace emerald
