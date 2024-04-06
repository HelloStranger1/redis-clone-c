#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "parser.h"

#define ALLOCATE_ARR(type, count) (type*) malloc(sizeof(type) * count)

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isEmptyString(char* c) {
    return *c == '-' && *(c + 1) == '1';
}


/**
 * Like atoi, except looks for \r\n instead of \0.
 * Expects only to recieve an unsigned int.
*/
static int convert_to_int(char** c) {
    unsigned int value = 0;
    while(**c != '\r') {
        if (!isDigit(**c)) {
            return -1;
        }

        value *= 10;
        value += (int)(**c - '0');

        (*c)++;
    }
    *c += 2; // Skip \r\n
    return value;
}


static RespData* parse_resp_arr(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        fprintf(stderr, "Couldn't allocate memory.");
        exit(EXIT_FAILURE);
    }

    resp->type = RESP_ARRAY;
    resp->as.arr = (DataArr*) malloc(sizeof(DataArr));

    int itemCount = convert_to_int(c);
    if (itemCount == -1) {
       printf("Couldn't convert length to number. original string was: %s", ((*c) - 2)); 
       free(resp);
       return NULL;
    }

    resp->as.arr->length = itemCount;
    resp->as.arr->values = ALLOCATE_ARR(RespData*, itemCount);

    for (int i = 0; i < itemCount; i++) {
        AS_ARR(resp)->values[i] = parse_resp_data(c);
    }

    return resp;
}

static RespData* parse_resp_blk_string(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        fprintf(stderr, "Couldn't allocate memory.");
        exit(EXIT_FAILURE);
    }

    resp->type = RESP_BULK_STRING;
    resp->as.blk_str = (BlkStr*) malloc(sizeof(BlkStr));
    if (isEmptyString(*c)) {
        *c += 4; //Skipping the "-1\r\n"
        resp->as.blk_str->length = 0;
        resp->as.blk_str->chars = NULL;
        return resp;
    }

    int strLen = convert_to_int(c);
    resp->as.blk_str->length = strLen;
    resp->as.blk_str->chars = malloc(strLen + 1);
    memcpy(resp->as.blk_str->chars, *c, strLen);
    resp->as.blk_str->chars[strLen] = '\0';
    *c += strLen + 2;
    return resp;
}

static RespData* parse_resp_integer(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        fprintf(stderr, "Couldn't allocate memory.");
        exit(EXIT_FAILURE);
    }

    resp->type = RESP_INTEGER;

    bool isNegative = (**c == '-');
    if (**c == '+' || **c == '-') {
        (*c)++;
    }

    resp->as.integer = convert_to_int(c);
    if (isNegative) {
        resp->as.integer = (-1)*AS_INTEGER(resp);
    }

    return resp;
}


RespData* parse_resp_data(char** input) { 
    char type = **input;
    (*input)++;
    switch (type) {
        case '+': 
            printf("simple strings not yet implemented");
            return NULL;
        case '-': 
            printf("Errors not yet implemented");
            return NULL;
        case ':': 
            return parse_resp_integer(input);
        case '$': 
            return parse_resp_blk_string(input);
        case '*': 
            return parse_resp_arr(input);
        default:
            printf("either not yet implemented or uknown type");
            return NULL;
    }
}

char* convert_data_to_blk(RespData* input) {
    if (input->type != RESP_BULK_STRING) {
        return "";
    }
    int inputLength = AS_BLK_STR(input)->length;
    if (AS_BLK_STR(input)->chars == NULL) {
        return strdup("$-1\r\n");
    }
    char idk[10];
    int encodedLength = inputLength + sprintf(idk, "$%d\r\n", inputLength) + 2;
    char* encodedString = (char*) malloc(encodedLength + 1);

    sprintf(encodedString, "$%u\r\n", inputLength);

    strcpy(encodedString + strlen(encodedString), AS_BLK_STR(input)->chars);

    strcat(encodedString, "\r\n");

    return encodedString;
}

