#ifndef _DATASTREAM_H
#define _DATASTREAM_H

#include <stdbool.h>

#include "array.h"
#include "maths.h"

struct data_stream {
    int max_len, len, pos;
    char *data; 
};
array_t(struct data_stream, data_stream_array)

void data_stream_init(struct data_stream *stream);
void data_stream_deinit(struct data_stream *stream);
void data_stream_reset_pos(struct data_stream *stream);
void data_stream_push(struct data_stream *stream, char *data, int len);
bool data_stream_copy(struct data_stream *stream, void *dest, int len);
bool data_stream_compress(struct data_stream *stream);
bool data_stream_decompress(struct data_stream *stream);

void serialize_char(struct data_stream *stream, char v);
char deserialize_char(struct data_stream *stream);
void serialize_char_array(struct data_stream *stream, char *v, int l);
void deserialize_char_array(struct data_stream *stream, char *v, int l);
void serialize_int(struct data_stream *stream, int v);
int deserialize_int(struct data_stream *stream);
void serialize_float(struct data_stream *stream, float v);
float deserialize_float(struct data_stream *stream);
void serialize_vec2(struct data_stream *stream, vec2 v);
vec2 deserialize_vec2(struct data_stream *stream);
void serialize_vec2_array(struct data_stream *stream, vec2 *v, int l);
void deserialize_vec2_array(struct data_stream *stream, vec2 *v, int l);
void serialize_vec3(struct data_stream *stream, vec3 v);
vec3 deserialize_vec3(struct data_stream *stream);
void serialize_vec3_array(struct data_stream *stream, vec3 *v, int l);
void deserialize_vec3_array(struct data_stream *stream, vec3 *v, int l);
void serialize_quat(struct data_stream *stream, quat q);
quat deserialize_quat(struct data_stream *stream);
void serialize_string(struct data_stream *stream, const char *str);
char *deserialize_string(struct data_stream *stream);

#endif
