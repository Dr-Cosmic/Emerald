# The Emerald Language Reference

Emerald is a dynamically-executed language with optional type annotations,
curly-brace blocks, and semicolon-terminated statements. Everything is passed
by value; there are no pointers or address operators. Null values are called
`nil`. Memory is managed automatically.

Source files end in `.em` or `.emerald`. Comments start with `#` and run to the
end of the line.

```emerald
# this is a comment
int x = 5;          # statements end with semicolons
if (x == 5) {       # blocks use curly brackets, never indentation
    print("five");
}
```

## 1. Types and variables

The primitive types are `int`, `float`, `char`, `string`, plus booleans
(`true` / `false`), arrays, class objects, `File` handles, tables (returned by
SQL queries), and `nil`.

```emerald
int a = 2;
int b;                  # defaults to 0 (floats to 0.0, strings to "")
float g = 1.5;
char mychar = 'j';
string mystring = "string";
```

Typed declarations coerce their initializer where sensible (`int n = 3.9;`
stores 3). Variables can also be created by plain assignment without a type:

```emerald
x = 10;                 # untyped: takes whatever it is given
x = "now a string";     # and may be freely re-assigned
```

### Type casting

```emerald
int x = 5;
x = float(5.2);         # x is now the float 5.2
print(int("42"));       # 42
print(char(106));       # j
print(int('j'));        # 106
print(string(3.5));     # "3.5"
```

### nil

```emerald
if (x == nil) { print("nothing here"); }
```

`nil` equals only `nil`. Reading an undeclared variable is an error; a missing
function argument arrives as `nil`.

## 2. Operators

Arithmetic: `+`, `-`, `*`, `/`, `//`, `%`, `**`

* `/` is true division and always yields a float: `7 / 2` is `3.5`.
* `//` is floor division: `7 // 2` is `3`, `-7 // 2` is `-4`.
* `%` is modulo with the sign of the divisor (like Python).
* `**` is exponentiation and binds tighter than unary minus: `-2 ** 2` is `-4`.
* `+` also concatenates strings (`"a" + "b"`, `"n: " + 5`) and arrays.
* `char` values behave as their character code in arithmetic: `'A' + 1` is `66`.

Comparison: `<`, `<=`, `>`, `>=` (numbers, chars, and strings — strings compare
lexicographically).

Equality: `==`, `!=` are loose, JavaScript-style; `===`, `!==` are strict:

```emerald
print(5 == 5.0);        # true   (loose: numeric coercion)
print(5 === 5.0);       # false  (strict: int and float are different types)
print("5" == 5);        # true   (loose: string/number coercion)
print('j' == "j");      # true
print('j' === "j");     # false
```

Logic: `&&`, `||`, `!` — short-circuiting, with the usual truthiness rules
(`nil`, `0`, `0.0`, `""`, `[]`, and `false` are falsy).

Increment / decrement: `++x`, `x++`, `--x`, `x--` on ints, floats, and chars.

Built-in math: `abs(n)`, `floor(n)` and `top(n)` (round down / up, returning
ints), `log(n)` (base 10), `log(base, n)`, `ln(n)`, `sqrt(n)`, `exp(n)`, and
the trigonometric family including hyperbolics: `sin cos tan asin acos atan
sinh cosh tanh asinh acosh atanh`. The constants `pi` and `e` are predefined.

## 3. Control flow

```emerald
if (x == 5) { print("five"); }
elif (x != 7) { print("not seven"); }
else { print("something else"); }

for (i = 1; i < 4; ++i) { print(i); }

int a[] = [1, 2, 3, 4];
for i in a { print(i); }        # for-in over arrays, strings, or tables

while (x < 3) { x = x + 1; }
```

`break;` and `continue;` work inside every loop form.

## 4. Functions

`fn` defines a function. Declaring a return type is unnecessary — a function
may return nothing, or a value of any type.

```emerald
fn add(a, b) { return a + b; }
fn greet(name) { print("hi {name}"); }

print(add(3, 4));       # 7
greet("emerald");
```

Functions are values: `f = add; print(f(1, 2));`. Recursion works as expected.
Because Emerald passes everything by value, a function can never mutate its
caller's variables:

```emerald
fn tryMutate(arr) { arr[0] = 999; }
int nums[] = [1, 2, 3];
tryMutate(nums);
print(nums[0]);         # still 1
```

## 5. Arrays

Arrays are Java-style in syntax but dynamic and heterogeneous — they hold
values of any type and grow on demand. The only size limit is the OS.

```emerald
int a[] = [1, 2, 3, 4];
print(a[2]);            # 3
a[10] = 99;             # writing past the end grows the array (gaps are nil)

# 2D and beyond:
int b[] = [1, 2, 3, 4], [5, 6, 7, 8];
b[1][2] = 42;

# gather several elements at once:
print(a[3, 0, 2]);      # [4, 1, 3]

# slices via get:
print(a.get(1));        # element 1
print(a.get(1, 2));     # elements 1..2 inclusive, as a new array
```

Arrays copy by value on assignment and when passed to functions.

## 6. Strings

Double quotes make strings; single quotes make chars. Strings support the
usual escapes (`\n`, `\t`, `\"`, `\\`, ...).

`{expr}` inside a string interpolates any expression, automatically cast to a
string. Use `{{` and `}}` for literal braces.

```emerald
int x = 42;
print("X plus Y equals {x + 1}");   # X plus Y equals 43
print("{{literal}}");               # {literal}

string s = "emerald";
print(s.get(0));        # 'e'      (single index -> char)
print(s.get(2, 4));     # "era"    (inclusive range -> string)
print(s[1]);            # 'm'      (indexing works too)
print(s[0, 2, 4]);      # "eea"    (gather)
print(len(s));          # 7
```

## 7. Printing and input

```emerald
print("plain text\n with escapes");
print(x, " ", y, " ", z);        # multiple values, concatenated
print("interpolated {x}");
printdlm(" ", x, y, z);          # joined with the given delimiter
printtbl(mytable);               # neat table with | and ___ (see SQL below)

x = getinput();                  # read one line from the console (a string)
```

Printed objects show their fields one per line. `len(x)` works on any type:
string length, array size, file character count, table row count, and `1` for
ints, floats, and chars. `length(x)` is a synonym.

## 8. Classes

`class` defines a class; the parameter list is the constructor. Field
declarations in the body become per-instance fields; `fn` declarations become
methods. Class names begin with an uppercase letter, instance names with a
lowercase letter.

```emerald
class Newclass(param1, param2) {
    int a = 2;
    int b;                              # defaults to 0
    float g;
    char mychar = 'j';
    string mystring = "string";
    Otherclass otherclassobject(a, b);  # fields can be class instances

    fn describe() {
        print("a={a} b={b}");           # methods see fields directly
    }
}

Newclass thing(1, 2);
thing.describe();
thing.a = 10;                           # component access with .
print(thing.a);
```

### Inheritance

```emerald
class Newerclass(newparam) = Newclass(a, b) {
    mynewparam = newparam;      # plain assignment in a class body makes a field
}
```

The parent's constructor runs first (its arguments may use the child's
parameters), then the child's body. The child inherits every field and method.

### Objects are values

```emerald
class newcar;           # declares an (empty) object variable
newcar = oldcar;        # copies oldcar - the two are now independent
```

### Class reassignment and automatic typecasting

```emerald
Motorcycle mymotorcycle(a, b);
mymotorcycle = Car(c, d);   # mymotorcycle is now a Car; the old data is deleted
mymotorcycle = Car;         # also allowed: instantiates Car with no arguments
```

Classes can be nested, and class declarations inside a class body become
fields of the instance.

## 9. Files

A `File` is declared with an absolute or relative path. Declaring a `File`
creates the file on disk if it does not already exist.

```emerald
File myfile = "notes.txt";      # or an absolute path
File mynewfile;                 # creates "mynewfile" in the current directory
File mynewfile2 = "my new file";

myfile.write("fresh content");     # write / overwrite everything
myfile.append(" and more");        # add to the end
myfile.write(45, "patch");         # overwrite starting at character index 45

print(len(myfile));                # character count
myfile.read();                     # standalone read() prints the file
myfile.read(5);                    # prints the first 6 chars (indices 0..5)
myfile.read(6, 70);                # prints chars at indices 6..70 inclusive
myfile.read(5,);                   # shorthand: everything after the first 6
myfile.read(length(myfile) / 2);   # prints (roughly) the first half

string s = myfile.read(0, 4);      # inside an expression, read() returns
print("got: {s}");                 # the characters instead of printing
```

## 10. Shell commands

`shellexec("...")` runs a command through the system shell — like `subprocess`
in Python — and returns its standard output as a string:

```emerald
string listing = shellexec("ls -la");
print(listing);
```

## 11. SQL

Emerald ships with a small SQL engine. Data lives in a custom-named folder
(default `emerald_data`) with one JSON file per table, so your data is
readable, portable, and diff-able. Tables can reference and interact with each
other within the same data folder.

```emerald
sqldatadir("garage_data");     # choose the data folder (optional)

sqlexec("CREATE TABLE cars (id int, model text, hp int)");
sqlexec("INSERT INTO cars VALUES (1, 'Roadster', 450), (2, 'Wagon', 180)");
sqlexec("UPDATE cars SET hp = 500 WHERE id = 1");
sqlexec("DELETE FROM cars WHERE hp < 200");

table = execsql("SELECT * FROM cars WHERE hp > 100 ORDER BY hp DESC LIMIT 5");
printtbl(table);               # neat | and ___ table, every cell equal size
print(execsql("SELECT model FROM cars"));   # print() renders tables too

for row in execsql("SELECT model, hp FROM cars") {
    print(row[0], " -> ", row[1]);
}
```

`sqlexec` and `execsql` are interchangeable. SELECT returns a table value
(iterable, indexable by row, with `.rows`, `.columns`, and `len()`);
INSERT/UPDATE/DELETE return the number of rows affected.

Supported SQL: `CREATE TABLE`, `DROP TABLE [IF EXISTS]`, `INSERT INTO ...
VALUES (...), (...)`, `SELECT cols|* FROM t [WHERE ...] [ORDER BY col
[ASC|DESC]] [LIMIT n]`, `UPDATE ... SET ... [WHERE ...]`, `DELETE FROM ...
[WHERE ...]`. WHERE supports `=, ==, !=, <>, <, <=, >, >=`, `AND`, `OR`,
`NOT`, and parentheses.

## 12. Imports

```emerald
import mymodule;                    # loads mymodule.em / mymodule.emerald
print(mymodule.someFunction(3));

from mymodule import someFunction;  # bring one symbol into scope
from mymodule import SomeClass;
```

Modules are looked up next to the running script, then in the current
directory. A module is executed once and cached; its top-level functions,
classes, and variables become its members.

## 13. The REPL

Running `emerald` with no arguments starts an interactive session. Statements
work exactly as in a file; bare expressions print their value, and the
trailing semicolon is optional for them. Multi-line blocks are supported — the
prompt changes to `...` until braces balance.

## 14. Built-in function summary

| Builtin | Purpose |
| --- | --- |
| `print(...)` | print values (concatenated) followed by a newline |
| `printdlm(d, ...)` | print values joined by delimiter `d` |
| `printtbl(t)` | pretty-print a table |
| `len(x)` / `length(x)` | size of anything (1 for int/float/char) |
| `getinput()` | read one line from the console |
| `shellexec(cmd)` | run a shell command, return its stdout |
| `sqlexec(sql)` / `execsql(sql)` | run a SQL statement |
| `sqldatadir(dir)` | choose the SQL data folder |
| `int, float, char, string` | type casts |
| `abs, floor, top` | absolute value; round down / up |
| `log, ln, sqrt, exp` | logarithms (base 10 / custom base / natural), roots, exp |
| `sin..atanh` | full trigonometric + hyperbolic family |
| `type(x)` | the name of a value's type |
| `File(path)` | build a file handle in expression position |

## 15. Errors

Errors report the stage, position, and problem:

```
emerald: runtime error at line 2, col 7: Undefined name 'y'
emerald: parse error at line 1, col 10: Expected an expression
```
