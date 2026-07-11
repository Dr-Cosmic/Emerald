sqldatadir("emerald_test_data");
sqlexec("DROP TABLE IF EXISTS people");
sqlexec("CREATE TABLE people (id int, name text, age int)");
int inserted = sqlexec("INSERT INTO people VALUES (1, 'Ada Lovelace', 36), (2, 'Alan Turing', 41), (3, 'Grace Hopper', 85)");
print("inserted {inserted}");

printtbl(execsql("SELECT * FROM people"));
printtbl(sqlexec("SELECT name, age FROM people WHERE age > 40 ORDER BY age DESC"));

int updated = sqlexec("UPDATE people SET age = 37 WHERE id = 1");
print("updated {updated}");

int removed = sqlexec("DELETE FROM people WHERE name = 'Alan Turing'");
print("removed {removed}");

result = execsql("SELECT * FROM people ORDER BY id");
print(len(result));
print(result.rows, " ", result.columns);

for row in execsql("SELECT name FROM people ORDER BY name") {
    print("person: ", row[0]);
}

# print() renders tables the same way printtbl does
print(execsql("SELECT id, name FROM people WHERE id = 1"));

# LIMIT
printtbl(execsql("SELECT name FROM people ORDER BY age DESC LIMIT 1"));

sqlexec("DROP TABLE people");
