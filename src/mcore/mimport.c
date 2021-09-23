#include <assert.h>
#include <stdbool.h>

#include "3rd_party/cute_headers/cute_files.h"
#include "mcore/mcommon.h"
#include "mcore/mdata.h"
#include "mcore/mimport.h"
#include "mcore/mlog.h"

static void _import_texture(mdatafile_t *file, unsigned char *data, int data_len) {
    mdatafile_add_string(file, "filter", "linear", true);
    mdatafile_add_data(file, "data", data, data_len);
}

static void _import_default(mdatafile_t *file, unsigned char *data, int data_len) {
    mdatafile_add_data(file, "data", data, data_len);
}

static void _visit_file(cf_file_t *file, void *udata) {
    if (strcmp(file->ext, ".mdata") == 0) {
        return;
    }

    mlog_note("Importing file %s", file->path);

    unsigned char *data;
    int data_len;
    if (!mread_file(file->path, &data, &data_len)) {
        mlog_error("Could not load file %s.", file->path);
    }

    mdatafile_t *mdatafile = mdatafile_load(file->path);
    mdatafile_cache_old_vals(mdatafile);
    if ((strcmp(file->ext, ".png") == 0) ||
            (strcmp(file->ext, ".bmp") == 0) ||
            (strcmp(file->ext, ".jpg") == 0)) {
        _import_texture(mdatafile, data, data_len);
    }
    else if ((strcmp(file->ext, ".cfg") == 0) ||
            (strcmp(file->ext, ".mscript") == 0) ||
            (strcmp(file->ext, ".ogg") == 0) ||
            (strcmp(file->ext, ".fnt") == 0) ||
            (strcmp(file->ext, ".terrain_model") == 0) ||
            (strcmp(file->ext, ".hole") == 0) ||
            (strcmp(file->ext, ".model") == 0)) {
        _import_default(mdatafile, data, data_len);
    }
    else {
        mlog_warning("No importer for file %s", file->path);
    }
    mdatafile_save(mdatafile);
    mdatafile_delete(mdatafile);
}

void mimport_run(void) {
    cf_traverse("data", _visit_file, NULL);
}
