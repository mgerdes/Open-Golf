#version 330

uniform vec4 course_editor_terrain_fs_params[3];
uniform sampler2D ce_tex0;
uniform sampler2D ce_tex1;
uniform sampler2D ce_lightmap_tex0;
uniform sampler2D ce_lightmap_tex1;

in vec2 frag_texture_coord;
in vec2 frag_lightmap_uv;
in vec3 frag_color0;
in vec3 frag_color1;
in float frag_material_idx;
in vec3 frag_normal;
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
    float _154 = round(frag_material_idx);
    if (_154 == 0.0)
    {
        color = texture(ce_tex0, frag_texture_coord).xyz;
    }
    else
    {
        if (_154 == 1.0)
        {
            color = texture(ce_tex1, frag_texture_coord).xyz;
        }
    }
    float kd = 0.0;
    for (int i = 0; i < 16; i++)
    {
        kd += max(dot(normalize(frag_normal), -light_dir[i]), 0.4000000059604644775390625);
    }
    kd = (kd * 0.0625) + 0.300000011920928955078125;
    vec4 _215 = texture(ce_lightmap_tex0, frag_lightmap_uv);
    float _216 = _215.x;
    vec4 _221 = texture(ce_lightmap_tex1, frag_lightmap_uv);
    float _235 = _216 + ((_221.x - _216) * course_editor_terrain_fs_params[2].x);
    int _238 = int(course_editor_terrain_fs_params[0].x);
    if (_238 == 0)
    {
        g_frag_color = vec4(color * _235, 1.0);
    }
    else
    {
        if (_238 == 1)
        {
            g_frag_color = vec4(color * kd, 1.0);
        }
        else
        {
            if (_238 == 2)
            {
                g_frag_color = vec4(_235, _235, _235, 1.0);
            }
            else
            {
                if (_238 == 3)
                {
                    g_frag_color = vec4(frag_lightmap_uv, 0.0, 1.0);
                }
                else
                {
                    if (_238 < 7)
                    {
                        g_frag_color = course_editor_terrain_fs_params[1];
                    }
                }
            }
        }
    }
}

