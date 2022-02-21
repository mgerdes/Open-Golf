//@begin_vert
#version 450
layout(binding = 0) uniform editor_water_vs_params {
    mat4 proj_view_mat;
    mat4 model_mat;
};

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 texture_coord;
layout(location = 2) in vec2 lightmap_uv;

layout(location = 0) out vec2 frag_texture_coord;
layout(location = 1) out vec2 frag_lightmap_uv;

void main() {
    frag_texture_coord = texture_coord;
    frag_lightmap_uv = lightmap_uv;
    gl_Position = proj_view_mat * model_mat * vec4(position, 1.0);
}
//@end

//@begin_frag
#version 450
layout(binding = 0) uniform editor_water_fs_params {
    float draw_type;
    float t;
};
layout(binding = 0) uniform sampler2D editor_water_lightmap_tex;
layout(binding = 1) uniform sampler2D editor_water_noise_tex0, editor_water_noise_tex1;

layout(location = 0) in vec2 frag_texture_coord;
layout(location = 1) in vec2 frag_lightmap_uv;

layout(location = 0) out vec4 g_frag_color;

void main() {
    vec2 tc0 = vec2(frag_texture_coord.x, 0.1 * (frag_texture_coord.y + 16 * t / 100));
    vec2 tc1 = vec2(frag_texture_coord.x, frag_texture_coord.y + t / 100);

    float noise0 = texture(editor_water_noise_tex1, tc0).w;
    float noise1 = texture(editor_water_noise_tex0, tc1).w;

    vec3 color = vec3(1.0/255.0, 124.0/255.0, 143.0/255.0);
    if (noise1 > 0.1) {
        color = vec3(0.6);
    }
    if (noise0 > 0.1) {
        color = vec3(0.8);
    } 
    if (frag_texture_coord.x > 0.95) {
        float a = (frag_texture_coord.x - 0.95) / 0.05;
        color = a * vec3(1.0) + (1.0 - a) * color;
    }
    if (frag_texture_coord.x < 0.05) {
        float a = (0.05 - frag_texture_coord.x) / 0.05;
        color = a * vec3(1.0) + (1.0 - a) * color;
    }

    float ao = texture(editor_water_lightmap_tex, frag_lightmap_uv).r;

    if (int(draw_type) == 0) {
        g_frag_color = vec4(ao * color, 0.85);
    }
    else if (int(draw_type) == 1) {
        g_frag_color = vec4(color, 0.85);
    }
    else if (int(draw_type) == 2) {
        g_frag_color = vec4(ao, ao, ao, 0.85);
    }
    else if (int(draw_type) == 3) {
        g_frag_color = vec4(frag_lightmap_uv, 0.0, 0.85);
    }
}
//@end
