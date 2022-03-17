void generate(float N, float r0, float r1, float a, float texture_length, float texture_dx, float border_height, float border_width, float border_ground_y) {
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

        points[points.length] = V3(r0 * c, border_height, r0 * s);
        points[points.length] = V3((r0 - border_width) * c, border_height, (r0 - border_width) * s);
        points[points.length] = V3((r0 - border_width) * c, border_ground_y, (r0 - border_width) * s);

        points[points.length] = V3(r1 * c, border_height, r1 * s);
        points[points.length] = V3((r1 + border_width) * c, border_height, (r1 + border_width) * s);
        points[points.length] = V3((r1 + border_width) * c, border_ground_y, (r1 + border_width) * s);
    }

    for (int i = 0; i < points.length; i = i + 1) {
        terrain_model_add_point(points[i]);
    }

    list idx = [];
    list uv = [];

    for (int i = 0; i < N - 1; i = i + 1) {
        idx[0] = 8 * (i + 0) + 1;
        idx[1] = 8 * (i + 0) + 0;
        idx[2] = 8 * (i + 1) + 0;
        idx[3] = 8 * (i + 1) + 1;

        vec3 p0 = points[idx[0]];
        vec3 p1 = points[idx[1]];
        vec3 p2 = points[idx[2]];
        vec3 p3 = points[idx[3]];

        uv[0] = V2(0.5*p0.x + texture_dx, 0.5*p0.z);
        uv[1] = V2(0.5*p1.x + texture_dx, 0.5*p1.z);
        uv[2] = V2(0.5*p2.x + texture_dx, 0.5*p2.z);
        uv[3] = V2(0.5*p3.x + texture_dx, 0.5*p3.z);

        terrain_model_add_face("ground", idx, uv);
    }

    float length0 = 0;
    float length1 = 0;
    for (int i = 0; i < N - 1; i = i + 1) {
        idx[0] = 8 * (i + 0) + 0;
        idx[1] = 8 * (i + 0) + 2;
        idx[2] = 8 * (i + 1) + 2;
        idx[3] = 8 * (i + 1) + 0;

        vec3 p0 = points[idx[0]];
        vec3 p1 = points[idx[1]];
        float dist0 = vec3_distance(p0, p1);

        uv[0] = V2(0.375, 0.5*length0); 
        uv[1] = V2(0, 0.5*length0); 
        uv[2] = V2(0, 0.5*(length0 + dist0)); 
        uv[3] = V2(0.375, 0.5*(length0 + dist0)); 

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 0) + 2;
        idx[1] = 8 * (i + 0) + 3;
        idx[2] = 8 * (i + 1) + 3;
        idx[3] = 8 * (i + 1) + 2;

        uv[0] = V2(0, 0.5*length0); 
        uv[1] = V2(0.5, 0.5*length0); 
        uv[2] = V2(0.5, 0.5*(length0 + dist0)); 
        uv[3] = V2(0, 0.5*(length0 + dist0)); 

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 0) + 3;
        idx[1] = 8 * (i + 0) + 4;
        idx[2] = 8 * (i + 1) + 4;
        idx[3] = 8 * (i + 1) + 3;

        uv[0] = V2(0, 0.5*(length0 + 1)); 
        uv[1] = V2(border_height - border_ground_y, 0.5*(length0 + 1)); 
        uv[2] = V2(border_height - border_ground_y, 0.5*(length0 + dist0 + 1)); 
        uv[3] = V2(0, 0.5*(length0 + dist0 + 1)); 

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 1) + 1;
        idx[1] = 8 * (i + 1) + 5;
        idx[2] = 8 * (i + 0) + 5;
        idx[3] = 8 * (i + 0) + 1;

        p0 = points[idx[3]];
        p1 = points[idx[0]];
        float dist1 = vec3_distance(p0, p1);

        uv[0] = V2(0.375, 0.5*(length1 + dist1)); 
        uv[1] = V2(0, 0.5*(length1 + dist1)); 
        uv[2] = V2(0, 0.5*length1); 
        uv[3] = V2(0.375, 0.5*length1); 

        terrain_model_add_face("wall-side", idx, uv);

        idx[0] = 8 * (i + 1) + 5;
        idx[1] = 8 * (i + 1) + 6;
        idx[2] = 8 * (i + 0) + 6;
        idx[3] = 8 * (i + 0) + 5;

        uv[0] = V2(0, 0.5*(length1 + dist1)); 
        uv[1] = V2(0.5, 0.5*(length1 + dist1)); 
        uv[2] = V2(0.5, 0.5*length1); 
        uv[3] = V2(0, 0.5*length1); 

        terrain_model_add_face("wall-top", idx, uv);

        idx[0] = 8 * (i + 1) + 6;
        idx[1] = 8 * (i + 1) + 7;
        idx[2] = 8 * (i + 0) + 7;
        idx[3] = 8 * (i + 0) + 6;

        uv[0] = V2(0, 0.5*(length1 + dist1)); 
        uv[1] = V2(border_height - border_ground_y, 0.5*(length1 + dist1)); 
        uv[2] = V2(border_height - border_ground_y, 0.5*length1); 
        uv[3] = V2(0, 0.5*length1); 

        terrain_model_add_face("wall-side", idx, uv);

        length0 = length0 + dist0;
        length1 = length1 + dist1;
    }
}
