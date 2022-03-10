void generate(float N, float width, float height, float length, float theta0, float uv_x0) {
    list idx = [];
    list uv = [];
    float y0 = height * sin(theta0);

    for (int i = 0; i <= N; i = i + 1) {
        float angle = theta0 + (PI - 2 * theta0) * i / N;

        float x = cos(angle);
        float y = sin(angle);
        terrain_model_add_point(V3(0, height * y - y0, length * x));
        terrain_model_add_point(V3(width, height * y - y0, length * x));

        if (i > 0) {
            idx[0] = 2 * (i - 1);
            idx[1] = 2 * (i - 1) + 1;
            idx[2] = 2 * i + 1;
            idx[3] = 2 * i;

            uv[0] = V2(uv_x0, 0.5 * (i - 1) / N);
            uv[1] = V2(width + uv_x0, 0.5 * (i - 1) / N);
            uv[2] = V2(width + uv_x0, 0.5 * i / N);
            uv[3] = V2(uv_x0, 0.5 * i / N);

            terrain_model_add_face("bump_face", idx, uv);
        }
    }
}
