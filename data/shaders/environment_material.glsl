@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs environment_material_vs
uniform environment_material_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

in vec3 position;
in vec2 texturecoord;
in vec3 normal;
in vec2 lightmap_uv;

out vec3 frag_position;
out vec2 frag_texturecoord;
out vec3 frag_normal;
out vec2 frag_lightmap_uv;

void main() {
    frag_position = (model_mat * vec4(position, 1.0)).xyz;
    frag_texturecoord = texturecoord;
    frag_normal = normal;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs environment_material_fs
uniform environment_material_fs_params {
    vec4 ball_position;
    float lightmap_texture_a;
    float uv_scale;
};

uniform sampler2D environment_material_texture;
uniform sampler2D environment_material_lightmap_texture0;
uniform sampler2D environment_material_lightmap_texture1;

in vec3 frag_position;
in vec2 frag_texturecoord;
in vec3 frag_normal;
in vec2 frag_lightmap_uv;

out vec4 g_frag_color;

void main() {
    float gi0 = (1 - lightmap_texture_a) * texture(environment_material_lightmap_texture0, frag_lightmap_uv).x; 
    float gi1 = lightmap_texture_a * texture(environment_material_lightmap_texture1, frag_lightmap_uv).x; 
    float gi = (gi0 + gi1);
    float dist_to_ball = distance(frag_position.xz, ball_position.xz);
    if (dist_to_ball < 0.14 && frag_position.y < ball_position.y) {
        float scale = (0.14 - dist_to_ball) / 0.14;   
        gi -= scale * scale * 0.8;
    }
    gi = max(gi, 0);

    vec3 color = texture(environment_material_texture, uv_scale * frag_texturecoord).xyz; 
    color = color + 0.001 * (frag_normal.xyz + frag_position.xyz);

    g_frag_color = vec4(gi * color, 1.0);
    //g_frag_color *= 0.001;
    //g_frag_color += vec4(frag_lightmap_uv, 0.0, 1.0);
}
@end

@program environment_material environment_material_vs environment_material_fs
