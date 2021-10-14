@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs water_ripple_vs
uniform water_ripple_vs_params {
    mat4 mvp_mat;
};

in vec3 position;
in vec2 texture_coord;

out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = mvp_mat * vec4(position, 1.0);
}
@end

@fs water_ripple_fs
uniform water_ripple_fs_params {
    float t;
    vec4 uniform_color;
};
uniform sampler2D water_ripple_noise_tex;

in vec2 frag_texture_coord;

out vec4 g_frag_color;

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
@end

@program water_ripple water_ripple_vs water_ripple_fs
