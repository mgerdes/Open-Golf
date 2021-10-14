#ifndef _GOLF_RENDERER_H
#define _GOLF_RENDERER_H

#include "3rd_party/map/map.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "3rd_party/vec/vec.h"
#include "mcore/maths.h"
#include "mcore/mdata.h"

typedef struct golf_renderer_texture {
    mdata_texture_t *texture_data;
    int width;
    int height;
    sg_image sg_image;
} golf_renderer_texture_t; 

typedef map_t(golf_renderer_texture_t) map_golf_renderer_texture_t;

typedef struct golf_renderer_model {
    mdata_model_t *model_data;
    sg_buffer sg_positions_buf;
    sg_buffer sg_normals_buf;
    sg_buffer sg_texcoords_buf;
} golf_renderer_model_t;

typedef map_t(golf_renderer_model_t) map_golf_renderer_model_t;

typedef struct golf_renderer_font {
    mdata_font_t *font_data;
    sg_image atlas_images[3];
} golf_renderer_font_t;

typedef map_t(golf_renderer_font_t) map_golf_renderer_font_t;

typedef struct golf_renderer_ui_pixel_pack_icon {
	vec2 uv0, uv1;
} golf_renderer_ui_pixel_pack_icon_t;

typedef map_t(golf_renderer_ui_pixel_pack_icon_t) map_golf_renderer_ui_pixel_pack_icon_t;

typedef struct golf_renderer_ui_pixel_pack_square {
	golf_renderer_ui_pixel_pack_icon_t tl, tm, tr;
	golf_renderer_ui_pixel_pack_icon_t ml, mm, mr;
	golf_renderer_ui_pixel_pack_icon_t bl, bm, br;
} golf_renderer_ui_pixel_pack_square_t;

typedef map_t(golf_renderer_ui_pixel_pack_square_t) map_golf_renderer_ui_pixel_pack_square_t;

typedef struct golf_renderer_ui_pixel_pack {
	const char *texture;
	map_golf_renderer_ui_pixel_pack_square_t squares;
	map_golf_renderer_ui_pixel_pack_icon_t icons;
} golf_renderer_ui_pixel_pack_t;

typedef map_t(golf_renderer_ui_pixel_pack_t) map_golf_renderer_ui_pixel_pack_t;

typedef map_t(sg_shader) map_sg_shader_t;
typedef map_t(sg_pipeline) map_sg_pipeline_t;

typedef struct golf_renderer {
	map_sg_shader_t shaders_map;
	map_sg_pipeline_t pipelines_map;
	map_golf_renderer_model_t models_map;
	map_golf_renderer_texture_t textures_map;
	map_golf_renderer_font_t fonts_map;
	map_golf_renderer_ui_pixel_pack_t ui_pixel_packs_map;

	mat4 ui_proj_mat;
} golf_renderer_t;

golf_renderer_t *golf_renderer_get(void);
void golf_renderer_init(void);
void golf_renderer_draw(void);

#endif
