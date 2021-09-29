@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs water_around_ball_vs
uniform water_around_ball_vs_params {
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

@fs water_around_ball_fs
uniform water_around_ball_fs_params {
    float t;
};
uniform sampler2D water_around_ball_noise_tex;

in vec2 frag_texture_coord;

out vec4 g_frag_color;

void main() {
    vec2 tc = vec2(frag_texture_coord.x, frag_texture_coord.y);
    float noise = texture(water_around_ball_noise_tex, 0.5*tc + vec2(0.2*t, 0.2*t)).r;
    float l = length(vec2(tc.x - 0.5, tc.y - 0.5));
    vec3 color = vec3(31.0/255.0, 154.0/255.0, 173.0/255.0);

    float alpha = 0.8;
    color = (0.9+0.1*noise)*vec3(0.7, 0.7, 0.7);
    if (l > 0.24) {
        alpha = 0.0;
    }
    else if (l > 0.20) {
        float a = (0.24 - l)/0.04;
        alpha = 0.8*a*noise;
    }
    else if (l > 0.16) {
        float a = (0.20 - l)/0.04;
        alpha = 0.8*((1.0 - a)*noise + a*1.0);
    }
    else {
        alpha = 0.8;
    }

    g_frag_color = vec4(color, alpha);
}
@end

@program water_around_ball water_around_ball_vs water_around_ball_fs
