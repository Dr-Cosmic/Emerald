// parser.h - Recursive-descent parser that builds an AST from tokens.
#pragma once
#include "token.h"
#include "ast.h"
#include <vector>
#include <memory>

namespace emerald {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Parse a full program.
    std::unique_ptr<Program> parseProgram();

    // Parse a single expression from an isolated token stream (used for
    // string interpolation). Expects EOF after the expression.
    NodePtr parseStandaloneExpression();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    // --- token helpers ---
    const Token& peek(size_t offset = 0) const;
    const Token& previous() const;
    bool atEnd() const;
    bool check(TokenType type) const;
    bool checkNext(TokenType type) const;
    const Token& advance();
    bool match(TokenType type);
    const Token& expect(TokenType type, const std::string& message);
    [[noreturn]] void error(const Token& tok, const std::string& message);

    // --- statements ---
    NodePtr parseStatement();
    NodePtr parseFnDecl();
    NodePtr parseClassDecl();
    NodePtr parseVarDecl();
    NodePtr parseIf();
    NodePtr parseFor();
    NodePtr parseWhile();
    NodePtr parseReturn();
    NodePtr parseImport();
    std::unique_ptr<Block> parseBlock();
    std::vector<Param> parseParamList();

    bool looksLikeVarDecl() const;
    static bool isTypeKeyword(TokenType t);
    static std::string typeKeywordName(TokenType t);
    static bool startsWithUppercase(const std::string& s);

    // --- expressions (precedence climbing) ---
    NodePtr parseExpression();
    NodePtr parseAssignment();
    NodePtr parseLogicOr();
    NodePtr parseLogicAnd();
    NodePtr parseEquality();
    NodePtr parseComparison();
    NodePtr parseAdditive();
    NodePtr parseMultiplicative();
    NodePtr parseUnary();
    NodePtr parsePower();
    NodePtr parsePostfix();
    NodePtr parsePrimary();
    NodePtr parseArrayLiteral();
    std::vector<NodePtr> parseArguments(bool* trailingComma = nullptr);

    // Build an interpolated-string node from a raw string value.
    NodePtr buildStringLiteral(const std::string& raw, int line, int col);

    bool isLValue(const Node* n) const;
};

} // namespace emerald
