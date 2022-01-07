void generate(int x) {
    list pts = [];
    for (int i = 0; i < x; i = i + 1) {
        pts[i] = V3(i, i + 1, i + 3);
        print(i);
    }
    print(pts);
}
