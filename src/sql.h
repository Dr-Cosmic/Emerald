// sql.h - A small SQL engine that stores tables as JSON on disk.
//
// Supports a practical subset:
//   CREATE TABLE name (col [type], ...)
//   DROP TABLE [IF EXISTS] name
//   INSERT INTO name [(cols)] VALUES (...) [, (...)]
//   SELECT * | col, ... FROM name [WHERE cond] [ORDER BY col [ASC|DESC]] [LIMIT n]
//   UPDATE name SET col = val, ... [WHERE cond]
//   DELETE FROM name [WHERE cond]
#pragma once
#include "value.h"
#include <string>

namespace emerald {

class SqlEngine {
public:
    explicit SqlEngine(std::string dataDir = "emerald_data");

    void setDataDir(const std::string& dir);
    const std::string& dataDir() const { return dataDir_; }

    // Execute one SQL statement. Returns:
    //   SELECT              -> a table value
    //   INSERT/UPDATE/DELETE -> int (rows affected)
    //   CREATE/DROP         -> nil
    // Throws RuntimeError on any SQL error.
    Value exec(const std::string& sql);

private:
    std::string dataDir_;

    void ensureDataDir() const;
    std::string tablePath(const std::string& name) const;
    bool tableExists(const std::string& name) const;
    bool loadTable(const std::string& name, TableData& out) const;
    void saveTable(const TableData& t) const;
    void dropTableFile(const std::string& name) const;
};

} // namespace emerald
