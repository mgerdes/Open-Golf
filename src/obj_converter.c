#define _CRT_SECURE_NO_WARNINGS

#include "array.h"
#include "file.h"
#include "log.h"
#include "map.h"
#include "stb_image_write.h"
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"

void convert_objs(void) {
    struct array_vec3 colors;
    map_int_t color_idx_map;
    array_init(&colors);
    map_init(&color_idx_map);

    struct directory dir;
    directory_init(&dir, "obj_models", false);
    for (int i = 0; i < dir.num_files; i++) {
        struct file f = dir.files[i];
        if (strcmp(f.ext, ".obj") != 0) {
            continue;
        }
        file_load_data(&f);

        char basename[FILES_MAX_PATH];
        strcpy(basename, f.name);
        basename[strlen(basename) - strlen(".obj")] = 0;
        char out_filename[FILES_MAX_PATH];
        sprintf(out_filename, "assets/models/%s.terrain_model", basename);
        FILE *out_file = fopen(out_filename, "w");

        tinyobj_attrib_t attrib;
        tinyobj_shape_t *shapes = NULL;
        size_t num_shapes = 0;
        tinyobj_material_t *materials = NULL;
        size_t num_materials = 0;
        const char *materials_dir = "obj_models"; 
        tinyobj_parse_obj(&attrib, &shapes, &num_shapes, &materials,
                &num_materials, materials_dir, f.data, f.data_len, 
                TINYOBJ_FLAG_TRIANGULATE);

        for (int i = 0; i < (int)attrib.num_face_num_verts; i++) {
            int face_num_verts = attrib.face_num_verts[i];
            assert(face_num_verts == 3);

            int v_idx0 = attrib.faces[3*i+0].v_idx;
            int v_idx1 = attrib.faces[3*i+1].v_idx;
            int v_idx2 = attrib.faces[3*i+2].v_idx;
            
            vec3 v0;
            v0.x = attrib.vertices[3*v_idx0 + 0];
            v0.y = attrib.vertices[3*v_idx0 + 1];
            v0.z = attrib.vertices[3*v_idx0 + 2];

            vec3 v1;
            v1.x = attrib.vertices[3*v_idx1 + 0];
            v1.y = attrib.vertices[3*v_idx1 + 1];
            v1.z = attrib.vertices[3*v_idx1 + 2];

            vec3 v2;
            v2.x = attrib.vertices[3*v_idx2 + 0];
            v2.y = attrib.vertices[3*v_idx2 + 1];
            v2.z = attrib.vertices[3*v_idx2 + 2];

            int vn_idx0 = attrib.faces[3*i+0].vn_idx;
            int vn_idx1 = attrib.faces[3*i+1].vn_idx;
            int vn_idx2 = attrib.faces[3*i+2].vn_idx;
            
            vec3 vn0;
            vn0.x = attrib.normals[3*vn_idx0 + 0];
            vn0.y = attrib.normals[3*vn_idx0 + 1];
            vn0.z = attrib.normals[3*vn_idx0 + 2];

            vec3 vn1;
            vn1.x = attrib.normals[3*vn_idx1 + 0];
            vn1.y = attrib.normals[3*vn_idx1 + 1];
            vn1.z = attrib.normals[3*vn_idx1 + 2];

            vec3 vn2;
            vn2.x = attrib.normals[3*vn_idx2 + 0];
            vn2.y = attrib.normals[3*vn_idx2 + 1];
            vn2.z = attrib.normals[3*vn_idx2 + 2];

            int vt_idx0 = attrib.faces[3*i+0].vt_idx;
            int vt_idx1 = attrib.faces[3*i+1].vt_idx;
            int vt_idx2 = attrib.faces[3*i+2].vt_idx;
            
            vec2 vt0;
            vt0.x = attrib.texcoords[2*vt_idx0 + 0];
            vt0.y = attrib.texcoords[2*vt_idx0 + 1];

            vec2 vt1;
            vt1.x = attrib.texcoords[2*vt_idx1 + 0];
            vt1.y = attrib.texcoords[2*vt_idx1 + 1];

            vec2 vt2;
            vt2.x = attrib.texcoords[2*vt_idx2 + 0];
            vt2.y = attrib.texcoords[2*vt_idx2 + 1];

            int mat_id = attrib.material_ids[i];
            const char *mat_name = materials[mat_id].name;
            int *color_idx = map_get(&color_idx_map, mat_name);
            if (!color_idx) {
                vec3 c;
                c.x = materials[mat_id].diffuse[0];
                c.y = materials[mat_id].diffuse[1];
                c.z = materials[mat_id].diffuse[2];
                array_push(&colors, c);
                map_set(&color_idx_map, mat_name, colors.length - 1);
                color_idx = map_get(&color_idx_map, mat_name);
            }

            vec2 vt;
            vt.x = ((*color_idx % 10) * 10.0f + 5.0f) / 100.0f;
            vt.y = ((*color_idx / 10) * 10.0f + 5.0f) / 100.0f;

            fprintf(out_file, "%f %f %f %f %f %f %f %f\n",
                    v0.x, v0.y, v0.z, vn0.x, vn0.y, vn0.z, vt.x, vt.y);
            fprintf(out_file, "%f %f %f %f %f %f %f %f\n",
                    v1.x, v1.y, v1.z, vn1.x, vn1.y, vn1.z, vt.x, vt.y);
            fprintf(out_file, "%f %f %f %f %f %f %f %f\n",
                    v2.x, v2.y, v2.z, vn2.x, vn2.y, vn2.z, vt.x, vt.y);
        }

        tinyobj_attrib_free(&attrib);
        tinyobj_shapes_free(shapes, num_shapes);
        tinyobj_materials_free(materials, num_materials);
        fclose(out_file);
        file_delete_data(&f);
    }

    {
        int image_size = 100;
        unsigned char *image_data = malloc(sizeof(unsigned char)*3*image_size*image_size);
        for (int i = 0; i < image_size; i++) {
            int row = i / 10;

            for (int j = 0; j < image_size; j++) {
                int col = j / 10;

                int mat_idx = 10 * row + col;
                vec3 color;
                if (mat_idx < colors.length) {
                    color = colors.data[mat_idx];
                }
                else {
                    color = V3(0.0f, 0.0f, 0.0f);
                }

                int r = (int) (255*color.x);
                if (r < 0) r = 0;
                if (r > 255) r = 255;

                int g = (int) (255*color.y);
                if (g < 0) g = 0;
                if (g > 255) g = 255;

                int b = (int) (255*color.z);
                if (b < 0) b = 0;
                if (b > 255) b = 255;

                image_data[3*((image_size*i) + j) + 0] = (unsigned char) r;
                image_data[3*((image_size*i) + j) + 1] = (unsigned char) g;
                image_data[3*((image_size*i) + j) + 2] = (unsigned char) b;
            }
        }
        stbi_write_bmp("assets/textures/environment_material.bmp", image_size, image_size, 3, image_data); 
        free(image_data);
    }

    directory_deinit(&dir);

    map_deinit(&color_idx_map);
    array_deinit(&colors);
}
