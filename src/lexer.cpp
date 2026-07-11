// lexer.cpp - Implementation of the Emerald tokenizer.
#include "lexer.h"
#include "errors.h"
#include <cctype>
#include <cstdlib>

namespace emerald {

Lexer::Lexer(std::string source, std::string filename)
    : src_(std::move(source)), filename_(std::move(filename)) {}

const std::unordered_map<std::string, TokenType>& Lexer::keywords() {
    static const std::unordered_map<std::string, TokenType> kw = {
        {"fn", TokenType::KwFn},
        {"class", TokenType::KwClass},
        {"int", TokenType::KwInt},
        {"float", TokenType::KwFloat},
        {"char", TokenType::KwChar},
        {"string", TokenType::KwString},
        {"File", TokenType::KwFile},
        {"for", TokenType::KwFor},
        {"in", TokenType::KwIn},
        {"while", TokenType::KwWhile},
        {"if", TokenType::KwIf},
        {"elif", TokenType::KwElif},
        {"else", TokenType::KwElse},
        {"return", TokenType::KwReturn},
        {"import", TokenType::KwImport},
        {"from", TokenType::KwFrom},
        {"break", TokenType::KwBreak},
        {"continue", TokenType::KwContinue},
        {"true", TokenType::KwTrue},
        {"false", TokenType::KwFalse},
        {"nil", TokenType::KwNil},
    };
    return kw;
}

bool Lexer::isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}
bool Lexer::isIdentPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}
bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

char Lexer::peek(size_t offset) const {
    size_t p = pos_ + offset;
    if (p >= src_.size()) return '\0';
    return src_[p];
}

char Lexer::advance() {
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; }
    else { col_++; }
    return c;
}

bool Lexer::match(char expected) {
    if (atEnd() || src_[pos_] != expected) return false;
    advance();
    return true;
}

void Lexer::addToken(TokenType type, const std::string& lexeme, int startCol) {
    tokens_.emplace_back(type, lexeme, line_, startCol);
}

void Lexer::error(const std::string& msg, int atLine, int atCol) {
    throw LexError(msg, atLine, atCol);
}

std::vector<Token> Lexer::tokenize() {
    while (!atEnd()) {
        scanToken();
    }
    tokens_.emplace_back(TokenType::Eof, "", line_, col_);
    return tokens_;
}

void Lexer::skipLineComment() {
    while (!atEnd() && peek() != '\n') advance();
}

void Lexer::scanToken() {
    int startCol = col_;
    char c = peek();

    // Whitespace
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { advance(); return; }

    // Comment
    if (c == '#') { advance(); skipLineComment(); return; }

    // Numbers
    if (isDigit(c)) { scanNumber(); return; }

    // Identifiers / keywords
    if (isIdentStart(c)) { scanIdentifier(); return; }

    // Strings
    if (c == '"') { scanString(); return; }

    // Character literals
    if (c == '\'') { scanChar(); return; }

    // Operators & punctuation
    advance(); // consume c
    switch (c) {
        case '(': addToken(TokenType::LParen, "(", startCol); return;
        case ')': addToken(TokenType::RParen, ")", startCol); return;
        case '{': addToken(TokenType::LBrace, "{", startCol); return;
        case '}': addToken(TokenType::RBrace, "}", startCol); return;
        case '[': addToken(TokenType::LBracket, "[", startCol); return;
        case ']': addToken(TokenType::RBracket, "]", startCol); return;
        case ';': addToken(TokenType::Semicolon, ";", startCol); return;
        case ',': addToken(TokenType::Comma, ",", startCol); return;
        case '.': addToken(TokenType::Dot, ".", startCol); return;
        case '%': addToken(TokenType::Percent, "%", startCol); return;
        case '+':
            if (match('+')) addToken(TokenType::Increment, "++", startCol);
            else addToken(TokenType::Plus, "+", startCol);
            return;
        case '-':
            if (match('-')) addToken(TokenType::Decrement, "--", startCol);
            else addToken(TokenType::Minus, "-", startCol);
            return;
        case '*':
            if (match('*')) addToken(TokenType::Power, "**", startCol);
            else addToken(TokenType::Star, "*", startCol);
            return;
        case '/':
            if (match('/')) addToken(TokenType::FloorDiv, "//", startCol);
            else addToken(TokenType::Slash, "/", startCol);
            return;
        case '=':
            if (match('=')) {
                if (match('=')) addToken(TokenType::StrictEq, "===", startCol);
                else addToken(TokenType::Eq, "==", startCol);
            } else {
                addToken(TokenType::Assign, "=", startCol);
            }
            return;
        case '!':
            if (match('=')) {
                if (match('=')) addToken(TokenType::StrictNotEq, "!==", startCol);
                else addToken(TokenType::NotEq, "!=", startCol);
            } else {
                addToken(TokenType::Not, "!", startCol);
            }
            return;
        case '<':
            if (match('=')) addToken(TokenType::Le, "<=", startCol);
            else addToken(TokenType::Lt, "<", startCol);
            return;
        case '>':
            if (match('=')) addToken(TokenType::Ge, ">=", startCol);
            else addToken(TokenType::Gt, ">", startCol);
            return;
        case '&':
            if (match('&')) { addToken(TokenType::And, "&&", startCol); return; }
            error("Unexpected character '&' (did you mean '&&'?)", line_, startCol);
        case '|':
            if (match('|')) { addToken(TokenType::Or, "||", startCol); return; }
            error("Unexpected character '|' (did you mean '||'?)", line_, startCol);
        default: {
            std::string m = "Unexpected character '";
            m += c;
            m += "'";
            error(m, line_, startCol);
        }
    }
}

void Lexer::scanNumber() {
    int startCol = col_;
    int startLine = line_;
    std::string text;
    bool isFloat = false;

    while (isDigit(peek())) text += advance();

    // Fractional part - only if a digit follows the dot (so `5.foo` stays INT + DOT).
    if (peek() == '.' && isDigit(peek(1))) {
        isFloat = true;
        text += advance(); // '.'
        while (isDigit(peek())) text += advance();
    }

    // Exponent part.
    if (peek() == 'e' || peek() == 'E') {
        char next = peek(1);
        if (isDigit(next) || ((next == '+' || next == '-') && isDigit(peek(2)))) {
            isFloat = true;
            text += advance(); // e/E
            if (peek() == '+' || peek() == '-') text += advance();
            while (isDigit(peek())) text += advance();
        }
    }

    Token tok(isFloat ? TokenType::Float : TokenType::Int, text, startLine, startCol);
    if (isFloat) {
        tok.floatValue = std::strtod(text.c_str(), nullptr);
    } else {
        errno = 0;
        tok.intValue = std::strtoll(text.c_str(), nullptr, 10);
    }
    tokens_.push_back(tok);
}

void Lexer::scanIdentifier() {
    int startCol = col_;
    int startLine = line_;
    std::string text;
    while (isIdentPart(peek())) text += advance();

    auto it = keywords().find(text);
    if (it != keywords().end()) {
        tokens_.emplace_back(it->second, text, startLine, startCol);
    } else {
        tokens_.emplace_back(TokenType::Identifier, text, startLine, startCol);
    }
}

void Lexer::scanString() {
    int startCol = col_;
    int startLine = line_;
    advance(); // opening quote
    std::string value;
    while (!atEnd() && peek() != '"') {
        char c = peek();
        if (c == '\\') {
            advance(); // backslash
            if (atEnd()) error("Unterminated string literal", startLine, startCol);
            char esc = advance();
            switch (esc) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '0': value += '\0'; break;
                case 'a': value += '\a'; break;
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                case 'v': value += '\v'; break;
                default:
                    // Keep unknown escapes as the literal character.
                    value += esc;
                    break;
            }
        } else {
            value += advance();
        }
    }
    if (atEnd()) error("Unterminated string literal", startLine, startCol);
    advance(); // closing quote

    Token tok(TokenType::String, value, startLine, startCol);
    tok.strValue = value;
    tokens_.push_back(tok);
}

void Lexer::scanChar() {
    int startCol = col_;
    int startLine = line_;
    advance(); // opening quote
    if (atEnd()) error("Unterminated character literal", startLine, startCol);

    char value;
    char c = peek();
    if (c == '\\') {
        advance();
        if (atEnd()) error("Unterminated character literal", startLine, startCol);
        char esc = advance();
        switch (esc) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case 'r': value = '\r'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            case '"': value = '"'; break;
            case '0': value = '\0'; break;
            case 'a': value = '\a'; break;
            case 'b': value = '\b'; break;
            case 'f': value = '\f'; break;
            case 'v': value = '\v'; break;
            default: value = esc; break;
        }
    } else {
        value = advance();
    }

    if (atEnd() || peek() != '\'') {
        error("Unterminated or multi-character character literal", startLine, startCol);
    }
    advance(); // closing quote

    Token tok(TokenType::Char, std::string(1, value), startLine, startCol);
    tok.intValue = static_cast<unsigned char>(value);
    tok.strValue = std::string(1, value);
    tokens_.push_back(tok);
}

} // namespace emerald
