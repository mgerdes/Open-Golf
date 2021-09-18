#version 330

uniform vec4 ball_fs_params[1];
uniform sampler2D normal_map;

in vec2 frag_texture_coord;
in vec3 frag_normal;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    float _47 = abs(dot(normalize((texture(normal_map, vec2(frag_texture_coord.x, 1.0 - frag_texture_coord.y)).xyz * 2.0) - vec3(1.0)), vec3(0.1881441771984100341796875, 0.282216250896453857421875, 0.940720856189727783203125)));
    float kd0 = _47;
    if (_47 < 0.0)
    {
        kd0 = 0.0;
    }
    float _63 = (normalize(frag_normal).y + 1.0) * 0.5;
    float kd1 = _63;
    if (_63 < 0.300000011920928955078125)
    {
        kd1 = 0.300000011920928955078125;
    }
    g_frag_color = vec4(ball_fs_params[0].xyz * (kd0 * kd1), 1.0);
}

