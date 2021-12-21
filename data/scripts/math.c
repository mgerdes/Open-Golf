typedef struct vec2 {
    float x;
    float y;    
} vec2;

vec2 V2(float x, float y) {
    vec2 v;
    v.x = x;
    v.y = y;
    return v;
};

vec2 vec2_add(vec2 a, struct vec2 b) {
    vec2 v;
    v.x = a.x + b.x;
    v.y = a.y + b.y;
    return v;
};

typedef struct vec3 {
    float x;
    float y;
    float z;
} vec3;

vec3 V3(float x, float y, float z) {
    vec3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
};

vec3 vec3_add(vec3 a, vec3 b) {
    vec3 v;
    v.x = a.x + b.x;
    v.y = a.y + b.y;
    v.z = a.z + b.z;
    return v;
};
