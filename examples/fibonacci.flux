// Fibonacci — recursive and iterative

fn fib_recursive(n) {
    if (n <= 1) {
        return n;
    }
    return fib_recursive(n - 1) + fib_recursive(n - 2);
}

fn fib_iterative(n) {
    if (n <= 1) {
        return n;
    }
    let a = 0;
    let b = 1;
    let i = 2;
    while (i <= n) {
        let temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }
    return b;
}

print("Fibonacci (recursive):");
let i = 0;
while (i <= 10) {
    print("fib(" + i + ") = " + fib_recursive(i));
    i = i + 1;
}

print("");
print("Fibonacci (iterative) fib(30) = " + fib_iterative(30));
