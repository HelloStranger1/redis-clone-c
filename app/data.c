#include "data.h"

#include <string.h>

uint32_t hash_string(const char* key, size_t length) {
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
    return hash;
}

RespData* copy_data(RespData* data) {
    RespData* copied = malloc(sizeof(*copied));
    if (!copied) {
        die("Couldn't allocate memory");
    }

    copied->type = data->type;
    switch (data->type) {
        case RESP_INTEGER: 
            copied->as.integer = AS_INTEGER(data);
            break;
        case RESP_BOOL:
            copied->as.boolean = AS_BOOL(data);
            break;
        case RESP_ARRAY:
            copied->as.arr = (DataArr*) malloc(sizeof(DataArr));
            copied->as.arr->length = AS_ARR(data)->length;
            copied->as.arr->values = (RespData**) malloc(sizeof(RespData*) * copied->as.arr->length);
            for (int i = 0; i < copied->as.arr->length; i++) {
                AS_ARR(copied)->values[i] = copy_data(AS_ARR(data)->values[i]);
            }
            break;
        case RESP_BULK_STRING: 
            copied->as.blk_str = (BlkStr*) malloc(sizeof(BlkStr));
            copied->as.blk_str->length = AS_BLK_STR(data)->length;
            copied->as.blk_str->chars = strdup(AS_BLK_STR(data)->chars);
            break;
        case RESP_SIMPLE_STRING: 
        case RESP_SIMPLE_ERROR:
            copied->as.simple_str = strdup(AS_SIMPLE_STR(data));
            break;
    }
    return copied;
}

void free_data(void *resp_data) {
    if (resp_data == NULL) return;

    RespData *data = (RespData*) resp_data;
    switch (data->type) {
        case RESP_INTEGER:
        case RESP_BOOL:
        case RESP_ARRAY:
            for (int i = 0; i < AS_ARR(data)->length; i++) {
                free_data(AS_ARR(data)->values[i]);
            }
            free(AS_ARR(data)->values);
            free(AS_ARR(data));
            free(data);
            break;
        case RESP_BULK_STRING:
            if (AS_BLK_STR(data)->length != 0) {
                free(AS_BLK_STR(data)->chars);
            }
            free(AS_BLK_STR(data));
            free(data);
            break;
        case RESP_SIMPLE_STRING:
        case RESP_SIMPLE_ERROR:
            free(AS_SIMPLE_STR(data));
            free(data);
            break;
    }
}
void printData(RespData* data) {
    printf("Not yet implemented");
}