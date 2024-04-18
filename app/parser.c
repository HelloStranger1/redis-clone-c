#include <string.h>

#include "parser.h"

/*
static bool isDigit(char c) {
    return c >= '0' && c <= '9';
} */
/*
static bool isEmptyString(char* c) {
    return *c == '-' && *(c + 1) == '1';
}
*/

/**
 * Like atoi, except looks for \r\n instead of \0.
 * Expects only to recieve an unsigned int.
*/
/*
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

*/
static RespData* parse_resp_arr(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        die("Couldn't allocate memory");
    }

    resp->type = RESP_ARRAY;
    resp->as.arr = (DataArr*) malloc(sizeof(DataArr));

    int itemCount = (int) strtol(*c, c, 10);
    
    /*if (itemCount == -1) {
       printf("Couldn't convert length to number. original string was: %s", ((*c) - 2)); 
       free(resp->as.arr);
       free(resp);
       return NULL;
    } */

    if (itemCount == -1) {
        // Null array
        resp->as.arr->length = -1;
        resp->as.arr->values = NULL;
        *c += 2; // Skipping \r\n
        return resp;
    }

    resp->as.arr->length = itemCount;
    resp->as.arr->values = (RespData**) malloc(sizeof(RespData*) * itemCount);

    for (int i = 0; i < itemCount; i++) {
        resp->as.arr->values[i] = parse_resp_data(c);
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

    int str_len = (int) strtol(*c, c, 10);
    if (str_len == -1) {
        *c += 2; //Skipping the "\r\n"
        resp->as.blk_str->length = 0;
        resp->as.blk_str->chars = NULL;
        return resp;
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

    // bool isNegative = (**c == '-');
    // if (**c == '+' || **c == '-') {
    //     (*c)++;
    // }

    resp->as.integer = (int) strtol(*c, c, 10);
    /*
    if (resp->as.integer == -1) {
         // convert_to_int returns a positive value, so it failed here
        printf("Couldn't convert string to number. original string was: %s", ((*c) - 2)); 
        free(resp);
        return NULL;
    }
    
    if (isNegative) {
        resp->as.integer = (-1)*AS_INTEGER(resp);
    }
    */

    return resp;
}

static RespData* parse_resp_bool(char** c) {
    RespData* resp = malloc(sizeof(*resp));

    if (!resp) {
        die("Couldn't allocate memory");
    }

    resp->type = RESP_BOOL;

    if (**c == 't') {
        resp->as.boolean = true;
    } else {
        resp->as.boolean = false;
    }

    *c += 3;

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
        case '#':
            return parse_resp_bool(input);
        default:
            printf("either not yet implemented or uknown type, type was %c\n", type);
            return NULL;
    }
}

/**
 * Returns the new capacity of dest
*/
/*
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
*/
/*
char *convert_data_to_blk(RespData* input) {
    // We use a defulat capacity of 64 since its enough most of the time
    int capacity = 64;
    char *result = malloc(capacity * sizeof(char));
    result[0] = '\0';
    resp_to_blk(input, &result, &capacity);
    return result;
}
*/
/*
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
*/
/*
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
*/

char *encode_resp_simple_string(char *simple_str) {
    char *encoded_ptr;
    int len;

    len = strlen(simple_str);

    // Allocate space for the +, \r and \name
    encoded_ptr = (char*) malloc((len + 3) * sizeof(char));

    strcpy(encoded_ptr, "+");
    strcat(encoded_ptr, simple_str);
    strcat(encoded_ptr, "\r\n");

    return encoded_ptr;
}

char *encode_resp_simple_error(char *simple_err) {
    char *encoded_ptr;
    int len;

    len = strlen(simple_err);

    // Allocate space for the -, \r and \name
    encoded_ptr = (char*) malloc((len + 3) * sizeof(char));

    strcpy(encoded_ptr, "-");
    strcat(encoded_ptr, simple_err);
    strcat(encoded_ptr, "\r\n");

    return encoded_ptr;
}

char *encode_resp_bool(bool resp_bool) {
    char *encoded_ptr;

    // Allocate space for #, t or f, \r and \n.
    encoded_ptr = (char*) malloc((4) * sizeof(char));

    char value = resp_bool ? 't' : 'f';
    sprintf(encoded_ptr, "#%c\r\n", value);

    return encoded_ptr;
}

char *encode_resp_integer(int resp_int) {
    char *encoded_ptr;

    int len = snprintf(NULL, 0, "%d", resp_int);
    len += 3; // Add space for :, \r and \n

    encoded_ptr = (char*) malloc( (len+1) * sizeof(char));
    
    sprintf(encoded_ptr, ":%d\r\n", resp_int);

    return encoded_ptr;
}

char *encode_resp_bulk_string(BlkStr* bulk_string) {
    char *encoded_ptr;
    int len;

    if (bulk_string->length == 0 && bulk_string->chars == NULL) {
        encoded_ptr = (char*) malloc(6 * sizeof(char));
        strcpy(encoded_ptr, "$-1\r\n");
        return encoded_ptr;
    }

    len = bulk_string->length + 5; // The length of the string, and $, \r, \r, \n and \n (for the length as well)
    len += snprintf(NULL, 0, "%d", bulk_string->length);

    encoded_ptr = (char*) malloc((len + 1) * sizeof(char));

    sprintf(encoded_ptr, "$%d\r\n", bulk_string->length);
    strcat(encoded_ptr, bulk_string->chars);
    strcat(encoded_ptr, "\r\n");

    return encoded_ptr;
}

char *encode_resp_array(DataArr *resp_arr) {
    char *encoded_ptr, *encoded_values[resp_arr->length];
    int len;

    len = 3; // *, \r and \n
    len += snprintf(NULL, 0, "%d", resp_arr->length);

    for (int i = 0; i < resp_arr->length; i++) {
        encoded_values[i] = encode_resp_data(resp_arr->values[i]);
        len += strlen(encoded_values[i]);
    }

    encoded_ptr = (char*) malloc( (len+1) * sizeof(char));

    sprintf(encoded_ptr, "*%d\r\n", resp_arr->length);

    for (int i = 0; i < resp_arr->length; i++) {
        strcat(encoded_ptr, encoded_values[i]);
        free(encoded_values[i]);
    }

    return encoded_ptr;
}

char *encode_resp_data(RespData* data) {
    switch (data->type) {
        case RESP_SIMPLE_STRING:
            return encode_resp_simple_string(data->as.simple_str);
        case RESP_SIMPLE_ERROR:
            return encode_resp_simple_error(data->as.simple_str);
        case RESP_BOOL:
            return encode_resp_bool(data->as.boolean);
        case RESP_INTEGER:
            return encode_resp_integer(data->as.integer);
        case RESP_BULK_STRING:
            return encode_resp_bulk_string(data->as.blk_str);
        case RESP_ARRAY:
            return encode_resp_array(data->as.arr);
        default:
            // Unreachable
            fprintf(stderr, "Couldn't recognise RESP type.");
            return "";
    }
}

