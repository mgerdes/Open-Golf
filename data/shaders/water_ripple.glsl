//@begin_vert
#version 450

layout(binding = 0) uniform vs_params {
    mat4 mvp_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texture_coord;

layout(location = 0) out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = mvp_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450

layout(binding = 0) uniform fs_params {
    float t;
    vec4 uniform_color;
};
layout(binding = 0) uniform sampler2D water_ripple_noise_tex;

layout(location = 0) in vec2 frag_texture_coord;

layout(location = 0) out vec4 g_frag_color;

void main() {
    vec2 tc = vec2(frag_texture_coord.x, frag_texture_coord.y);
    float noise = texture(water_ripple_noise_tex, 0.3*tc + vec2(0.2*t, 0.2*t)).r;
    float l = length(vec2(tc.x - 0.5, tc.y - 0.5));
    vec3 color = vec3(31.0/255.0, 154.0/255.0, 173.0/255.0);

    float alpha = 1.0;
    float r0 = 0.05 + 0.4*t;
    float r1 = 0.08 + 0.4*t;
    if (l > r0 && l < r1) {
        color = uniform_color.xyz;
        alpha = 0.5;

        if (t > 0.8) {
            alpha = 0.5 * (1.0 - t) / 0.2;
        }
    }
    else {
        color = (0.5+0.5*noise)*vec3(0.8, 0.8, 0.8);
        alpha = 0.5*t*(0.5 - l);
    }

    g_frag_color = vec4(color, alpha);
}
//@end
