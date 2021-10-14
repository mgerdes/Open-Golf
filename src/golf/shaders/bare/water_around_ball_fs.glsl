#version 330

uniform vec4 water_around_ball_fs_params[1];
uniform sampler2D water_around_ball_noise_tex;

in vec2 frag_texture_coord;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    vec4 _48 = texture(water_around_ball_noise_tex, (vec2(frag_texture_coord.x, frag_texture_coord.y) * 0.5) + vec2(0.20000000298023223876953125 * water_around_ball_fs_params[0].x));
    float _49 = _48.x;
    float _58 = length(vec2(frag_texture_coord.x - 0.5, frag_texture_coord.y - 0.5));
    float alpha = 0.800000011920928955078125;
    if (_58 > 0.23999999463558197021484375)
    {
        alpha = 0.0;
    }
    else
    {
        if (_58 > 0.20000000298023223876953125)
        {
            alpha = ((0.23999999463558197021484375 - _58) * 20.0) * _49;
        }
        else
        {
            if (_58 > 0.1599999964237213134765625)
            {
                float _106 = (0.20000000298023223876953125 - _58) * 25.0;
                alpha = 0.800000011920928955078125 * (((1.0 - _106) * _49) + _106);
            }
            else
            {
                alpha = 0.800000011920928955078125;
            }
        }
    }
    g_frag_color = vec4(vec3(0.699999988079071044921875) * (0.89999997615814208984375 + (0.100000001490116119384765625 * _49)), alpha);
}

