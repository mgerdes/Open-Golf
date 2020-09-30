#version 330

uniform vec4 environment_vs_params[8];
out vec3 frag_position;
layout(location = 0) in vec3 position;
out vec2 frag_texture_coord;
layout(location = 1) in vec2 texture_coord;
out vec2 frag_lightmap_uv;
layout(location = 2) in vec2 lightmap_uv;

void main()
{
    mat4 _20 = mat4(environment_vs_params[0], environment_vs_params[1], environment_vs_params[2], environment_vs_params[3]);
    vec4 _28 = vec4(position, 1.0);
    frag_position = (_20 * _28).xyz;
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = (mat4(environment_vs_params[4], environment_vs_params[5], environment_vs_params[6], environment_vs_params[7]) * _20) * _28;
}

