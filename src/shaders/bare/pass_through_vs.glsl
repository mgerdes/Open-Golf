#version 330

uniform vec4 pass_through_vs_params[4];
layout(location = 0) in vec3 position;

void main()
{
    gl_Position = mat4(pass_through_vs_params[0], pass_through_vs_params[1], pass_through_vs_params[2], pass_through_vs_params[3]) * vec4(position, 1.0);
}

