# #!/usr/bin/env bash

# set -e

# echo "========================================"
# echo "C"
# echo "========================================"

# cat > fib.c <<'EOF'
# #include <stdio.h>

# unsigned long long fibonacci(int n) {
#     unsigned long long a = 0, b = 1;
#     for (int i = 0; i < n; i++) {
#         unsigned long long t = a;
#         a = b;
#         b += t;
#     }
#     return a;
# }

# int main(void) {
#     printf("%llu\n", fibonacci(69));
# }
# EOF

# cc -O2 fib.c -o fib_c
# /usr/bin/time -l ./fib_c

# echo
# echo "========================================"
# echo "C++"
# echo "========================================"

# cat > fib.cpp <<'EOF'
# #include <iostream>

# unsigned long long fibonacci(int n) {
#     unsigned long long a = 0, b = 1;
#     for (int i = 0; i < n; i++) {
#         unsigned long long t = a;
#         a = b;
#         b += t;
#     }
#     return a;
# }

# int main() {
#     std::cout << fibonacci(69) << '\n';
# }
# EOF

# c++ -O2 fib.cpp -o fib_cpp
# /usr/bin/time -l ./fib_cpp

# echo
# echo "========================================"
# echo "Swift"
# echo "========================================"

# cat > fib.swift <<'EOF'
# func fibonacci(_ n: Int) -> UInt64 {
#     var a: UInt64 = 0
#     var b: UInt64 = 1

#     for _ in 0..<n {
#         let t = a
#         a = b
#         b += t
#     }

#     return a
# }

# print(fibonacci(69))
# EOF

# swiftc -O fib.swift -o fib_swift
# /usr/bin/time -l ./fib_swift

# echo
# echo "========================================"
# echo "Python"
# echo "========================================"

# cat > fib.py <<'EOF'
# def fibonacci(n):
#     a, b = 0, 1
#     for _ in range(n):
#         a, b = b, a + b
#     return a

# print(fibonacci(69))
# EOF

# /usr/bin/time -l python3 fib.py

# echo
# echo "========================================"
# echo "Ruby"
# echo "========================================"

# cat > fib.rb <<'EOF'
# def fibonacci(n)
#   a = 0
#   b = 1

#   n.times do
#     a, b = b, a + b
#   end

#   a
# end

# puts fibonacci(69)
# EOF

# /usr/bin/time -l ruby fib.rb

# echo
# echo "========================================"
# echo "Java"
# echo "========================================"

# cat > Fib.java <<'EOF'
# public class Fib {
#     static long fibonacci(int n) {
#         long a = 0;
#         long b = 1;

#         for (int i = 0; i < n; i++) {
#             long t = a;
#             a = b;
#             b += t;
#         }

#         return a;
#     }

#     public static void main(String[] args) {
#         System.out.println(fibonacci(69));
#     }
# }
# EOF

# javac Fib.java
# /usr/bin/time -l java Fib


# echo
# echo "========================================"
# echo "Emerald"
# echo "========================================"

# (
#     cd ../Downloads/Emerald-main || exit 1

#     ./scripts/build.sh

#     /usr/bin/time -l ./build/emerald examples/fib.em
# )



#!/usr/bin/env bash

set -e

ITERATIONS=1000000

echo "========================================"
echo "C"
echo "========================================"

cat > fib.c <<EOF
#include <stdio.h>

unsigned long long fibonacci(int n) {
    unsigned long long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        unsigned long long t = a;
        a = b;
        b += t;
    }
    return a;
}

int main(void) {
    unsigned long long result = 0;

    for (int i = 0; i < $ITERATIONS; i++) {
        result = fibonacci(69);
    }

    printf("%llu\n", result);
}
EOF

cc -O2 fib.c -o fib_c
/usr/bin/time -l ./fib_c


echo
echo "========================================"
echo "C++"
echo "========================================"

cat > fib.cpp <<EOF
#include <iostream>

unsigned long long fibonacci(int n) {
    unsigned long long a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        unsigned long long t = a;
        a = b;
        b += t;
    }
    return a;
}

int main() {
    unsigned long long result = 0;

    for (int i = 0; i < $ITERATIONS; i++) {
        result = fibonacci(69);
    }

    std::cout << result << '\n';
}
EOF

c++ -O2 fib.cpp -o fib_cpp
/usr/bin/time -l ./fib_cpp


echo
echo "========================================"
echo "Swift"
echo "========================================"

cat > fib.swift <<EOF
func fibonacci(_ n: Int) -> UInt64 {
    var a: UInt64 = 0
    var b: UInt64 = 1

    for _ in 0..<n {
        let t = a
        a = b
        b += t
    }

    return a
}

var result: UInt64 = 0

for _ in 0..<$ITERATIONS {
    result = fibonacci(69)
}

print(result)
EOF

swiftc -O fib.swift -o fib_swift
/usr/bin/time -l ./fib_swift


echo
echo "========================================"
echo "Python"
echo "========================================"

cat > fib.py <<EOF
def fibonacci(n):
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a

result = 0

for _ in range($ITERATIONS):
    result = fibonacci(69)

print(result)
EOF

/usr/bin/time -l python3 fib.py


echo
echo "========================================"
echo "Ruby"
echo "========================================"

cat > fib.rb <<EOF
def fibonacci(n)
  a = 0
  b = 1

  n.times do
    a, b = b, a + b
  end

  a
end

result = 0

$ITERATIONS.times do
  result = fibonacci(69)
end

puts result
EOF

/usr/bin/time -l ruby fib.rb


echo
echo "========================================"
echo "Java"
echo "========================================"

cat > Fib.java <<EOF
public class Fib {
    static long fibonacci(int n) {
        long a = 0;
        long b = 1;

        for (int i = 0; i < n; i++) {
            long t = a;
            a = b;
            b += t;
        }

        return a;
    }

    public static void main(String[] args) {
        long result = 0;

        for (int i = 0; i < $ITERATIONS; i++) {
            result = fibonacci(69);
        }

        System.out.println(result);
    }
}
EOF

javac Fib.java
/usr/bin/time -l java Fib


echo
echo "========================================"
echo "Emerald"
echo "========================================"

(
    cd "$HOME/Downloads/Emerald-main" || exit 1

    ./scripts/build.sh

    /usr/bin/time -l ./build/emerald examples/fib.em
)