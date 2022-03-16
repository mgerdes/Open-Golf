void generate(float N, float width, float height, float length, float wall_height, float texture_length) {
    list points = [];

    float extra_room = 1.5;
    for (int i = 0; i < N; i = i + 1) {
        float t = i / (N - 1);
        float theta0 = 0 - 0.5 * PI;
        float theta1 = 1.5 * PI;
        float theta = theta0 + (theta1 - theta0) * t;

        points[points.length] = V3(length * cos(theta), height * sin(theta), (width + extra_room) * t);
        points[points.length] = V3(length * cos(theta), height * sin(theta), (width + extra_room) * t + width);

        points[points.length] = V3((length - wall_height) * cos(theta), (height - wall_height) * sin(theta), (width + extra_room) * t);
        points[points.length] = V3((length - wall_height) * cos(theta), (height - wall_height) * sin(theta), (width + extra_room) * t - 0.5);
        points[points.length] = V3((length + wall_height) * cos(theta), (height + wall_height) * sin(theta), (width + extra_room) * t - 0.5);

        points[points.length] = V3((length - wall_height) * cos(theta), (height - wall_height) * sin(theta), (width + extra_room) * t + width);
        points[points.length] = V3((length - wall_height) * cos(theta), (height - wall_height) * sin(theta), (width + extra_room) * t + width + 0.5);
        points[points.length] = V3((length + wall_height) * cos(theta), (height + wall_height) * sin(theta), (width + extra_room) * t + width + 0.5);
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }

    list idx = [];
    list uv = [];
    vec3 pa;
    vec3 pb;
    vec3 pc;
    vec3 pd;

    float total_dist = 0;
    for (int i = 0; i < N - 1; i = i + 1) {
        int a = 8 * (i + 0) + 0;
        int d = 8 * (i + 1) + 0;

        pa = points[a];
        pd = points[d];

        float dist0 = vec3_distance(pa, pd);
        total_dist = total_dist + dist0;
    }

    float dist = 0;
    for (int i = 0; i < N - 1; i = i + 1) {
        idx[0] = 8 * (i + 0) + 0;
        idx[1] = 8 * (i + 0) + 1;
        idx[2] = 8 * (i + 1) + 1;
        idx[3] = 8 * (i + 1) + 0;

        pa = points[idx[0]];
        pb = points[idx[1]];
        pc = points[idx[2]];
        pd = points[idx[3]];

        float dist0 = vec3_distance(pa, pd);
        float dist1 = vec3_distance(pa, pb);

        float m0 = dist / total_dist;
        float m1 = (dist + dist0) / total_dist;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0.5 * dist1, m0 * texture_length);
        uv[2] = V2(0.5 * dist1, m1 * texture_length);
        uv[3] = V2(0, m1 * texture_length);

        terrain_model_add_face("ground", idx, uv);

        idx[0] = 8 * (i + 0) + 0;
        idx[1] = 8 * (i + 1) + 0;
        idx[2] = 8 * (i + 1) + 2;
        idx[3] = 8 * (i + 0) + 2;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.25, m1 * texture_length);
        uv[3] = V2(0.25, m0 * texture_length);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 2;
        idx[1] = 8 * (i + 1) + 2;
        idx[2] = 8 * (i + 1) + 3;
        idx[3] = 8 * (i + 0) + 3;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.25, m1 * texture_length);
        uv[3] = V2(0.25, m0 * texture_length);

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 0) + 3;
        idx[1] = 8 * (i + 1) + 3;
        idx[2] = 8 * (i + 1) + 4;
        idx[3] = 8 * (i + 0) + 4;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.5, m1 * texture_length);
        uv[3] = V2(0.5, m0 * texture_length);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 5;
        idx[1] = 8 * (i + 1) + 5;
        idx[2] = 8 * (i + 1) + 1;
        idx[3] = 8 * (i + 0) + 1;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.25, m1 * texture_length);
        uv[3] = V2(0.25, m0 * texture_length);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 6;
        idx[1] = 8 * (i + 1) + 6;
        idx[2] = 8 * (i + 1) + 5;
        idx[3] = 8 * (i + 0) + 5;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.25, m1 * texture_length);
        uv[3] = V2(0.25, m0 * texture_length);

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 0) + 7;
        idx[1] = 8 * (i + 1) + 7;
        idx[2] = 8 * (i + 1) + 6;
        idx[3] = 8 * (i + 0) + 6;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.5, m1 * texture_length);
        uv[3] = V2(0.5, m0 * texture_length);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 1;
        idx[1] = 8 * (i + 1) + 1;
        idx[2] = 8 * (i + 1) + 7;
        idx[3] = 8 * (i + 0) + 7;

        uv[0] = V2(0, 0);
        uv[1] = V2(0, 0);
        uv[2] = V2(0, 0);
        uv[3] = V2(0, 0);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 4;
        idx[1] = 8 * (i + 1) + 4;
        idx[2] = 8 * (i + 1) + 7;
        idx[3] = 8 * (i + 0) + 7;

        uv[0] = V2(0, m0 * texture_length);
        uv[1] = V2(0, m1 * texture_length);
        uv[2] = V2(0.5*dist1, m1 * texture_length);
        uv[3] = V2(0.5*dist1, m0 * texture_length);

        terrain_model_add_face("wall-side", idx, uv);

        dist = dist + dist0;
    }
}
