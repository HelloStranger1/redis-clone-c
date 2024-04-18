#include <string.h>

#include "parser.h"

static RespData* parse_resp_arr(char** c) {
    RespData* resp = malloc(sizeof(*resp));
    if (!resp) {
        die("Couldn't allocate memory");
    }

    resp->type = RESP_ARRAY;
    resp->as.arr = (DataArr*) malloc(sizeof(DataArr));

    int itemCount = (int) strtol(*c, c, 10);
    

    *c += 2; //Skipping the "\r\n"
    if (itemCount == -1) {
        // Null array
        resp->as.arr->length = -1;
        resp->as.arr->values = NULL;
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
    *c += 2; //Skipping the "\r\n"
    if (str_len == -1) {
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


    resp->as.integer = (int) strtol(*c, c, 10);
    *c += 2; //Skipping the "\r\n"
    
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

static RespData* parse_resp_simple_string(char **c) {
    RespData* resp = malloc(sizeof(*resp));

    if (!resp) {
        die("Couldn't allocate memory");
    }

    resp->type = RESP_SIMPLE_STRING;

    char *end = strstr(*c, "\r\n");
    *end = '\0';
    resp->as.simple_str = strdup(*c);

    *c = end + 2;

    return resp;

}

RespData* parse_resp_data(char** input) { 
    char type = **input;
    (*input)++;
    switch (type) {
        case '+': 
            return parse_resp_simple_string(input);
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

