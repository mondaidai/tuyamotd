#ifndef JSON_WRITER_H
#define JSON_WRITER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char * buf;
    size_t size;
    size_t len;
    bool failed;

    int depth;
    bool need_comma[16];
} json_writer_t;

void json_init(json_writer_t *w, char *buffer, size_t size);

int json_object_begin(json_writer_t *w);
int json_object_end(json_writer_t *w);
int json_object_field_begin(json_writer_t *w, const char *name);

int json_array_begin(json_writer_t *w, const char *name);
int json_array_end(json_writer_t *w);
int json_array_field_begin(json_writer_t *w, const char *name);

int json_object_element_begin(json_writer_t *w);
int json_string_element(json_writer_t *w, const char *value);
int json_raw_element(json_writer_t *w, const char *value);

int json_string(json_writer_t * w, const char * name, const char *value);

int json_u64(json_writer_t * w, const char * name, uint64_t value);

bool json_ok(const json_writer_t *w);

#endif