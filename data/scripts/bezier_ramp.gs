void generate(float N, float height, float width, float length,
        float texture_dx, float texture_dy, 
        float border_height, float border_width, float border_ground_y,
        vec2 bp0, vec2 bp1, vec2 bp2, vec2 bp3) {
    list points = [];

    for (int i = 0; i < N + 1; i = i + 1) {
        float t = i / N;

        vec2 bp = vec2_bezier4(bp0, bp1, bp2, bp3, t);
        float y = height * bp.y;
        float z = length * bp.x;

        vec3 p0 = V3(0, y, z);
        vec3 p1 = V3(width, y, z);
        points[points.length] = p0;
        points[points.length] = p1;

        vec3 p2 = V3(0, y + border_height, z);
        vec3 p3 = V3(0 - border_width, y + border_height, z);
        vec3 p4 = V3(0 - border_width, border_ground_y, z);
        points[points.length] = p2;
        points[points.length] = p3;
        points[points.length] = p4;

        vec3 p5 = V3(width, y + border_height, z);
        vec3 p6 = V3(width + border_width, y + border_height, z);
        vec3 p7 = V3(width + border_width, border_ground_y, z);
        points[points.length] = p5;
        points[points.length] = p6;
        points[points.length] = p7;
    }

    float total_dist = 0;
    for (int i = 0; i < N; i = i + 1) {
        vec3 p0 = points[8 * i];
        vec3 p1 = points[8 * (i + 1)];
        total_dist = total_dist + vec3_distance(p0, p1);
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }

    list idx = [];
    list uv = [];
    float dist = 0;

    for (int i = 0; i < N; i = i + 1) {
        idx[0] = 8 * (i + 1) + 0;
        idx[1] = 8 * (i + 1) + 1;
        idx[2] = 8 * (i + 0) + 1;
        idx[3] = 8 * (i + 0) + 0;

        vec3 p0 = points[idx[0]];
        vec3 p1 = points[idx[3]];
        float d0 = vec3_distance(p0, p1);
        float m0 = 0.5 * length * dist / total_dist;
        float m1 = 0.5 * length * (dist + d0) / total_dist;

        uv[0] = V2(texture_dx, m1);
        uv[1] = V2(texture_dx + 0.5 * width, m1);
        uv[2] = V2(texture_dx + 0.5 * width, m0);
        uv[3] = V2(texture_dx, m0);

        terrain_model_add_face("ground", idx, uv);

        idx[0] = 8 * (i + 0) + 0;
        idx[1] = 8 * (i + 0) + 2;
        idx[2] = 8 * (i + 1) + 2;
        idx[3] = 8 * (i + 1) + 0;

        uv[0] = V2(0, m0);
        uv[1] = V2(0.25, m0);
        uv[2] = V2(0.25, m1);
        uv[3] = V2(0, m1);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 2;
        idx[1] = 8 * (i + 0) + 3;
        idx[2] = 8 * (i + 1) + 3;
        idx[3] = 8 * (i + 1) + 2;

        uv[0] = V2(0, m0);
        uv[1] = V2(0.5, m0);
        uv[2] = V2(0.5, m1);
        uv[3] = V2(0, m1);

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 0) + 3;
        idx[1] = 8 * (i + 0) + 4;
        idx[2] = 8 * (i + 1) + 4;
        idx[3] = 8 * (i + 1) + 3;

        float height0 = vec3_distance(points[idx[0]], points[idx[1]]);
        float height1 = vec3_distance(points[idx[2]], points[idx[3]]);

        uv[0] = V2(0, m0);
        uv[1] = V2(2 * height0, m0);
        uv[2] = V2(2 * height1, m1);
        uv[3] = V2(0, m1);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 1) + 1;
        idx[1] = 8 * (i + 1) + 5;
        idx[2] = 8 * (i + 0) + 5;
        idx[3] = 8 * (i + 0) + 1;

        uv[0] = V2(0, m1);
        uv[1] = V2(0.25, m1);
        uv[2] = V2(0.25, m0);
        uv[3] = V2(0, m0);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 1) + 5;
        idx[1] = 8 * (i + 1) + 6;
        idx[2] = 8 * (i + 0) + 6;
        idx[3] = 8 * (i + 0) + 5;

        uv[0] = V2(0, m1);
        uv[1] = V2(0.5, m1);
        uv[2] = V2(0.5, m0);
        uv[3] = V2(0, m0);

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 1) + 6;
        idx[1] = 8 * (i + 1) + 7;
        idx[2] = 8 * (i + 0) + 7;
        idx[3] = 8 * (i + 0) + 6;

        height0 = vec3_distance(points[idx[0]], points[idx[3]]);
        height1 = vec3_distance(points[idx[1]], points[idx[2]]);

        uv[0] = V2(0, m1);
        uv[1] = V2(2*height, m1);
        uv[2] = V2(2*height, m0);
        uv[3] = V2(0, m0);

        terrain_model_add_face("wall-side", idx, uv);

        dist = dist + d0;
    }
}
