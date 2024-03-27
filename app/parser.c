#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "parser.h"

/**
 * Like atoi, except looks for \r\n instead of \0.
 * Expects only to recieve an unsigned int.
*/
static int convert_to_int(const char* c) {
    unsigned int value = 0;
    while(*c != '\r') {
        if (*c >= '0' && *c <= '9') {
            value *= 10;
            value += (int)(*c - '0');
        } else {
            // Unexpected
            return -1;
        }
        c++;
    }
    c += 2; // Skip \r\n
    return value;
}


static RespData* parse_resp_arr(const char* c) {
    RespData* resp = malloc(sizeof(*resp));
    resp->type = RESP_ARRAY;

    int itemCount = convert_to_int(c);
    if (itemCount == -1) {
       printf("something fucked up, debug"); 
       free(resp);
       exit(EXIT_FAILURE);
    }

    resp->data.array.len = itemCount;
    resp->data.array.data = malloc(itemCount * sizeof(RespData*));

    for (int i = 0; i < itemCount; i++) {
        resp->data.array.data[i] = parse_resp_data(c);
    }

    return resp;
}

static RespData* parse_resp_blk_string(const char* c) {
    RespData* resp = malloc(sizeof(*resp));
    resp->type = RESP_BULK_STRING;

    if (*c == '-' && *(c+1) == '1') {
        c += 4;
        resp->data.blkString.len = 0;
        resp->data.blkString.chars = NULL;
        return resp;
    }

    int strLen = convert_to_uint(c);
    resp->data.blkString.len = strLen;
    resp->data.blkString.chars = malloc(strLen + 1);
    memcpy(resp->data.blkString.chars, c, strLen);
    resp->data.blkString.chars[strLen] = '\0';
    c += strLen + 2;

    return resp;
}

static RespData* parse_resp_integer(const char* c) {
    RespData* resp = malloc(sizeof(*resp));
    resp->type = RESP_INTEGER;
    bool isNegative = (*c == '-');
    if (*c == '+' || *c == '-') {
        c++;
    }

    resp->data.integer = convert_to_int(c);
    if (isNegative) {
        resp->data.integer = (-1)*resp->data.integer;
    }

    return resp;
}


RespData* parse_resp_data(const char* input) { 
    switch (*input) {
        case '+': 
            printf("simple strings not yet implemented");
            return NULL;
        case '-': 
            printf("Errors not yet implemented");
            return NULL;
        case ':': 
            return parse_resp_integer(++input);
        case '$': 
            return parse_resp_blk_string(++input);
        case '*': 
            return parse_resp_arr(++input);
        default:
            printf("either not yet implemented or uknown type");
            return NULL;
    }
}

char* convert_data_to_blk(RespData* input) {
    if (input->type != RESP_BULK_STRING) {
        return "";
    }
    size_t inputLength = input->data.blkString.len;
    size_t encodedLength = inputLength + sprintf(NULL, 0, "$%d\r\n", inputLength) + 2;
    char* encodedString = (char*) malloc(encodedLength + 1);

    sprintf(encodedString, encodedLength + 1, "$%zu\r\n", inputLength);

    strcpy(encodedString + strlen(encodedString), input->data.blkString.chars);

    strcat(encodedString, "\r\n");

    return encodedString;
}

void free_resp(RespData* resp) {
    if (!resp) {
        return;
    }

    switch (resp->type){
        case RESP_SIMPLE_STRING:
            free(resp->data.simpleString);
            break;
        case RESP_SIMPLE_ERROR:
            free(resp->data.simpleError);
            break;
        case RESP_BULK_STRING:
            free(resp->data.blkString.chars);
            break;
        case RESP_ARRAY:
            for (int i = 0; i < resp->data.array.len; i++) {
                free_resp(&resp->data.array.data[i]);
            }
            free(resp->data.array.data);
            break;
        default:
            break;
    }

    free(resp);
}
