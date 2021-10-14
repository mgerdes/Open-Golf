#version 330

uniform vec4 cup_vs_params[8];
out vec2 frag_lightmap_uv;
layout(location = 1) in vec2 lightmap_uv;
layout(location = 0) in vec3 position;

void main()
{
    frag_lightmap_uv = lightmap_uv;
    gl_Position = (mat4(cup_vs_params[0], cup_vs_params[1], cup_vs_params[2], cup_vs_params[3]) * mat4(cup_vs_params[4], cup_vs_params[5], cup_vs_params[6], cup_vs_params[7])) * vec4(position, 1.0);
}

