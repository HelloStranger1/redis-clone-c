#include <string.h>

#include "parser.h"

static void resp_to_blk(RespData* input, char** result_string, int *dest_capacity);

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
        die("Couldn't allocate memory");
    }

    resp->type = RESP_ARRAY;
    resp->as.arr = (DataArr*) malloc(sizeof(DataArr));

    int itemCount = convert_to_int(c);
    if (itemCount == -1) {
       printf("Couldn't convert length to number. original string was: %s", ((*c) - 2)); 
       free(resp->as.arr);
       free(resp);
       return NULL;
    }

    resp->as.arr->length = itemCount;
    resp->as.arr->values = (RespData**) malloc(sizeof(RespData*) * itemCount);

    for (int i = 0; i < itemCount; i++) {
        AS_ARR(resp)->values[i] = parse_resp_data(c);
    }

    return resp;
}

static RespData* parse_resp_blk_string(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        die("Couldn't allocate memory");
    }

    resp->type = RESP_BULK_STRING;
    resp->as.blk_str = (BlkStr*) malloc(sizeof(BlkStr));
    if (isEmptyString(*c)) {
        *c += 4; //Skipping the "-1\r\n"
        resp->as.blk_str->length = 0;
        resp->as.blk_str->chars = NULL;
        return resp;
    }

    int str_len = convert_to_int(c);
    if (str_len == -1) {
       printf("Couldn't convert length to number. original string was: %s", ((*c) - 2)); 
       free(resp->as.blk_str);
       free(resp);
       return NULL;
    }

    resp->as.blk_str->length = str_len;
    resp->as.blk_str->chars = malloc( sizeof(char) * (str_len + 1));
    if (!resp->as.blk_str->chars) {
        die("Couldn't allocate memory");
    }
    memcpy(resp->as.blk_str->chars, *c, str_len);
    resp->as.blk_str->chars[str_len] = '\0';
    *c += str_len + 2;
    return resp;
}

static RespData* parse_resp_integer(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        die("Couldn't allocate memory");
    }

    resp->type = RESP_INTEGER;

    bool isNegative = (**c == '-');
    if (**c == '+' || **c == '-') {
        (*c)++;
    }

    resp->as.integer = convert_to_int(c);
    if (resp->as.integer == -1) {
        // convert_to_int returns a positive value, so it failed here
        printf("Couldn't convert string to number. original string was: %s", ((*c) - 2)); 
        free(resp);
        return NULL;
    }

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

/**
 * Returns the new capacity of dest
*/
static void append_string(char** dest, const char* src, int *dest_capacity) {
    int src_len = strlen(src);
    int dest_len;
    if (*dest == NULL) {
        int new_len = src_len + 1;
        *dest = malloc(sizeof(char) * (new_len));
        strcpy(*dest, src);
        return; 
    } 
    dest_len = strlen(*dest);
    if (*dest_capacity > dest_len + src_len) {
        strcat(*dest, src);
        return;
    }
    int new_capacity = (2 * (*dest_capacity) > (src_len + dest_len)) ? 2* (*dest_capacity) : (dest_len + src_len + 1);
    *dest_capacity = new_capacity;
    *dest = realloc(*dest, new_capacity * sizeof(char));
    strcat(*dest, src);
}

char *convert_data_to_blk(RespData* input) {
    // We use a defulat capacity of 64 since its enough most of the time
    int capacity = 64;
    char *result = malloc(capacity * sizeof(char));
    result[0] = '\0';
    resp_to_blk(input, &result, &capacity);
    return result;
}

char* convert_blk_data_to_str(RespData* input) {
    if (input->type != RESP_BULK_STRING) {
        return "";
    }
    int inputLength = AS_BLK_STR(input)->length;
    if (AS_BLK_STR(input)->chars == NULL) {
        return "$-1\r\n";
    }

    char response[512];
    char *encodedString = response;

    sprintf(encodedString, "$%d\r\n", inputLength);
    strcat(encodedString, AS_BLK_STR(input)->chars);
    strcat(encodedString, "\r\n");

    return encodedString;

}

static void resp_to_blk(RespData* input, char** result_string, int *dest_capacity) {
    switch (input->type) {
        case RESP_SIMPLE_ERROR:
        case RESP_SIMPLE_STRING: 
            append_string(result_string, "+", dest_capacity);
            append_string(result_string, AS_SIMPLE_STR(input), dest_capacity);
            append_string(result_string, "\r\n", dest_capacity);
            break;
        case RESP_INTEGER: {
            char int_str[20] = {0};
            sprintf(int_str, ":%d\r\n", AS_INTEGER(input));
            append_string(result_string, int_str, dest_capacity);
            break;
        }
        case RESP_BOOL: 
            if (AS_BOOL(input)) {
                append_string(result_string, "#t\r\n", dest_capacity);
            } else {
                append_string(result_string, "#f\r\n", dest_capacity);
            }
            break;
        case RESP_BULK_STRING:
            append_string(result_string, convert_blk_data_to_str(input), dest_capacity);
            break;
        case RESP_ARRAY: {
            char arr_length_str[20] = {0};
            sprintf(arr_length_str, "$%d\r\n", AS_ARR(input)->length);
            append_string(result_string, arr_length_str, dest_capacity);
            for (int i = 0; i < AS_ARR(input)->length; i++) {
                resp_to_blk(AS_ARR(input)->values[i], result_string, dest_capacity);
            }
            break;
        }
        default:
            break;
    }
}

