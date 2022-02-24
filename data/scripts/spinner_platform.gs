void generate(float N, float radius, float width, float height) {
    terrain_model_add_point(0, 0, 0);
    terrain_model_add_point(width, 0, 0);
    terrain_model_add_point(width, 0, height);
    terrain_model_add_point(0, 0, height);

    for (int i = 0; i < N; i = i + 1) {
        float a = 2 * PI * i / N;
        float x = r * cos(a);
        float y = 0;
        float z = r * sin(a);
        terrain_model_add_point(x, y, z);
    }

    for (int i = 0; i < N; i = i + 1) {
        int idx0 = 0;
        int idx1 = 4 + i;
        int idx2 = 4 + i + 1;
        if (i == N - 1) {
            idx2 = 4;
        }

        float a = i / N;
        if (a < 0.25) {
            idx0 = 0;
        }
        else if (a < 0.5) {
            idx1 = 1;
        }
        else if (a < 0.75) {
            idx2 = 2;
        }
        else {
            idx3 = 3;
        }

        terrain_model_add_face(idx0, idx1, idx2);

        print(i);
    }
}
