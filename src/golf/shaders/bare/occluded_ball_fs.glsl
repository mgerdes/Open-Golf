#version 330

uniform vec4 occluded_ball_fs_params[2];
in vec3 frag_normal;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    float _37 = min(max(dot(frag_normal, normalize(occluded_ball_fs_params[1].xyz - occluded_ball_fs_params[0].xyz)), 0.0), 1.0);
    float _43 = 0.20000000298023223876953125 + (0.5 * _37);
    g_frag_color = vec4(_43, _43, _43, 0.100000001490116119384765625 + (0.5 * (1.0 - _37)));
}

