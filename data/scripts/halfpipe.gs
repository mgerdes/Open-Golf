void generate(float N, float width, float radius, float a, float texture_height, float border_width, float border_ground_y) {
    list points = [];

    for (int i = 0; i < N; i = i + 1) {
        float t = i / (N - 1);
        float theta = 1.5 * PI - 2.0 * PI * a * t;

        float x;
        float y;
        float z;
        
        x = radius * cos(theta);
        y = radius * sin(theta);
        z = 0;
        points[points.length] = V3(x, y, z);

        x = radius * cos(theta);
        y = radius * sin(theta);
        z = width;
        points[points.length] = V3(x, y, z);

        x = 0 - radius - border_width;
        y = radius * sin(theta);
        z = 0;
        points[points.length] = V3(x, y, z);

        x = 0 - radius - border_width;
        y = radius * sin(theta);
        z = width;
        points[points.length] = V3(x, y, z);
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }

    list idx = [];
    list uv = [];

    // Ground
    for (int i = 0; i < N - 1; i = i + 1) {
        float t0 = i / (N - 1);
        float t1 = (i + 1) / (N - 1);

        idx[0] = 4 * i + 0;
        idx[1] = 4 * i + 4;
        idx[2] = 4 * i + 5;
        idx[3] = 4 * i + 1;

        uv[0] = V2(0, t0 * texture_height);
        uv[1] = V2(0, t1 * texture_height);
        uv[2] = V2(width, t1 * texture_height);
        uv[3] = V2(width, t0 * texture_height);

        terrain_model_add_face("ground", idx, uv);
    }

    // Side 1
    for (int i = 0; i < N - 1; i = i + 1) {
        float t0 = i / (N - 1);
        float t1 = (i + 1) / (N - 1);

        idx[0] = 4 * i + 0;
        idx[1] = 4 * i + 2;
        idx[2] = 4 * i + 6;
        idx[3] = 4 * i + 4;

        vec3 pa = points[idx[0]];
        vec3 pb = points[idx[1]];
        vec3 pc = points[idx[2]];
        vec3 pd = points[idx[3]];

        uv[0] = V2(pa.y, (pa.x - pb.x));
        uv[1] = V2(pb.y, 0);
        uv[2] = V2(pc.y, 0);
        uv[3] = V2(pd.y, pd.x - pc.x);

        terrain_model_add_face("wall-side", idx, uv);
    }

    // Side 2
    for (int i = 0; i < N - 1; i = i + 1) {
        float t0 = i / (N - 1);
        float t1 = (i + 1) / (N - 1);

        idx[0] = 4 * i + 3;
        idx[1] = 4 * i + 1;
        idx[2] = 4 * i + 5;
        idx[3] = 4 * i + 7;

        vec3 pa = points[idx[0]];
        vec3 pb = points[idx[1]];
        vec3 pc = points[idx[2]];
        vec3 pd = points[idx[3]];

        uv[0] = V2(pa.y, 0);
        uv[1] = V2(pb.y, pa.x - pb.x);
        uv[2] = V2(pc.y, pd.x - pc.x);
        uv[3] = V2(pd.y, 0);

        terrain_model_add_face("wall-side", idx, uv);
    }

    // Back
    for (int i = 0; i < N - 1; i = i + 1) {
        float t0 = i / (N - 1);
        float t1 = (i + 1) / (N - 1);

        idx[0] = 2;
        idx[1] = 3;
        idx[2] = 4 * N - 1;
        idx[3] = 4 * N - 2;

        uv[0] = V2(0, 0);
        uv[1] = V2(0, 0);
        uv[2] = V2(0, 0);
        uv[3] = V2(0, 0);

        terrain_model_add_face("wall-side", idx, uv);
    }

    // Top
    for (int i = 0; i < N - 1; i = i + 1) {
        float t0 = i / (N - 1);
        float t1 = (i + 1) / (N - 1);

        idx[0] = 4 * N - 1;
        idx[1] = 4 * N - 3;
        idx[2] = 4 * N - 4;
        idx[3] = 4 * N - 2;

        uv[0] = V2(0, 0);
        uv[1] = V2(0, 0);
        uv[2] = V2(0, 0);
        uv[3] = V2(0, 0);

        terrain_model_add_face("wall-top", idx, uv);
    }
}
