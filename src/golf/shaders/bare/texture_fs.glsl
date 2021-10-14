#version 330

uniform sampler2D texture_image;

in vec2 frag_texture_coord;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    vec2 _39 = vec2(1.0 / float(textureSize(texture_image, 0).x), 1.0 / float(textureSize(texture_image, 0).y));
    vec4 _55 = vec4(frag_texture_coord, frag_texture_coord - (_39 * 0.75));
    vec2 _61 = _55.zw;
    vec2 _98 = _55.xy;
    float _109 = dot(textureLod(texture_image, _61, 0.0).xyz, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    float _113 = dot(textureLod(texture_image, _61 + (vec2(1.0, 0.0) * _39), 0.0).xyz, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    float _117 = dot(textureLod(texture_image, _61 + (vec2(0.0, 1.0) * _39), 0.0).xyz, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    float _121 = dot(textureLod(texture_image, _61 + _39, 0.0).xyz, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    float _125 = dot(textureLod(texture_image, _98, 0.0).xyz, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    float _149 = _109 + _113;
    float _154 = -(_149 - (_117 + _121));
    vec2 dir;
    vec2 _272 = dir;
    _272.x = _154;
    float _162 = (_109 + _117) - (_113 + _121);
    vec2 _274 = _272;
    _274.y = _162;
    vec2 _197 = min(vec2(8.0), max(vec2(-8.0), _274 * (1.0 / (min(abs(_154), abs(_162)) + max(((_149 + _117) + _121) * 0.03125, 0.0078125))))) * _39;
    dir = _197;
    vec3 _219 = (textureLod(texture_image, _98 + (_197 * (-0.16666667163372039794921875)), 0.0).xyz + textureLod(texture_image, _98 + (_197 * 0.16666667163372039794921875), 0.0).xyz) * 0.5;
    vec3 _243 = (_219 * 0.5) + ((textureLod(texture_image, _98 + (_197 * (-0.5)), 0.0).xyz + textureLod(texture_image, _98 + (_197 * 0.5), 0.0).xyz) * 0.25);
    float _247 = dot(_243, vec3(0.2989999949932098388671875, 0.58700001239776611328125, 0.114000000059604644775390625));
    if ((_247 < min(_125, min(min(_109, _113), min(_117, _121)))) || (_247 > max(_125, max(max(_109, _113), max(_117, _121)))))
    {
        g_frag_color = vec4(_219, 1.0);
    }
    else
    {
        g_frag_color = vec4(_243, 1.0);
    }
}

