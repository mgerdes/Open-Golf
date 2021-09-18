#version 330

uniform vec4 aim_icon_vs_params[4];
out float frag_alpha;
layout(location = 1) in float alpha;
layout(location = 0) in vec3 position;

void main()
{
    frag_alpha = alpha;
    gl_Position = mat4(aim_icon_vs_params[0], aim_icon_vs_params[1], aim_icon_vs_params[2], aim_icon_vs_params[3]) * vec4(position, 1.0);
}

