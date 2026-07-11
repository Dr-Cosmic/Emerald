fn add(a, b) { return a + b; }
print(add(3, 4));

fn noReturn() { print("side effect"); }
noReturn();

fn nothing() { }
print(nothing() == nil);

fn fib(n) {
    if (n < 2) { return n; }
    return fib(n - 1) + fib(n - 2);
}
print(fib(12));

# pass by value
fn tryMutateArray(a) { a[0] = 999; }
int nums[] = [1, 2, 3];
tryMutateArray(nums);
print(nums[0]);

fn tryMutateInt(v) { v = 100; }
int plain = 7;
tryMutateInt(plain);
print(plain);

# functions as values
fn double(v) { return v * 2; }
mydouble = double;
print(mydouble(21));

# closures over globals
int counterBase = 10;
fn offset(v) { return counterBase + v; }
print(offset(5));

# early return
fn firstBig(items, threshold) {
    for v in items {
        if (v > threshold) { return v; }
    }
    return nil;
}
int data[] = [1, 8, 3, 12];
print(firstBig(data, 5));
print(firstBig(data, 100) == nil);
