#version 330

uniform vec4 terrain_vs_params[18];
out vec3 frag_normal;
layout(location = 1) in vec3 normal;
out vec2 frag_texture_coord;
layout(location = 2) in vec2 texture_coord;
out vec2 frag_lightmap_uv;
layout(location = 3) in vec2 lightmap_uv;
out vec3 frag_color0;
layout(location = 4) in float material_idx;
out vec3 frag_color1;
out vec3 frag_position;
layout(location = 0) in vec3 position;
out float frag_material_idx;

void main()
{
    mat4 _24 = mat4(terrain_vs_params[4], terrain_vs_params[5], terrain_vs_params[6], terrain_vs_params[7]);
    frag_normal = (transpose(inverse(_24)) * vec4(normal, 0.0)).xyz;
    frag_normal = normalize(frag_normal);
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    int _53 = int(material_idx);
    frag_color0 = terrain_vs_params[_53 * 1 + 8].xyz;
    frag_color1 = terrain_vs_params[_53 * 1 + 13].xyz;
    vec4 _74 = vec4(position, 1.0);
    frag_position = (_24 * _74).xyz;
    frag_material_idx = material_idx;
    gl_Position = (mat4(terrain_vs_params[0], terrain_vs_params[1], terrain_vs_params[2], terrain_vs_params[3]) * _24) * _74;
}

