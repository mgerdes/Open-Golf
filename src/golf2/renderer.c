#include "golf2/renderer.h"

#include <assert.h>

#include "3rd_party/stb/stb_image.h"
#include "3rd_party/sokol/sokol_app.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "mcore/maths.h"
#include "mcore/mdata.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"
#include "mcore/mstring.h"
#include "golf2/shaders/ui_sprite.glsl.h"
#include "golf2/ui.h"

static golf_renderer_t renderer;

static void _set_ui_proj_mat(vec2 pos_offset) {
    float fb_width = (float) 1280;
    float fb_height = (float) 720;
    float w_width = (float) sapp_width();
    float w_height = (float) sapp_height();
    float w_fb_width = w_width;
    float w_fb_height = (fb_height/fb_width)*w_fb_width;
    if (w_fb_height > w_height) {
        w_fb_height = w_height;
        w_fb_width = (fb_width/fb_height)*w_fb_height;
    }
    renderer.ui_proj_mat = mat4_multiply_n(4,
            mat4_orthographic_projection(0.0f, w_width, 0.0f, w_height, 0.0f, 1.0f),
            mat4_translation(V3(pos_offset.x, pos_offset.y, 0.0f)),
            mat4_translation(V3(0.5f*w_width - 0.5f*w_fb_width, 0.5f*w_height - 0.5f*w_fb_height, 0.0f)),
            mat4_scale(V3(w_fb_width/fb_width, w_fb_height/fb_height, 1.0f))
            );
}

golf_renderer_t *golf_renderer_get(void) {
    return &renderer;
}

void golf_renderer_init(void) {
}

void golf_renderer_draw(void) {
    _set_ui_proj_mat(V2(0.0f, 0.0f));
}
