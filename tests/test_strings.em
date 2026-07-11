string s = "hello world";
print(s);
print(len(s));
print(s.get(0));
print(s.get(0, 4));
print(s[6]);
print(s[0, 2, 4]);

# escapes
print("line1\nline2");
print("tab\tstop");
print("quote: \" backslash: \\");

# interpolation
int x = 42;
float pi_ish = 3.14;
print("x is {x} and pi-ish is {pi_ish}");
print("math inline: {x * 2}");
string name = "Emerald";
print("hello {name}!");
print("{{escaped braces}}");

# concatenation and repetition of concat
print("abc" + "def");
print("num: " + 5);
print(1 + " and " + 2.5);

# comparisons
print("apple" < "banana", " ", "b" > "a", " ", 'a' < 'b');
print("same" == "same", " ", "a" != "b");
print('j' == "j", " ", 'j' === "j");

# chars behave numerically in arithmetic
char c = 'A';
print(c + 1);
print(char(c + 1));
print(int('j'));

# string/number loose equality (JavaScript-style ==)
print("5" == 5, " ", "5" === 5, " ", "5.0" == 5);

# casting
print(int("123") + 1);
print(float("2.5") + 0.5);
print(string(99) + "!");
