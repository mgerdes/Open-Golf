void generate(float N, float radius, float width, float height) {
    terrain_model_add_point(V3(0, 0, 0));
    terrain_model_add_point(V3(width, 0, 0));
    terrain_model_add_point(V3(width, 0, height));
    terrain_model_add_point(V3(0, 0, height));

    for (int i = 0; i < N; i = i + 1) {
        float a = 2 * PI * i / N;

        float x = (radius + 0.3) * cos(a) + 0.5 * width;
        float y = 0;
        float z = (radius + 0.3) * sin(a) + 0.5 * height;
        terrain_model_add_point(V3(x, y, z));
    }

    for (int i = 0; i < N; i = i + 1) {
        float a = 2 * PI * i / N;

        float x = (radius + 0.15) * cos(a) + 0.5 * width;
        float y = 0;
        float z = (radius + 0.15) * sin(a) + 0.5 * height;
        terrain_model_add_point(V3(x, y, z));
    }

    for (int i = 0; i < N; i = i + 1) {
        float a = 2 * PI * i / N;

        float x = (radius - 0.15) * cos(a) + 0.5 * width;
        float y = 0;
        float z = (radius - 0.15) * sin(a) + 0.5 * height;
        terrain_model_add_point(V3(x, y, z));
    }

    for (int i = 0; i < N; i = i + 1) {
        float a = 2 * PI * i / N;

        float x = (radius - 0.3) * cos(a) + 0.5 * width;
        float y = 0;
        float z = (radius - 0.3) * sin(a) + 0.5 * height;
        terrain_model_add_point(V3(x, y, z));
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
            idx0 = 2;
        }
        else if (a < 0.5) {
            idx0 = 3;
        }
        else if (a < 0.75) {
            idx0 = 0;
        }
        else {
            idx0 = 1;
        }

        terrain_model_add_face(idx0, idx1, idx2);
    }

    {
        int idx0 = 2;
        int idx1 = 1;
        int idx2 = 4;
        terrain_model_add_face(idx0, idx1, idx2);
    }

    {
        int idx0 = 3;
        int idx1 = 2;
        int idx2 = 4 + 0.25 * N;
        terrain_model_add_face(idx0, idx1, idx2);
    }

    {
        int idx0 = 0;
        int idx1 = 3;
        int idx2 = 4 + 0.5 * N;
        terrain_model_add_face(idx0, idx1, idx2);
    }

    {
        int idx0 = 1;
        int idx1 = 0;
        int idx2 = 4 + 0.75 * N;
        terrain_model_add_face(idx0, idx1, idx2);
    }

    for (int i = 0; i < N; i = i + 1) {
        int idx0 = 4 + i;
        int idx1 = 4 + N + i;
        int idx2 = 4 + N + i + 1;
        int idx3 = 4 + i + 1;
        if (i == N - 1) {
            idx2 = 4 + N;
            idx3 = 4;
        }
        terrain_model_add_face(idx0, idx1, idx2, idx3);
    }

    for (int i = 0; i < N; i = i + 1) {
        int idx0 = 4 + N + i;
        int idx1 = 4 + 2 * N + i;
        int idx2 = 4 + 2 * N + i + 1;
        int idx3 = 4 + N + i + 1;
        if (i == N - 1) {
            idx2 = 4 + 2 * N;
            idx3 = 4 + N;
        }
        terrain_model_add_face(idx0, idx1, idx2, idx3);
    }

    for (int i = 0; i < N; i = i + 1) {
        int idx0 = 4 + 2 * N + i;
        int idx1 = 4 + 3 * N + i;
        int idx2 = 4 + 3 * N + i + 1;
        int idx3 = 4 + 2 * N + i + 1;
        if (i == N - 1) {
            idx2 = 4 + 3 * N;
            idx3 = 4 + 2 * N;
        }
        terrain_model_add_face(idx0, idx1, idx2, idx3);
    }
}
