void generate(float N, float inner_radius, float width, float height, float texture_dx, float texture_length) {
    list points = [];

    float inner_length = 0.5 * PI * inner_radius;
    float outer_length = 0.5 * PI * (inner_radius + width);
    for (int i = 0; i < N; i = i + 1) {
        float t0 = i / (N - 1);
        float theta0 = 0.5 * PI * t0;

        for (int j = 0; j < N; j = j + 1) {
            float t1 = j / (N - 1);
            float theta1 = 0.65 * PI * t1;
            float a = t0 * 2;
            if (a > 1) {
                a = 2 - a;
            }
            a = 1 - a;
            a = 1 - cos(0.5 * PI * a);
            float r0 = inner_radius + (width * sin(theta1));
            float x0 = r0 * cos(theta0);
            float y0 = (1 - a) * height * 2 * (1 - cos(theta1));
            float z0 = r0 * sin(theta0);

            float r1 = inner_radius + width * t1;
            float x1 = r1 * cos(theta0);
            float y1 = 0;
            float z1 = r1 * sin(theta0);

            float r = r0 + (r1 - r0) * a;
            float x = x0 + (x1 - x0) * a;
            float y = y0 + (y1 - y0) * a;
            float z = z0 + (z1 - z0) * a;

            if (j == 0) {
                float border_radius = r1 - 0.5;
                float border_x = border_radius * cos(theta0);
                float border_z = border_radius * sin(theta0);

                vec3 p0 = V3(border_x, 0-5, border_z);
                vec3 p1 = V3(border_x, y0 + 0.375, border_z);
                vec3 p2 = V3(x, y0 + 0.375, z);

                points[points.length] = p0;
                points[points.length] = p1;
                points[points.length] = p2;
            }

            vec3 p = V3(x, y, z);
            points[points.length] = p;

            if (j == (N - 1)) {
                float border_radius = r1 + 0.5;
                float border_x = border_radius * cos(theta0);
                float border_z = border_radius * sin(theta0);

                vec3 p0 = V3(x, y + 0.375, z);
                vec3 p1 = V3(border_x, y + 0.375, border_z);
                vec3 p2 = V3(border_x, 0-3, border_z);
                points[points.length] = p0;
                points[points.length] = p1;
                points[points.length] = p2;
            }
        }
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }

    list idx = [];
    list uv = [];

    // Border 1
    for (int i = 0; i < (N - 1); i = i + 1) {
        int a = (i + 0) * (N + 6) + 0;
        int b = (i + 1) * (N + 6) + 0;
        int c = (i + 1) * (N + 6) + 1;
        int d = (i + 0) * (N + 6) + 1;
        float tc0x = 0;
        float tc1x = 3.25;
        float tc0y = (i / N) * inner_length;
        float tc1y = ((i + 1) / N) * inner_length;

        idx[0] = a;
        idx[1] = b;
        idx[2] = c;
        idx[3] = d;

        uv[0] = V2(0.5*tc1x, 0.5*tc0y);
        uv[1] = V2(0.5*tc1x, 0.5*tc1y);
        uv[2] = V2(0.5*tc0x, 0.5*tc1y);
        uv[3] = V2(0.5*tc0x, 0.5*tc0y);

        terrain_model_add_face("wall-top", idx, uv);
    }

    // Border 2
    for (int i = 0; i < (N - 1); i = i + 1) {
        int a = (i + 0) * (N + 6) + 1;
        int b = (i + 1) * (N + 6) + 1;
        int c = (i + 1) * (N + 6) + 2;
        int d = (i + 0) * (N + 6) + 2;
        float tc0x = 0;
        float tc1x = 1;
        float tc0y = (i / N) * inner_length;
        float tc1y = ((i + 1) / N) * inner_length;

        idx[0] = a;
        idx[1] = b;
        idx[2] = c;
        idx[3] = d;

        uv[0] = V2(0.5*tc0x, 0.5*tc0y);
        uv[1] = V2(0.5*tc0x, 0.5*tc1y);
        uv[2] = V2(0.5*tc1x, 0.5*tc1y);
        uv[3] = V2(0.5*tc1x, 0.5*tc0y);

        terrain_model_add_face("wall-top", idx, uv);
    }

    // Border 3
    for (int i = 0; i < (N - 1); i = i + 1) {
        int a = (i + 0) * (N + 6) + 2;
        int b = (i + 1) * (N + 6) + 2;
        int c = (i + 1) * (N + 6) + 3;
        int d = (i + 0) * (N + 6) + 3;
        float tc0x = 0;
        float tc1x = 1;
        float tc0y = (i / N) * inner_length;
        float tc1y = ((i + 1) / N) * inner_length;

        idx[0] = a;
        idx[1] = b;
        idx[2] = c;
        idx[3] = d;

        uv[0] = V2(0.25*tc0x, 0.5*tc0y);
        uv[1] = V2(0.25*tc0x, 0.5*tc1y);
        uv[2] = V2(0.25*tc1x, 0.5*tc1y);
        uv[3] = V2(0.25*tc1x, 0.5*tc0y);

        terrain_model_add_face("wall-side", idx, uv);
    }

    // Border 4
    for (int i = 0; i < (N - 1); i = i + 1) {
        int a = (i + 0) * (N + 6) + N + 2;
        int b = (i + 1) * (N + 6) + N + 2;
        int c = (i + 1) * (N + 6) + N + 3;
        int d = (i + 0) * (N + 6) + N + 3;
        float tc0x = 0;
        float tc1x = 1;
        float tc0y = (i / N) * inner_length;
        float tc1y = ((i + 1) / N) * inner_length;

        idx[0] = a;
        idx[1] = b;
        idx[2] = c;
        idx[3] = d;

        uv[0] = V2(0.25*tc1x, 0.5*tc0y);
        uv[1] = V2(0.25*tc1x, 0.5*tc1y);
        uv[2] = V2(0.25*tc0x, 0.5*tc1y);
        uv[3] = V2(0.25*tc0x, 0.5*tc0y);

        terrain_model_add_face("wall-side", idx, uv);
    }

    // Border 5
    for (int i = 0; i < (N - 1); i = i + 1) {
        int a = (i + 0) * (N + 6) + N + 3;
        int b = (i + 1) * (N + 6) + N + 3;
        int c = (i + 1) * (N + 6) + N + 4;
        int d = (i + 0) * (N + 6) + N + 4;
        float tc0x = 0;
        float tc1x = 1;
        float tc0y = (i / N) * inner_length;
        float tc1y = ((i + 1) / N) * inner_length;

        idx[0] = a;
        idx[1] = b;
        idx[2] = c;
        idx[3] = d;

        uv[0] = V2(0.5*tc0x, 0.5*tc0y);
        uv[1] = V2(0.5*tc0x, 0.5*tc1y);
        uv[2] = V2(0.5*tc1x, 0.5*tc1y);
        uv[3] = V2(0.5*tc1x, 0.5*tc0y);

        terrain_model_add_face("wall-top", idx, uv);
    }

    // Border 6
    for (int i = 0; i < (N - 1); i = i + 1) {
        int a = (i + 0) * (N + 6) + N + 4;
        int b = (i + 1) * (N + 6) + N + 4;
        int c = (i + 1) * (N + 6) + N + 5;
        int d = (i + 0) * (N + 6) + N + 5;

        float tc0x = 0;
        float tc1x = 1;
        float tc0y = (i / N) * inner_length;
        float tc1y = ((i + 1) / N) * inner_length;

        float ay = points[a].y;
        float by = points[b].y;
        float cy = points[c].y;
        float dy = points[d].y;

        idx[0] = a;
        idx[1] = b;
        idx[2] = c;
        idx[3] = d;

        uv[0] = V2(0, 0.5*tc0y);
        uv[1] = V2(0, 0.5*tc1y);
        uv[2] = V2(by - cy, 0.5*tc1y);
        uv[3] = V2(ay - dy, 0.5*tc0y);

        terrain_model_add_face("wall-top", idx, uv);
    }

    // Ground
    for (int i = 0; i < (N - 1); i = i + 1) {
        float w0 = 0;
        float w1 = 0;
        for (int j = 0; j < N - 1; j = j + 1) {
            int a = (i + 0) * (N + 6) + (j + 0) + 3;
            int b = (i + 1) * (N + 6) + (j + 0) + 3;
            int c = (i + 1) * (N + 6) + (j + 1) + 3;
            int d = (i + 0) * (N + 6) + (j + 1) + 3;

            vec3 pa = points[a];
            vec3 pb = points[b];
            vec3 pc = points[c];
            vec3 pd = points[d];

            float w_dist0 = vec3_distance(pa, pd);
            float w_dist1 = vec3_distance(pb, pc);

            float tj0 = (j + 0) / (N - 1);
            float tj1 = (j + 1) / (N - 1);
            float ti0 = (i + 0) / (N - 1);
            float ti1 = (i + 1) / (N - 1);

            float atcx = texture_dx + w0;
            float btcx = texture_dx + w1;
            float ctcx = texture_dx + w1 + w_dist1;
            float dtcx = texture_dx + w0 + w_dist0;

            w0 = w0 + w_dist0;
            w1 = w1 + w_dist1;

            float atcy = texture_length * ti0;
            float btcy = texture_length * ti1;
            float ctcy = texture_length * ti1;
            float dtcy = texture_length * ti0;

            idx[0] = a;
            idx[1] = b;
            idx[2] = c;
            idx[3] = d;

            uv[0] = V2(0.5 * atcx, 0.5 * atcy);
            uv[1] = V2(0.5 * btcx, 0.5 * btcy);
            uv[2] = V2(0.5 * ctcx, 0.5 * ctcy);
            uv[3] = V2(0.5 * dtcx, 0.5 * dtcy);

            terrain_model_add_face("ground", idx, uv);
        }
    }
}
