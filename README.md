# Emerald

Author: Jean-Jacques François Reibel

Emerald is a small, friendly programming language in the spirit of Ruby and Python,
with curly-brace blocks, Java-style semicolons, dynamic arrays, classes with
inheritance, built-in file handling, shell execution, and a built-in SQL engine
that stores tables as JSON.

This repository contains the complete reference implementation, written in
portable C++17: lexer, parser, AST, and a tree-walking interpreter, plus a REPL,
a test suite, examples, and full language documentation.

Emerald source files use the `.em` or `.emerald` extension.

```emerald
class Animal(name, sound) {
    string label = name;
    string noise = sound;
    fn speak() { print("{label} says {noise}"); }
}

Animal dog("Rex", "woof");
dog.speak();                      # Rex says woof

for (i = 1; i <= 3; ++i) {
    print("counting {i}");
}

sqlexec("CREATE TABLE pets (name text, legs int)");
sqlexec("INSERT INTO pets VALUES ('Rex', 4)");
printtbl(execsql("SELECT * FROM pets"));
```

## Building

Requirements: a C++17 compiler and CMake 3.10+. No third-party libraries.

### Linux (Debian, Ubuntu, Red Hat, Fedora, Azure images, ...) and macOS

```sh
# Debian/Ubuntu prerequisites:   sudo apt install g++ cmake
# Red Hat/Fedora prerequisites:  sudo dnf install gcc-c++ cmake
# macOS prerequisites:           xcode-select --install && brew install cmake

./scripts/build.sh              # builds into ./build
./build/emerald examples/hello.em
sudo cmake --install build      # optional: installs `emerald` to /usr/local/bin
```

### Windows

With Visual Studio (2017 or newer) installed:

```bat
scripts\build.bat
build\Release\emerald.exe examples\hello.em
```

Or from a "Developer Command Prompt" / with MinGW, the standard CMake flow works:

```bat
cmake -S . -B build
cmake --build build --config Release
```

## Running

```sh
emerald program.em        # run a program (.em or .emerald)
emerald                   # start the interactive REPL
emerald --help
```

## Testing

```sh
cd build && ctest
```

Each test in `tests/` is an Emerald program paired with a `.expected` output file.

## Layout

```
src/        lexer, parser, AST, interpreter, builtins, SQL engine, JSON
tests/      language test programs + expected outputs (run via ctest)
examples/   small showcase programs
docs/       LANGUAGE.md - the full language reference
scripts/    convenience build scripts for Unix and Windows
```

## Documentation

See [docs/LANGUAGE.md](docs/LANGUAGE.md) for the complete language reference:
types, operators, control flow, classes and inheritance, arrays, strings,
file I/O, `shellexec`, the SQL engine, imports, and the built-in function list.
