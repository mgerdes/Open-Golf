//@begin_vert
#version 450

layout(binding = 0) uniform environment_material_vs_params {
    mat4 model_mat;
    mat4 proj_view_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texturecoord;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec2 lightmap_uv;

layout(location = 0) out vec3 frag_position;
layout(location = 1) out vec2 frag_texturecoord;
layout(location = 2) out vec3 frag_normal;
layout(location = 3) out vec2 frag_lightmap_uv;

void main() {
    frag_position = (model_mat * vec4(position, 1.0)).xyz;
    frag_texturecoord = texturecoord;
    frag_normal = normal;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450

layout(binding = 0) uniform environment_material_fs_params {
    vec4 ball_position;
    float lightmap_texture_a;
    float uv_scale;
};

layout(binding = 0) uniform sampler2D environment_material_texture;
layout(binding = 1) uniform sampler2D environment_material_lightmap_texture0;
layout(binding = 2) uniform sampler2D environment_material_lightmap_texture1;

layout(location = 0) in vec3 frag_position;
layout(location = 1) in vec2 frag_texturecoord;
layout(location = 2) in vec3 frag_normal;
layout(location = 3) in vec2 frag_lightmap_uv;

layout(location = 0) out vec4 g_frag_color;

void main() {
    float gi0 = (1 - lightmap_texture_a) * texture(environment_material_lightmap_texture0, frag_lightmap_uv).x; 
    float gi1 = lightmap_texture_a * texture(environment_material_lightmap_texture1, frag_lightmap_uv).x; 
    float gi = (gi0 + gi1);
    float dist_to_ball = distance(frag_position.xz, ball_position.xz);
    if (dist_to_ball < 0.14 && frag_position.y < ball_position.y) {
        float scale = (0.14 - dist_to_ball) / 0.14;   
        gi -= scale * scale * 0.8;
    }
    gi = max(gi, 0);

    vec3 color = texture(environment_material_texture, uv_scale * frag_texturecoord).xyz; 
    color = color + 0.001 * (frag_normal.xyz + frag_position.xyz);

    g_frag_color = vec4(gi * color, 1.0);
    //g_frag_color *= 0.001;
    //g_frag_color += vec4(frag_lightmap_uv, 0.0, 1.0);
}
//@end
