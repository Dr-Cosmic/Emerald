# database.em - Emerald's built-in SQL engine (tables stored as JSON).

sqldatadir("garage_data");
sqlexec("DROP TABLE IF EXISTS cars");
sqlexec("CREATE TABLE cars (id int, model text, hp int)");
sqlexec("INSERT INTO cars VALUES (1, 'Roadster', 450), (2, 'Wagon', 180), (3, 'Coupe', 320)");

print("All cars:");
printtbl(execsql("SELECT * FROM cars"));

print("Powerful cars:");
printtbl(execsql("SELECT model, hp FROM cars WHERE hp > 200 ORDER BY hp DESC"));

sqlexec("UPDATE cars SET hp = 500 WHERE model = 'Roadster'");
print("After a tune-up:");
print(execsql("SELECT * FROM cars WHERE id = 1"));

# Query results are tables you can iterate:
for row in execsql("SELECT model FROM cars ORDER BY model") {
    print("- ", row[0]);
}
