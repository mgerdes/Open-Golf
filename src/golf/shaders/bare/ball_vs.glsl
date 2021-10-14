#version 330

uniform vec4 ball_vs_params[8];
layout(location = 0) in vec3 position;
out vec3 frag_normal;
layout(location = 1) in vec3 normal;
out vec2 frag_texture_coord;
layout(location = 2) in vec2 texture_coord;

void main()
{
    mat4 _26 = mat4(ball_vs_params[4], ball_vs_params[5], ball_vs_params[6], ball_vs_params[7]);
    gl_Position = (mat4(ball_vs_params[0], ball_vs_params[1], ball_vs_params[2], ball_vs_params[3]) * _26) * vec4(position, 1.0);
    frag_normal = normalize((transpose(inverse(_26)) * vec4(normal, 0.0)).xyz);
    frag_texture_coord = texture_coord;
}

