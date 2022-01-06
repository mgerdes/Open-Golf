void generate(int width, float height, float r, float N, float delta) {
    for (int i = 0; i <= N; i = i + 1) {
        float a = 2 * PI * i / N + 1.5 * PI;
        float x = r * cos(a);
        float y = height * r * sin(a);
        float z = (i / N) * delta;
        terrain_model_add_point(V3(x, y, z + width));
        terrain_model_add_point(V3(x, y, z));
    }

    for (int i = 0; i < N; i = i + 1) {
        float idx0 = 2 * i;
        float idx1 = 2 * i + 1;
        float idx2 = 2 * (i + 1);
        float idx3 = 2 * (i + 1) + 1;
        terrain_model_add_face(idx1, idx0, idx2, idx3);
    }
}
