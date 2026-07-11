// builtins.cpp - Built-in functions and built-in methods for Emerald.
#include "interpreter.h"
#include "errors.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

#ifdef _WIN32
#define EMERALD_POPEN _popen
#define EMERALD_PCLOSE _pclose
#else
#define EMERALD_POPEN popen
#define EMERALD_PCLOSE pclose
#endif

namespace emerald {

//======================= file helpers =======================

std::string fileReadAll(const std::string& path, int line, int col) {
    std::ifstream in(path, std::ios::binary);
    if (!in.good())
        throw RuntimeError("Cannot open file '" + path + "'", line, col);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void fileWriteAll(const std::string& path, const std::string& content,
                  int line, int col) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.good())
        throw RuntimeError("Cannot write file '" + path + "'", line, col);
    out << content;
}

//======================= table rendering =======================

std::string Interpreter::renderTable(const TableData& t) {
    if (t.columns.empty()) return "(empty table)";

    auto cellText = [](const Value& v) {
        std::string s = v.type == ValueType::String ? *v.str : v.toString();
        for (char& ch : s) if (ch == '\n' || ch == '\r') ch = ' ';
        return s;
    };

    // Every cell gets the same width: the widest value or header wins.
    size_t width = 0;
    for (auto& c : t.columns) width = std::max(width, c.size());
    for (auto& row : t.rows)
        for (auto& v : row) width = std::max(width, cellText(v).size());

    size_t ncols = t.columns.size();
    size_t inner = width + 2;                      // " value "
    size_t total = ncols * (inner + 1) + 1;        // pipes included

    auto pad = [&](const std::string& s) {
        std::string out = " " + s;
        out.resize(inner, ' ');
        return out;
    };

    std::string sep = "|";
    for (size_t i = 0; i < ncols; i++) sep += std::string(inner, '_') + "|";

    std::ostringstream out;
    out << " " << std::string(total - 2, '_') << "\n";

    out << "|";
    for (auto& c : t.columns) out << pad(c) << "|";
    out << "\n" << sep << "\n";

    for (auto& row : t.rows) {
        out << "|";
        for (size_t i = 0; i < ncols; i++) {
            std::string cell = i < row.size() ? cellText(row[i]) : "";
            out << pad(cell) << "|";
        }
        out << "\n" << sep << "\n";
    }

    std::string s = out.str();
    if (!s.empty() && s.back() == '\n') s.pop_back();
    return s;
}

//======================= built-in methods =======================
// Methods available on File, string, and array receivers.

static void checkArgCount(const std::string& name, const std::vector<Value>& args,
                          size_t min, size_t max) {
    if (args.size() < min || args.size() > max) {
        throw RuntimeError("Wrong number of arguments to " + name + "() (got " +
                           std::to_string(args.size()) + ")");
    }
}

bool Interpreter::callBuiltinMethod(const Value& receiver, const std::string& name,
                                    std::vector<Value>& args, bool trailingComma,
                                    int line, int col, Value& out) {
    //---------------- File methods ----------------
    if (receiver.type == ValueType::File) {
        const std::string& path = receiver.file->path;

        if (name == "read") {
            std::string content = fileReadAll(path, line, col);
            std::string result;
            if (args.empty()) {
                // read() -> the whole file
                result = content;
            } else if (args.size() == 1 && trailingComma) {
                // read(N,) -> everything AFTER the first N+1 characters
                long long n = args[0].asInt();
                size_t start = n < 0 ? 0 : static_cast<size_t>(n) + 1;
                result = start >= content.size() ? "" : content.substr(start);
            } else if (args.size() == 1) {
                // read(N) -> the first N+1 characters
                long long n = args[0].asInt();
                if (n < 0) n = -1;
                result = content.substr(0, static_cast<size_t>(n + 1));
            } else if (args.size() == 2) {
                // read(a, b) -> characters at indices a..b inclusive
                long long a = args[0].asInt();
                long long b = args[1].asInt();
                if (a < 0) a = 0;
                if (b < a || static_cast<size_t>(a) >= content.size()) {
                    result = "";
                } else {
                    size_t end = std::min(static_cast<size_t>(b),
                                          content.size() - 1);
                    result = content.substr(static_cast<size_t>(a),
                                            end - static_cast<size_t>(a) + 1);
                }
            } else {
                throw RuntimeError("read() takes at most 2 arguments", line, col);
            }
            out = Value::makeString(std::move(result));
            return true;
        }

        if (name == "write") {
            if (args.size() == 1) {
                // write(s) -> write / overwrite the whole file
                out = Value::makeNil();
                fileWriteAll(path, args[0].toString(), line, col);
                return true;
            }
            if (args.size() == 2) {
                // write(pos, s) -> overwrite starting at character index pos
                long long pos = args[0].asInt();
                if (pos < 0) pos = 0;
                std::string text = args[1].toString();
                std::string content;
                {
                    std::ifstream in(path, std::ios::binary);
                    if (in.good()) {
                        std::stringstream ss;
                        ss << in.rdbuf();
                        content = ss.str();
                    }
                }
                if (content.size() < static_cast<size_t>(pos))
                    content.resize(static_cast<size_t>(pos), ' ');
                if (content.size() < static_cast<size_t>(pos) + text.size())
                    content.resize(static_cast<size_t>(pos) + text.size(), ' ');
                content.replace(static_cast<size_t>(pos), text.size(), text);
                fileWriteAll(path, content, line, col);
                out = Value::makeNil();
                return true;
            }
            throw RuntimeError("write() takes 1 or 2 arguments", line, col);
        }

        if (name == "append") {
            checkArgCount("append", args, 1, 1);
            std::ofstream f(path, std::ios::binary | std::ios::app);
            if (!f.good())
                throw RuntimeError("Cannot open file '" + path + "' for append",
                                   line, col);
            f << args[0].toString();
            out = Value::makeNil();
            return true;
        }

        return false;
    }

    //---------------- string methods ----------------
    if (receiver.type == ValueType::String) {
        const std::string& s = *receiver.str;

        if (name == "get") {
            checkArgCount("get", args, 1, 2);
            long long a = args[0].asInt();
            if (a < 0 || static_cast<size_t>(a) >= s.size())
                throw RuntimeError("String index " + std::to_string(a) +
                                   " out of range (length " +
                                   std::to_string(s.size()) + ")", line, col);
            if (args.size() == 1) {
                out = Value::makeChar(s[static_cast<size_t>(a)]);
                return true;
            }
            long long b = args[1].asInt();
            if (b < a) { out = Value::makeString(""); return true; }
            size_t end = std::min(static_cast<size_t>(b), s.size() - 1);
            out = Value::makeString(s.substr(static_cast<size_t>(a),
                                             end - static_cast<size_t>(a) + 1));
            return true;
        }
        return false;
    }

    //---------------- array methods ----------------
    if (receiver.type == ValueType::Array) {
        auto& a = *receiver.arr;

        if (name == "get") {
            checkArgCount("get", args, 1, 2);
            long long i = args[0].asInt();
            if (i < 0 || static_cast<size_t>(i) >= a.size())
                throw RuntimeError("Array index " + std::to_string(i) +
                                   " out of range (size " +
                                   std::to_string(a.size()) + ")", line, col);
            if (args.size() == 1) {
                out = a[static_cast<size_t>(i)].clone();
                return true;
            }
            long long j = args[1].asInt();
            std::vector<Value> slice;
            if (j >= i) {
                size_t end = std::min(static_cast<size_t>(j), a.size() - 1);
                for (size_t k = static_cast<size_t>(i); k <= end; k++)
                    slice.push_back(a[k].clone());
            }
            out = Value::makeArray(std::move(slice));
            return true;
        }
        return false;
    }

    return false;
}

//======================= built-in functions =======================

static double argNum(const std::vector<Value>& args, size_t i,
                     const std::string& fname) {
    if (i >= args.size())
        throw RuntimeError(fname + "() is missing argument " + std::to_string(i + 1));
    return args[i].asDouble();
}

static Value lenOf(const Value& v) {
    switch (v.type) {
        case ValueType::Int:
        case ValueType::Float:
        case ValueType::Char:
        case ValueType::Bool:
            return Value::makeInt(1);
        case ValueType::String:
            return Value::makeInt(static_cast<long long>(v.str->size()));
        case ValueType::Array:
            return Value::makeInt(static_cast<long long>(v.arr->size()));
        case ValueType::Object:
            return Value::makeInt(static_cast<long long>(v.obj->fields.size()));
        case ValueType::Table:
            return Value::makeInt(static_cast<long long>(v.table->rows.size()));
        case ValueType::File: {
            std::ifstream in(v.file->path, std::ios::binary | std::ios::ate);
            if (!in.good())
                throw RuntimeError("Cannot open file '" + v.file->path + "'");
            return Value::makeInt(static_cast<long long>(in.tellg()));
        }
        case ValueType::Nil:
            return Value::makeInt(0);
        default:
            return Value::makeInt(1);
    }
}

static std::string valueToPrintable(const Value& v) {
    if (v.type == ValueType::Table) return Interpreter::renderTable(*v.table);
    return v.toString();
}

void installEmeraldBuiltins(Interpreter& interp, Environment& g) {
    (void)interp;
    auto def = [&](const std::string& name, BuiltinFn fn) {
        g.define(name, Value::makeBuiltin(name, std::move(fn)));
    };

    //---------------- printing ----------------
    def("print", [](Interpreter& in, std::vector<Value>& args) {
        std::string s;
        for (auto& a : args) s += valueToPrintable(a);
        (*in.out) << s << "\n";
        return Value::makeNil();
    });

    def("printdlm", [](Interpreter& in, std::vector<Value>& args) {
        if (args.empty())
            throw RuntimeError("printdlm() needs a delimiter argument");
        std::string delim = args[0].toString();
        std::string s;
        for (size_t i = 1; i < args.size(); i++) {
            if (i > 1) s += delim;
            s += valueToPrintable(args[i]);
        }
        (*in.out) << s << "\n";
        return Value::makeNil();
    });

    def("printtbl", [](Interpreter& in, std::vector<Value>& args) {
        if (args.size() != 1 || args[0].type != ValueType::Table)
            throw RuntimeError("printtbl() takes exactly one table argument");
        (*in.out) << Interpreter::renderTable(*args[0].table) << "\n";
        return Value::makeNil();
    });

    //---------------- general ----------------
    def("len", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("len() takes exactly one argument");
        return lenOf(args[0]);
    });
    def("length", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("length() takes exactly one argument");
        return lenOf(args[0]);
    });

    def("type", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("type() takes exactly one argument");
        return Value::makeString(args[0].typeName());
    });

    def("getinput", [](Interpreter&, std::vector<Value>& args) {
        if (!args.empty()) {
            // Optional prompt (extension).
            std::cout << args[0].toString() << std::flush;
        }
        std::string line;
        if (!std::getline(std::cin, line)) return Value::makeString("");
        if (!line.empty() && line.back() == '\r') line.pop_back();
        return Value::makeString(std::move(line));
    });

    //---------------- shell ----------------
    def("shellexec", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1 || args[0].type != ValueType::String)
            throw RuntimeError("shellexec() takes exactly one string argument");
        FILE* pipe = EMERALD_POPEN(args[0].str->c_str(), "r");
        if (!pipe)
            throw RuntimeError("shellexec: failed to run command");
        std::string output;
        char buf[4096];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0)
            output.append(buf, n);
        EMERALD_PCLOSE(pipe);
        if (!output.empty() && output.back() == '\n') output.pop_back();
        return Value::makeString(std::move(output));
    });

    //---------------- SQL ----------------
    auto sqlFn = [](Interpreter& in, std::vector<Value>& args) {
        if (args.size() != 1 || args[0].type != ValueType::String)
            throw RuntimeError("sqlexec() takes exactly one string argument");
        return in.sql().exec(*args[0].str);
    };
    def("sqlexec", sqlFn);
    def("execsql", sqlFn);

    def("sqldatadir", [](Interpreter& in, std::vector<Value>& args) {
        if (args.size() != 1 || args[0].type != ValueType::String)
            throw RuntimeError("sqldatadir() takes exactly one string argument");
        in.sql().setDataDir(*args[0].str);
        return Value::makeNil();
    });

    //---------------- math ----------------
    def("abs", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("abs() takes exactly one argument");
        const Value& v = args[0];
        if (v.type == ValueType::Int) return Value::makeInt(v.i < 0 ? -v.i : v.i);
        return Value::makeFloat(std::fabs(v.asDouble()));
    });

    def("floor", [](Interpreter&, std::vector<Value>& args) {
        return Value::makeInt(static_cast<long long>(
            std::floor(argNum(args, 0, "floor"))));
    });

    // top() rounds up (ceiling).
    def("top", [](Interpreter&, std::vector<Value>& args) {
        return Value::makeInt(static_cast<long long>(
            std::ceil(argNum(args, 0, "top"))));
    });

    def("log", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() == 1) {
            // log(num) is base-10.
            return Value::makeFloat(std::log10(argNum(args, 0, "log")));
        }
        if (args.size() == 2) {
            // log(base, num)
            double base = argNum(args, 0, "log");
            double num = argNum(args, 1, "log");
            return Value::makeFloat(std::log(num) / std::log(base));
        }
        throw RuntimeError("log() takes 1 or 2 arguments");
    });

    def("ln", [](Interpreter&, std::vector<Value>& args) {
        return Value::makeFloat(std::log(argNum(args, 0, "ln")));
    });

    def("sqrt", [](Interpreter&, std::vector<Value>& args) {
        return Value::makeFloat(std::sqrt(argNum(args, 0, "sqrt")));
    });
    def("exp", [](Interpreter&, std::vector<Value>& args) {
        return Value::makeFloat(std::exp(argNum(args, 0, "exp")));
    });

    auto unaryMath = [&](const std::string& name, double (*fn)(double)) {
        def(name, [name, fn](Interpreter&, std::vector<Value>& args) {
            if (args.size() != 1)
                throw RuntimeError(name + "() takes exactly one argument");
            return Value::makeFloat(fn(args[0].asDouble()));
        });
    };
    unaryMath("sin", std::sin);   unaryMath("cos", std::cos);   unaryMath("tan", std::tan);
    unaryMath("asin", std::asin); unaryMath("acos", std::acos); unaryMath("atan", std::atan);
    unaryMath("sinh", std::sinh); unaryMath("cosh", std::cosh); unaryMath("tanh", std::tanh);
    unaryMath("asinh", std::asinh);
    unaryMath("acosh", std::acosh);
    unaryMath("atanh", std::atanh);

    g.define("pi", Value::makeFloat(3.14159265358979323846));
    g.define("e", Value::makeFloat(2.71828182845904523536));

    //---------------- type casts ----------------
    def("int", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("int() takes exactly one argument");
        const Value& v = args[0];
        switch (v.type) {
            case ValueType::Int:    return v;
            case ValueType::Float:  return Value::makeInt(static_cast<long long>(v.f));
            case ValueType::Char:   return Value::makeInt(
                static_cast<long long>(static_cast<unsigned char>(v.c)));
            case ValueType::Bool:   return Value::makeInt(v.b ? 1 : 0);
            case ValueType::Nil:    return Value::makeInt(0);
            case ValueType::String: {
                try { return Value::makeInt(std::stoll(*v.str)); }
                catch (...) {
                    throw RuntimeError("int(): cannot convert \"" + *v.str + "\"");
                }
            }
            default:
                throw RuntimeError("int(): cannot convert a value of type '" +
                                   v.typeName() + "'");
        }
    });

    def("float", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("float() takes exactly one argument");
        const Value& v = args[0];
        switch (v.type) {
            case ValueType::Float:  return v;
            case ValueType::Int:    return Value::makeFloat(static_cast<double>(v.i));
            case ValueType::Char:   return Value::makeFloat(
                static_cast<double>(static_cast<unsigned char>(v.c)));
            case ValueType::Bool:   return Value::makeFloat(v.b ? 1.0 : 0.0);
            case ValueType::Nil:    return Value::makeFloat(0.0);
            case ValueType::String: {
                try { return Value::makeFloat(std::stod(*v.str)); }
                catch (...) {
                    throw RuntimeError("float(): cannot convert \"" + *v.str + "\"");
                }
            }
            default:
                throw RuntimeError("float(): cannot convert a value of type '" +
                                   v.typeName() + "'");
        }
    });

    def("char", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("char() takes exactly one argument");
        const Value& v = args[0];
        switch (v.type) {
            case ValueType::Char: return v;
            case ValueType::Int:  return Value::makeChar(static_cast<char>(v.i));
            case ValueType::Float:
                return Value::makeChar(static_cast<char>(static_cast<long long>(v.f)));
            case ValueType::String:
                if (v.str->size() == 1) return Value::makeChar((*v.str)[0]);
                throw RuntimeError("char(): string must have exactly one character");
            default:
                throw RuntimeError("char(): cannot convert a value of type '" +
                                   v.typeName() + "'");
        }
    });

    def("string", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1) throw RuntimeError("string() takes exactly one argument");
        return Value::makeString(args[0].toString());
    });

    // File("path") builds a file handle (touching the file if it is missing).
    def("File", [](Interpreter&, std::vector<Value>& args) {
        if (args.size() != 1 || args[0].type != ValueType::String)
            throw RuntimeError("File() takes exactly one string argument");
        auto fd = std::make_shared<FileData>();
        fd->path = *args[0].str;
        std::ifstream probe(fd->path, std::ios::binary);
        if (!probe.good()) {
            std::ofstream touch(fd->path, std::ios::binary);
            if (!touch.good())
                throw RuntimeError("Cannot create file '" + fd->path + "'");
        }
        return Value::makeFile(fd);
    });
}

} // namespace emerald
