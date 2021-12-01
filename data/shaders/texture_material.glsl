@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs texture_material_vs
uniform texture_material_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

in vec3 position;
in vec2 texturecoord;
in vec3 normal;

out vec3 frag_position;
out vec2 frag_texturecoord;
out vec3 frag_normal;

void main() {
    frag_position = (model_mat * vec4(position, 1.0)).xyz;
    frag_texturecoord = texturecoord;
    frag_normal = normal;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
@end

@fs texture_material_fs
uniform sampler2D texture_material_tex;

in vec3 frag_position;
in vec2 frag_texturecoord;
in vec3 frag_normal;

out vec4 g_frag_color;

void main() {
    vec3 color = texture(texture_material_tex, frag_texturecoord).xyz; 
    color = color + 0.001 * (frag_normal.xyz + frag_position.xyz);
    g_frag_color = vec4(color, 1.0);
}
@end

@program texture_material texture_material_vs texture_material_fs
