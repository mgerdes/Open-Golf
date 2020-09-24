#version 330

uniform vec4 course_editor_water_vs_params[8];
out vec2 frag_texture_coord;
layout(location = 1) in vec2 texture_coord;
layout(location = 0) in vec3 position;

void main()
{
    frag_texture_coord = texture_coord;
    gl_Position = (mat4(course_editor_water_vs_params[0], course_editor_water_vs_params[1], course_editor_water_vs_params[2], course_editor_water_vs_params[3]) * mat4(course_editor_water_vs_params[4], course_editor_water_vs_params[5], course_editor_water_vs_params[6], course_editor_water_vs_params[7])) * vec4(position, 1.0);
}

