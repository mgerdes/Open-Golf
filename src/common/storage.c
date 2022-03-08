#define _CRT_SECURE_NO_WARNINGS

#include "common/storage.h"

#include <stdio.h>

#if GOLF_PLATFORM_ANDROID

#include <sys/stat.h>
#include <android/native_activity.h>

#elif GOLF_PLATFORM_EMSCRIPTEN

#include <emscripten.h>
#include "3rd_party/sokol/sokol_time.h"

#endif

#include "3rd_party/parson/parson.h"
#include "3rd_party/sokol/sokol_app.h"
#include "common/log.h"
#include "common/map.h"

static JSON_Value *_storage_json_val;
static JSON_Object *_storage_json_obj;
JSON_Value * json_parse_string(const char *string);

static void _save_buf_to_file(const char *file, char *buf, int buf_size) {
    FILE *f = fopen(file, "wb");    
    fwrite(buf, buf_size, 1, f);
    fclose(f);
}

static bool _get_buf_from_file(const char *file, char **buf, int *buf_len) {
    FILE *f = fopen(file, "rb");  
    if (!f) { 
        return false;
    }

    fseek(f, 0, SEEK_END);
    int num_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *bytes = (char*)golf_alloc(num_bytes + 1);
    int ret = (int)fread(bytes, sizeof(char), num_bytes, f);
    if (ret == -1) {
        fclose(f);
        golf_free(bytes);
        return false;
    }
    fclose(f);
    bytes[num_bytes] = 0;

    *buf = bytes;
    *buf_len = num_bytes;

    return true;
}

static bool _golf_storage_get_buf(char **buf, int *buf_len) {

#if GOLF_PLATFORM_WINDOWS | GOLF_PLATFORM_LINUX

    return _get_buf_from_file("storage.json", buf, buf_len);

#elif GOLF_PLATFORM_ANDROID

    ANativeActivity *native_activity = sapp_android_get_native_activity();
    const char *internal_path = native_activity->internalDataPath;
    char storage_path[1024];
    snprintf(storage_path, 1024, "%s/storage.json", internal_path);
    
    return _get_buf_from_file(storage_path, buf, buf_len);

#elif GOLF_PLATFORM_IOS



#elif GOLF_PLATFORM_EMSCRIPTEN

    return _get_buf_from_file("/opengolf_persistent_data/storage.json", buf, buf_len);

#else

#error Unknown platform

#endif
}

void golf_storage_init(void) {
#if GOLF_PLATFORM_ANDROID

    ANativeActivity *native_activity = sapp_android_get_native_activity();
    const char *internal_path = native_activity->internalDataPath;
    
    // Create the internal storage directory if it doesn't exist yet
    struct stat sb;
    int res = stat(internal_path, &sb); 
    if (!(0 == res && sb.st_mode & S_IFDIR)) {
        golf_log_note("Creating internal path directory");
        res = mkdir(internal_path, 0770);
    }

#elif GOLF_PLATFORM_EMSCRIPTEN

    EM_ASM(
            FS.mkdir('/opengolf_persistent_data');
            FS.mount(IDBFS, {}, '/opengolf_persistent_data');
            Module.syncdone = 0;
            FS.syncfs(true, function(err) {
                assert(!err);
                Module.syncdone = 1;
                });
          );

#endif
}

bool golf_storage_finish_init(void) {
#ifdef GOLF_PLATFORM_EMSCRIPTEN

    if (emscripten_run_script_int("Module.syncdone") == 0) {
        return false;
    }

#endif

    int json_buffer_len;
    char *json_buffer = "";
    if (_golf_storage_get_buf(&json_buffer, &json_buffer_len)) {
        _storage_json_val = json_parse_string(json_buffer);
        golf_free(json_buffer);
    }
    else {
        _storage_json_val = json_value_init_object();
    }
    _storage_json_obj = json_value_get_object(_storage_json_val);
    return true;
}

void golf_storage_set_num(const char *key, float num) {
    json_object_set_number(_storage_json_obj, key, num);
}

bool golf_storage_get_num(const char *key, float *num) {
    JSON_Value  *json_val = json_object_get_value(_storage_json_obj, key);
    if (!json_val || json_value_get_type(json_val) != JSONNumber) {
        return false;
    }

    *num = (float)json_value_get_number(json_val);
    return true;
}

void golf_storage_save(void) {
    int buf_size = (int)json_serialization_size(_storage_json_val);
    char *buf = golf_alloc(buf_size);
    json_serialize_to_buffer(_storage_json_val, buf, buf_size);

    // use buf_size-1 because we don't need to write out the null byte
#if GOLF_PLATFORM_WINDOWS | GOLF_PLATFORM_LINUX

    _save_buf_to_file("storage.json", buf, buf_size - 1);

#elif GOLF_PLATFORM_ANDROID

    ANativeActivity *native_activity = sapp_android_get_native_activity();
    const char *internal_path = native_activity->internalDataPath;
    char storage_path[1024];
    snprintf(storage_path, 1024, "%s/storage.json", internal_path);

    _save_buf_to_file(storage_path, buf, buf_size - 1);

#elif GOLF_PLATFORM_IOS



#elif GOLF_PLATFORM_EMSCRIPTEN

    _save_buf_to_file("/opengolf_persistent_data/storage.json", buf, buf_size - 1);

    EM_ASM(
            FS.syncfs(function (err) {
                assert(!err);
                console.log("SAVED");
                });
          );

#else

#error Unknown platform

#endif

    golf_free(buf);
}
