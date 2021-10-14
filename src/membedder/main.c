#include "3rd_party/cute_headers/cute_files.h"
#include "3rd_party/vec/vec.h"

#include "mcore/mcommon.h"
#include "mcore/mfile.h"
#include "mcore/mimport.h"

static int num_files = 0;
FILE *out_file = NULL;

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
    unsigned char *bytes = (unsigned char *)malloc(num_bytes);
    int ret = (int) fread(bytes, sizeof(unsigned char), num_bytes, f);
    if (ret == -1) {
        assert(false);
    }

    fprintf(out_file, "static unsigned char _embedded_file_%d[] = { ", num_files);
    for (int i = 0; i < num_bytes; i++) {
        fprintf(out_file, "0x%X, ", bytes[i]);
    }
    fprintf(out_file, "};\n");

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

    fprintf(out_file, " { \"%s\", \"%s\", sizeof(_embedded_file_%d), _embedded_file_%d }, ", base_file.path, base_file.ext, num_files, num_files);

    num_files++;
}

int main(int argc, char **argv) {
    mimport_init(0, NULL);
    mimport_run();

    out_file = fopen("src/membedder/membedded_files.h", "wb");

    num_files = 0;
    cf_traverse("data", _visit_file_0, NULL);

    fprintf(out_file, "static membedded_file_t _embedded_files[] = {\n");
    num_files = 0;
    cf_traverse("data", _visit_file_1, NULL);
    fprintf(out_file, "};\n");

    fprintf(out_file, "static int _num_embedded_files = %d;\n", num_files);
    fclose(out_file);
    return 0;
}
