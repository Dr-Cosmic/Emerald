// interpreter.cpp - Core evaluation logic for Emerald.
#include "interpreter.h"
#include "errors.h"
#include "lexer.h"
#include "parser.h"
#include <cmath>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

namespace emerald {

//======================= construction =======================

Interpreter::Interpreter() : globals_(std::make_shared<Environment>()) {
    installBuiltins();
}

void Interpreter::installBuiltins() {
    installEmeraldBuiltins(*this, *globals_);
}

//======================= program execution =======================

void Interpreter::run(Program& program) {
    for (auto& stmt : program.statements) {
        execute(stmt.get(), globals_);
    }
}

Value Interpreter::runStatement(Node* stmt) {
    if (stmt->kind == NodeKind::ExprStmt) {
        return evaluate(static_cast<ExprStmt*>(stmt)->expr.get(), globals_);
    }
    execute(stmt, globals_);
    return Value::makeNil();
}

//======================= statements =======================

void Interpreter::execute(Node* node, std::shared_ptr<Environment> env) {
    switch (node->kind) {
        case NodeKind::VarDecl:    execVarDecl(static_cast<VarDecl*>(node), env); return;
        case NodeKind::ExprStmt: {
            auto* es = static_cast<ExprStmt*>(node);
            Value v = evaluate(es->expr.get(), env);
            // Spec: `myfile.read(...);` as a standalone statement prints what it
            // read. When read() appears inside a larger expression, it only
            // returns the characters.
            if (es->expr->kind == NodeKind::Call) {
                auto* call = static_cast<Call*>(es->expr.get());
                if (call->callee->kind == NodeKind::Member &&
                    static_cast<Member*>(call->callee.get())->name == "read" &&
                    v.type == ValueType::String) {
                    (*out) << *v.str;
                    if (v.str->empty() || v.str->back() != '\n') (*out) << "\n";
                }
            }
            return;
        }
        case NodeKind::Block: {
            auto child = std::make_shared<Environment>(env);
            executeBlock(static_cast<Block*>(node), child);
            return;
        }
        case NodeKind::FnDecl:     execFnDecl(static_cast<FnDecl*>(node), env); return;
        case NodeKind::ClassDecl:  execClassDecl(static_cast<ClassDecl*>(node), env); return;
        case NodeKind::Return: {
            auto* r = static_cast<Return*>(node);
            ReturnSignal sig;
            sig.value = r->value ? evaluate(r->value.get(), env).clone() : Value::makeNil();
            throw sig;
        }
        case NodeKind::If:         execIf(static_cast<If*>(node), env); return;
        case NodeKind::ForClassic: execForClassic(static_cast<ForClassic*>(node), env); return;
        case NodeKind::ForIn:      execForIn(static_cast<ForIn*>(node), env); return;
        case NodeKind::While:      execWhile(static_cast<While*>(node), env); return;
        case NodeKind::Import:     execImport(static_cast<Import*>(node), env); return;
        case NodeKind::Break:      throw BreakSignal{};
        case NodeKind::Continue:   throw ContinueSignal{};
        default:
            // An expression used in statement position.
            evaluate(node, env);
            return;
    }
}

void Interpreter::executeBlock(Block* block, std::shared_ptr<Environment> env) {
    for (auto& stmt : block->statements) {
        execute(stmt.get(), env);
    }
}

void Interpreter::execVarDecl(VarDecl* d, std::shared_ptr<Environment> env) {
    const std::string& t = d->declaredType;

    // Array declaration: `int a[] = [...]` (element type is not enforced;
    // Emerald arrays are dynamic and hold any type).
    if (d->isArray) {
        Value v = d->initializer
            ? evaluate(d->initializer.get(), env).clone()
            : Value::makeArray({});
        if (v.type != ValueType::Array && !v.isNil()) {
            throw RuntimeError("Array declaration of '" + d->name +
                               "' requires an array initializer", d->line, d->col);
        }
        env->declare(d->name, v);
        return;
    }

    // File declaration.
    if (t == "File") {
        std::string path;
        if (d->initializer) {
            Value v = evaluate(d->initializer.get(), env);
            if (v.type != ValueType::String)
                throw RuntimeError("File path must be a string", d->line, d->col);
            path = *v.str;
        } else {
            // `File mynewfile;` creates a file named after the variable in the
            // current directory.
            path = d->name;
        }
        auto fd = std::make_shared<FileData>();
        fd->path = path;
        // "Make new file": touch it if it does not already exist.
        {
            std::ifstream probe(path, std::ios::binary);
            if (!probe.good()) {
                std::ofstream touch(path, std::ios::binary);
                if (!touch.good())
                    throw RuntimeError("Cannot create file '" + path + "'",
                                       d->line, d->col);
            }
        }
        env->declare(d->name, Value::makeFile(fd));
        return;
    }

    // Primitive declarations with type coercion.
    if (t == "int" || t == "float" || t == "char" || t == "string") {
        Value v;
        if (d->initializer) {
            v = coerceToType(t, evaluate(d->initializer.get(), env), d->line, d->col);
        } else {
            if (t == "int") v = Value::makeInt(0);
            else if (t == "float") v = Value::makeFloat(0.0);
            else if (t == "char") v = Value::makeChar('\0');
            else v = Value::makeString("");
        }
        env->declare(d->name, v.clone());
        return;
    }

    // Class-typed declaration: `Motorcycle m(a, b);` / `Car c;` / `Car c = other;`
    Value classVal = env->get(t, d->line, d->col);
    if (classVal.type != ValueType::Class) {
        throw RuntimeError("'" + t + "' is not a class", d->line, d->col);
    }
    if (d->initializer) {
        Value v = evaluate(d->initializer.get(), env);
        if (v.type == ValueType::Class) {
            // `Car c = Truck;` -> automatic typecast (instantiate with no args).
            std::vector<Value> noArgs;
            v = instantiate(v.cls, noArgs, d->line, d->col);
        }
        env->declare(d->name, v.clone());
        return;
    }
    std::vector<Value> args;
    for (auto& a : d->ctorArgs) args.push_back(evaluate(a.get(), env).clone());
    Value inst = instantiate(classVal.cls, args, d->line, d->col);
    env->declare(d->name, inst);
}

void Interpreter::execFnDecl(FnDecl* d, std::shared_ptr<Environment> env) {
    auto fn = std::make_shared<FunctionData>();
    fn->name = d->name;
    for (auto& p : d->params) fn->params.emplace_back(p.name, p.type);
    fn->body = d->body.get();
    fn->closure = env;
    env->declare(d->name, Value::makeFunction(fn));
}

void Interpreter::execClassDecl(ClassDecl* d, std::shared_ptr<Environment> env) {
    // `class newcar;` declares a nil object variable.
    if (d->isObjectVarDecl) {
        env->declare(d->name, Value::makeNil());
        return;
    }
    auto cls = std::make_shared<ClassData>();
    cls->name = d->name;
    for (auto& p : d->params) cls->params.emplace_back(p.name, p.type);
    cls->decl = d;
    cls->closure = env;
    if (d->hasParent) {
        Value parent = env->get(d->parentName, d->line, d->col);
        if (parent.type != ValueType::Class)
            throw RuntimeError("Parent '" + d->parentName + "' is not a class",
                               d->line, d->col);
        cls->parent = parent.cls;
    }
    env->declare(d->name, Value::makeClass(cls));
}

void Interpreter::execIf(If* s, std::shared_ptr<Environment> env) {
    for (auto& branch : s->branches) {
        if (!branch.condition || evaluate(branch.condition.get(), env).truthy()) {
            auto child = std::make_shared<Environment>(env);
            executeBlock(branch.body.get(), child);
            return;
        }
    }
}

void Interpreter::execForClassic(ForClassic* s, std::shared_ptr<Environment> env) {
    auto loopEnv = std::make_shared<Environment>(env);
    if (s->init) execute(s->init.get(), loopEnv);
    while (true) {
        if (s->condition && !evaluate(s->condition.get(), loopEnv).truthy()) break;
        try {
            auto bodyEnv = std::make_shared<Environment>(loopEnv);
            executeBlock(s->body.get(), bodyEnv);
        } catch (BreakSignal&) {
            break;
        } catch (ContinueSignal&) {
            // fall through to update
        }
        if (s->update) evaluate(s->update.get(), loopEnv);
    }
}

void Interpreter::execForIn(ForIn* s, std::shared_ptr<Environment> env) {
    Value iterable = evaluate(s->iterable.get(), env);
    auto loopEnv = std::make_shared<Environment>(env);
    loopEnv->define(s->varName, Value::makeNil());

    auto runBody = [&](const Value& element) -> bool {
        loopEnv->define(s->varName, element.clone()); // pass by value
        try {
            auto bodyEnv = std::make_shared<Environment>(loopEnv);
            executeBlock(s->body.get(), bodyEnv);
        } catch (BreakSignal&) {
            return false;
        } catch (ContinueSignal&) {
            // continue with next element
        }
        return true;
    };

    if (iterable.type == ValueType::Array) {
        // Iterate a snapshot so mutation of the source mid-loop is safe.
        std::vector<Value> snapshot = *iterable.arr;
        for (auto& el : snapshot) {
            if (!runBody(el)) break;
        }
    } else if (iterable.type == ValueType::String) {
        for (char ch : *iterable.str) {
            if (!runBody(Value::makeChar(ch))) break;
        }
    } else if (iterable.type == ValueType::Table) {
        for (auto& row : iterable.table->rows) {
            if (!runBody(Value::makeArray(row))) break;
        }
    } else {
        throw RuntimeError("Cannot iterate over a value of type '" +
                           iterable.typeName() + "'", s->line, s->col);
    }
}

void Interpreter::execWhile(While* s, std::shared_ptr<Environment> env) {
    while (evaluate(s->condition.get(), env).truthy()) {
        try {
            auto bodyEnv = std::make_shared<Environment>(env);
            executeBlock(s->body.get(), bodyEnv);
        } catch (BreakSignal&) {
            break;
        } catch (ContinueSignal&) {
            continue;
        }
    }
}

//======================= imports =======================

std::string Interpreter::findModuleFile(const std::string& moduleName) const {
    std::vector<std::string> dirs;
    if (!scriptDir_.empty()) dirs.push_back(scriptDir_);
    dirs.push_back(".");

    const char* exts[] = { ".em", ".emerald" };
    for (const auto& dir : dirs) {
        for (const char* ext : exts) {
            std::string candidate = dir + "/" + moduleName + ext;
            std::ifstream probe(candidate);
            if (probe.good()) return candidate;
        }
    }
    return "";
}

Value Interpreter::loadModule(const std::string& moduleName, int line, int col) {
    std::string path = findModuleFile(moduleName);
    if (path.empty()) {
        throw RuntimeError("Cannot find module '" + moduleName +
                           "' (looked for " + moduleName + ".em / " +
                           moduleName + ".emerald)", line, col);
    }
    auto cached = moduleCache_.find(path);
    if (cached != moduleCache_.end()) return cached->second;
    if (importing_.count(path)) {
        throw RuntimeError("Circular import of module '" + moduleName + "'",
                           line, col);
    }

    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    importing_.insert(path);
    Value moduleValue;
    try {
        Lexer lexer(source);
        Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();

        auto moduleEnv = std::make_shared<Environment>(globals_);
        for (auto& stmt : program->statements) {
            execute(stmt.get(), moduleEnv);
        }
        auto mod = std::make_shared<ModuleData>();
        mod->name = moduleName;
        mod->env = moduleEnv;
        moduleValue = Value::makeModule(mod);

        // The module's AST must outlive the module (functions hold raw Block*).
        // Stash the program inside a keeper tied to the module environment.
        static std::vector<std::unique_ptr<Program>> keeper;
        keeper.push_back(std::move(program));
    } catch (...) {
        importing_.erase(path);
        throw;
    }
    importing_.erase(path);
    moduleCache_[path] = moduleValue;
    return moduleValue;
}

void Interpreter::execImport(Import* s, std::shared_ptr<Environment> env) {
    Value mod = loadModule(s->moduleName, s->line, s->col);
    if (!s->isFrom) {
        env->declare(s->moduleName, mod);
        return;
    }
    Value symbol;
    if (!mod.mod->env->getOwn(s->symbolName, symbol)) {
        throw RuntimeError("Module '" + s->moduleName + "' has no symbol '" +
                           s->symbolName + "'", s->line, s->col);
    }
    env->declare(s->symbolName, symbol);
}

//======================= expressions =======================

Value Interpreter::evaluate(Node* node, std::shared_ptr<Environment> env) {
    switch (node->kind) {
        case NodeKind::IntLit:   return Value::makeInt(static_cast<IntLit*>(node)->value);
        case NodeKind::FloatLit: return Value::makeFloat(static_cast<FloatLit*>(node)->value);
        case NodeKind::CharLit:  return Value::makeChar(static_cast<CharLit*>(node)->value);
        case NodeKind::BoolLit:  return Value::makeBool(static_cast<BoolLit*>(node)->value);
        case NodeKind::NilLit:   return Value::makeNil();
        case NodeKind::StringLit:
            return evalStringLit(static_cast<StringLit*>(node), env);
        case NodeKind::ArrayLit: {
            auto* a = static_cast<ArrayLit*>(node);
            std::vector<Value> elems;
            elems.reserve(a->elements.size());
            for (auto& e : a->elements) elems.push_back(evaluate(e.get(), env).clone());
            return Value::makeArray(std::move(elems));
        }
        case NodeKind::Identifier: {
            auto* id = static_cast<Identifier*>(node);
            return env->get(id->name, id->line, id->col);
        }
        case NodeKind::Binary:    return evalBinary(static_cast<Binary*>(node), env);
        case NodeKind::Unary:     return evalUnary(static_cast<Unary*>(node), env);
        case NodeKind::PostfixOp: return evalPostfix(static_cast<PostfixOp*>(node), env);
        case NodeKind::Assign:    return evalAssign(static_cast<Assign*>(node), env);
        case NodeKind::Call:      return evalCall(static_cast<Call*>(node), env);
        case NodeKind::Index:     return evalIndex(static_cast<Index*>(node), env);
        case NodeKind::Member:    return evalMember(static_cast<Member*>(node), env);
        default:
            throw RuntimeError("Invalid expression", node->line, node->col);
    }
}

Value Interpreter::evalStringLit(StringLit* s, std::shared_ptr<Environment> env) {
    std::string result;
    for (auto& part : s->parts) {
        if (part.isExpr) {
            Value v = evaluate(part.expr.get(), env);
            result += v.toString(); // interpolation auto-casts to string
        } else {
            result += part.literal;
        }
    }
    return Value::makeString(std::move(result));
}

//----------------------- operators -----------------------

static bool numericIsInt(const Value& v) {
    return v.type == ValueType::Int || v.type == ValueType::Char ||
           v.type == ValueType::Bool;
}

Value Interpreter::binaryNumeric(const std::string& op, const Value& l,
                                 const Value& r, int line, int col) {
    if (!l.isNumeric() && l.type != ValueType::Bool)
        throw RuntimeError("Left operand of '" + op + "' is not numeric (got " +
                           l.typeName() + ")", line, col);
    if (!r.isNumeric() && r.type != ValueType::Bool)
        throw RuntimeError("Right operand of '" + op + "' is not numeric (got " +
                           r.typeName() + ")", line, col);

    bool bothInt = numericIsInt(l) && numericIsInt(r);

    if (op == "+") {
        if (bothInt) return Value::makeInt(l.asInt() + r.asInt());
        return Value::makeFloat(l.asDouble() + r.asDouble());
    }
    if (op == "-") {
        if (bothInt) return Value::makeInt(l.asInt() - r.asInt());
        return Value::makeFloat(l.asDouble() - r.asDouble());
    }
    if (op == "*") {
        if (bothInt) return Value::makeInt(l.asInt() * r.asInt());
        return Value::makeFloat(l.asDouble() * r.asDouble());
    }
    if (op == "/") {
        double rd = r.asDouble();
        if (rd == 0.0) throw RuntimeError("Division by zero", line, col);
        // True division always yields a float (like Python 3).
        return Value::makeFloat(l.asDouble() / rd);
    }
    if (op == "//") {
        if (bothInt) {
            long long ri = r.asInt();
            if (ri == 0) throw RuntimeError("Division by zero", line, col);
            long long li = l.asInt();
            long long q = li / ri;
            if ((li % ri != 0) && ((li < 0) != (ri < 0))) q--; // floor division
            return Value::makeInt(q);
        }
        double rd = r.asDouble();
        if (rd == 0.0) throw RuntimeError("Division by zero", line, col);
        return Value::makeFloat(std::floor(l.asDouble() / rd));
    }
    if (op == "%") {
        if (bothInt) {
            long long ri = r.asInt();
            if (ri == 0) throw RuntimeError("Modulo by zero", line, col);
            long long m = l.asInt() % ri;
            if (m != 0 && ((m < 0) != (ri < 0))) m += ri; // Python-style sign
            return Value::makeInt(m);
        }
        double rd = r.asDouble();
        if (rd == 0.0) throw RuntimeError("Modulo by zero", line, col);
        double m = std::fmod(l.asDouble(), rd);
        if (m != 0.0 && ((m < 0) != (rd < 0))) m += rd;
        return Value::makeFloat(m);
    }
    if (op == "**") {
        if (bothInt && r.asInt() >= 0) {
            long long base = l.asInt(), exp = r.asInt(), result = 1;
            while (exp > 0) {
                if (exp & 1) result *= base;
                base *= base;
                exp >>= 1;
            }
            return Value::makeInt(result);
        }
        return Value::makeFloat(std::pow(l.asDouble(), r.asDouble()));
    }
    throw RuntimeError("Unknown numeric operator '" + op + "'", line, col);
}

bool Interpreter::looseEquals(const Value& l, const Value& r) {
    if (l.isNil() || r.isNil()) return l.isNil() && r.isNil();

    auto asComparableString = [](const Value& v, std::string& out) -> bool {
        if (v.type == ValueType::String) { out = *v.str; return true; }
        if (v.type == ValueType::Char) { out = std::string(1, v.c); return true; }
        return false;
    };

    bool lNum = l.isNumeric() || l.type == ValueType::Bool;
    bool rNum = r.isNumeric() || r.type == ValueType::Bool;

    // string vs string / char (compare text)
    if (l.type == ValueType::String || r.type == ValueType::String) {
        std::string ls, rs;
        bool lOk = asComparableString(l, ls), rOk = asComparableString(r, rs);
        if (lOk && rOk) return ls == rs;
        // string vs number: JavaScript-style numeric coercion
        const Value& s = lOk ? l : r;
        const Value& n = lOk ? r : l;
        if (n.isNumeric() || n.type == ValueType::Bool) {
            try {
                size_t pos = 0;
                double parsed = std::stod(*s.str, &pos);
                while (pos < s.str->size() &&
                       std::isspace(static_cast<unsigned char>((*s.str)[pos]))) pos++;
                if (pos != s.str->size()) return false;
                return parsed == n.asDouble();
            } catch (...) {
                return false;
            }
        }
        return false;
    }

    if (lNum && rNum) return l.asDouble() == r.asDouble();

    if (l.type == ValueType::Array && r.type == ValueType::Array) {
        if (l.arr->size() != r.arr->size()) return false;
        for (size_t i = 0; i < l.arr->size(); i++)
            if (!looseEquals((*l.arr)[i], (*r.arr)[i])) return false;
        return true;
    }
    if (l.type == ValueType::Object && r.type == ValueType::Object) {
        if (l.obj == r.obj) return true;
        if (l.obj->fields.size() != r.obj->fields.size()) return false;
        for (auto& kv : l.obj->fields) {
            auto it = r.obj->fields.find(kv.first);
            if (it == r.obj->fields.end()) return false;
            if (!looseEquals(kv.second, it->second)) return false;
        }
        return true;
    }
    if (l.type == ValueType::Class && r.type == ValueType::Class)
        return l.cls == r.cls;
    if (l.type == ValueType::Function && r.type == ValueType::Function)
        return l.fn == r.fn;
    if (l.type == ValueType::File && r.type == ValueType::File)
        return l.file->path == r.file->path;
    return false;
}

bool Interpreter::strictEquals(const Value& l, const Value& r) {
    if (l.type != r.type) return false; // strict: types must match exactly
    switch (l.type) {
        case ValueType::Nil:    return true;
        case ValueType::Int:    return l.i == r.i;
        case ValueType::Float:  return l.f == r.f;
        case ValueType::Char:   return l.c == r.c;
        case ValueType::Bool:   return l.b == r.b;
        case ValueType::String: return *l.str == *r.str;
        default:                return looseEquals(l, r);
    }
}

int Interpreter::compareValues(const Value& l, const Value& r, int line, int col) {
    if ((l.type == ValueType::String || l.type == ValueType::Char) &&
        (r.type == ValueType::String || r.type == ValueType::Char)) {
        std::string ls = l.type == ValueType::Char ? std::string(1, l.c) : *l.str;
        std::string rs = r.type == ValueType::Char ? std::string(1, r.c) : *r.str;
        return ls.compare(rs) < 0 ? -1 : (ls == rs ? 0 : 1);
    }
    double ld = l.asDouble(), rd = r.asDouble(); // throws if not numeric
    (void)line; (void)col;
    return ld < rd ? -1 : (ld == rd ? 0 : 1);
}

Value Interpreter::evalBinary(Binary* b, std::shared_ptr<Environment> env) {
    const std::string& op = b->op;

    // Short-circuit logic.
    if (op == "&&") {
        Value l = evaluate(b->left.get(), env);
        if (!l.truthy()) return Value::makeBool(false);
        return Value::makeBool(evaluate(b->right.get(), env).truthy());
    }
    if (op == "||") {
        Value l = evaluate(b->left.get(), env);
        if (l.truthy()) return Value::makeBool(true);
        return Value::makeBool(evaluate(b->right.get(), env).truthy());
    }

    Value l = evaluate(b->left.get(), env);
    Value r = evaluate(b->right.get(), env);

    if (op == "==")  return Value::makeBool(looseEquals(l, r));
    if (op == "!=")  return Value::makeBool(!looseEquals(l, r));
    if (op == "===") return Value::makeBool(strictEquals(l, r));
    if (op == "!==") return Value::makeBool(!strictEquals(l, r));

    if (op == "<")  return Value::makeBool(compareValues(l, r, b->line, b->col) < 0);
    if (op == "<=") return Value::makeBool(compareValues(l, r, b->line, b->col) <= 0);
    if (op == ">")  return Value::makeBool(compareValues(l, r, b->line, b->col) > 0);
    if (op == ">=") return Value::makeBool(compareValues(l, r, b->line, b->col) >= 0);

    // '+' also concatenates strings and arrays.
    if (op == "+") {
        if (l.type == ValueType::String || r.type == ValueType::String) {
            return Value::makeString(l.toString() + r.toString());
        }
        if (l.type == ValueType::Array && r.type == ValueType::Array) {
            std::vector<Value> combined;
            combined.reserve(l.arr->size() + r.arr->size());
            for (auto& v : *l.arr) combined.push_back(v.clone());
            for (auto& v : *r.arr) combined.push_back(v.clone());
            return Value::makeArray(std::move(combined));
        }
    }

    return binaryNumeric(op, l, r, b->line, b->col);
}

Value Interpreter::applyIncDec(Node* operand, const std::string& op, bool prefix,
                               std::shared_ptr<Environment> env) {
    Value old = evaluate(operand, env);
    Value next;
    long long delta = (op == "++") ? 1 : -1;
    switch (old.type) {
        case ValueType::Int:   next = Value::makeInt(old.i + delta); break;
        case ValueType::Float: next = Value::makeFloat(old.f + delta); break;
        case ValueType::Char:  next = Value::makeChar(static_cast<char>(old.c + delta)); break;
        default:
            throw RuntimeError("Cannot apply '" + op + "' to a value of type '" +
                               old.typeName() + "'", operand->line, operand->col);
    }
    assignTo(operand, next, env);
    return prefix ? next : old;
}

Value Interpreter::evalUnary(Unary* u, std::shared_ptr<Environment> env) {
    if (u->op == "++" || u->op == "--") {
        return applyIncDec(u->operand.get(), u->op, /*prefix=*/true, env);
    }
    Value v = evaluate(u->operand.get(), env);
    if (u->op == "!") return Value::makeBool(!v.truthy());
    if (u->op == "-") {
        switch (v.type) {
            case ValueType::Int:   return Value::makeInt(-v.i);
            case ValueType::Float: return Value::makeFloat(-v.f);
            case ValueType::Char:  return Value::makeInt(-static_cast<long long>(
                                       static_cast<unsigned char>(v.c)));
            case ValueType::Bool:  return Value::makeInt(v.b ? -1 : 0);
            default:
                throw RuntimeError("Cannot negate a value of type '" +
                                   v.typeName() + "'", u->line, u->col);
        }
    }
    if (u->op == "+") {
        if (v.isNumeric() || v.type == ValueType::Bool) return v;
        throw RuntimeError("Cannot apply unary '+' to a value of type '" +
                           v.typeName() + "'", u->line, u->col);
    }
    throw RuntimeError("Unknown unary operator '" + u->op + "'", u->line, u->col);
}

Value Interpreter::evalPostfix(PostfixOp* p, std::shared_ptr<Environment> env) {
    return applyIncDec(p->operand.get(), p->op, /*prefix=*/false, env);
}

//----------------------- assignment -----------------------

Value Interpreter::assignTo(Node* target, Value value, std::shared_ptr<Environment> env) {
    switch (target->kind) {
        case NodeKind::Identifier: {
            auto* id = static_cast<Identifier*>(target);
            env->assign(id->name, value);
            return value;
        }
        case NodeKind::Member: {
            auto* m = static_cast<Member*>(target);
            Value obj = evaluate(m->object.get(), env);
            if (obj.type == ValueType::Object) {
                obj.obj->setField(m->name, value);
                return value;
            }
            if (obj.type == ValueType::Module) {
                throw RuntimeError("Cannot assign into module '" + obj.mod->name + "'",
                                   m->line, m->col);
            }
            throw RuntimeError("Cannot set member '" + m->name + "' on a value of type '" +
                               obj.typeName() + "'", m->line, m->col);
        }
        case NodeKind::Index: {
            auto* idx = static_cast<Index*>(target);
            if (idx->indices.size() != 1) {
                throw RuntimeError("Cannot assign to a multi-index gather",
                                   idx->line, idx->col);
            }
            Value obj = evaluate(idx->object.get(), env);
            if (obj.type != ValueType::Array) {
                throw RuntimeError("Cannot index-assign into a value of type '" +
                                   obj.typeName() + "'", idx->line, idx->col);
            }
            Value iv = evaluate(idx->indices[0].get(), env);
            long long i = iv.asInt();
            if (i < 0) throw RuntimeError("Negative array index", idx->line, idx->col);
            // Emerald arrays are dynamic: writing past the end grows the array.
            if (static_cast<size_t>(i) >= obj.arr->size()) {
                obj.arr->resize(static_cast<size_t>(i) + 1, Value::makeNil());
            }
            (*obj.arr)[static_cast<size_t>(i)] = value;
            return value;
        }
        default:
            throw RuntimeError("Invalid assignment target", target->line, target->col);
    }
}

Value Interpreter::evalAssign(Assign* a, std::shared_ptr<Environment> env) {
    Value v = evaluate(a->value.get(), env);
    // `mymotorcyle = Car;` -> automatic typecast: assigning a class value
    // instantiates it (with no constructor arguments).
    if (v.type == ValueType::Class) {
        std::vector<Value> noArgs;
        v = instantiate(v.cls, noArgs, a->line, a->col);
    }
    // Everything is passed (and assigned) by value.
    return assignTo(a->target.get(), v.clone(), env);
}

//----------------------- member access & indexing -----------------------

Value Interpreter::evalMember(Member* m, std::shared_ptr<Environment> env) {
    Value obj = evaluate(m->object.get(), env);
    switch (obj.type) {
        case ValueType::Object: {
            if (obj.obj->hasField(m->name)) return obj.obj->getField(m->name);
            throw RuntimeError("Object of class '" +
                               (obj.obj->cls ? obj.obj->cls->name : "?") +
                               "' has no member '" + m->name + "'", m->line, m->col);
        }
        case ValueType::Module: {
            Value out;
            if (obj.mod->env->getOwn(m->name, out)) return out;
            throw RuntimeError("Module '" + obj.mod->name + "' has no member '" +
                               m->name + "'", m->line, m->col);
        }
        case ValueType::Table: {
            if (m->name == "rows") return Value::makeInt(
                static_cast<long long>(obj.table->rows.size()));
            if (m->name == "columns") {
                std::vector<Value> cols;
                for (auto& c : obj.table->columns) cols.push_back(Value::makeString(c));
                return Value::makeArray(std::move(cols));
            }
            if (m->name == "name") return Value::makeString(obj.table->name);
            throw RuntimeError("Table has no member '" + m->name + "'",
                               m->line, m->col);
        }
        case ValueType::File: {
            if (m->name == "path") return Value::makeString(obj.file->path);
            throw RuntimeError(
                "File member '" + m->name + "' must be called as a method",
                m->line, m->col);
        }
        default:
            throw RuntimeError("Cannot access member '" + m->name +
                               "' on a value of type '" + obj.typeName() + "'",
                               m->line, m->col);
    }
}

Value Interpreter::evalIndex(Index* idx, std::shared_ptr<Environment> env) {
    Value obj = evaluate(idx->object.get(), env);

    auto oneIndex = [&](Node* e) -> long long {
        Value iv = evaluate(e, env);
        return iv.asInt();
    };

    if (obj.type == ValueType::Array) {
        if (idx->indices.size() == 1) {
            long long i = oneIndex(idx->indices[0].get());
            if (i < 0 || static_cast<size_t>(i) >= obj.arr->size())
                throw RuntimeError("Array index " + std::to_string(i) +
                                   " out of range (size " +
                                   std::to_string(obj.arr->size()) + ")",
                                   idx->line, idx->col);
            return (*obj.arr)[static_cast<size_t>(i)];
        }
        // Gather: array[5, 8, 4] -> [array[5], array[8], array[4]]
        std::vector<Value> gathered;
        for (auto& e : idx->indices) {
            long long i = oneIndex(e.get());
            if (i < 0 || static_cast<size_t>(i) >= obj.arr->size())
                throw RuntimeError("Array index " + std::to_string(i) +
                                   " out of range (size " +
                                   std::to_string(obj.arr->size()) + ")",
                                   idx->line, idx->col);
            gathered.push_back((*obj.arr)[static_cast<size_t>(i)].clone());
        }
        return Value::makeArray(std::move(gathered));
    }

    if (obj.type == ValueType::String) {
        const std::string& s = *obj.str;
        if (idx->indices.size() == 1) {
            long long i = oneIndex(idx->indices[0].get());
            if (i < 0 || static_cast<size_t>(i) >= s.size())
                throw RuntimeError("String index " + std::to_string(i) +
                                   " out of range (length " +
                                   std::to_string(s.size()) + ")",
                                   idx->line, idx->col);
            return Value::makeChar(s[static_cast<size_t>(i)]);
        }
        std::string gathered;
        for (auto& e : idx->indices) {
            long long i = oneIndex(e.get());
            if (i < 0 || static_cast<size_t>(i) >= s.size())
                throw RuntimeError("String index " + std::to_string(i) +
                                   " out of range (length " +
                                   std::to_string(s.size()) + ")",
                                   idx->line, idx->col);
            gathered += s[static_cast<size_t>(i)];
        }
        return Value::makeString(std::move(gathered));
    }

    if (obj.type == ValueType::Table) {
        if (idx->indices.size() == 1) {
            long long i = oneIndex(idx->indices[0].get());
            if (i < 0 || static_cast<size_t>(i) >= obj.table->rows.size())
                throw RuntimeError("Table row " + std::to_string(i) +
                                   " out of range", idx->line, idx->col);
            return Value::makeArray(obj.table->rows[static_cast<size_t>(i)]);
        }
    }

    throw RuntimeError("Cannot index a value of type '" + obj.typeName() + "'",
                       idx->line, idx->col);
}

//----------------------- calls -----------------------

Value Interpreter::evalCall(Call* c, std::shared_ptr<Environment> env) {
    // Method-call form: receiver.name(args)
    if (c->callee->kind == NodeKind::Member) {
        auto* m = static_cast<Member*>(c->callee.get());
        Value receiver = evaluate(m->object.get(), env);

        std::vector<Value> args;
        args.reserve(c->args.size());
        for (auto& a : c->args) args.push_back(evaluate(a.get(), env));

        // Built-in methods on File / string / array / table values.
        Value out;
        if (callBuiltinMethod(receiver, m->name, args, c->trailingComma,
                              c->line, c->col, out)) {
            return out;
        }

        // Object method / callable field, or module function.
        Value callee;
        if (receiver.type == ValueType::Object && receiver.obj->hasField(m->name)) {
            callee = receiver.obj->getField(m->name);
        } else if (receiver.type == ValueType::Module) {
            if (!receiver.mod->env->getOwn(m->name, callee))
                throw RuntimeError("Module '" + receiver.mod->name +
                                   "' has no member '" + m->name + "'",
                                   c->line, c->col);
        } else {
            throw RuntimeError("No method '" + m->name + "' on a value of type '" +
                               receiver.typeName() + "'", c->line, c->col);
        }
        // Arguments are passed by value.
        for (auto& a : args) a = a.clone();
        return callValue(callee, args, c->line, c->col);
    }

    Value callee = evaluate(c->callee.get(), env);
    std::vector<Value> args;
    args.reserve(c->args.size());
    for (auto& a : c->args) args.push_back(evaluate(a.get(), env).clone());
    return callValue(callee, args, c->line, c->col);
}

Value Interpreter::callValue(const Value& callee, std::vector<Value>& args,
                             int line, int col) {
    switch (callee.type) {
        case ValueType::Builtin:
            try {
                return callee.builtin(*this, args);
            } catch (RuntimeError& e) {
                if (e.line < 0) throw RuntimeError(std::string(e.what()), line, col);
                throw;
            }
        case ValueType::Function: {
            auto& fn = *callee.fn;
            auto callEnv = std::make_shared<Environment>(fn.closure);
            for (size_t i = 0; i < fn.params.size(); i++) {
                Value arg = i < args.size() ? args[i] : Value::makeNil();
                callEnv->define(fn.params[i].first, arg);
            }
            try {
                executeBlock(fn.body, callEnv);
            } catch (ReturnSignal& sig) {
                return sig.value;
            }
            return Value::makeNil();
        }
        case ValueType::Class:
            return instantiate(callee.cls, args, line, col);
        default:
            throw RuntimeError("Value of type '" + callee.typeName() +
                               "' is not callable", line, col);
    }
}

//----------------------- class instantiation -----------------------

namespace {
// Runs one class body (parent chain first) against a shared object.
void constructLevel(Interpreter& interp, const std::shared_ptr<ClassData>& cls,
                    const std::shared_ptr<ObjectData>& obj,
                    std::vector<Value>& args, int line, int col,
                    std::vector<std::shared_ptr<Environment>>& methodEnvs);
} // namespace

Value Interpreter::instantiate(const std::shared_ptr<ClassData>& cls,
                               std::vector<Value>& args, int line, int col) {
    auto obj = std::make_shared<ObjectData>();
    obj->cls = cls;
    std::vector<std::shared_ptr<Environment>> methodEnvs;
    constructLevel(*this, cls, obj, args, line, col, methodEnvs);
    // Construction is complete: method closures must no longer create fields
    // on assignment to unknown names.
    for (auto& e : methodEnvs) e->constructing = false;
    return Value::makeObject(obj);
}

namespace {
void constructLevel(Interpreter& interp, const std::shared_ptr<ClassData>& cls,
                    const std::shared_ptr<ObjectData>& obj,
                    std::vector<Value>& args, int line, int col,
                    std::vector<std::shared_ptr<Environment>>& methodEnvs) {
    auto env = std::make_shared<Environment>(cls->closure);
    env->object = obj;
    env->constructing = true;
    methodEnvs.push_back(env);

    // Bind constructor parameters as locals (they do not become fields).
    for (size_t i = 0; i < cls->params.size(); i++) {
        Value arg = i < args.size() ? args[i].clone() : Value::makeNil();
        env->define(cls->params[i].first, arg);
    }

    // Run the parent constructor first (its args are evaluated in the child's
    // constructor scope, so they can reference the child's parameters).
    if (cls->decl && cls->decl->hasParent && cls->parent) {
        std::vector<Value> parentArgs;
        for (auto& a : cls->decl->parentArgs) {
            parentArgs.push_back(interp.evaluate(a.get(), env).clone());
        }
        constructLevel(interp, cls->parent, obj, parentArgs, line, col, methodEnvs);
    }

    // Execute the class body: declarations become fields, nested fn/class
    // declarations become methods / nested classes.
    if (cls->decl && cls->decl->body) {
        interp.executeBlock(cls->decl->body.get(), env);
    }
}
} // namespace

//----------------------- declared-type coercion -----------------------

Value Interpreter::coerceToType(const std::string& typeName, const Value& v,
                                int line, int col) {
    if (typeName == "int") {
        switch (v.type) {
            case ValueType::Nil:   return Value::makeInt(0);
            case ValueType::Int:   return v;
            case ValueType::Float: return Value::makeInt(static_cast<long long>(v.f));
            case ValueType::Char:  return Value::makeInt(
                static_cast<long long>(static_cast<unsigned char>(v.c)));
            case ValueType::Bool:  return Value::makeInt(v.b ? 1 : 0);
            case ValueType::String: {
                try { return Value::makeInt(std::stoll(*v.str)); }
                catch (...) {
                    throw RuntimeError("Cannot convert \"" + *v.str + "\" to int",
                                       line, col);
                }
            }
            default: break;
        }
        throw RuntimeError("Cannot store a value of type '" + v.typeName() +
                           "' in an int variable", line, col);
    }
    if (typeName == "float") {
        switch (v.type) {
            case ValueType::Nil:   return Value::makeFloat(0.0);
            case ValueType::Float: return v;
            case ValueType::Int:   return Value::makeFloat(static_cast<double>(v.i));
            case ValueType::Char:  return Value::makeFloat(
                static_cast<double>(static_cast<unsigned char>(v.c)));
            case ValueType::Bool:  return Value::makeFloat(v.b ? 1.0 : 0.0);
            case ValueType::String: {
                try { return Value::makeFloat(std::stod(*v.str)); }
                catch (...) {
                    throw RuntimeError("Cannot convert \"" + *v.str + "\" to float",
                                       line, col);
                }
            }
            default: break;
        }
        throw RuntimeError("Cannot store a value of type '" + v.typeName() +
                           "' in a float variable", line, col);
    }
    if (typeName == "char") {
        switch (v.type) {
            case ValueType::Nil:  return Value::makeChar('\0');
            case ValueType::Char: return v;
            case ValueType::Int:  return Value::makeChar(static_cast<char>(v.i));
            case ValueType::String:
                if (v.str->size() == 1) return Value::makeChar((*v.str)[0]);
                throw RuntimeError("Cannot store a string of length " +
                                   std::to_string(v.str->size()) +
                                   " in a char variable", line, col);
            default: break;
        }
        throw RuntimeError("Cannot store a value of type '" + v.typeName() +
                           "' in a char variable", line, col);
    }
    if (typeName == "string") {
        if (v.type == ValueType::String) return v;
        if (v.isNil()) return Value::makeString("");
        return Value::makeString(v.toString());
    }
    return v;
}

} // namespace emerald
