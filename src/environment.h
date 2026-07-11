// environment.h - Lexical scoping for the Emerald interpreter.
#pragma once
#include "value.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace emerald {

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment() = default;
    explicit Environment(std::shared_ptr<Environment> parent) : parent_(std::move(parent)) {}

    // When set, unresolved reads/writes consult this object's fields (used for
    // class construction and method bodies).
    std::shared_ptr<ObjectData> object;
    // When true, variable declarations and assignments to unknown names create
    // fields on `object` instead of local variables (only the constructor env).
    bool constructing = false;

    std::shared_ptr<Environment> parent() const { return parent_; }

    // Define a new binding in this scope (overwrites any existing local).
    void define(const std::string& name, const Value& value);

    // Declare a variable via a typed declaration (`int x = ...`). In a
    // constructor scope this creates a field instead of a local.
    void declare(const std::string& name, const Value& value);

    // Look up a name through the scope chain. Throws if not found.
    Value get(const std::string& name, int line = -1, int col = -1);

    // Assign to an existing binding, or create one following Emerald's rules.
    void assign(const std::string& name, const Value& value);

    // True if the name resolves anywhere in the chain.
    bool has(const std::string& name) const;

    // Fetch a symbol from *this* scope only (used for module member access).
    bool getOwn(const std::string& name, Value& out) const;

    // Copy this single scope frame, re-parenting it and binding it to a
    // different object. Used when an object is cloned so that its methods
    // operate on the copy rather than the original.
    std::shared_ptr<Environment> cloneFrameFor(std::shared_ptr<Environment> newParent,
                                               std::shared_ptr<ObjectData> newObject) const;

    // All local binding names in definition order (used for module export).
    const std::vector<std::string>& localOrder() const { return order_; }

private:
    std::shared_ptr<Environment> parent_;
    std::unordered_map<std::string, Value> vars_;
    std::vector<std::string> order_;

    bool tryAssign(const std::string& name, const Value& value);
    void setLocal(const std::string& name, const Value& value);
};

} // namespace emerald
