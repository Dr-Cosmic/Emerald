int a = 0;
int b = 1;
for (int i = 0; i < 1000000; i++){
    int t = a;
    a = b;
    b = b + t;
}
print(a);