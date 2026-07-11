// lexer.h - Converts Emerald source text into a stream of tokens.
#pragma once
#include "token.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace emerald {

class Lexer {
public:
    explicit Lexer(std::string source, std::string filename = "<input>");

    // Tokenize the entire source. Throws LexError on invalid input.
    std::vector<Token> tokenize();

private:
    std::string src_;
    std::string filename_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    std::vector<Token> tokens_;

    static const std::unordered_map<std::string, TokenType>& keywords();

    bool atEnd() const { return pos_ >= src_.size(); }
    char peek(size_t offset = 0) const;
    char advance();
    bool match(char expected);

    void addToken(TokenType type, const std::string& lexeme, int startCol);

    void scanToken();
    void scanNumber();
    void scanString();
    void scanChar();
    void scanIdentifier();
    void skipLineComment();

    [[noreturn]] void error(const std::string& msg, int atLine, int atCol);

    static bool isIdentStart(char c);
    static bool isIdentPart(char c);
    static bool isDigit(char c);
};

} // namespace emerald
