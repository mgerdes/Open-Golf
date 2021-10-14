#version 330

uniform vec4 water_ripple_fs_params[2];
uniform sampler2D water_ripple_noise_tex;

in vec2 frag_texture_coord;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    vec4 _48 = texture(water_ripple_noise_tex, (vec2(frag_texture_coord.x, frag_texture_coord.y) * 0.300000011920928955078125) + vec2(0.20000000298023223876953125 * water_ripple_fs_params[0].x));
    float _59 = length(vec2(frag_texture_coord.x - 0.5, frag_texture_coord.y - 0.5));
    vec3 color = vec3(0.121568627655506134033203125, 0.603921592235565185546875, 0.67843139171600341796875);
    float alpha = 1.0;
    float _74 = 0.4000000059604644775390625 * water_ripple_fs_params[0].x;
    if ((_59 > (0.0500000007450580596923828125 + _74)) && (_59 < (0.07999999821186065673828125 + _74)))
    {
        color = water_ripple_fs_params[1].xyz;
        alpha = 0.5;
        if (water_ripple_fs_params[0].x > 0.800000011920928955078125)
        {
            alpha = (1.0 - water_ripple_fs_params[0].x) * 2.5;
        }
    }
    else
    {
        color = vec3(0.800000011920928955078125) * (0.5 + (0.5 * _48.x));
        alpha = (0.5 * water_ripple_fs_params[0].x) * (0.5 - _59);
    }
    g_frag_color = vec4(color, alpha);
}

