int fib(int n) {
    if (n == 0) {
        return 0;
    }
    else if (n == 1) {
        return 1;
    }
    else {
        return fib(n - 1) + fib(n - 2);
    }
}

void generate(int x) {
    list pts = [];
    for (int i = 0; i < x; i = i + 1) {
        pts[i] = V3(i, i + 1, i + 3);
        print(i);
    }
    print(pts);
}

generate(10);
print("fib(11) = ", fib(10));
print("fib(11) = ", fib(11));
