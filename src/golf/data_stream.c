#include "data_stream.h"

#include <string.h>

#include "3rd_party/miniz/miniz.h"

void data_stream_init(struct data_stream *stream) {
    stream->max_len = 1024;
    stream->pos = 0;
    stream->len = 0;
    stream->data = malloc(sizeof(char)*stream->max_len);
}

void data_stream_deinit(struct data_stream *stream) {
    free(stream->data);
    stream->data = NULL;
}

void data_stream_reset_pos(struct data_stream *stream) {
    stream->pos = 0;
}

void data_stream_push(struct data_stream *stream, char *data, int len) {
    if (stream->len + len >= stream->max_len) {
        stream->max_len = 2 * (stream->len + len) + 1;

        char *new_data = malloc(sizeof(char) * stream->max_len);
        memcpy(new_data, stream->data, stream->len); 
        free(stream->data);
        stream->data = new_data;
    }

    memcpy(stream->data + stream->len, data, len);
    stream->len += len;
}

bool data_stream_copy(struct data_stream *stream, void *dest, int len) {
    if (stream->pos + len > stream->len) {
        assert(false);
        return false;
    }

    memcpy(dest, stream->data + stream->pos, len);
    stream->pos += len;
    return true;
}

bool data_stream_compress(struct data_stream *stream) {
    int uncompressed_len = stream->len;
    mz_ulong mz_compressed_len = mz_compressBound(stream->len);
    int compressed_len = (int) mz_compressed_len;
    char *compressed_data = malloc(sizeof(char) * compressed_len);
    mz_compress((unsigned char *) compressed_data, &mz_compressed_len, 
            (unsigned char *) stream->data, (mz_ulong) stream->len);
    compressed_len = (int) mz_compressed_len;

    stream->len = 0;
    data_stream_push(stream, (char*) &uncompressed_len, sizeof(int));
    data_stream_push(stream, compressed_data, compressed_len);
    free(compressed_data);
    return true;
}

bool data_stream_decompress(struct data_stream *stream) {
    stream->pos = 0;

    int uncompressed_len;
    data_stream_copy(stream, &uncompressed_len, sizeof(int));

    char *uncompressed_data = malloc(sizeof(char) * uncompressed_len);
    mz_ulong mz_uncompressed_len = (mz_ulong) uncompressed_len;
    int err = mz_uncompress((unsigned char *) uncompressed_data, &mz_uncompressed_len,
            (unsigned char *) (stream->data + stream->pos), (mz_ulong) (stream->len - stream->pos));
    uncompressed_len = (int) mz_uncompressed_len;
    if (err != MZ_OK) {
        printf("Error decompressing data stream: %s\n", mz_error(err));
        return false;
    }

    stream->len = 0;
    stream->pos = 0;
    data_stream_push(stream, uncompressed_data, uncompressed_len);
    free(uncompressed_data);
    return true;
}

void serialize_char(struct data_stream *stream, char v) {
    data_stream_push(stream, (char*) &v, sizeof(char));
}

char deserialize_char(struct data_stream *stream) {
    char v;
    data_stream_copy(stream, &v, sizeof(char));
    return v;
}

void serialize_char_array(struct data_stream *stream, char *v, int l) {
    data_stream_push(stream, v, sizeof(char) * l);
}

void deserialize_char_array(struct data_stream *stream, char *v, int l) {
    data_stream_copy(stream, v, sizeof(char) * l);
}

void serialize_int(struct data_stream *stream, int v) {
    data_stream_push(stream, (char*) &v, sizeof(int));
}

int deserialize_int(struct data_stream *stream) {
    int v;
    data_stream_copy(stream, &v, sizeof(int));
    return v;
}

void serialize_float(struct data_stream *stream, float v) {
    data_stream_push(stream, (char*) &v, sizeof(float));
}

float deserialize_float(struct data_stream *stream) {
    float v;
    data_stream_copy(stream, &v, sizeof(float));
    return v;
}

void serialize_vec2(struct data_stream *stream, vec2 v) {
    serialize_float(stream, v.x);
    serialize_float(stream, v.y);
}

vec2 deserialize_vec2(struct data_stream *stream) {
    vec2 v;
    v.x = deserialize_float(stream);
    v.y = deserialize_float(stream);
    return v;
}

void serialize_vec2_array(struct data_stream *stream, vec2 *v, int l) {
    for (int i = 0; i < l; i++) {
        serialize_vec2(stream, v[i]);
    }
}

void deserialize_vec2_array(struct data_stream *stream, vec2 *v, int l) {
    for (int i = 0; i < l; i++) {
        v[i] = deserialize_vec2(stream);
    }
}

void serialize_vec3(struct data_stream *stream, vec3 v) {
    serialize_float(stream, v.x);
    serialize_float(stream, v.y);
    serialize_float(stream, v.z);
}

vec3 deserialize_vec3(struct data_stream *stream) {
    vec3 v;
    v.x = deserialize_float(stream);
    v.y = deserialize_float(stream);
    v.z = deserialize_float(stream);
    return v;
}

void serialize_vec3_array(struct data_stream *stream, vec3 *v, int l) {
    for (int i = 0; i < l; i++) {
        serialize_vec3(stream, v[l]);
    }
}

void deserialize_vec3_array(struct data_stream *stream, vec3 *v, int l) {
    for (int i = 0; i < l; i++) {
        v[i] = deserialize_vec3(stream);
    }
}

void serialize_quat(struct data_stream *stream, quat q) {
    serialize_float(stream, q.x);
    serialize_float(stream, q.y);
    serialize_float(stream, q.z);
    serialize_float(stream, q.w);
}

quat deserialize_quat(struct data_stream *stream) {
    quat q;
    q.x = deserialize_float(stream);
    q.y = deserialize_float(stream);
    q.z = deserialize_float(stream);
    q.w = deserialize_float(stream);
    return q;
}

void serialize_string(struct data_stream *stream, const char *str) {
    int len = (int) strlen(str);
    serialize_int(stream, len);
    for (int i = 0; i < len; i++) {
        serialize_char(stream, str[i]);
    }
}

char *deserialize_string(struct data_stream *stream) {
    int len = deserialize_int(stream);    
    char *str = malloc(sizeof(char) * (len + 1));
    for (int i = 0; i < len; i++) {
        str[i] = deserialize_char(stream);
    }
    str[len] = 0;
    return str;
}
