void generate(float N, float width, float height, float length) {
    list idx = [];
    list uv = [];
    for (int i = 0; i <= N; i = i + 1) {
        float angle = PI * i / N;

        float x = cos(angle);
        float y = sin(angle);
        terrain_model_add_point(V3(0, height * y, length * x));
        terrain_model_add_point(V3(width, height * y, length * x));

        if (i > 0) {
            idx[0] = 2 * (i - 1);
            idx[1] = 2 * (i - 1) + 1;
            idx[2] = 2 * i + 1;
            idx[3] = 2 * i;

            uv[0] = 0;
            uv[1] = 0;
            uv[2] = 0;
            uv[3] = 0;

            terrain_model_add_face("default", idx, uv);
        }
    }
}
