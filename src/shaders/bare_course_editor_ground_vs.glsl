#version 330

uniform vec4 course_editor_ground_vs_params[8];
out vec3 frag_normal;
layout(location = 1) in vec3 normal;
out vec2 frag_texture_coord;
layout(location = 2) in vec2 texture_coord;
out vec2 frag_lightmap_uv;
layout(location = 3) in vec2 lightmap_uv;
layout(location = 0) in vec3 position;

void main()
{
    mat4 _20 = mat4(course_editor_ground_vs_params[4], course_editor_ground_vs_params[5], course_editor_ground_vs_params[6], course_editor_ground_vs_params[7]);
    frag_normal = (transpose(inverse(_20)) * vec4(normal, 0.0)).xyz;
    frag_normal = normalize(frag_normal);
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = (mat4(course_editor_ground_vs_params[0], course_editor_ground_vs_params[1], course_editor_ground_vs_params[2], course_editor_ground_vs_params[3]) * _20) * vec4(position, 1.0);
}

