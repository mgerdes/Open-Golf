#version 330

uniform vec4 aim_icon_fs_params[1];
layout(location = 0) out vec4 g_frag_color;
in float frag_alpha;

void main()
{
    g_frag_color = vec4(aim_icon_fs_params[0].xyz, aim_icon_fs_params[0].w * frag_alpha);
}

