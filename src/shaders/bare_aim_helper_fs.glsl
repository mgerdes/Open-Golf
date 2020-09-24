#version 330

uniform vec4 aim_helper_fs_params[3];
uniform sampler2D aim_helper_image;

in vec2 frag_texture_coord;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    float a = texture(aim_helper_image, (aim_helper_fs_params[1].zw * frag_texture_coord) + aim_helper_fs_params[1].xy).w;
    float _57 = (aim_helper_fs_params[2].x + ((aim_helper_fs_params[2].y - aim_helper_fs_params[2].x) * frag_texture_coord.x)) / aim_helper_fs_params[2].z;
    if (_57 > 0.4000000059604644775390625)
    {
        float _70 = 1.0 - ((_57 - 0.4000000059604644775390625) * 1.66666662693023681640625);
        a = (a * _70) * _70;
    }
    g_frag_color = vec4(aim_helper_fs_params[0].xyz, aim_helper_fs_params[0].w * a);
}

