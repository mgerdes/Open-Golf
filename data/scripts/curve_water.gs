void generate(float N, float r0, float r1, float a) {
    list points = [];
    float theta0 = 0;
    float theta1 = 2 * PI * a;

    for (int i = 0; i < N; i = i + 1) {
        float t = i / (N - 1);
        float theta = theta0 + (theta1 - theta0) * t;
        float c = cos(theta);
        float s = sin(theta);
        points[points.length] = V3(r0 * c, 0, r0 * s);
        points[points.length] = V3(r1 * c, 0, r1 * s);
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }

    list idx = [];
    list uv = [];

    for (int i = 0; i < N - 1; i = i + 1) {
        float t0 = i / (N - 1);
        float t1 = (i + 1) / (N - 1);

        idx[0] = 2 * i + 2;
        idx[1] = 2 * i + 3;
        idx[2] = 2 * i + 1;
        idx[3] = 2 * i + 0;

        vec3 p0 = points[idx[0]];
        vec3 p3 = points[idx[3]];
        vec3 water_dir = vec3_normalize(p3 - p0);

        uv[0] = V2(0, t1);
        uv[1] = V2(1, t1);
        uv[2] = V2(1, t0);
        uv[3] = V2(0, t0);

        terrain_model_add_water_face("ground", idx, uv, water_dir);
    }
}
