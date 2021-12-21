#include <stdio.h>

void generate(int x, vec2 a, vec2 b, vec3 c, float d, int n) {
    for (int i = 0; i < n; i++) {
        printf("%d\n", i);
    }
    printf("RUNNING GENERATE\n");
    printf("x = %d\n", x);
    printf("a = <%f, %f>\n", a.x, a.y);
    printf("b = <%f, %f>\n", b.x, b.y);
    printf("c = <%f, %f, %f>\n", c.x, c.y, c.z);
    printf("d = %f\n", d);
};
