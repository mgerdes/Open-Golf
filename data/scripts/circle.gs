void generate(float N, float r) {
    list pts = [];
    list idx = [];
    list uv = [];

    float theta0 = asin(2.5 / r);
    float theta1 = 2 * PI - theta0;
    float x0 = r * cos(theta0);

    vec3 p = V3(0, 0, 0);
    terrain_model_add_point(p);
    pts[pts.length] = p;

    int ground_start = pts.length;
    for (int i = 0; i <= N; i = i + 1) {
        float angle = theta0 + (theta1 - theta0) * i / N;
        float x = r * cos(angle);
        float y = r * sin(angle);

        p = V3(x - x0, 0, y);
        terrain_model_add_point(p);
        pts[pts.length] = p;
    }

    for (int i = 0; i < N; i = i + 1) {
        idx[0] = 0;
        idx[1] = ground_start + i + 1;
        idx[2] = ground_start + i;

        vec3 p0 = pts[idx[0]];
        vec3 p1 = pts[idx[1]];
        vec3 p2 = pts[idx[2]];

        uv[0] = V2(0.5 * p0.x, 0.5 * (p0.z - 2.5));
        uv[1] = V2(0.5 * p1.x, 0.5 * (p1.z - 2.5));
        uv[2] = V2(0.5 * p2.x, 0.5 * (p2.z - 2.5));
        terrain_model_add_face("ground", idx, uv);
    }

    int start1 = pts.length;
    for (int i = 0; i <= N; i = i + 1) {
        vec3 p0 = V3(0, 0, 0);
        vec3 p1 = pts[i + ground_start];
        vec3 d = vec3_normalize(p1 - p0);

        p = V3(p1.x, 0.375, p1.z); 
        terrain_model_add_point(p);
        pts[pts.length] = p;
    }

    int start2 = pts.length;
    for (int i = 0; i <= N; i = i + 1) {
        vec3 p0 = V3(0, 0, 0);
        vec3 p1 = pts[i + ground_start];
        vec3 d = vec3_normalize(p1 - p0);

        p = V3(p1.x + d.x * 0.5, 0.375, p1.z + d.z * 0.5); 
        terrain_model_add_point(p);
        pts[pts.length] = p;
    }

    int start3 = pts.length;
    for (int i = 0; i <= N; i = i + 1) {
        vec3 p0 = V3(0, 0, 0);
        vec3 p1 = pts[i + ground_start];
        vec3 d = vec3_normalize(p1 - p0);

        p = V3(p1.x + d.x * 0.5, 0-2, p1.z + d.z * 0.5); 
        terrain_model_add_point(p);
        pts[pts.length] = p;
    }

    float l = 0;
    for (int i = 0; i < N; i = i + 1) {
        int idx0 = ground_start + i;
        int idx1 = ground_start + i + 1;
        int idx2 = start1 + i + 1;
        int idx3 = start1 + i;

        vec3 p0 = pts[idx0];
        vec3 p1 = pts[idx1];
        vec3 p2 = pts[idx2];
        float d = vec3_length(p1 - p0);

        idx[0] = idx0;
        idx[1] = idx1;
        idx[2] = idx2;
        idx[3] = idx3;

        uv[0] = V2(p2.y, l);
        uv[1] = V2(p2.y, l + d);
        uv[2] = V2(p0.y, l + d);
        uv[3] = V2(p0.y, l);

        l = l + d;

        terrain_model_add_face("wall-side", idx, uv);
    }

    l = 0;
    float l0 = 0;
    float l1 = 0;
    for (int i = 0; i < N; i = i + 1) {
        int idx0 = start1 + i;
        int idx1 = start1 + i + 1;
        int idx2 = start2 + i + 1;
        int idx3 = start2 + i;

        vec3 p0 = pts[idx0];
        vec3 p1 = pts[idx1];
        vec3 p2 = pts[idx2];
        vec3 p3 = pts[idx3];
        float d0 = vec3_distance(p0, p1);
        float d1 = vec3_distance(p2, p3);

        idx[0] = idx0;
        idx[1] = idx1;
        idx[2] = idx2;
        idx[3] = idx3;

        uv[0] = V2(0, l0);
        uv[1] = V2(0, l0 + d0);
        uv[2] = V2(vec3_distance(p2, p1), l1 + d1);
        uv[3] = V2(vec3_distance(p3, p0), l1);

        l0 = l0 + d0;
        l1 = l1 + d1;

        terrain_model_add_face("wall-top", idx, uv);
    }

    l = 0;
    for (int i = 0; i < N; i = i + 1) {
        int idx0 = start2 + i;
        int idx1 = start2 + i + 1;
        int idx2 = start3 + i + 1;
        int idx3 = start3 + i;

        vec3 p0 = pts[idx0];
        vec3 p1 = pts[idx1];
        vec3 p2 = pts[idx2];
        float d = vec3_length(p1 - p0);

        idx[0] = idx0;
        idx[1] = idx1;
        idx[2] = idx2;
        idx[3] = idx3;

        uv[0] = V2(p2.y, l);
        uv[1] = V2(p2.y, l + d);
        uv[2] = V2(p0.y, l + d);
        uv[3] = V2(p0.y, l);

        l = l + d;

        terrain_model_add_face("wall-side", idx, uv);
    }
}
