#include "3rd_party/cute_headers/cute_files.h"
#include "3rd_party/vec/vec.h"

#include "mcore/mcommon.h"
#include "mcore/mfile.h"
#include "mcore/mimport.h"
#include "mcore/mstring.h"

static mstring_t str;
static int num_files = 0;

static void _visit_file_0(cf_file_t *file, void *udata) {
    if (file->is_dir) {
        return;
    }

    if (strcmp(file->ext, ".mdata") != 0) {
        return;
    }

    FILE *f = fopen(file->path, "rb");
    if (!f) {
        assert(false);
    }

    fseek(f, 0, SEEK_END);
    int num_bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *bytes = (char *)malloc(num_bytes);
    int ret = (int) fread(bytes, sizeof(unsigned char), num_bytes, f);
    if (ret == -1) {
        assert(false);
    }

    mstring_appendf(&str, "static unsigned char _embedded_file_%d[] = { ", num_files);
    for (int i = 0; i < num_bytes; i++) {
        mstring_appendf(&str, "0x%X, ", bytes[i]);
    }
    mstring_appendf(&str, "};\n");

    free(bytes);
    fclose(f);

    num_files++;
}

static void _visit_file_1(cf_file_t *file, void *udata) {
    if (file->is_dir) {
        return;
    }

    if (strcmp(file->ext, ".mdata") != 0) {
        return;
    }

    char base_filepath[1024];
    snprintf(base_filepath, 1024, "%s", file->path);
    int filepath_len = strlen(file->path);
    base_filepath[filepath_len - 6] = 0;
    mfile_t base_file = mfile(base_filepath);

    mstring_appendf(&str, " { \"%s\", \"%s\", sizeof(_embedded_file_%d), _embedded_file_%d }, ", base_file.path, base_file.ext, num_files, num_files);

    num_files++;
}

int main(int argc, char **argv) {
    mimport_init();
    mimport_run(true);

    mstring_init(&str, "");
    mstring_appendf(&str, "typedef struct _embedded_file { const char *path; const char *ext; int data_len; const char *data; } _embedded_file_t;\n");

    num_files = 0;
    cf_traverse("data", _visit_file_0, NULL);

    mstring_appendf(&str, "static _embedded_file_t _embedded_files[] = {\n");

    num_files = 0;
    cf_traverse("data", _visit_file_1, NULL);

    mstring_appendf(&str, "};\n");
    mstring_appendf(&str, "static int _num_embedded_files = %d;\n", num_files);

    FILE *f = fopen("src/membedder/membedded_files.h", "wb");
    if (f) {
        fwrite(str.cstr, sizeof(char), str.len, f); 
        fclose(f);
    }
    else {
        assert(false);
    }

    mstring_deinit(&str);
    return 0;
}
