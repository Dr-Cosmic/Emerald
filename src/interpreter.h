// interpreter.h - The tree-walking evaluator for Emerald.
#pragma once
#include "ast.h"
#include "value.h"
#include "environment.h"
#include "sql.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <iostream>

namespace emerald {

// Non-error control-flow signals implemented as exceptions.
struct BreakSignal {};
struct ContinueSignal {};
struct ReturnSignal { Value value; };

class Interpreter {
public:
    Interpreter();

    // Execute a whole parsed program in the global scope.
    void run(Program& program);

    // Execute a single statement (used by the REPL). If the statement is an
    // expression statement, its value is returned (nil otherwise).
    Value runStatement(Node* stmt);

    // The directory of the currently running script; used to resolve imports.
    void setScriptDir(const std::string& dir) { scriptDir_ = dir; }
    const std::string& scriptDir() const { return scriptDir_; }

    std::shared_ptr<Environment> globals() { return globals_; }

    SqlEngine& sql() { return sql_; }

    // Output stream for print builtins (std::cout by default; swappable for tests).
    std::ostream* out = &std::cout;

    // --- evaluation (public so builtins can call back in) ---
    Value evaluate(Node* node, std::shared_ptr<Environment> env);
    void execute(Node* node, std::shared_ptr<Environment> env);
    void executeBlock(Block* block, std::shared_ptr<Environment> env);

    // Call any callable value with already-evaluated arguments.
    Value callValue(const Value& callee, std::vector<Value>& args, int line, int col);

    // Instantiate a class (runs constructor body, handles inheritance).
    Value instantiate(const std::shared_ptr<ClassData>& cls,
                      std::vector<Value>& args, int line, int col);

    // Render a table the way printtbl does (also used when print() gets a table).
    static std::string renderTable(const TableData& t);

private:
    std::shared_ptr<Environment> globals_;
    SqlEngine sql_;
    std::string scriptDir_;

    // Import machinery.
    std::unordered_map<std::string, Value> moduleCache_; // abs path -> module value
    std::unordered_set<std::string> importing_;          // cycle detection

    void installBuiltins();

    // --- statements ---
    void execVarDecl(VarDecl* d, std::shared_ptr<Environment> env);
    void execFnDecl(FnDecl* d, std::shared_ptr<Environment> env);
    void execClassDecl(ClassDecl* d, std::shared_ptr<Environment> env);
    void execIf(If* s, std::shared_ptr<Environment> env);
    void execForClassic(ForClassic* s, std::shared_ptr<Environment> env);
    void execForIn(ForIn* s, std::shared_ptr<Environment> env);
    void execWhile(While* s, std::shared_ptr<Environment> env);
    void execImport(Import* s, std::shared_ptr<Environment> env);

    // --- expressions ---
    Value evalStringLit(StringLit* s, std::shared_ptr<Environment> env);
    Value evalBinary(Binary* b, std::shared_ptr<Environment> env);
    Value evalUnary(Unary* u, std::shared_ptr<Environment> env);
    Value evalPostfix(PostfixOp* p, std::shared_ptr<Environment> env);
    Value evalAssign(Assign* a, std::shared_ptr<Environment> env);
    Value evalCall(Call* c, std::shared_ptr<Environment> env);
    Value evalIndex(Index* idx, std::shared_ptr<Environment> env);
    Value evalMember(Member* m, std::shared_ptr<Environment> env);

    // Assignment plumbing (returns the stored value).
    Value assignTo(Node* target, Value value, std::shared_ptr<Environment> env);
    Value applyIncDec(Node* operand, const std::string& op, bool prefix,
                      std::shared_ptr<Environment> env);

    // Method dispatch on built-in receiver types (File, string, array, table).
    bool callBuiltinMethod(const Value& receiver, const std::string& name,
                           std::vector<Value>& args, bool trailingComma,
                           int line, int col, Value& out);

    // Operators.
    Value binaryNumeric(const std::string& op, const Value& l, const Value& r,
                        int line, int col);
    static bool looseEquals(const Value& l, const Value& r);
    static bool strictEquals(const Value& l, const Value& r);
    static int compareValues(const Value& l, const Value& r, int line, int col);

    // Declared-type coercion for `int x = ...` style declarations.
    Value coerceToType(const std::string& typeName, const Value& v, int line, int col);

    // Import resolution.
    Value loadModule(const std::string& moduleName, int line, int col);
    std::string findModuleFile(const std::string& moduleName) const;

    friend void installEmeraldBuiltins(Interpreter& interp, Environment& g);
};

// Defined in builtins.cpp.
void installEmeraldBuiltins(Interpreter& interp, Environment& g);

// File helpers shared between builtins and interpreter (defined in builtins.cpp).
std::string fileReadAll(const std::string& path, int line, int col);
void fileWriteAll(const std::string& path, const std::string& content, int line, int col);

} // namespace emerald
