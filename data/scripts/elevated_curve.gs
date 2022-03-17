void generate(float N, float r0, float r1, float a, float height, float texture_length, float texture_dx, float border_height, float border_width, float border_ground_y, float transition_N, float transition_length, float transition_scale) {
    list points = [];

    float theta0 = 0;
    float theta1 = 2 * PI * a;

    for (int i = 0; i < transition_N; i = i + 1) {
        float t = i / transition_N;

        float x0 = r0;
        float z0 = transition_length * t;
        float y0 = transition_scale * t * t * z0;

        float x1 = r1;
        float z1 = transition_length * t;
        float y1 = transition_scale * t * t * z0;

        points[points.length] = V3(x0, height * y0, z0);
        points[points.length] = V3(x1, height * y1, z1);

        float bx0 = r0 - border_width;
        float bz0 = transition_length * t;

        float bx1 = r1 + border_width;
        float bz1 = transition_length * t;

        points[points.length] = V3(x0, height * y0 + border_height, z0);
        points[points.length] = V3(bx0, height * y0 + border_height, bz0);
        points[points.length] = V3(bx0, border_ground_y, bz0);

        points[points.length] = V3(x1, height * y1 + border_height, z1);
        points[points.length] = V3(bx1, height * y1 + border_height, bz1);
        points[points.length] = V3(bx1, border_ground_y, bz1);
    }

    for (int i = 0; i < N; i = i + 1) {
        float t = i / (N - 1);

        float theta = theta0 + (theta1 - theta0) * t;
        float c = cos(theta);
        float s = sin(theta);

        float x0 = r0 * c;
        float z0 = r0 * s + transition_length;

        float x1 = r1 * c;
        float z1 = r1 * s + transition_length;

        float y0 = z0 - transition_length + (transition_length * transition_scale);
        float y1 = z1 - transition_length + (transition_length * transition_scale);

        points[points.length] = V3(x0, y0 * height, z0);
        points[points.length] = V3(x1, y1 * height, z1);

        float bx0 = (r0 - border_width) * c;
        float bz0 = (r0 - border_width) * s + transition_length;

        float bx1 = (r1 + border_width) * c;
        float bz1 = (r1 + border_width) * s + transition_length;

        points[points.length] = V3(x0, y0 * height + border_height, z0);
        points[points.length] = V3(bx0, y0 * height + border_height, bz0);
        points[points.length] = V3(bx0, border_ground_y, bz0);

        points[points.length] = V3(x1, y1 * height + border_height, z1);
        points[points.length] = V3(bx1, y1 * height + border_height, bz1);
        points[points.length] = V3(bx1, border_ground_y, bz1);
    }

    for (int i = 0; i < transition_N; i = i + 1) {
        float t = (transition_N - i - 1) / transition_N;

        float x0 = 0-r0;
        float z0 = transition_length * t;
        float y0 = transition_scale * t * t * z0;

        float x1 = 0-r1;
        float z1 = transition_length * t;
        float y1 = transition_scale * t * t * z0;

        points[points.length] = V3(x0, height * y0, z0);
        points[points.length] = V3(x1, height * y1, z1);

        float bx0 = 0 - (r0 - border_width);
        float bz0 = transition_length * t;

        float bx1 = 0 - (r1 + border_width);
        float bz1 = transition_length * t;

        points[points.length] = V3(x0, height * y0 + border_height, z0);
        points[points.length] = V3(bx0, height * y0 + border_height, bz0);
        points[points.length] = V3(bx0, border_ground_y, bz0);

        points[points.length] = V3(x1, height * y1 + border_height, z1);
        points[points.length] = V3(bx1, height * y1 + border_height, bz1);
        points[points.length] = V3(bx1, border_ground_y, bz1);
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }


    list idx = [];
    list uv = [];

    float tex_scale = 0.5;

    float total_length = 0;
    for (int i = 0; i < N + 2 * transition_N - 1; i = i + 1) {
        idx[0] = 8 * (i + 0) + 1;           
        idx[1] = 8 * (i + 0) + 0;           
        idx[2] = 8 * (i + 1) + 0;           
        idx[3] = 8 * (i + 1) + 1;           

        vec3 p0 = points[idx[0]];
        vec3 p1 = points[idx[1]];
        vec3 p2 = points[idx[2]];
        vec3 p3 = points[idx[3]];
        float dist = vec3_distance(p1, p2);

        total_length = total_length + dist;
    }

    float length = 0;
    for (int i = 0; i < N + 2 * transition_N - 1; i = i + 1) {
        idx[0] = 8 * (i + 0) + 1;           
        idx[1] = 8 * (i + 0) + 0;           
        idx[2] = 8 * (i + 1) + 0;           
        idx[3] = 8 * (i + 1) + 1;           

        vec3 p0 = points[idx[0]];
        vec3 p1 = points[idx[1]];
        vec3 p2 = points[idx[2]];
        vec3 p3 = points[idx[3]];
        float dist = vec3_distance(p1, p2);

        float c0 = tex_scale * texture_length * length / total_length;
        float c1 = tex_scale * texture_length * (length + dist) / total_length;

        uv[0] = V2(tex_scale * (r1 - r0) + texture_dx, c0);
        uv[1] = V2(texture_dx, c0);
        uv[2] = V2(texture_dx, c1);
        uv[3] = V2(tex_scale * (r1 - r0) + texture_dx, c1);

        terrain_model_add_face("ground", idx, uv);

        idx[0] = 8 * (i + 0) + 0;
        idx[1] = 8 * (i + 0) + 2;
        idx[2] = 8 * (i + 1) + 2;
        idx[3] = 8 * (i + 1) + 0;

        uv[0] = V2(0.5, c0);
        uv[1] = V2(0, c0);
        uv[2] = V2(0, c1);
        uv[3] = V2(0.5, c1);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 2;
        idx[1] = 8 * (i + 0) + 3;
        idx[2] = 8 * (i + 1) + 3;
        idx[3] = 8 * (i + 1) + 2;

        uv[0] = V2(0, c0);
        uv[1] = V2(0.5, c0);
        uv[2] = V2(0.5, c1);
        uv[3] = V2(0, c1);

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 0) + 3;
        idx[1] = 8 * (i + 0) + 4;
        idx[2] = 8 * (i + 1) + 4;
        idx[3] = 8 * (i + 1) + 3;

        uv[0] = V2(0, c0);
        uv[1] = V2(tex_scale * (p1.y - border_ground_y), c0);
        uv[2] = V2(tex_scale * (p2.y - border_ground_y), c1);
        uv[3] = V2(0, c1);

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 5;
        idx[1] = 8 * (i + 0) + 1;
        idx[2] = 8 * (i + 1) + 1;
        idx[3] = 8 * (i + 1) + 5;

        uv[0] = V2(0, c0);
        uv[1] = V2(0.5, c0);
        uv[2] = V2(0.5, c1);
        uv[3] = V2(0, c1);

        terrain_model_add_face("wall-side", idx, uv);

        idx[3] = 8 * (i + 0) + 5;
        idx[2] = 8 * (i + 0) + 6;
        idx[1] = 8 * (i + 1) + 6;
        idx[0] = 8 * (i + 1) + 5;

        uv[3] = V2(0, c0);
        uv[2] = V2(0.5, c0);
        uv[1] = V2(0.5, c1);
        uv[0] = V2(0, c1);

        terrain_model_add_face("wall-top", idx, uv);

        idx[3] = 8 * (i + 0) + 6;
        idx[2] = 8 * (i + 0) + 7;
        idx[1] = 8 * (i + 1) + 7;
        idx[0] = 8 * (i + 1) + 6;

        uv[3] = V2(0, c0);
        uv[2] = V2(tex_scale * (p1.y - border_ground_y), c0);
        uv[1] = V2(tex_scale * (p2.y - border_ground_y), c1);
        uv[0] = V2(0, c1);

        terrain_model_add_face("wall-side", idx, uv);

        length = length + dist;
    }
}
