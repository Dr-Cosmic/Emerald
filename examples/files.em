# files.em - Working with files.

File journal = "journal.txt";
journal.write("Day 1: started learning Emerald.");
journal.append("\nDay 2: wrote a program that writes programs.");

print("The journal has {len(journal)} characters:");
journal.read();          # a standalone read() prints the file

print("First 5 characters:");
journal.read(4);         # read(N) prints characters 0..N

print("Characters 7..13:");
journal.read(7, 13);

print("Everything after the first 5 characters:");
journal.read(4,);        # trailing-comma shorthand

# read() can also be used as an expression:
string firstHalf = journal.read(length(journal) / 2);
print("Half of it is {len(firstHalf)} characters long.");

journal.write(0, "DAY");     # overwrite starting at character 1
print(journal.read(0, 10));
