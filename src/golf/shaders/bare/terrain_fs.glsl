#version 330

uniform vec4 terrain_fs_params[2];
uniform sampler2D mat_tex0;
uniform sampler2D mat_tex1;
uniform sampler2D lightmap_tex0;
uniform sampler2D lightmap_tex1;

in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;
in vec3 frag_color0;
in vec3 frag_color1;
in vec3 frag_normal;
in float frag_material_idx;
in vec3 frag_position;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    vec3 light_dir[16];
    light_dir[0] = vec3(0.521704971790313720703125, -0.81872999668121337890625, 0.23980300128459930419921875);
    light_dir[1] = vec3(-0.64021599292755126953125, -0.699186980724334716796875, 0.3182159960269927978515625);
    light_dir[2] = vec3(-0.15373800694942474365234375, -0.982361018657684326171875, 0.106449000537395477294921875);
    light_dir[3] = vec3(0.739134013652801513671875, -0.4232099950313568115234375, 0.5239989757537841796875);
    light_dir[4] = vec3(0.3420380055904388427734375, -0.902010977268218994140625, -0.263413012027740478515625);
    light_dir[5] = vec3(-0.3218159973621368408203125, -0.422089993953704833984375, 0.847510993480682373046875);
    light_dir[6] = vec3(0.3038940131664276123046875, -0.945710003376007080078125, 0.1152440011501312255859375);
    light_dir[7] = vec3(-0.2738589942455291748046875, -0.896176993846893310546875, 0.349096000194549560546875);
    light_dir[8] = vec3(-0.64784801006317138671875, -0.654807984828948974609375, 0.389254987239837646484375);
    light_dir[9] = vec3(0.109160996973514556884765625, -0.762332975864410400390625, 0.6379129886627197265625);
    light_dir[10] = vec3(0.2267830073833465576171875, -0.2944900095462799072265625, 0.928355991840362548828125);
    light_dir[11] = vec3(0.0166929997503757476806640625, -0.9926459789276123046875, -0.11989299952983856201171875);
    light_dir[12] = vec3(-0.4492549896240234375, -0.4914880096912384033203125, -0.7460629940032958984375);
    light_dir[13] = vec3(0.16172699630260467529296875, -0.1967259943485260009765625, -0.96702802181243896484375);
    light_dir[14] = vec3(0.6336460113525390625, -0.773217976093292236328125, 0.025035999715328216552734375);
    light_dir[15] = vec3(0.62467300891876220703125, -0.720305979251861572265625, 0.30156600475311279296875);
    vec3 color;
    if (((int(frag_texture_coord.x + (9.9999997473787516355514526367188e-06 * frag_lightmap_uv.x)) + int(frag_texture_coord.y + (9.9999997473787516355514526367188e-06 * frag_lightmap_uv.y))) % 2) == 0)
    {
        color = frag_color0;
    }
    else
    {
        color = frag_color1;
    }
    float kd = 0.0;
    for (int i = 0; i < 16; i++)
    {
        kd += max(dot(normalize(frag_normal), -light_dir[i]), 0.100000001490116119384765625);
    }
    kd *= 0.0625;
    float _183 = round(frag_material_idx);
    if (_183 == 0.0)
    {
        color = texture(mat_tex0, frag_texture_coord).xyz;
    }
    else
    {
        if (_183 == 1.0)
        {
            color = texture(mat_tex1, frag_texture_coord).xyz;
        }
    }
    vec4 _212 = texture(lightmap_tex0, frag_lightmap_uv);
    float _213 = _212.x;
    float ao = _213 + ((texture(lightmap_tex1, frag_lightmap_uv).x - _213) * terrain_fs_params[1].x);
    float _241 = distance(frag_position.xz, terrain_fs_params[0].xz);
    bool _244 = _241 < 0.14000000059604644775390625;
    bool _252;
    if (_244)
    {
        _252 = frag_position.y < terrain_fs_params[0].y;
    }
    else
    {
        _252 = _244;
    }
    if (_252)
    {
        float _258 = (0.14000000059604644775390625 - _241) * 7.142857074737548828125;
        ao -= ((_258 * _258) * 0.800000011920928955078125);
    }
    float _266 = ao;
    float _267 = max(_266, 0.0);
    ao = _267;
    g_frag_color = vec4(color * (_267 + (9.9999997473787516355514526367188e-05 * kd)), terrain_fs_params[1].y);
}

