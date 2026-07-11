File f = "emerald_test_scratch.txt";
f.write("Hello, Emerald! This file has some text in it.");
print(len(f));

# read() returns content usable in expressions
string all = f.read();
print(all);

# read(N) -> first N+1 chars
print(f.read(4));

# read(a, b) -> chars at indices a..b inclusive
print(f.read(7, 13));

# read(N,) -> everything after the first N+1 chars
print(f.read(4,));

# standalone read prints
f.read(0, 4);

f.append(" Appended.");
print(f.read());

f.write(7, "EMERALD");
print(f.read(0, 20));

print(len(f));

# write() overwrites the whole file
f.write("short");
print(f.read());
print(len(f));

# half-file read (spec example uses length())
f.write("0123456789");
print(f.read(length(f) / 2));
