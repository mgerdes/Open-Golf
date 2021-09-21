#include "3rd_party/cute_headers/cute_files.h"
#include "3rd_party/vec/vec.h"

#include "mcore/mcommon.h"
#include "mcore/mstring.h"

static mstring_t str;
static int num_files = 0;

static void _visit_file(cf_file_t *file, void *udata) {
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
    char *bytes = (char *)malloc(num_bytes + 1);
    bytes[num_bytes] = 0;
    int ret = (int) fread(bytes, sizeof(char), num_bytes, f);
    if (ret == -1) {
        assert(false);
    }

    mstring_appendf(&str, "(_embedded_file_t) { .path = \"%s\", .data_len = %d, .data = { ", file->path, num_bytes);
    for (int i = 0; i < num_bytes; i++) {
        mstring_appendf(&str, "0x%X, ", bytes[i]);
    }
    mstring_appendf(&str, "} },\n");

    free(bytes);
    fclose(f);

    num_files++;
}

int main(int argc, char **argv) {
    mstring_init(&str, "");
    mstring_appendf(&str, "typedef struct _embedded_file { const char *path; int data_len, const char *data; } _embedded_file_t;\n");
    mstring_appendf(&str, "static _embedded_file_t files[] = {\n");
    cf_traverse("data", _visit_file, NULL);
    mstring_appendf(&str, "};\n");
    mstring_appendf(&str, "static int num_embedded_files = %d;\n", num_files);

    FILE *f = fopen("membedded_files.h", "wb");
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
