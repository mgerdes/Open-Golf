#version 330

uniform vec4 water_vs_params[8];
out vec2 frag_texture_coord;
layout(location = 1) in vec2 texture_coord;
layout(location = 0) in vec3 position;

void main()
{
    frag_texture_coord = texture_coord;
    gl_Position = (mat4(water_vs_params[0], water_vs_params[1], water_vs_params[2], water_vs_params[3]) * mat4(water_vs_params[4], water_vs_params[5], water_vs_params[6], water_vs_params[7])) * vec4(position, 1.0);
}

