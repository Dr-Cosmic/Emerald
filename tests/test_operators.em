# loose vs strict equality
print(5 == 5.0, " ", 5 === 5.0);
print(1 == true, " ", 1 === true);
print(0 == false, " ", nil == nil, " ", nil == 0);
print(5 != 5.0, " ", 5 !== 5.0);

# arrays deep equality
int a[] = [1, 2, [3, 4]];
int b[] = [1, 2, [3, 4]];
int c[] = [1, 2, [3, 5]];
print(a == b, " ", a == c);

# floor division and modulo (Python-style)
print(7 // 2, " ", -7 // 2, " ", 7.5 // 2);
print(7 % 3, " ", -7 % 3);

# power
print(2 ** 10);
print(2 ** 0.5 > 1.414 && 2 ** 0.5 < 1.415);
print(3 ** 3);

# logic and truthiness
print(true && true, " ", true && false, " ", false || true);
print(!true, " ", !0, " ", !"", " ", ![]);
print(1 && "yes", " ", nil || false);

# comparison chain results
print(3 < 5, " ", 5 <= 5, " ", 6 > 9, " ", 6 >= 6);

# unary
print(-5, " ", -(2.5), " ", -'A');

# increment / decrement (prefix and postfix)
int k = 5;
print(k++);
print(k);
print(++k);
print(k--);
print(--k);
print(k);
float fk = 1.5;
print(++fk);
char ck = 'a';
print(++ck);

# precedence
print(2 + 3 * 4);
print((2 + 3) * 4);
print(2 ** 3 * 2);
print(-2 ** 2);
print(10 - 4 - 3);
