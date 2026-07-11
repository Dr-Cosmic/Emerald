// errors.h - Exception types for the Emerald language implementation.
#pragma once
#include <stdexcept>
#include <string>

namespace emerald {

// Base class for all Emerald-related errors that should be reported to the user.
class EmeraldError : public std::runtime_error {
public:
    int line;
    int col;
    std::string stage; // "lexer", "parser", "runtime", etc.
    EmeraldError(const std::string& msg, const std::string& stage_ = "error",
                 int line_ = -1, int col_ = -1)
        : std::runtime_error(msg), line(line_), col(col_), stage(stage_) {}
};

class LexError : public EmeraldError {
public:
    LexError(const std::string& msg, int line_, int col_)
        : EmeraldError(msg, "lex error", line_, col_) {}
};

class ParseError : public EmeraldError {
public:
    ParseError(const std::string& msg, int line_, int col_)
        : EmeraldError(msg, "parse error", line_, col_) {}
};

class RuntimeError : public EmeraldError {
public:
    RuntimeError(const std::string& msg, int line_ = -1, int col_ = -1)
        : EmeraldError(msg, "runtime error", line_, col_) {}
};

} // namespace emerald
