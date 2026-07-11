// sql.cpp - Implementation of the JSON-backed SQL engine.
#include "sql.h"
#include "json.h"
#include "errors.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <functional>
#include <memory>

namespace fs = std::filesystem;

namespace emerald {

// ============================================================
// Value <-> JSON conversion for persistence
// ============================================================

static json::Value valueToJson(const Value& v) {
    switch (v.type) {
        case ValueType::Nil: return json::Value::makeNull();
        case ValueType::Int: return json::Value::makeInt(v.i);
        case ValueType::Float: return json::Value::makeDouble(v.f);
        case ValueType::Bool: return json::Value::makeBool(v.b);
        case ValueType::Char: return json::Value::makeString(std::string(1, v.c));
        case ValueType::String: return json::Value::makeString(v.str ? *v.str : std::string());
        default: return json::Value::makeString(v.toString());
    }
}

static Value jsonToValue(const json::Value& j) {
    switch (j.type) {
        case json::Type::Null: return Value::makeNil();
        case json::Type::Bool: return Value::makeBool(j.b);
        case json::Type::Int: return Value::makeInt(j.i);
        case json::Type::Double: return Value::makeFloat(j.d);
        case json::Type::String: return Value::makeString(j.s);
        default: return Value::makeNil();
    }
}

// ============================================================
// SqlEngine storage
// ============================================================

SqlEngine::SqlEngine(std::string dataDir) : dataDir_(std::move(dataDir)) {}

void SqlEngine::setDataDir(const std::string& dir) { dataDir_ = dir; }

void SqlEngine::ensureDataDir() const {
    std::error_code ec;
    fs::create_directories(dataDir_, ec);
}

std::string SqlEngine::tablePath(const std::string& name) const {
    return (fs::path(dataDir_) / (name + ".json")).string();
}

bool SqlEngine::tableExists(const std::string& name) const {
    std::error_code ec;
    return fs::exists(tablePath(name), ec);
}

bool SqlEngine::loadTable(const std::string& name, TableData& out) const {
    std::ifstream in(tablePath(name), std::ios::binary);
    if (!in) return false;
    std::stringstream ss;
    ss << in.rdbuf();
    std::string text = ss.str();

    json::Value root;
    std::string err;
    if (!json::parse(text, root, err)) {
        throw RuntimeError("Corrupt table file for '" + name + "': " + err);
    }
    out.name = name;
    out.columns.clear();
    out.rows.clear();

    if (const json::Value* cols = root.find("columns")) {
        for (const auto& c : cols->arr) out.columns.push_back(c.s);
    }
    if (const json::Value* rows = root.find("rows")) {
        for (const auto& r : rows->arr) {
            std::vector<Value> row;
            for (const auto& cell : r.arr) row.push_back(jsonToValue(cell));
            out.rows.push_back(std::move(row));
        }
    }
    return true;
}

void SqlEngine::saveTable(const TableData& t) const {
    json::Value root = json::Value::makeObject();
    root.set("name", json::Value::makeString(t.name));
    json::Value cols = json::Value::makeArray();
    for (const auto& c : t.columns) cols.arr.push_back(json::Value::makeString(c));
    root.set("columns", cols);
    json::Value rows = json::Value::makeArray();
    for (const auto& r : t.rows) {
        json::Value jr = json::Value::makeArray();
        for (const auto& cell : r) jr.arr.push_back(valueToJson(cell));
        rows.arr.push_back(jr);
    }
    root.set("rows", rows);

    ensureDataDir();
    std::ofstream out(tablePath(t.name), std::ios::binary | std::ios::trunc);
    if (!out) throw RuntimeError("Cannot write table file for '" + t.name + "'");
    out << json::stringify(root, true);
}

void SqlEngine::dropTableFile(const std::string& name) const {
    std::error_code ec;
    fs::remove(tablePath(name), ec);
}

// ============================================================
// SQL tokenizer
// ============================================================

namespace {

enum class SqlTokType { Ident, Keyword, Number, String, Op, Punct, End };

struct SqlToken {
    SqlTokType type;
    std::string text;   // raw / normalized (keywords upper-cased)
    bool isFloat = false;
    long long ival = 0;
    double dval = 0.0;
};

bool isKeyword(const std::string& up) {
    static const char* kws[] = {
        "CREATE","TABLE","DROP","IF","EXISTS","INSERT","INTO","VALUES",
        "SELECT","FROM","WHERE","UPDATE","SET","DELETE","ORDER","BY",
        "ASC","DESC","LIMIT","AND","OR","NOT","NULL","TRUE","FALSE",
        "INT","INTEGER","FLOAT","REAL","TEXT","STRING","CHAR","BOOL","BOOLEAN"
    };
    for (const char* k : kws) if (up == k) return true;
    return false;
}

class SqlLexer {
public:
    explicit SqlLexer(const std::string& s) : src_(s) {}

    std::vector<SqlToken> tokenize() {
        std::vector<SqlToken> toks;
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (std::isspace(static_cast<unsigned char>(c))) { pos_++; continue; }
            if (c == '-' && pos_ + 1 < src_.size() && src_[pos_ + 1] == '-') {
                while (pos_ < src_.size() && src_[pos_] != '\n') pos_++;
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                toks.push_back(readIdent());
            } else if (std::isdigit(static_cast<unsigned char>(c)) ||
                       (c == '.' && pos_ + 1 < src_.size() &&
                        std::isdigit(static_cast<unsigned char>(src_[pos_ + 1])))) {
                toks.push_back(readNumber());
            } else if (c == '\'') {
                toks.push_back(readString());
            } else {
                toks.push_back(readOp());
            }
        }
        toks.push_back(SqlToken{SqlTokType::End, "", false, 0, 0.0});
        return toks;
    }

private:
    const std::string& src_;
    size_t pos_ = 0;

    SqlToken readIdent() {
        std::string t;
        while (pos_ < src_.size() &&
               (std::isalnum(static_cast<unsigned char>(src_[pos_])) || src_[pos_] == '_')) {
            t += src_[pos_++];
        }
        std::string up = t;
        std::transform(up.begin(), up.end(), up.begin(), ::toupper);
        if (isKeyword(up)) return SqlToken{SqlTokType::Keyword, up, false, 0, 0.0};
        return SqlToken{SqlTokType::Ident, t, false, 0, 0.0};
    }

    SqlToken readNumber() {
        std::string t;
        bool isFloat = false;
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) t += src_[pos_++];
        if (pos_ < src_.size() && src_[pos_] == '.') {
            isFloat = true; t += src_[pos_++];
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) t += src_[pos_++];
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            isFloat = true; t += src_[pos_++];
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) t += src_[pos_++];
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) t += src_[pos_++];
        }
        SqlToken tok{SqlTokType::Number, t, isFloat, 0, 0.0};
        if (isFloat) tok.dval = std::strtod(t.c_str(), nullptr);
        else tok.ival = std::strtoll(t.c_str(), nullptr, 10);
        return tok;
    }

    SqlToken readString() {
        pos_++; // opening quote
        std::string t;
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '\'') {
                if (pos_ < src_.size() && src_[pos_] == '\'') { t += '\''; pos_++; continue; }
                break;
            }
            t += c;
        }
        return SqlToken{SqlTokType::String, t, false, 0, 0.0};
    }

    SqlToken readOp() {
        char c = src_[pos_];
        auto two = [&](const char* s) {
            return pos_ + 1 < src_.size() && src_[pos_] == s[0] && src_[pos_ + 1] == s[1];
        };
        if (two("<=")) { pos_ += 2; return SqlToken{SqlTokType::Op, "<=", false, 0, 0.0}; }
        if (two(">=")) { pos_ += 2; return SqlToken{SqlTokType::Op, ">=", false, 0, 0.0}; }
        if (two("<>")) { pos_ += 2; return SqlToken{SqlTokType::Op, "!=", false, 0, 0.0}; }
        if (two("!=")) { pos_ += 2; return SqlToken{SqlTokType::Op, "!=", false, 0, 0.0}; }
        pos_++;
        switch (c) {
            case '=': return SqlToken{SqlTokType::Op, "=", false, 0, 0.0};
            case '<': return SqlToken{SqlTokType::Op, "<", false, 0, 0.0};
            case '>': return SqlToken{SqlTokType::Op, ">", false, 0, 0.0};
            case ',': return SqlToken{SqlTokType::Punct, ",", false, 0, 0.0};
            case '(': return SqlToken{SqlTokType::Punct, "(", false, 0, 0.0};
            case ')': return SqlToken{SqlTokType::Punct, ")", false, 0, 0.0};
            case '*': return SqlToken{SqlTokType::Punct, "*", false, 0, 0.0};
            case ';': return SqlToken{SqlTokType::Punct, ";", false, 0, 0.0};
            case '.': return SqlToken{SqlTokType::Punct, ".", false, 0, 0.0};
        }
        throw RuntimeError(std::string("Unexpected character in SQL: '") + c + "'");
    }
};

// ============================================================
// WHERE predicate + comparison helpers
// ============================================================

bool valuesEqual(const Value& a, const Value& b) {
    if (a.isNil() || b.isNil()) return a.isNil() && b.isNil();
    if (a.isNumeric() && b.isNumeric()) return a.asDouble() == b.asDouble();
    if (a.type == ValueType::Bool && b.type == ValueType::Bool) return a.b == b.b;
    return a.toString() == b.toString();
}

int valuesCompare(const Value& a, const Value& b) {
    if (a.isNumeric() && b.isNumeric()) {
        double x = a.asDouble(), y = b.asDouble();
        return x < y ? -1 : (x > y ? 1 : 0);
    }
    std::string x = a.toString(), y = b.toString();
    return x < y ? -1 : (x > y ? 1 : 0);
}

} // anonymous namespace

// ============================================================
// SQL parser + executor
// ============================================================

namespace {

class SqlParser {
public:
    SqlParser(std::vector<SqlToken> toks, SqlEngine& engine,
              std::function<bool(const std::string&, TableData&)> loader,
              std::function<void(const TableData&)> saver,
              std::function<bool(const std::string&)> exists,
              std::function<void(const std::string&)> dropper)
        : toks_(std::move(toks)), engine_(engine),
          load_(std::move(loader)), save_(std::move(saver)),
          exists_(std::move(exists)), drop_(std::move(dropper)) {}

    Value run() {
        Value result = statement();
        // Allow a trailing semicolon.
        if (cur().type == SqlTokType::Punct && cur().text == ";") advance();
        if (cur().type != SqlTokType::End) fail("Unexpected tokens after SQL statement");
        return result;
    }

private:
    std::vector<SqlToken> toks_;
    size_t pos_ = 0;
    SqlEngine& engine_;
    std::function<bool(const std::string&, TableData&)> load_;
    std::function<void(const TableData&)> save_;
    std::function<bool(const std::string&)> exists_;
    std::function<void(const std::string&)> drop_;

    const SqlToken& cur() const { return toks_[pos_]; }
    const SqlToken& advance() { return toks_[pos_ < toks_.size() - 1 ? pos_++ : pos_]; }
    bool isKw(const char* k) const { return cur().type == SqlTokType::Keyword && cur().text == k; }
    bool isPunct(const char* p) const { return cur().type == SqlTokType::Punct && cur().text == p; }

    [[noreturn]] void fail(const std::string& m) { throw RuntimeError("SQL error: " + m); }

    std::string expectIdent(const std::string& what) {
        if (cur().type == SqlTokType::Ident) return advance().text;
        // Some non-reserved words may arrive as keywords; accept them as names.
        if (cur().type == SqlTokType::Keyword) return advance().text;
        fail("Expected " + what);
    }
    void expectKw(const char* k) {
        if (!isKw(k)) fail(std::string("Expected '") + k + "'");
        advance();
    }
    void expectPunct(const char* p) {
        if (!isPunct(p)) fail(std::string("Expected '") + p + "'");
        advance();
    }

    TableData loadOrFail(const std::string& name) {
        TableData t;
        if (!load_(name, t)) fail("No such table: " + name);
        return t;
    }

    int columnIndex(const TableData& t, const std::string& col) {
        for (size_t k = 0; k < t.columns.size(); ++k) if (t.columns[k] == col) return static_cast<int>(k);
        return -1;
    }

    // ---- literal / operand ----
    Value literal() {
        const SqlToken& t = cur();
        if (t.type == SqlTokType::Number) {
            advance();
            return t.isFloat ? Value::makeFloat(t.dval) : Value::makeInt(t.ival);
        }
        if (t.type == SqlTokType::String) { advance(); return Value::makeString(t.text); }
        if (isKw("TRUE")) { advance(); return Value::makeBool(true); }
        if (isKw("FALSE")) { advance(); return Value::makeBool(false); }
        if (isKw("NULL")) { advance(); return Value::makeNil(); }
        // Negative numbers
        if (t.type == SqlTokType::Op && t.text == "<") {} // no-op guard
        fail("Expected a literal value");
    }

    Value signedLiteral() {
        bool neg = false;
        if (cur().type == SqlTokType::Op && cur().text == "-") { neg = true; advance(); }
        else if (cur().type == SqlTokType::Punct && cur().text == "-") { neg = true; advance(); }
        Value v = literal();
        if (neg) {
            if (v.type == ValueType::Int) v.i = -v.i;
            else if (v.type == ValueType::Float) v.f = -v.f;
            else fail("Cannot negate non-numeric literal");
        }
        return v;
    }

    // ---- WHERE predicate ----
    using Predicate = std::function<bool(const std::vector<Value>&)>;

    Predicate parseWhere(const TableData& t) {
        return parseOr(t);
    }

    Predicate parseOr(const TableData& t) {
        Predicate left = parseAnd(t);
        while (isKw("OR")) {
            advance();
            Predicate right = parseAnd(t);
            Predicate l = left; Predicate r = right;
            left = [l, r](const std::vector<Value>& row) { return l(row) || r(row); };
        }
        return left;
    }

    Predicate parseAnd(const TableData& t) {
        Predicate left = parseNot(t);
        while (isKw("AND")) {
            advance();
            Predicate right = parseNot(t);
            Predicate l = left; Predicate r = right;
            left = [l, r](const std::vector<Value>& row) { return l(row) && r(row); };
        }
        return left;
    }

    Predicate parseNot(const TableData& t) {
        if (isKw("NOT")) {
            advance();
            Predicate inner = parseNot(t);
            return [inner](const std::vector<Value>& row) { return !inner(row); };
        }
        return parseComparison(t);
    }

    // An operand in a comparison: a column reference or a literal.
    // Returns a getter that extracts the operand's value from a row.
    std::function<Value(const std::vector<Value>&)> parseOperand(const TableData& t) {
        if (cur().type == SqlTokType::Ident) {
            std::string col = advance().text;
            int idx = columnIndex(t, col);
            if (idx < 0) fail("No such column: " + col);
            return [idx](const std::vector<Value>& row) -> Value {
                return (idx < static_cast<int>(row.size())) ? row[idx] : Value::makeNil();
            };
        }
        Value lit = signedLiteral();
        return [lit](const std::vector<Value>&) -> Value { return lit; };
    }

    Predicate parseComparison(const TableData& t) {
        if (isPunct("(")) {
            advance();
            Predicate inner = parseOr(t);
            expectPunct(")");
            return inner;
        }
        auto lhs = parseOperand(t);
        if (cur().type != SqlTokType::Op) fail("Expected comparison operator in WHERE");
        std::string op = advance().text;
        auto rhs = parseOperand(t);
        return [lhs, rhs, op](const std::vector<Value>& row) -> bool {
            Value a = lhs(row), bv = rhs(row);
            if (op == "=") return valuesEqual(a, bv);
            if (op == "!=") return !valuesEqual(a, bv);
            if (a.isNil() || bv.isNil()) return false;
            int cmp = valuesCompare(a, bv);
            if (op == "<") return cmp < 0;
            if (op == ">") return cmp > 0;
            if (op == "<=") return cmp <= 0;
            if (op == ">=") return cmp >= 0;
            return false;
        };
    }

    // ---- statements ----
    Value statement() {
        if (isKw("CREATE")) return createTable();
        if (isKw("DROP")) return dropTable();
        if (isKw("INSERT")) return insert();
        if (isKw("SELECT")) return select();
        if (isKw("UPDATE")) return update();
        if (isKw("DELETE")) return del();
        fail("Unsupported or empty SQL statement");
    }

    Value createTable() {
        expectKw("CREATE");
        expectKw("TABLE");
        bool ifNotExists = false;
        // Optional (non-standard-order tolerant) IF NOT EXISTS
        if (isKw("IF")) { advance(); if (isKw("NOT")) advance(); if (isKw("EXISTS")) advance(); ifNotExists = true; }
        std::string name = expectIdent("table name");
        if (exists_(name)) {
            if (ifNotExists) return Value::makeNil();
            fail("Table already exists: " + name);
        }
        expectPunct("(");
        TableData t;
        t.name = name;
        do {
            std::string col = expectIdent("column name");
            t.columns.push_back(col);
            // Optional column type - consumed and ignored.
            if (cur().type == SqlTokType::Keyword &&
                (cur().text == "INT" || cur().text == "INTEGER" || cur().text == "FLOAT" ||
                 cur().text == "REAL" || cur().text == "TEXT" || cur().text == "STRING" ||
                 cur().text == "CHAR" || cur().text == "BOOL" || cur().text == "BOOLEAN")) {
                advance();
            }
        } while (isPunct(",") && (advance(), true));
        expectPunct(")");
        save_(t);
        return Value::makeNil();
    }

    Value dropTable() {
        expectKw("DROP");
        expectKw("TABLE");
        bool ifExists = false;
        if (isKw("IF")) { advance(); if (isKw("EXISTS")) advance(); ifExists = true; }
        std::string name = expectIdent("table name");
        if (!exists_(name)) {
            if (ifExists) return Value::makeNil();
            fail("No such table: " + name);
        }
        drop_(name);
        return Value::makeNil();
    }

    Value insert() {
        expectKw("INSERT");
        expectKw("INTO");
        std::string name = expectIdent("table name");
        TableData t = loadOrFail(name);

        std::vector<std::string> cols = t.columns;
        bool explicitCols = false;
        if (isPunct("(")) {
            advance();
            cols.clear();
            explicitCols = true;
            do {
                cols.push_back(expectIdent("column name"));
            } while (isPunct(",") && (advance(), true));
            expectPunct(")");
        }
        expectKw("VALUES");

        int inserted = 0;
        do {
            expectPunct("(");
            std::vector<Value> tuple;
            if (!isPunct(")")) {
                do {
                    tuple.push_back(signedLiteral());
                } while (isPunct(",") && (advance(), true));
            }
            expectPunct(")");

            if (tuple.size() != cols.size()) {
                fail("INSERT column/value count mismatch");
            }
            std::vector<Value> row(t.columns.size(), Value::makeNil());
            for (size_t k = 0; k < cols.size(); ++k) {
                int idx = columnIndex(t, cols[k]);
                if (idx < 0) fail("No such column: " + cols[k]);
                row[idx] = tuple[k];
            }
            (void)explicitCols;
            t.rows.push_back(std::move(row));
            inserted++;
        } while (isPunct(",") && (advance(), true));

        save_(t);
        return Value::makeInt(inserted);
    }

    Value select() {
        expectKw("SELECT");
        bool star = false;
        std::vector<std::string> cols;
        if (isPunct("*")) { advance(); star = true; }
        else {
            do { cols.push_back(expectIdent("column name")); }
            while (isPunct(",") && (advance(), true));
        }
        expectKw("FROM");
        std::string name = expectIdent("table name");
        TableData t = loadOrFail(name);

        Predicate pred = nullptr;
        if (isKw("WHERE")) { advance(); pred = parseWhere(t); }

        // ORDER BY
        std::string orderCol;
        bool desc = false;
        if (isKw("ORDER")) {
            advance();
            expectKw("BY");
            orderCol = expectIdent("column name");
            if (isKw("ASC")) advance();
            else if (isKw("DESC")) { advance(); desc = true; }
        }

        long long limit = -1;
        if (isKw("LIMIT")) {
            advance();
            if (cur().type != SqlTokType::Number || cur().isFloat) fail("LIMIT expects an integer");
            limit = advance().ival;
        }

        // Build the output schema.
        auto result = std::make_shared<TableData>();
        result->name = name;
        std::vector<int> colIdx;
        if (star) {
            result->columns = t.columns;
            for (size_t k = 0; k < t.columns.size(); ++k) colIdx.push_back(static_cast<int>(k));
        } else {
            for (const auto& c : cols) {
                int idx = columnIndex(t, c);
                if (idx < 0) fail("No such column: " + c);
                result->columns.push_back(c);
                colIdx.push_back(idx);
            }
        }

        // Filter rows.
        std::vector<const std::vector<Value>*> selected;
        for (const auto& row : t.rows) {
            if (!pred || pred(row)) selected.push_back(&row);
        }

        // Order.
        if (!orderCol.empty()) {
            int oi = columnIndex(t, orderCol);
            if (oi < 0) fail("No such column: " + orderCol);
            Value nilVal = Value::makeNil();
            std::stable_sort(selected.begin(), selected.end(),
                [oi, desc, &nilVal](const std::vector<Value>* a, const std::vector<Value>* b) {
                    const Value& av = (oi < (int)a->size()) ? (*a)[oi] : nilVal;
                    const Value& bv = (oi < (int)b->size()) ? (*b)[oi] : nilVal;
                    int cmp = valuesCompare(av, bv);
                    return desc ? cmp > 0 : cmp < 0;
                });
        }

        // Project + limit.
        long long count = 0;
        for (const auto* row : selected) {
            if (limit >= 0 && count >= limit) break;
            std::vector<Value> out;
            for (int idx : colIdx) {
                out.push_back(idx < (int)row->size() ? (*row)[idx] : Value::makeNil());
            }
            result->rows.push_back(std::move(out));
            count++;
        }

        return Value::makeTable(result);
    }

    Value update() {
        expectKw("UPDATE");
        std::string name = expectIdent("table name");
        TableData t = loadOrFail(name);
        expectKw("SET");

        std::vector<std::pair<int, Value>> assigns;
        do {
            std::string col = expectIdent("column name");
            int idx = columnIndex(t, col);
            if (idx < 0) fail("No such column: " + col);
            if (cur().type != SqlTokType::Op || cur().text != "=") fail("Expected '=' in SET clause");
            advance();
            Value v = signedLiteral();
            assigns.emplace_back(idx, v);
        } while (isPunct(",") && (advance(), true));

        Predicate pred = nullptr;
        if (isKw("WHERE")) { advance(); pred = parseWhere(t); }

        int affected = 0;
        for (auto& row : t.rows) {
            if (!pred || pred(row)) {
                for (auto& a : assigns) row[a.first] = a.second;
                affected++;
            }
        }
        save_(t);
        return Value::makeInt(affected);
    }

    Value del() {
        expectKw("DELETE");
        expectKw("FROM");
        std::string name = expectIdent("table name");
        TableData t = loadOrFail(name);

        Predicate pred = nullptr;
        if (isKw("WHERE")) { advance(); pred = parseWhere(t); }

        std::vector<std::vector<Value>> kept;
        int deleted = 0;
        for (auto& row : t.rows) {
            if (pred && pred(row)) { deleted++; }
            else kept.push_back(std::move(row));
        }
        t.rows = std::move(kept);
        save_(t);
        return Value::makeInt(deleted);
    }
};

} // anonymous namespace

Value SqlEngine::exec(const std::string& sql) {
    SqlLexer lexer(sql);
    std::vector<SqlToken> toks = lexer.tokenize();

    // Empty statement (only End) -> nil.
    if (toks.size() == 1) return Value::makeNil();

    SqlParser parser(
        std::move(toks), *this,
        [this](const std::string& n, TableData& t) { return loadTable(n, t); },
        [this](const TableData& t) { saveTable(t); },
        [this](const std::string& n) { return tableExists(n); },
        [this](const std::string& n) { dropTableFile(n); });
    return parser.run();
}

} // namespace emerald
