#version 330

uniform vec4 single_color_vs_params[8];
out vec3 frag_normal;
layout(location = 1) in vec3 normal;
layout(location = 0) in vec3 position;

void main()
{
    mat4 _20 = mat4(single_color_vs_params[0], single_color_vs_params[1], single_color_vs_params[2], single_color_vs_params[3]);
    frag_normal = normalize((transpose(inverse(_20)) * vec4(normal, 0.0)).xyz);
    gl_Position = (mat4(single_color_vs_params[4], single_color_vs_params[5], single_color_vs_params[6], single_color_vs_params[7]) * _20) * vec4(position, 1.0);
}

