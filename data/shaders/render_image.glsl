@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs render_image_vs
in vec3 position;
in vec2 texture_coord;

out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = vec4(position, -1);
}
@end

@fs render_image_fs
uniform sampler2D render_image_texture;

in vec2 frag_texture_coord;

out vec4 g_frag_color;

void main() {
    g_frag_color = texture(render_image_texture, frag_texture_coord);
}
@end

@program render_image render_image_vs render_image_fs
