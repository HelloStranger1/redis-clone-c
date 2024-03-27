#ifndef redis_parser_h
#define redis_parser_h

#define MAX_ARRAY_SIZE 64

typedef struct RespData RespData;

typedef enum {
    RESP_SIMPLE_STRING,
    RESP_SIMPLE_ERROR,
    RESP_INTEGER,
    RESP_BULK_STRING,
    RESP_ARRAY,
    RESP_NULL,
    // RESP_BOOL,
    // RESP_DOUBLE,
    // RESP_BIG_NUM,
    // RESP_BULK_ERROR,
    // RESP_VERBATIM_STRING,
    // RESP_MAP,
    // RESP_SET,
    // RESP_PUSHES,
    // RESP_COMMAND,
} DataType;

// The main thing. everything is just nested inside.


typedef struct {
    DataType type;
    union {
        char* simpleString;
        char* simpleError;
        int integer;
        struct {
            int len;
            char* chars;
        } blkString;
        struct {
            int len;
            struct RespData** data;
        } array;
    } data;
} RespData;

RespData* parse_resp_data(const char* input);
char* convert_data_to_blk(RespData* input);
void free_resp(RespData* resp);
#endif