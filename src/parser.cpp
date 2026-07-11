// parser.cpp - Implementation of the Emerald recursive-descent parser.
#include "parser.h"
#include "lexer.h"
#include "errors.h"
#include <cctype>

namespace emerald {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

// -------------------- token helpers --------------------

const Token& Parser::peek(size_t offset) const {
    size_t p = pos_ + offset;
    if (p >= tokens_.size()) return tokens_.back(); // EOF
    return tokens_[p];
}

const Token& Parser::previous() const {
    return tokens_[pos_ == 0 ? 0 : pos_ - 1];
}

bool Parser::atEnd() const {
    return peek().type == TokenType::Eof;
}

bool Parser::check(TokenType type) const {
    return peek().type == type;
}

bool Parser::checkNext(TokenType type) const {
    return peek(1).type == type;
}

const Token& Parser::advance() {
    if (!atEnd()) pos_++;
    return previous();
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

const Token& Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    error(peek(), message);
}

void Parser::error(const Token& tok, const std::string& message) {
    std::string full = message + " (got '" +
        (tok.type == TokenType::Eof ? std::string("end of file")
                                    : std::string(tokenTypeName(tok.type))) +
        (tok.lexeme.empty() ? std::string("") : (" \"" + tok.lexeme + "\"")) + "')";
    throw ParseError(full, tok.line, tok.col);
}

// -------------------- helpers --------------------

bool Parser::isTypeKeyword(TokenType t) {
    return t == TokenType::KwInt || t == TokenType::KwFloat ||
           t == TokenType::KwChar || t == TokenType::KwString ||
           t == TokenType::KwFile;
}

std::string Parser::typeKeywordName(TokenType t) {
    switch (t) {
        case TokenType::KwInt: return "int";
        case TokenType::KwFloat: return "float";
        case TokenType::KwChar: return "char";
        case TokenType::KwString: return "string";
        case TokenType::KwFile: return "File";
        default: return "";
    }
}

bool Parser::startsWithUppercase(const std::string& s) {
    return !s.empty() && std::isupper(static_cast<unsigned char>(s[0]));
}

// Decide whether the upcoming tokens begin a variable declaration.
//   int x ...          -> type keyword followed by an identifier
//   ClassName obj ...  -> uppercase identifier followed by an identifier
bool Parser::looksLikeVarDecl() const {
    TokenType t = peek().type;
    if (isTypeKeyword(t) && peek(1).type == TokenType::Identifier) {
        return true;
    }
    if (t == TokenType::Identifier && startsWithUppercase(peek().lexeme) &&
        peek(1).type == TokenType::Identifier) {
        return true;
    }
    return false;
}

// -------------------- program / standalone --------------------

std::unique_ptr<Program> Parser::parseProgram() {
    auto program = std::make_unique<Program>();
    while (!atEnd()) {
        program->statements.push_back(parseStatement());
    }
    return program;
}

NodePtr Parser::parseStandaloneExpression() {
    NodePtr expr = parseExpression();
    if (!atEnd()) {
        error(peek(), "Unexpected token after interpolation expression");
    }
    return expr;
}

// -------------------- statements --------------------

NodePtr Parser::parseStatement() {
    switch (peek().type) {
        case TokenType::KwFn: return parseFnDecl();
        case TokenType::KwClass: return parseClassDecl();
        case TokenType::KwIf: return parseIf();
        case TokenType::KwFor: return parseFor();
        case TokenType::KwWhile: return parseWhile();
        case TokenType::KwReturn: return parseReturn();
        case TokenType::KwImport:
        case TokenType::KwFrom: return parseImport();
        case TokenType::KwBreak: {
            const Token& t = advance();
            expect(TokenType::Semicolon, "Expected ';' after 'break'");
            auto n = std::make_unique<Break>();
            n->line = t.line; n->col = t.col;
            return n;
        }
        case TokenType::KwContinue: {
            const Token& t = advance();
            expect(TokenType::Semicolon, "Expected ';' after 'continue'");
            auto n = std::make_unique<Continue>();
            n->line = t.line; n->col = t.col;
            return n;
        }
        case TokenType::LBrace:
            return parseBlock();
        default:
            break;
    }

    if (looksLikeVarDecl()) {
        return parseVarDecl();
    }

    // Expression statement.
    const Token& start = peek();
    NodePtr expr = parseExpression();
    expect(TokenType::Semicolon, "Expected ';' after expression");
    auto stmt = std::make_unique<ExprStmt>();
    stmt->expr = std::move(expr);
    stmt->line = start.line; stmt->col = start.col;
    return stmt;
}

std::vector<Param> Parser::parseParamList() {
    std::vector<Param> params;
    if (check(TokenType::RParen)) return params;
    do {
        Param p;
        // Optional type prefix: `int x` or bare `x`.
        if (isTypeKeyword(peek().type) && peek(1).type == TokenType::Identifier) {
            p.type = typeKeywordName(peek().type);
            advance();
        } else if (check(TokenType::Identifier) && startsWithUppercase(peek().lexeme) &&
                   peek(1).type == TokenType::Identifier) {
            p.type = peek().lexeme;
            advance();
        }
        const Token& name = expect(TokenType::Identifier, "Expected parameter name");
        p.name = name.lexeme;
        params.push_back(p);
    } while (match(TokenType::Comma));
    return params;
}

NodePtr Parser::parseFnDecl() {
    const Token& kw = advance(); // 'fn'
    const Token& name = expect(TokenType::Identifier, "Expected function name after 'fn'");
    auto fn = std::make_unique<FnDecl>();
    fn->name = name.lexeme;
    fn->line = kw.line; fn->col = kw.col;
    expect(TokenType::LParen, "Expected '(' after function name");
    fn->params = parseParamList();
    expect(TokenType::RParen, "Expected ')' after parameters");
    fn->body = parseBlock();
    return fn;
}

NodePtr Parser::parseClassDecl() {
    const Token& kw = advance(); // 'class'
    const Token& name = expect(TokenType::Identifier, "Expected class name after 'class'");
    auto cls = std::make_unique<ClassDecl>();
    cls->name = name.lexeme;
    cls->line = kw.line; cls->col = kw.col;

    // `class foo;` -> object variable declaration (initialized to nil).
    if (check(TokenType::Semicolon)) {
        advance();
        cls->isObjectVarDecl = true;
        return cls;
    }

    // Optional constructor parameters.
    if (match(TokenType::LParen)) {
        cls->params = parseParamList();
        expect(TokenType::RParen, "Expected ')' after class parameters");
    }

    // Optional inheritance: = Parent(args)
    if (match(TokenType::Assign)) {
        const Token& parent = expect(TokenType::Identifier, "Expected parent class name after '='");
        cls->hasParent = true;
        cls->parentName = parent.lexeme;
        expect(TokenType::LParen, "Expected '(' after parent class name");
        cls->parentArgs = parseArguments();
        expect(TokenType::RParen, "Expected ')' after parent arguments");
    }

    cls->body = parseBlock();
    return cls;
}

NodePtr Parser::parseVarDecl() {
    const Token& typeTok = advance();
    std::string typeName = isTypeKeyword(typeTok.type)
        ? typeKeywordName(typeTok.type)
        : typeTok.lexeme;

    const Token& name = expect(TokenType::Identifier, "Expected variable name in declaration");
    auto decl = std::make_unique<VarDecl>();
    decl->declaredType = typeName;
    decl->name = name.lexeme;
    decl->line = typeTok.line; decl->col = typeTok.col;

    // Array marker `[]`.
    if (check(TokenType::LBracket) && checkNext(TokenType::RBracket)) {
        advance(); advance();
        decl->isArray = true;
    }

    // Constructor arguments `( ... )` (for class / File types).
    if (match(TokenType::LParen)) {
        decl->hasCtorArgs = true;
        decl->ctorArgs = parseArguments();
        expect(TokenType::RParen, "Expected ')' after constructor arguments");
    }

    // Initializer.
    if (match(TokenType::Assign)) {
        if (decl->isArray) {
            // One or more comma-separated expressions. Multiple -> 2D array.
            std::vector<NodePtr> rows;
            rows.push_back(parseExpression());
            while (match(TokenType::Comma)) {
                rows.push_back(parseExpression());
            }
            if (rows.size() == 1) {
                decl->initializer = std::move(rows[0]);
            } else {
                auto arr = std::make_unique<ArrayLit>();
                arr->line = decl->line; arr->col = decl->col;
                for (auto& r : rows) arr->elements.push_back(std::move(r));
                decl->initializer = std::move(arr);
            }
        } else {
            decl->initializer = parseExpression();
        }
    }

    expect(TokenType::Semicolon, "Expected ';' after variable declaration");
    return decl;
}

std::unique_ptr<Block> Parser::parseBlock() {
    const Token& lb = expect(TokenType::LBrace, "Expected '{'");
    auto block = std::make_unique<Block>();
    block->line = lb.line; block->col = lb.col;
    while (!check(TokenType::RBrace) && !atEnd()) {
        block->statements.push_back(parseStatement());
    }
    expect(TokenType::RBrace, "Expected '}' to close block");
    return block;
}

NodePtr Parser::parseIf() {
    const Token& kw = advance(); // 'if'
    auto node = std::make_unique<If>();
    node->line = kw.line; node->col = kw.col;

    IfBranch first;
    expect(TokenType::LParen, "Expected '(' after 'if'");
    first.condition = parseExpression();
    expect(TokenType::RParen, "Expected ')' after if condition");
    first.body = parseBlock();
    node->branches.push_back(std::move(first));

    while (check(TokenType::KwElif)) {
        advance();
        IfBranch b;
        expect(TokenType::LParen, "Expected '(' after 'elif'");
        b.condition = parseExpression();
        expect(TokenType::RParen, "Expected ')' after elif condition");
        b.body = parseBlock();
        node->branches.push_back(std::move(b));
    }

    if (check(TokenType::KwElse)) {
        advance();
        IfBranch b;
        b.condition = nullptr;
        b.body = parseBlock();
        node->branches.push_back(std::move(b));
    }

    return node;
}

NodePtr Parser::parseFor() {
    const Token& kw = advance(); // 'for'

    // for-in:  for IDENT in EXPR { ... }
    if (check(TokenType::Identifier) && checkNext(TokenType::KwIn)) {
        auto node = std::make_unique<ForIn>();
        node->line = kw.line; node->col = kw.col;
        node->varName = advance().lexeme; // IDENT
        advance();                        // 'in'
        node->iterable = parseExpression();
        node->body = parseBlock();
        return node;
    }

    // C-style:  for ( init ; cond ; update ) { ... }
    auto node = std::make_unique<ForClassic>();
    node->line = kw.line; node->col = kw.col;
    expect(TokenType::LParen, "Expected '(' or 'IDENT in' after 'for'");

    // init
    if (!check(TokenType::Semicolon)) {
        if (looksLikeVarDecl()) {
            // Reuse var-decl but it consumes the ';'.
            node->init = parseVarDecl();
        } else {
            node->init = parseExpression();
            expect(TokenType::Semicolon, "Expected ';' after for-loop initializer");
        }
    } else {
        advance(); // ';'
    }

    // condition
    if (!check(TokenType::Semicolon)) {
        node->condition = parseExpression();
    }
    expect(TokenType::Semicolon, "Expected ';' after for-loop condition");

    // update
    if (!check(TokenType::RParen)) {
        node->update = parseExpression();
    }
    expect(TokenType::RParen, "Expected ')' after for-loop clauses");

    node->body = parseBlock();
    return node;
}

NodePtr Parser::parseWhile() {
    const Token& kw = advance(); // 'while'
    auto node = std::make_unique<While>();
    node->line = kw.line; node->col = kw.col;
    expect(TokenType::LParen, "Expected '(' after 'while'");
    node->condition = parseExpression();
    expect(TokenType::RParen, "Expected ')' after while condition");
    node->body = parseBlock();
    return node;
}

NodePtr Parser::parseReturn() {
    const Token& kw = advance(); // 'return'
    auto node = std::make_unique<Return>();
    node->line = kw.line; node->col = kw.col;
    if (!check(TokenType::Semicolon)) {
        node->value = parseExpression();
    }
    expect(TokenType::Semicolon, "Expected ';' after return statement");
    return node;
}

NodePtr Parser::parseImport() {
    if (check(TokenType::KwFrom)) {
        const Token& kw = advance(); // 'from'
        const Token& mod = expect(TokenType::Identifier, "Expected module name after 'from'");
        expect(TokenType::KwImport, "Expected 'import' after module name");
        const Token& sym = expect(TokenType::Identifier, "Expected symbol name after 'import'");
        expect(TokenType::Semicolon, "Expected ';' after import statement");
        auto node = std::make_unique<Import>();
        node->line = kw.line; node->col = kw.col;
        node->isFrom = true;
        node->moduleName = mod.lexeme;
        node->symbolName = sym.lexeme;
        return node;
    }
    const Token& kw = advance(); // 'import'
    const Token& mod = expect(TokenType::Identifier, "Expected module name after 'import'");
    expect(TokenType::Semicolon, "Expected ';' after import statement");
    auto node = std::make_unique<Import>();
    node->line = kw.line; node->col = kw.col;
    node->moduleName = mod.lexeme;
    return node;
}

// -------------------- expressions --------------------

NodePtr Parser::parseExpression() {
    return parseAssignment();
}

bool Parser::isLValue(const Node* n) const {
    return n->kind == NodeKind::Identifier ||
           n->kind == NodeKind::Member ||
           n->kind == NodeKind::Index;
}

NodePtr Parser::parseAssignment() {
    NodePtr left = parseLogicOr();
    if (check(TokenType::Assign)) {
        const Token& eq = advance();
        NodePtr right = parseAssignment(); // right associative
        if (!isLValue(left.get())) {
            error(eq, "Invalid assignment target");
        }
        auto node = std::make_unique<Assign>();
        node->line = eq.line; node->col = eq.col;
        node->target = std::move(left);
        node->value = std::move(right);
        return node;
    }
    return left;
}

NodePtr Parser::parseLogicOr() {
    NodePtr left = parseLogicAnd();
    while (check(TokenType::Or)) {
        const Token& op = advance();
        NodePtr right = parseLogicAnd();
        auto node = std::make_unique<Binary>();
        node->op = "||"; node->line = op.line; node->col = op.col;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

NodePtr Parser::parseLogicAnd() {
    NodePtr left = parseEquality();
    while (check(TokenType::And)) {
        const Token& op = advance();
        NodePtr right = parseEquality();
        auto node = std::make_unique<Binary>();
        node->op = "&&"; node->line = op.line; node->col = op.col;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

NodePtr Parser::parseEquality() {
    NodePtr left = parseComparison();
    while (check(TokenType::Eq) || check(TokenType::StrictEq) ||
           check(TokenType::NotEq) || check(TokenType::StrictNotEq)) {
        const Token& op = advance();
        NodePtr right = parseComparison();
        auto node = std::make_unique<Binary>();
        node->op = tokenTypeName(op.type);
        node->line = op.line; node->col = op.col;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

NodePtr Parser::parseComparison() {
    NodePtr left = parseAdditive();
    while (check(TokenType::Lt) || check(TokenType::Gt) ||
           check(TokenType::Le) || check(TokenType::Ge)) {
        const Token& op = advance();
        NodePtr right = parseAdditive();
        auto node = std::make_unique<Binary>();
        node->op = tokenTypeName(op.type);
        node->line = op.line; node->col = op.col;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

NodePtr Parser::parseAdditive() {
    NodePtr left = parseMultiplicative();
    while (check(TokenType::Plus) || check(TokenType::Minus)) {
        const Token& op = advance();
        NodePtr right = parseMultiplicative();
        auto node = std::make_unique<Binary>();
        node->op = tokenTypeName(op.type);
        node->line = op.line; node->col = op.col;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

NodePtr Parser::parseMultiplicative() {
    NodePtr left = parseUnary();
    while (check(TokenType::Star) || check(TokenType::Slash) ||
           check(TokenType::FloorDiv) || check(TokenType::Percent)) {
        const Token& op = advance();
        NodePtr right = parseUnary();
        auto node = std::make_unique<Binary>();
        node->op = tokenTypeName(op.type);
        node->line = op.line; node->col = op.col;
        node->left = std::move(left); node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

NodePtr Parser::parseUnary() {
    if (check(TokenType::Minus) || check(TokenType::Not) ||
        check(TokenType::Plus) || check(TokenType::Increment) ||
        check(TokenType::Decrement)) {
        const Token& op = advance();
        NodePtr operand = parseUnary();
        auto node = std::make_unique<Unary>();
        node->op = tokenTypeName(op.type);
        node->line = op.line; node->col = op.col;
        node->operand = std::move(operand);
        return node;
    }
    return parsePower();
}

NodePtr Parser::parsePower() {
    NodePtr base = parsePostfix();
    if (check(TokenType::Power)) {
        const Token& op = advance();
        // Right-associative; exponent may itself be unary (e.g. 2 ** -3).
        NodePtr exponent = parseUnary();
        auto node = std::make_unique<Binary>();
        node->op = "**"; node->line = op.line; node->col = op.col;
        node->left = std::move(base); node->right = std::move(exponent);
        return node;
    }
    return base;
}

std::vector<NodePtr> Parser::parseArguments(bool* trailingComma) {
    std::vector<NodePtr> args;
    if (trailingComma) *trailingComma = false;
    if (check(TokenType::RParen)) return args;
    while (true) {
        args.push_back(parseExpression());
        if (!match(TokenType::Comma)) break;
        if (check(TokenType::RParen)) {
            // Shorthand like myfile.read(5,) - remember the trailing comma.
            if (trailingComma) *trailingComma = true;
            break;
        }
    }
    return args;
}

NodePtr Parser::parsePostfix() {
    NodePtr expr = parsePrimary();
    while (true) {
        if (check(TokenType::LParen)) {
            const Token& lp = advance();
            auto call = std::make_unique<Call>();
            call->line = lp.line; call->col = lp.col;
            call->callee = std::move(expr);
            call->args = parseArguments(&call->trailingComma);
            expect(TokenType::RParen, "Expected ')' after call arguments");
            expr = std::move(call);
        } else if (check(TokenType::LBracket)) {
            const Token& lb = advance();
            auto idx = std::make_unique<Index>();
            idx->line = lb.line; idx->col = lb.col;
            idx->object = std::move(expr);
            idx->indices.push_back(parseExpression());
            while (match(TokenType::Comma)) {
                idx->indices.push_back(parseExpression());
            }
            expect(TokenType::RBracket, "Expected ']' after index");
            expr = std::move(idx);
        } else if (check(TokenType::Dot)) {
            const Token& dot = advance();
            const Token& name = expect(TokenType::Identifier, "Expected member name after '.'");
            auto member = std::make_unique<Member>();
            member->line = dot.line; member->col = dot.col;
            member->object = std::move(expr);
            member->name = name.lexeme;
            expr = std::move(member);
        } else if (check(TokenType::Increment) || check(TokenType::Decrement)) {
            const Token& op = advance();
            if (!isLValue(expr.get())) {
                error(op, "Invalid target for postfix operator");
            }
            auto node = std::make_unique<PostfixOp>();
            node->op = tokenTypeName(op.type);
            node->line = op.line; node->col = op.col;
            node->operand = std::move(expr);
            expr = std::move(node);
            // Postfix ++/-- does not chain further meaningfully.
            break;
        } else {
            break;
        }
    }
    return expr;
}

NodePtr Parser::parseArrayLiteral() {
    const Token& lb = advance(); // '['
    auto arr = std::make_unique<ArrayLit>();
    arr->line = lb.line; arr->col = lb.col;
    if (!check(TokenType::RBracket)) {
        do {
            arr->elements.push_back(parseExpression());
        } while (match(TokenType::Comma));
    }
    expect(TokenType::RBracket, "Expected ']' to close array literal");
    return arr;
}

NodePtr Parser::parsePrimary() {
    const Token& tok = peek();
    switch (tok.type) {
        case TokenType::Int: {
            advance();
            auto n = std::make_unique<IntLit>(tok.intValue);
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::Float: {
            advance();
            auto n = std::make_unique<FloatLit>(tok.floatValue);
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::Char: {
            advance();
            auto n = std::make_unique<CharLit>(static_cast<char>(tok.intValue));
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::String: {
            advance();
            return buildStringLiteral(tok.strValue, tok.line, tok.col);
        }
        case TokenType::KwTrue: {
            advance();
            auto n = std::make_unique<BoolLit>(true);
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::KwFalse: {
            advance();
            auto n = std::make_unique<BoolLit>(false);
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::KwNil: {
            advance();
            auto n = std::make_unique<NilLit>();
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::Identifier: {
            advance();
            auto n = std::make_unique<Identifier>(tok.lexeme);
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        // Type keywords used in expression position become identifiers, so that
        // `int(x)`, `float(x)`, etc. parse as calls and dispatch as casts.
        case TokenType::KwInt:
        case TokenType::KwFloat:
        case TokenType::KwChar:
        case TokenType::KwString:
        case TokenType::KwFile: {
            advance();
            auto n = std::make_unique<Identifier>(typeKeywordName(tok.type));
            n->line = tok.line; n->col = tok.col;
            return n;
        }
        case TokenType::LParen: {
            advance();
            NodePtr expr = parseExpression();
            expect(TokenType::RParen, "Expected ')' after expression");
            return expr;
        }
        case TokenType::LBracket:
            return parseArrayLiteral();
        default:
            error(tok, "Expected an expression");
    }
}

// -------------------- string interpolation --------------------

NodePtr Parser::buildStringLiteral(const std::string& raw, int line, int col) {
    auto lit = std::make_unique<StringLit>();
    lit->line = line; lit->col = col;

    std::string literalChunk;
    size_t i = 0;
    while (i < raw.size()) {
        char c = raw[i];
        if (c == '{') {
            if (i + 1 < raw.size() && raw[i + 1] == '{') {
                literalChunk += '{';
                i += 2;
                continue;
            }
            // Start of an interpolation expression.
            if (!literalChunk.empty()) {
                StringPart part;
                part.isExpr = false;
                part.literal = literalChunk;
                lit->parts.push_back(std::move(part));
                literalChunk.clear();
            }
            // Capture until the matching closing brace (supporting nested braces).
            size_t start = i + 1;
            int depth = 1;
            size_t j = start;
            while (j < raw.size() && depth > 0) {
                if (raw[j] == '{') depth++;
                else if (raw[j] == '}') depth--;
                if (depth == 0) break;
                j++;
            }
            if (depth != 0) {
                throw ParseError("Unterminated '{' in interpolated string", line, col);
            }
            std::string exprSrc = raw.substr(start, j - start);
            i = j + 1; // skip closing brace

            // Lex + parse the embedded expression.
            Lexer sub(exprSrc, "<interpolation>");
            std::vector<Token> subToks = sub.tokenize();
            Parser subParser(std::move(subToks));
            NodePtr exprNode = subParser.parseStandaloneExpression();

            StringPart part;
            part.isExpr = true;
            part.expr = std::move(exprNode);
            lit->parts.push_back(std::move(part));
        } else if (c == '}') {
            if (i + 1 < raw.size() && raw[i + 1] == '}') {
                literalChunk += '}';
                i += 2;
                continue;
            }
            // A lone closing brace is treated literally.
            literalChunk += '}';
            i++;
        } else {
            literalChunk += c;
            i++;
        }
    }
    if (!literalChunk.empty()) {
        StringPart part;
        part.isExpr = false;
        part.literal = literalChunk;
        lit->parts.push_back(std::move(part));
    }
    return lit;
}

} // namespace emerald
