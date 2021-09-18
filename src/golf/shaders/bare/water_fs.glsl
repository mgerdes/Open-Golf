#version 330

uniform vec4 water_fs_params[1];
uniform sampler2D water_noise_tex1;
uniform sampler2D water_noise_tex0;
uniform sampler2D water_lightmap_tex;

in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    vec4 _55 = texture(water_noise_tex1, vec2(frag_texture_coord.x, 0.100000001490116119384765625 * (frag_texture_coord.y + (0.800000011920928955078125 * water_fs_params[0].x))));
    vec3 color = vec3(0.0039215688593685626983642578125, 0.4862745106220245361328125, 0.56078433990478515625);
    if (texture(water_noise_tex0, vec2(frag_texture_coord.x, frag_texture_coord.y + (0.0625 * water_fs_params[0].x))).w > 0.100000001490116119384765625)
    {
        color = vec3(0.60000002384185791015625);
    }
    if (_55.w > 0.100000001490116119384765625)
    {
        color = vec3(0.800000011920928955078125);
    }
    if (frag_texture_coord.x > 0.949999988079071044921875)
    {
        float _94 = (frag_texture_coord.x - 0.949999988079071044921875) * 20.0;
        color = (vec3(1.0) * _94) + (color * (1.0 - _94));
    }
    if (frag_texture_coord.x < 0.0500000007450580596923828125)
    {
        float _113 = (0.0500000007450580596923828125 - frag_texture_coord.x) * 20.0;
        color = (vec3(1.0) * _113) + (color * (1.0 - _113));
    }
    g_frag_color = vec4(color * texture(water_lightmap_tex, frag_lightmap_uv).x, 0.85000002384185791015625);
}

