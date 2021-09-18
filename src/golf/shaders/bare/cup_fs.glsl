#version 330

uniform sampler2D lightmap_tex;

in vec2 frag_lightmap_uv;
layout(location = 0) out vec4 g_frag_color;

void main()
{
    g_frag_color = vec4(texture(lightmap_tex, frag_lightmap_uv).xxx, 1.0);
}

