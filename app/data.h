#ifndef redis_data_h
#define redis_data_h

#include "utils.h"

typedef enum {
    RESP_SIMPLE_STRING,
    RESP_SIMPLE_ERROR,
    RESP_INTEGER,
    RESP_BULK_STRING,
    RESP_ARRAY,
    RESP_BOOL,
    // RESP_DOUBLE,
    // RESP_BIG_NUM,
    // RESP_BULK_ERROR,
    // RESP_VERBATIM_STRING,
    // RESP_MAP,
    // RESP_SET,
    // RESP_PUSHES,
} DataType;

typedef struct {
    int length;
    char* chars;
} BlkStr;

typedef struct {
    int length;
    struct RespData** values;
} DataArr;

typedef struct RespData {
    DataType type;
    union {
        char* simple_str;
        int integer;
        bool boolean;
        BlkStr* blk_str;
        DataArr* arr;
    } as;
} RespData;



#define AS_SIMPLE_STR(resp_data) ((resp_data)->as.simple_str)
#define AS_SIMPLE_ERROR(resp_data) ((resp_data)->as.simple_str)
#define AS_INTEGER(resp_data) ((resp_data)->as.integer)
#define AS_BOOL(resp_data) ((resp_data)->as.boolean)
#define AS_BLK_STR(resp_data) ((resp_data)->as.blk_str)
#define AS_ARR(resp_data) ((resp_data)->as.arr)

/*
#define SIMPLE_STRING_DATA(value) ((RespData){RESP_SIMPLE_ERROR, {.simple_str = value}})
#define SIMPLE_ERR_DATA(value) ((RespData){RESP_SIMPLE_ERROR, {.simple_str = value}})
#define INTEGER_DATA(value) ((RespData){RESP_INTEGER, {.integer = value}})
#define BOOL_DATA(value) ((RespData){RESP_BOOL, {.boolean = value}})
#define BLK_STRING_DATA(value) ((RespData){RESP_BULK_STRING, {.blk_str = (BlkStr*)value}})
#define ARR_DATA(value) ((RespData){RESP_ARRAY, {.arr = (DataArr*)value}})
#define NULL_DATA ((RespData){RESP_NULL, {.integer = 0}})
*/

void free_data(void *resp_data);
void printData(RespData* data);
RespData* copy_data(RespData* data);
uint32_t hash_string(const char* key, size_t length);

#endif