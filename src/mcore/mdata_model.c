#include "mcore/mdata_model.h"

#include "3rd_party/fast_obj/fast_obj.h"
#include "mcore/mdata.h"
#include "golf/file.h"
#include "golf/log.h"
#include "golf/maths.h"
#include "golf/parson_helper.h"
#include "golf/string.h"

bool mdata_model_import(const char *path, char *data, int data_len) {
    JSON_Value *vertices_val = json_value_init_array();
    JSON_Array *vertices = json_value_get_array(vertices_val);

    fastObjMesh *m = fast_obj_read(path);
    for (int i = 0; i < (int)m->group_count; i++) {
        const fastObjGroup grp = m->groups[i];

        int idx = 0;
        for (int j = 0; j < (int)grp.face_count; j++) {
            int fv = m->face_vertices[grp.face_offset + j];
            if (fv != 3) {
                golf_log_warning("OBJ file isn't triangulated %s", path); 
            }

            for (int k = 0; k < fv; k++) {
                fastObjIndex mi = m->indices[grp.index_offset + idx];

                vec3 p;
                p.x = m->positions[3 * mi.p + 0];
                p.y = m->positions[3 * mi.p + 1];
                p.z = m->positions[3 * mi.p + 2];

                vec2 t;
                t.x = m->texcoords[2 * mi.t + 0];
                t.y = m->texcoords[2 * mi.t + 1];

                vec3 n;
                n.x = m->normals[3 * mi.n + 0];
                n.y = m->normals[3 * mi.n + 1];
                n.z = m->normals[3 * mi.n + 2];

                JSON_Value *position_array_val = json_value_init_array();
                JSON_Array *position_array = json_value_get_array(position_array_val);
                json_array_append_number(position_array, p.x);
                json_array_append_number(position_array, p.y);
                json_array_append_number(position_array, p.z);

                JSON_Value *texcoord_array_val = json_value_init_array();
                JSON_Array *texcoord_array = json_value_get_array(texcoord_array_val);
                json_array_append_number(texcoord_array, t.x);
                json_array_append_number(texcoord_array, t.y);

                JSON_Value *normal_array_val = json_value_init_array();
                JSON_Array *normal_array = json_value_get_array(normal_array_val);
                json_array_append_number(normal_array, n.x);
                json_array_append_number(normal_array, n.y);
                json_array_append_number(normal_array, n.z);

                JSON_Value *face_val = json_value_init_object();
                JSON_Object *face = json_value_get_object(face_val);
                json_object_set_value(face, "position", position_array_val);
                json_object_set_value(face, "texcoord", texcoord_array_val);
                json_object_set_value(face, "normal", normal_array_val);

                json_array_append_value(vertices, face_val);

                idx++;
            }
        }
    }
    fast_obj_destroy(m);

    JSON_Value *val = json_value_init_object();
    JSON_Object *obj = json_value_get_object(val);
    json_object_set_value(obj, "vertices", vertices_val);

    golf_string_t import_model_file_path;
    golf_string_initf(&import_model_file_path, "%s.import", path);
    json_serialize_to_file(val, import_model_file_path.cstr);
    golf_string_deinit(&import_model_file_path);
    json_value_free(val);
    return true;
}
