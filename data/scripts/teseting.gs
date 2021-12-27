void generate(int n, float width, float length, float height, vec2 bp0, vec2 bp1, vec2 bp2, vec2 bp3) {
    vec2[] points = [];

    bool should_make_walls = true;

    vec2 p = V2(0, 0);
    for (int i = 0; i <= n; i = i + 1) {
        float t = (float) i / n;
        vec2 bp = vec2_bezier(bp0, bp1, bp2, bp3, t);
        float z = length * bp.x;
        float y = height * bp.y;
        points[points.length] = V3(0, y, z);
        points[points.length] = V3(width, y, z);
    }
}
