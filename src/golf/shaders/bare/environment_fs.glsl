#version 330

uniform vec4 environment_fs_params[1];
uniform sampler2D environment_lightmap_tex;
uniform sampler2D environment_material_tex;

in vec2 frag_lightmap_uv;
in vec3 frag_position;
in vec2 frag_texture_coord;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    float ao = texture(environment_lightmap_tex, frag_lightmap_uv).x;
    float _39 = distance(frag_position.xz, environment_fs_params[0].xz);
    bool _43 = _39 < 0.14000000059604644775390625;
    bool _54;
    if (_43)
    {
        _54 = frag_position.y < environment_fs_params[0].y;
    }
    else
    {
        _54 = _43;
    }
    if (_54)
    {
        float _60 = (0.14000000059604644775390625 - _39) * 7.142857074737548828125;
        ao -= ((_60 * _60) * 0.800000011920928955078125);
    }
    g_frag_color = vec4(texture(environment_material_tex, frag_texture_coord).xyz * ao, 1.0);
}

