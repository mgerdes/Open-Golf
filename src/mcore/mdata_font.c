#include "mcore/mdata_font.h"

#include "3rd_party/stb/stb_image.h"
#include "3rd_party/sokol/sokol_gfx.h"
#include "3rd_party/stb/stb_image_write.h"
#include "3rd_party/stb/stb_truetype.h"
#include "golf/file.h"
#include "golf/log.h"
#include "golf/parson_helper.h"
#include "golf/string.h"
#include "mcore/mdata.h"

static void _stbi_write_func(void *context, void *data, int size) {
    vec_char_t *bmp = (vec_char_t*)context;
    vec_pusharr(bmp, (char*)data, size);
}

static JSON_Value *_font_atlas_import(const char *file_data, int font_size, int bitmap_size) {
    unsigned char *bitmap = malloc(bitmap_size * bitmap_size);
    stbtt_bakedchar cdata[96];
    memset(cdata, 0, sizeof(cdata));
    stbtt_BakeFontBitmap(file_data, 0, (float)-font_size, bitmap, bitmap_size, bitmap_size, 32, 95, cdata);

    float ascent, descent, linegap;
    stbtt_GetScaledFontVMetrics(file_data, 0, (float)-font_size, &ascent, &descent, &linegap);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);
    json_object_set_number(obj, "font_size", font_size);
    json_object_set_number(obj, "ascent", ascent);
    json_object_set_number(obj, "descent", descent);
    json_object_set_number(obj, "linegap", linegap);

    JSON_Value *char_datas_val = json_value_init_array();
    JSON_Array *char_datas_array = json_value_get_array(char_datas_val);
    for (int i = 0; i < 96; i++) {
        JSON_Value *char_data_val = json_value_init_object();
        JSON_Object *char_data_obj = json_value_get_object(char_data_val);
        json_object_set_number(char_data_obj, "c", 32 + i);
        json_object_set_number(char_data_obj, "x0", cdata[i].x0);
        json_object_set_number(char_data_obj, "x1", cdata[i].x1);
        json_object_set_number(char_data_obj, "y0", cdata[i].y0);
        json_object_set_number(char_data_obj, "y1", cdata[i].y1);
        json_object_set_number(char_data_obj, "xoff", cdata[i].xoff);
        json_object_set_number(char_data_obj, "yoff", cdata[i].yoff);
        json_object_set_number(char_data_obj, "xadvance", cdata[i].xadvance);
        json_array_append_value(char_datas_array, char_data_val);
    }
    json_object_set_value(obj, "char_datas", char_datas_val);

    json_object_set_number(obj, "bitmap_size", bitmap_size);
    {
        vec_char_t bmp;
        vec_init(&bmp);
        stbi_write_bmp_to_func(_stbi_write_func, &bmp, bitmap_size, bitmap_size, 1, bitmap);
        json_object_set_data(obj, "bitmap_data", bmp.data, bmp.length);
        vec_deinit(&bmp);
    }

    free(bitmap);
    return val;
}

bool mdata_font_import(const char *path, char *data, int data_len) {
    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);

    JSON_Value *atlases_val = json_value_init_array();
    JSON_Array *atlases_array = json_value_get_array(atlases_val);

    JSON_Value *small_atlas_val = _font_atlas_import(data, 16, 256);
    json_array_append_value(atlases_array, small_atlas_val);

    JSON_Value *medium_atlas_val = _font_atlas_import(data, 32, 256);
    json_array_append_value(atlases_array, medium_atlas_val);

    JSON_Value *large_atlas_val = _font_atlas_import(data, 64, 512);
    json_array_append_value(atlases_array, large_atlas_val);

    json_object_set_value(obj, "atlases", atlases_val);

    golf_string_t import_font_file_path;
    golf_string_initf(&import_font_file_path, "%s.import", path);
    json_serialize_to_file(val, import_font_file_path.cstr);
    golf_string_deinit(&import_font_file_path);

    json_value_free(val);
    return true;
}


static void _mdata_font_load_atlas(JSON_Object *atlas_obj, mdata_font_atlas_t *atlas) {
    atlas->font_size = (float)json_object_get_number(atlas_obj, "font_size");
    atlas->ascent = (float)json_object_get_number(atlas_obj, "ascent");
    atlas->descent = (float)json_object_get_number(atlas_obj, "descent");
    atlas->linegap = (float)json_object_get_number(atlas_obj, "linegap");
    atlas->bmp_size = (int)json_object_get_number(atlas_obj, "bitmap_size");
    json_object_get_data(atlas_obj, "bitmap_data", &atlas->bmp_data, &atlas->bmp_data_len);

    JSON_Array *char_datas_array = json_object_get_array(atlas_obj, "char_datas");
    for (int i = 0; i < json_array_get_count(char_datas_array); i++) {
        JSON_Object *char_data_obj = json_array_get_object(char_datas_array, i);
        int c = (int)json_object_get_number(char_data_obj, "c");
        if (c >= 0 && c < 256) {
            atlas->char_data[c].x0 = (float)json_object_get_number(char_data_obj, "x0");
            atlas->char_data[c].x1 = (float)json_object_get_number(char_data_obj, "x1");
            atlas->char_data[c].y0 = (float)json_object_get_number(char_data_obj, "y0");
            atlas->char_data[c].y1 = (float)json_object_get_number(char_data_obj, "y1");
            atlas->char_data[c].xoff = (float)json_object_get_number(char_data_obj, "xoff");
            atlas->char_data[c].yoff = (float)json_object_get_number(char_data_obj, "yoff");
            atlas->char_data[c].xadvance = (float)json_object_get_number(char_data_obj, "xadvance");
        }
    }

    int x, y, n;
    int force_channels = 4;
    stbi_set_flip_vertically_on_load(0);
    unsigned char *stb_data = stbi_load_from_memory(atlas->bmp_data, atlas->bmp_data_len, &x, &y, &n, force_channels);
    if (!stb_data) {
        golf_log_error("STB Failed to load image");
    }

    atlas->image = sg_make_image(&(sg_image_desc) {
            .width = atlas->bmp_size,
            .height = atlas->bmp_size,
            .pixel_format = SG_PIXELFORMAT_RGBA8,
            .min_filter = SG_FILTER_LINEAR,
            .mag_filter = SG_FILTER_LINEAR,
            .wrap_u = SG_WRAP_CLAMP_TO_EDGE,
            .wrap_v = SG_WRAP_CLAMP_TO_EDGE,
            .data.subimage[0][0] = {
            .ptr = stb_data,
            .size = 4*sizeof(char)*x*y,
            },
            });

    free(stb_data);
}

bool mdata_font_load(const char *path, char *data, int data_len) {
    JSON_Value *val = json_parse_string(data);
    JSON_Object *obj = json_value_get_object(val);
    if (!val) {
        golf_log_warning("Unable to parse json for mdatafile %s", path);
        return false;
    }

    JSON_Array *atlases_array = json_object_get_array(obj, "atlases");
    mdata_font_t *font = malloc(sizeof(mdata_font_t));
    for (int i = 0; i < json_array_get_count(atlases_array); i++) {
        JSON_Object *atlas_obj = json_array_get_object(atlases_array, i);
        _mdata_font_load_atlas(atlas_obj, &font->atlases[i]);
    }

    json_value_free(val);
    return true;
}

bool mdata_font_unload(const char *path) {
    return true;
}

bool mdata_font_reload(const char *path, char *data, int data_len) {
    return true;
}
