@ctype mat4 mat4
@ctype vec4 vec4
@ctype vec3 vec3
@ctype vec2 vec2

@vs texture_vs
uniform texture_vs_params {
    mat4 mvp_mat;
};

in vec3 position;
in vec2 texture_coord;

out vec2 frag_texture_coord;

void main() {
    frag_texture_coord = texture_coord;
    gl_Position = mvp_mat * vec4(position, 1.0);
}
@end

@fs texture_fs
uniform sampler2D texture_image;

in vec2 frag_texture_coord;

out vec4 g_frag_color;

#define FXAA_SPAN_MAX 8.0
#define FXAA_REDUCE_MUL   (1.0/FXAA_SPAN_MAX)
#define FXAA_REDUCE_MIN   (1.0/128.0)
#define FXAA_SUBPIX_SHIFT (1.0/4.0)

void main() {
    float width = float(textureSize(texture_image, 0).x);
    float height = float(textureSize(texture_image, 0).y);
    vec2 rcpFrame = vec2(1.0/width, 1.0/height);
    vec4 uv = vec4(frag_texture_coord, frag_texture_coord - (rcpFrame*(0.5 + FXAA_SUBPIX_SHIFT)));

    vec3 rgbNW = textureLod(texture_image, uv.zw, 0.0).xyz;
    vec3 rgbNE = textureLod(texture_image, uv.zw + vec2(1,0)*rcpFrame.xy, 0.0).xyz;
    vec3 rgbSW = textureLod(texture_image, uv.zw + vec2(0,1)*rcpFrame.xy, 0.0).xyz;
    vec3 rgbSE = textureLod(texture_image, uv.zw + vec2(1,1)*rcpFrame.xy, 0.0).xyz;
    vec3 rgbM  = textureLod(texture_image, uv.xy, 0.0).xyz;

    vec3 luma = vec3(0.299, 0.587, 0.114);
    float lumaNW = dot(rgbNW, luma);
    float lumaNE = dot(rgbNE, luma);
    float lumaSW = dot(rgbSW, luma);
    float lumaSE = dot(rgbSE, luma);
    float lumaM  = dot(rgbM,  luma);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL),
        FXAA_REDUCE_MIN);
    float rcpDirMin = 1.0/(min(abs(dir.x), abs(dir.y)) + dirReduce);
    
    dir = min(vec2( FXAA_SPAN_MAX,  FXAA_SPAN_MAX),
          max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX),
          dir * rcpDirMin)) * rcpFrame.xy;

    vec3 rgbA = (1.0/2.0) * (
        textureLod(texture_image, uv.xy + dir * (1.0/3.0 - 0.5), 0.0).xyz +
        textureLod(texture_image, uv.xy + dir * (2.0/3.0 - 0.5), 0.0).xyz);
    vec3 rgbB = rgbA * (1.0/2.0) + (1.0/4.0) * (
        textureLod(texture_image, uv.xy + dir * (0.0/3.0 - 0.5), 0.0).xyz +
        textureLod(texture_image, uv.xy + dir * (3.0/3.0 - 0.5), 0.0).xyz);
    
    float lumaB = dot(rgbB, luma);

    if((lumaB < lumaMin) || (lumaB > lumaMax)) {
        g_frag_color = vec4(rgbA, 1.0); 
    }
    else {
        g_frag_color = vec4(rgbB, 1.0); 
    }

    //g_frag_color = texture(texture_image, frag_texture_coord);
}
@end

@program texture texture_vs texture_fs

