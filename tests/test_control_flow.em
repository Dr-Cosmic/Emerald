# if / elif / else
int x = 5;
if (x == 5) { print("five"); }
elif (x != 7) { print("not seven"); }
else { print("other"); }

if (x > 10) { print("big"); }
elif (x > 3) { print("medium"); }
else { print("small"); }

if (x == 99) { print("no"); }
else { print("else taken"); }

# classic for
for (i = 1; i < 4; ++i) { print("i={i}"); }

# for-in over array
int arr[] = [10, 20, 30];
for v in arr { print(v); }

# for-in over string
for ch in "abc" { print(ch); }

# while
int n = 0;
while (n < 3) { n = n + 1; }
print("n={n}");

# break / continue
for (i = 0; i < 10; ++i) {
    if (i == 1) { continue; }
    if (i == 4) { break; }
    print("k={i}");
}
int w = 0;
while (true) {
    w = w + 1;
    if (w >= 3) { break; }
}
print("w={w}");

# nesting
for (i = 0; i < 2; ++i) {
    for (j = 0; j < 2; ++j) {
        print("{i},{j}");
    }
}
