int a[] = [1, 2, 3, 4];
print(a);
print(a[0], " ", a[3]);
print(len(a));

# gather
print(a[2, 0, 3]);

# 2D (spec syntax)
int b[] = [1, 2, 3, 4], [5, 6, 7, 8];
print(b);
print(b[1][2]);
b[1][2] = 99;
print(b[1][2]);

# 3D
int cube[] = [[1,2],[3,4]], [[5,6],[7,8]];
print(cube[1][0][1]);

# dynamic growth on write
int g[] = [1];
g[4] = 50;
print(g);
print(len(g));

# mixed types (arrays are dynamic and hold any type)
stuff = [1, 2.5, 'x', "str", [1, 2]];
print(stuff);
print(len(stuff));

# concat
int left[] = [1, 2];
int right[] = [3, 4];
print(left + right);

# get method
print(a.get(1));
print(a.get(1, 2));

# array copies are by value
int src[] = [7, 8, 9];
copy = src;
src[0] = 0;
print(copy);

# empty array declaration
int empty[];
print(len(empty), " ", empty);

# for-in with nested arrays
for row in b { print(row); }
