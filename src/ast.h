// ast.h - Abstract Syntax Tree node definitions for Emerald.
#pragma once
#include <string>
#include <vector>
#include <memory>

namespace emerald {

enum class NodeKind {
    // Expressions
    IntLit, FloatLit, CharLit, StringLit, BoolLit, NilLit,
    ArrayLit, Identifier, Binary, Unary, PostfixOp, Assign,
    Call, Index, Member,
    // Statements
    VarDecl, ExprStmt, Block, FnDecl, ClassDecl, Return,
    If, ForClassic, ForIn, While, Import, Break, Continue
};

struct Node {
    NodeKind kind;
    int line = 0;
    int col = 0;
    explicit Node(NodeKind k) : kind(k) {}
    virtual ~Node() = default;
};

using NodePtr = std::unique_ptr<Node>;

//======================= Expressions =======================

struct IntLit : Node {
    long long value;
    explicit IntLit(long long v) : Node(NodeKind::IntLit), value(v) {}
};

struct FloatLit : Node {
    double value;
    explicit FloatLit(double v) : Node(NodeKind::FloatLit), value(v) {}
};

struct CharLit : Node {
    char value;
    explicit CharLit(char v) : Node(NodeKind::CharLit), value(v) {}
};

struct BoolLit : Node {
    bool value;
    explicit BoolLit(bool v) : Node(NodeKind::BoolLit), value(v) {}
};

struct NilLit : Node {
    NilLit() : Node(NodeKind::NilLit) {}
};

// A piece of an interpolated string: either literal text or an embedded expression.
struct StringPart {
    bool isExpr;
    std::string literal;   // valid when !isExpr
    NodePtr expr;          // valid when isExpr
};

struct StringLit : Node {
    std::vector<StringPart> parts;
    StringLit() : Node(NodeKind::StringLit) {}
};

struct ArrayLit : Node {
    std::vector<NodePtr> elements;
    ArrayLit() : Node(NodeKind::ArrayLit) {}
};

struct Identifier : Node {
    std::string name;
    explicit Identifier(std::string n) : Node(NodeKind::Identifier), name(std::move(n)) {}
};

struct Binary : Node {
    std::string op;
    NodePtr left;
    NodePtr right;
    Binary() : Node(NodeKind::Binary) {}
};

struct Unary : Node {
    std::string op;   // "-", "!", "++", "--"
    NodePtr operand;
    Unary() : Node(NodeKind::Unary) {}
};

// Postfix ++ / -- applied to an lvalue.
struct PostfixOp : Node {
    std::string op;   // "++" or "--"
    NodePtr operand;
    PostfixOp() : Node(NodeKind::PostfixOp) {}
};

struct Assign : Node {
    NodePtr target;   // Identifier, Member, or Index
    NodePtr value;
    Assign() : Node(NodeKind::Assign) {}
};

struct Call : Node {
    NodePtr callee;
    std::vector<NodePtr> args;
    bool trailingComma = false; // e.g. myfile.read(5,) -> read from index 5 to end
    Call() : Node(NodeKind::Call) {}
};

struct Index : Node {
    NodePtr object;
    std::vector<NodePtr> indices; // one index = element; multiple = gather
    Index() : Node(NodeKind::Index) {}
};

struct Member : Node {
    NodePtr object;
    std::string name;
    Member() : Node(NodeKind::Member) {}
};

//======================= Statements =======================

struct VarDecl : Node {
    std::string declaredType; // "int","float","char","string","File", or a class name
    std::string name;
    bool isArray = false;
    std::vector<NodePtr> ctorArgs; // for class/File instantiation
    bool hasCtorArgs = false;
    NodePtr initializer;           // may be null
    VarDecl() : Node(NodeKind::VarDecl) {}
};

struct ExprStmt : Node {
    NodePtr expr;
    ExprStmt() : Node(NodeKind::ExprStmt) {}
};

struct Block : Node {
    std::vector<NodePtr> statements;
    Block() : Node(NodeKind::Block) {}
};

struct Param {
    std::string name;
    std::string type; // optional, "" if untyped
};

struct FnDecl : Node {
    std::string name;
    std::vector<Param> params;
    std::unique_ptr<Block> body;
    FnDecl() : Node(NodeKind::FnDecl) {}
};

struct ClassDecl : Node {
    std::string name;
    std::vector<Param> params;
    bool isObjectVarDecl = false; // `class foo;` form (declares a nil object variable)
    // Inheritance
    bool hasParent = false;
    std::string parentName;
    std::vector<NodePtr> parentArgs;
    std::unique_ptr<Block> body;
    ClassDecl() : Node(NodeKind::ClassDecl) {}
};

struct Return : Node {
    NodePtr value; // may be null
    Return() : Node(NodeKind::Return) {}
};

struct IfBranch {
    NodePtr condition;          // null for the final else
    std::unique_ptr<Block> body;
};

struct If : Node {
    std::vector<IfBranch> branches;
    If() : Node(NodeKind::If) {}
};

struct ForClassic : Node {
    NodePtr init;      // statement or expression (may be null)
    NodePtr condition; // expression (may be null -> always true)
    NodePtr update;    // expression (may be null)
    std::unique_ptr<Block> body;
    ForClassic() : Node(NodeKind::ForClassic) {}
};

struct ForIn : Node {
    std::string varName;
    NodePtr iterable;
    std::unique_ptr<Block> body;
    ForIn() : Node(NodeKind::ForIn) {}
};

struct While : Node {
    NodePtr condition;
    std::unique_ptr<Block> body;
    While() : Node(NodeKind::While) {}
};

struct Import : Node {
    std::string moduleName;
    bool isFrom = false;
    std::string symbolName; // valid when isFrom
    Import() : Node(NodeKind::Import) {}
};

struct Break : Node { Break() : Node(NodeKind::Break) {} };
struct Continue : Node { Continue() : Node(NodeKind::Continue) {} };

// A parsed program is just a list of statements.
struct Program {
    std::vector<NodePtr> statements;
};

} // namespace emerald
