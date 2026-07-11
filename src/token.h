// token.h - Token types and Token structure for the Emerald language.
#pragma once
#include <string>
#include <cstdint>

namespace emerald {

enum class TokenType {
    // Literals
    Int, Float, Char, String, Identifier,

    // Keywords
    KwFn, KwClass, KwInt, KwFloat, KwChar, KwString, KwFile,
    KwFor, KwIn, KwWhile, KwIf, KwElif, KwElse, KwReturn,
    KwImport, KwFrom, KwBreak, KwContinue,
    KwTrue, KwFalse, KwNil,

    // Punctuation / grouping
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Semicolon, Comma, Dot,

    // Operators
    Plus, Minus, Star, Slash, FloorDiv, Percent, Power,
    Assign,
    Eq, StrictEq, NotEq, StrictNotEq,
    Lt, Gt, Le, Ge,
    And, Or, Not,
    Increment, Decrement,

    // End of file
    Eof
};

struct Token {
    TokenType type;
    std::string lexeme;   // The raw text (for identifiers/errors)
    std::string strValue; // Processed string/char value for literals
    long long intValue = 0;
    double floatValue = 0.0;
    int line = 0;
    int col = 0;

    Token() : type(TokenType::Eof) {}
    Token(TokenType t, std::string lex, int ln, int c)
        : type(t), lexeme(std::move(lex)), line(ln), col(c) {}
};

inline const char* tokenTypeName(TokenType t) {
    switch (t) {
        case TokenType::Int: return "Int";
        case TokenType::Float: return "Float";
        case TokenType::Char: return "Char";
        case TokenType::String: return "String";
        case TokenType::Identifier: return "Identifier";
        case TokenType::KwFn: return "fn";
        case TokenType::KwClass: return "class";
        case TokenType::KwInt: return "int";
        case TokenType::KwFloat: return "float";
        case TokenType::KwChar: return "char";
        case TokenType::KwString: return "string";
        case TokenType::KwFile: return "File";
        case TokenType::KwFor: return "for";
        case TokenType::KwIn: return "in";
        case TokenType::KwWhile: return "while";
        case TokenType::KwIf: return "if";
        case TokenType::KwElif: return "elif";
        case TokenType::KwElse: return "else";
        case TokenType::KwReturn: return "return";
        case TokenType::KwImport: return "import";
        case TokenType::KwFrom: return "from";
        case TokenType::KwBreak: return "break";
        case TokenType::KwContinue: return "continue";
        case TokenType::KwTrue: return "true";
        case TokenType::KwFalse: return "false";
        case TokenType::KwNil: return "nil";
        case TokenType::LParen: return "(";
        case TokenType::RParen: return ")";
        case TokenType::LBrace: return "{";
        case TokenType::RBrace: return "}";
        case TokenType::LBracket: return "[";
        case TokenType::RBracket: return "]";
        case TokenType::Semicolon: return ";";
        case TokenType::Comma: return ",";
        case TokenType::Dot: return ".";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::FloorDiv: return "//";
        case TokenType::Percent: return "%";
        case TokenType::Power: return "**";
        case TokenType::Assign: return "=";
        case TokenType::Eq: return "==";
        case TokenType::StrictEq: return "===";
        case TokenType::NotEq: return "!=";
        case TokenType::StrictNotEq: return "!==";
        case TokenType::Lt: return "<";
        case TokenType::Gt: return ">";
        case TokenType::Le: return "<=";
        case TokenType::Ge: return ">=";
        case TokenType::And: return "&&";
        case TokenType::Or: return "||";
        case TokenType::Not: return "!";
        case TokenType::Increment: return "++";
        case TokenType::Decrement: return "--";
        case TokenType::Eof: return "<eof>";
    }
    return "<unknown>";
}

} // namespace emerald
