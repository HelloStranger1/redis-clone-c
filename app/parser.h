#ifndef redis_parser_h
#define redis_parser_h

#define MAX_ARRAY_SIZE 64

#include "common.h"
#include "data.h"

RespData* parse_resp_data(char** input);
char* convert_data_to_blk(RespData* input);


#endif