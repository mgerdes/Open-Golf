void generate(float N, float r0, float width, float height) {
    float r1 = r0 + width;
    list pts = [];
    list idx = [];
    list uv = [];
    for (int i = 0; i <= N; i = i + 1) {
        float a = i / N;

        float x = cos(PI * a);
        float y = sin(PI * a);

        vec3 p0 = V3((r0 - 0.5) * x, 0 - 3.5, (r0 - 0.5) * y);
        vec3 p1 = V3((r0 - 0.5) * x, x * height + 0.375, (r0 - 0.5) * y);
        vec3 p2 = V3(r0 * x, x * height + 0.375, r0 * y);
        vec3 p3 = V3(r0 * x, x * height, r0 * y);

        terrain_model_add_point(p0);
        terrain_model_add_point(p1);
        terrain_model_add_point(p2);
        terrain_model_add_point(p3);
        pts[pts.length] = p0;
        pts[pts.length] = p1;
        pts[pts.length] = p2;
        pts[pts.length] = p3;

        for (int j = 0; j <= N; j = j + 1) {
            float b = j / N;
            float r = r0 + b * (r1 - r0);
            float z2 = y * sin(PI + PI * b);

            vec3 p = V3(r * x, z2 + x * height, r * y);
            terrain_model_add_point(p);
            pts[pts.length] = p;
        }

        vec3 p4 = V3(r1 * x, x * height, r1 * y);
        vec3 p5 = V3(r1 * x, x * height + 0.375, r1 * y);
        vec3 p6 = V3((r1 + 0.5) * x, x * height + 0.375, (r1 + 0.5) * y);
        vec3 p7 = V3((r1 + 0.5) * x, 0 - 3.5, (r1 + 0.5) * y);

        terrain_model_add_point(p4);
        terrain_model_add_point(p5);
        terrain_model_add_point(p6);
        terrain_model_add_point(p7);
        pts[pts.length] = p4;
        pts[pts.length] = p5;
        pts[pts.length] = p6;
        pts[pts.length] = p7;
    }

    for (int i = 0; i < N; i = i + 1) {
        float a0 = i / N;
        float a1 = (i + 1) / N;

        for (int j = 0; j < N; j = j + 1) {
            float b0 = j / N;
            float b1 = (j + 1) / N;

            idx[0] = (8 + N + 1) * i + j + 4;
            idx[1] = (8 + N + 1) * (i + 1) + j + 4;
            idx[2] = (8 + N + 1) * (i + 1) + j + 5;
            idx[3] = (8 + N + 1) * i + j + 5;

            uv[0] = V2(2.5 * b0, 4 * a0);
            uv[1] = V2(2.5 * b0, 4 * a1);
            uv[2] = V2(2.5 * b1, 4 * a1);
            uv[3] = V2(2.5 * b1, 4 * a0);

            terrain_model_add_face("curve_down_ground", idx, uv);
        }
    }

    float l = 0;
    // Inner Out-side wall
    for (int i = 0; i < N; i = i + 1) {
        idx[3] = (8 + N + 1) * i;
        idx[2] = (8 + N + 1) * i + 1;
        idx[1] = (8 + N + 1) * (i + 1) + 1;
        idx[0] = (8 + N + 1) * (i + 1);

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];
        vec3 p3 = pts[idx[3]];

        float dist = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.z - p1.z) * (p2.z - p1.z));

        uv[0] = V2(0.125 + p0.y, l + dist);
        uv[1] = V2(0.125 + p1.y, l + dist);
        uv[2] = V2(0.125 + p2.y, l);
        uv[3] = V2(0.125 + p3.y, l);

        l = l + dist;
        terrain_model_add_face("curve_down_wall_side", idx, uv);
    }

    l = 0;
    // Inner top wall
    for (int i = 0; i < N; i = i + 1) {
        idx[3] = (8 + N + 1) * i + 1;
        idx[2] = (8 + N + 1) * i + 2;
        idx[1] = (8 + N + 1) * (i + 1) + 2;
        idx[0] = (8 + N + 1) * (i + 1) + 1;

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];
        vec3 p3 = pts[idx[3]];

        float dist = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.z - p1.z) * (p2.z - p1.z));

        uv[0] = V2(0, l + dist);
        uv[1] = V2(0.5, l + dist);
        uv[2] = V2(0.5, l);
        uv[3] = V2(0, l);

        l = l + dist;
        terrain_model_add_face("curve_down_wall_side", idx, uv);
    }

    l = 0;
    // Inner inner-side wall
    for (int i = 0; i < N; i = i + 1) {
        idx[3] = (8 + N + 1) * i + 2;
        idx[2] = (8 + N + 1) * i + 3;
        idx[1] = (8 + N + 1) * (i + 1) + 3;
        idx[0] = (8 + N + 1) * (i + 1) + 2;

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];
        vec3 p3 = pts[idx[3]];

        float dist = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.z - p1.z) * (p2.z - p1.z));

        uv[0] = V2(0, l + dist);
        uv[1] = V2(0.38, l + dist);
        uv[2] = V2(0.38, l);
        uv[3] = V2(0, l);

        l = l + dist;
        terrain_model_add_face("curve_down_wall_side", idx, uv);
    }

    l = 0;
    // Outer inner-side wall
    for (int i = 0; i < N; i = i + 1) {
        idx[3] = (8 + N + 1) * i + N + 5;
        idx[2] = (8 + N + 1) * i + N + 6;
        idx[1] = (8 + N + 1) * (i + 1) + N + 6;
        idx[0] = (8 + N + 1) * (i + 1) + N + 5;

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];
        vec3 p3 = pts[idx[3]];

        float dist = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.z - p1.z) * (p2.z - p1.z));

        uv[0] = V2(0.38, l + dist);
        uv[1] = V2(0.0, l + dist);
        uv[2] = V2(0.0, l);
        uv[3] = V2(0.38, l);

        l = l + dist;
        terrain_model_add_face("curve_down_wall_side", idx, uv);
    }

    l = 0;
    // Outer top wall
    for (int i = 0; i < N; i = i + 1) {
        idx[3] = (8 + N + 1) * i + N + 6;
        idx[2] = (8 + N + 1) * i + N + 7;
        idx[1] = (8 + N + 1) * (i + 1) + N + 7;
        idx[0] = (8 + N + 1) * (i + 1) + N + 6;

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];
        vec3 p3 = pts[idx[3]];

        float dist = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.z - p1.z) * (p2.z - p1.z));

        uv[0] = V2(0, l + dist);
        uv[1] = V2(0.5, l + dist);
        uv[2] = V2(0.5, l);
        uv[3] = V2(0.0, l);

        l = l + dist;
        terrain_model_add_face("curve_down_wall_side", idx, uv);
    }

    l = 0;
    // Outer out-side wall
    for (int i = 0; i < N; i = i + 1) {
        idx[3] = (8 + N + 1) * i + N + 7;
        idx[2] = (8 + N + 1) * i + N + 8;
        idx[1] = (8 + N + 1) * (i + 1) + N + 8;
        idx[0] = (8 + N + 1) * (i + 1) + N + 7;

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];
        vec3 p3 = pts[idx[3]];

        float dist = sqrt((p2.x - p1.x) * (p2.x - p1.x) + (p2.z - p1.z) * (p2.z - p1.z));

        uv[0] = V2(0.125 + p0.y, l + dist);
        uv[1] = V2(0.125 + p1.y, l + dist);
        uv[2] = V2(0.125 + p2.y, l);
        uv[3] = V2(0.125 + p3.y, l);

        l = l + dist;
        terrain_model_add_face("curve_down_wall_side", idx, uv);
    }
}
