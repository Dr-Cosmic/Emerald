// main.cpp - Command-line entry point for the Emerald interpreter.
//
//   emerald program.em          run a program (.em or .emerald)
//   emerald                     start an interactive REPL
//   emerald --help / --version
#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "errors.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

namespace {

const char* kVersion = "1.0.0";

void printUsage() {
    std::cout <<
        "Emerald " << kVersion << " - the Emerald programming language\n"
        "\n"
        "Usage:\n"
        "  emerald <script.em|script.emerald>   run an Emerald program\n"
        "  emerald                              start the interactive REPL\n"
        "  emerald --help                       show this help\n"
        "  emerald --version                    show the version\n";
}

bool hasEmeraldExtension(const std::string& path) {
    auto endsWith = [&](const std::string& suffix) {
        return path.size() >= suffix.size() &&
               path.compare(path.size() - suffix.size(), suffix.size(), suffix) == 0;
    };
    return endsWith(".em") || endsWith(".emerald");
}

std::string dirName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    if (slash == std::string::npos) return ".";
    return path.substr(0, slash);
}

void reportError(const emerald::EmeraldError& e) {
    std::cerr << "emerald: " << e.stage;
    if (e.line >= 0) {
        std::cerr << " at line " << e.line;
        if (e.col >= 0) std::cerr << ", col " << e.col;
    }
    std::cerr << ": " << e.what() << std::endl;
}

int runFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good()) {
        std::cerr << "emerald: cannot open '" << path << "'" << std::endl;
        return 1;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    std::string source = ss.str();

    emerald::Interpreter interp;
    interp.setScriptDir(dirName(path));
    try {
        emerald::Lexer lexer(source);
        emerald::Parser parser(lexer.tokenize());
        auto program = parser.parseProgram();
        interp.run(*program);
    } catch (const emerald::EmeraldError& e) {
        reportError(e);
        return 1;
    } catch (const emerald::BreakSignal&) {
        std::cerr << "emerald: 'break' used outside of a loop" << std::endl;
        return 1;
    } catch (const emerald::ContinueSignal&) {
        std::cerr << "emerald: 'continue' used outside of a loop" << std::endl;
        return 1;
    } catch (const emerald::ReturnSignal&) {
        // A top-level `return;` simply ends the program.
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "emerald: internal error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}

int runRepl() {
    std::cout << "Emerald " << kVersion
              << " REPL - type Emerald statements; Ctrl-D (or Ctrl-Z on Windows) to exit."
              << std::endl;

    emerald::Interpreter interp;
    interp.setScriptDir(".");
    std::string buffer;

    auto openBraces = [](const std::string& s) {
        int depth = 0;
        bool inString = false, inChar = false, escaped = false, inComment = false;
        for (char c : s) {
            if (inComment) { if (c == '\n') inComment = false; continue; }
            if (escaped) { escaped = false; continue; }
            if ((inString || inChar) && c == '\\') { escaped = true; continue; }
            if (inString) { if (c == '"') inString = false; continue; }
            if (inChar) { if (c == '\'') inChar = false; continue; }
            if (c == '"') inString = true;
            else if (c == '\'') inChar = true;
            else if (c == '#') inComment = true;
            else if (c == '{') depth++;
            else if (c == '}') depth--;
        }
        return depth;
    };

    while (true) {
        std::cout << (buffer.empty() ? ">>> " : "... ") << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) { std::cout << std::endl; break; }
        buffer += line + "\n";
        if (openBraces(buffer) > 0) continue; // wait for the closing brace

        try {
            std::unique_ptr<emerald::Program> program;
            try {
                emerald::Lexer lexer(buffer);
                emerald::Parser parser(lexer.tokenize());
                program = parser.parseProgram();
            } catch (const emerald::ParseError&) {
                // Convenience: allow bare expressions like `a * 2` without a
                // trailing semicolon. If the retry also fails, rethrow the
                // retry error (same position, clearer to the user).
                emerald::Lexer lexer(buffer + ";");
                emerald::Parser parser(lexer.tokenize());
                program = parser.parseProgram();
            }
            for (auto& stmt : program->statements) {
                emerald::Value v = interp.runStatement(stmt.get());
                if (!v.isNil()) std::cout << v.reprString() << std::endl;
            }
            // Keep the AST alive: functions/classes defined in the REPL hold
            // raw pointers into it.
            static std::vector<std::unique_ptr<emerald::Program>> history;
            history.push_back(std::move(program));
        } catch (const emerald::EmeraldError& e) {
            reportError(e);
        } catch (const emerald::BreakSignal&) {
            std::cerr << "emerald: 'break' used outside of a loop" << std::endl;
        } catch (const emerald::ContinueSignal&) {
            std::cerr << "emerald: 'continue' used outside of a loop" << std::endl;
        } catch (const emerald::ReturnSignal&) {
            std::cerr << "emerald: 'return' used outside of a function" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "emerald: internal error: " << e.what() << std::endl;
        }
        buffer.clear();
    }
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 1) return runRepl();

    std::string arg = argv[1];
    if (arg == "--help" || arg == "-h") { printUsage(); return 0; }
    if (arg == "--version" || arg == "-v") {
        std::cout << "Emerald " << kVersion << std::endl;
        return 0;
    }
    if (!hasEmeraldExtension(arg)) {
        std::cerr << "emerald: '" << arg
                  << "' does not look like an Emerald file (.em / .emerald)"
                  << std::endl;
        return 1;
    }
    return runFile(arg);
}
