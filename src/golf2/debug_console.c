#include "golf2/debug_console.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "3rd_party/cimgui/cimgui.h"
#include "mcore/maths.h"
#include "golf2/config.h"
#include "golf2/inputs.h"
#include "golf2/renderer.h"

void golf_debug_console_init() {
}

static void _debug_console_main_tab() {
    igText("Frame Rate: %0.3f\n", igGetIO()->Framerate);
    igText("Mouse Pos: <%0.3f, %0.3f>\n", golf_inputs_window_mouse_pos().x, golf_inputs_window_mouse_pos().y);
}

static void _debug_console_renderer_tab() {
    golf_renderer_t *renderer = golf_renderer_get();

    if (igCollapsingHeaderTreeNodeFlags("Fonts", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&renderer->fonts_map);

        while ((key = map_next(&renderer->fonts_map, &iter))) {
            golf_renderer_font_t *font = map_get(&renderer->fonts_map, key);
            mdata_font_t *font_data = font->font_data;

            if (igTreeNodeStr(key)) {
                for (int i = 0; i < 3; i++) {
                    igText("Image Size: %d", font_data->atlases[i].bmp_size);
                    igImage((ImTextureID)(intptr_t)font->atlas_images[i].id, (ImVec2){font_data->atlases[i].bmp_size, font_data->atlases[i].bmp_size}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                }
                igTreePop();
            }
        }
    }

	if (igCollapsingHeaderTreeNodeFlags("Textures", ImGuiTreeNodeFlags_None)) {
		const char *key;
		map_iter_t iter = map_iter(&renderer->textures_map);

		while ((key = map_next(&renderer->textures_map, &iter))) {
			golf_renderer_texture_t *texture = map_get(&renderer->textures_map, key);

            if (igTreeNodeStr(key)) {
				igText("Width: %d", texture->width);
				igText("Height: %d", texture->height);
				igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){texture->width, texture->height}, (ImVec2){0, 0}, (ImVec2){1, 1}, (ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 1});
                igTreePop();
			}
		}
	}

    if (igCollapsingHeaderTreeNodeFlags("Models", ImGuiTreeNodeFlags_None)) {
        const char *key;
        map_iter_t iter = map_iter(&renderer->models_map);

        while ((key = map_next(&renderer->models_map, &iter))) {
            golf_renderer_model_t *model = map_get(&renderer->models_map, key);
			mdata_model_t *model_data = model->model_data;

            if (igTreeNodeStr(key)) {
				igText("Num Positions: %d", model_data->positions.length);
				if (igCollapsingHeaderTreeNodeFlags("Positions", ImGuiTreeNodeFlags_None)) {
					for (int i = 0; i < model_data->positions.length; i++) {
						vec3 position = model_data->positions.data[i];
						igText("<%.3f, %.3f, %.3f>", position.x, position.y, position.z);
					}
				}

				igText("Num Tex Coords: %d", model_data->texcoords.length);
				if (igCollapsingHeaderTreeNodeFlags("Texcoords", ImGuiTreeNodeFlags_None)) {
					for (int i = 0; i < model_data->texcoords.length; i++) {
						vec2 texcoord = model_data->texcoords.data[i];
						igText("<%.3f, %.3f>", texcoord.x, texcoord.y);
					}
				}

				igText("Num Normals: %d", model_data->normals.length);
				if (igCollapsingHeaderTreeNodeFlags("Normals", ImGuiTreeNodeFlags_None)) {
					for (int i = 0; i < model_data->normals.length; i++) {
						vec3 normal = model_data->normals.data[i];
						igText("<%.3f, %.3f, %.3f>", normal.x, normal.y, normal.z);
					}
				}

                igTreePop();
            }
        }
    }

	if (igCollapsingHeaderTreeNodeFlags("UI Pixel Packs", ImGuiTreeNodeFlags_None)) {
		const char *key;
		map_iter_t iter = map_iter(&renderer->ui_pixel_packs_map);

		while ((key = map_next(&renderer->ui_pixel_packs_map, &iter))) {
			golf_renderer_ui_pixel_pack_t *ui_pixel_pack = map_get(&renderer->ui_pixel_packs_map, key);
			golf_renderer_texture_t *texture = map_get(&renderer->textures_map, ui_pixel_pack->texture);
			if (!texture) {
				continue;
			}

            if (igTreeNodeStr(key)) {
				if (igCollapsingHeaderTreeNodeFlags("Squares", ImGuiTreeNodeFlags_None)) {
					const char *key;
					map_iter_t iter = map_iter(&ui_pixel_pack->squares);

					while ((key = map_next(&ui_pixel_pack->squares, &iter))) {
						golf_renderer_ui_pixel_pack_square_t *square = map_get(&ui_pixel_pack->squares, key);

						igText("%s", key);
						igPushStyleVarVec2(ImGuiStyleVar_ItemSpacing, (ImVec2){0, 0});

						float sz = 15;
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->tl.uv0.x, square->tl.uv0.y}, (ImVec2){square->tl.uv1.x, square->tl.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
						igSameLine(0, 0);
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->tm.uv0.x, square->tl.uv0.y}, (ImVec2){square->tm.uv1.x, square->tl.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
						igSameLine(0, 0);
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->tr.uv0.x, square->tl.uv0.y}, (ImVec2){square->tr.uv1.x, square->tl.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->ml.uv0.x, square->ml.uv0.y}, (ImVec2){square->ml.uv1.x, square->ml.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
						igSameLine(0, 0);
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->mm.uv0.x, square->ml.uv0.y}, (ImVec2){square->mm.uv1.x, square->ml.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
						igSameLine(0, 0);
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->mr.uv0.x, square->ml.uv0.y}, (ImVec2){square->mr.uv1.x, square->ml.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->bl.uv0.x, square->bl.uv0.y}, (ImVec2){square->bl.uv1.x, square->bl.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
						igSameLine(0, 0);
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->bm.uv0.x, square->bl.uv0.y}, (ImVec2){square->bm.uv1.x, square->bl.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
						igSameLine(0, 0);
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){square->br.uv0.x, square->bl.uv0.y}, (ImVec2){square->br.uv1.x, square->bl.uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});

						igPopStyleVar(1);
					}
				}

				if (igCollapsingHeaderTreeNodeFlags("Icons", ImGuiTreeNodeFlags_None)) {
					const char *key;
					map_iter_t iter = map_iter(&ui_pixel_pack->icons);

					while ((key = map_next(&ui_pixel_pack->icons, &iter))) {
						golf_renderer_ui_pixel_pack_icon_t *icon = map_get(&ui_pixel_pack->icons, key);
						igText("%s", key);

						float sz = 30;
						igImage((ImTextureID)(intptr_t)texture->sg_image.id, (ImVec2){sz, sz},
								(ImVec2){icon->uv0.x, icon->uv0.y}, (ImVec2){icon->uv1.x, icon->uv1.y}, 
								(ImVec4){1, 1, 1, 1}, (ImVec4){1, 1, 1, 0});
					}
                }

                igTreePop();
            }
        }
    }
}

static void _debug_console_misc_tab() {
    if (igCollapsingHeaderTreeNodeFlags("Configs", ImGuiTreeNodeFlags_None)) {
        golf_config_t *config = golf_config_get();

        const char *key;
        map_iter_t iter = map_iter(&config->config_files);
        while ((key = map_next(&config->config_files, &iter))) {
            if (igTreeNodeStr(key)) {
                golf_config_file_t *config_file = map_get(&config->config_files, key);

                const char *prop_key;
                map_iter_t prop_iter = map_iter(&config_file->props);
                while ((prop_key = map_next(&config_file->props, &prop_iter))) {
                    mdata_config_property_t *prop = map_get(&config_file->props, prop_key);
                    switch (prop->type) {
                        case MDATA_CONFIG_PROPERTY_STRING:
                            igText("string %s: %s", prop_key, prop->string_val);
                            break;
                        case MDATA_CONFIG_PROPERTY_NUMBER:
                            igText("number %s: %.3f", prop_key, prop->number_val);
                            break;
                        case MDATA_CONFIG_PROPERTY_VEC2:
                            igText("vec2 %s: <%.3f, %.3f>", 
                                    prop_key, prop->vec2_val.x, prop->vec2_val.y);
                            break;
                        case MDATA_CONFIG_PROPERTY_VEC3:
                            igText("vec3 %s: <%.3f, %.3f, %.3f>", 
                                    prop_key, prop->vec3_val.x, prop->vec3_val.y, prop->vec3_val.z);
                            break;
                        case MDATA_CONFIG_PROPERTY_VEC4:
                            igText("vec4 %s: <%.3f, %.3f, %.3f, %.3f>", 
                                    prop_key, prop->vec4_val.x, prop->vec4_val.y, prop->vec4_val.z, prop->vec4_val.w);
                            break;
                    }
                }

                igTreePop();
            }
        }
    }
}

void golf_debug_console_update(float dt) {
    static bool debug_console_open = false;
    if (golf_inputs_button_clicked(SAPP_KEYCODE_GRAVE_ACCENT)) {
        debug_console_open = !debug_console_open;
    }

    if (debug_console_open) {
        igSetNextWindowSize((ImVec2){500, 500}, ImGuiCond_FirstUseEver);
        igSetNextWindowPos((ImVec2){5, 5}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
        if (igBegin("Debug Console", &debug_console_open, ImGuiWindowFlags_None)) {
            if (igBeginTabBar("##Tabs", ImGuiTabBarFlags_None)) {
                if (igBeginTabItem("Main", NULL, ImGuiTabItemFlags_None)) {
                    _debug_console_main_tab();
                    igEndTabItem();
                }
                if (igBeginTabItem("Renderer", NULL, ImGuiTabItemFlags_None)) {
                    _debug_console_renderer_tab();
                    igEndTabItem();
                }
                if (igBeginTabItem("Misc", NULL, ImGuiTabItemFlags_None)) {
                    _debug_console_misc_tab();
                    igEndTabItem();
                }
                igEndTabBar();
            }
            igEnd();
        }
    }
}
