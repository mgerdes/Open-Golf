#version 330

uniform vec4 ui_fs_params[3];
uniform sampler2D ui_texture;

in vec2 frag_texture_coord;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    vec2 _57 = vec2(ui_fs_params[1].x, ui_fs_params[1].y) + (frag_texture_coord * vec2(ui_fs_params[1].z, ui_fs_params[1].w));
    vec2 _109 = _57;
    _109.x = _57.x / float(textureSize(ui_texture, 0).x);
    vec2 _112 = _109;
    _112.y = _57.y / float(textureSize(ui_texture, 0).y);
    g_frag_color = texture(ui_texture, _112);
    float _83 = 1.0 - ui_fs_params[2].x;
    g_frag_color.w = (ui_fs_params[2].x * g_frag_color.x) + (_83 * g_frag_color.w);
    vec3 _104 = (g_frag_color.xyz * _83) + (ui_fs_params[0].xyz * ui_fs_params[2].x);
    g_frag_color = vec4(_104.x, _104.y, _104.z, g_frag_color.w);
}

