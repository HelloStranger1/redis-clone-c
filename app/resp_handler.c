#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "data.h"
#include "parser.h"
#include "resp_handler.h"


int send_simple_string(int client_fd, char *str) {
    RespData data = {
        .type = RESP_SIMPLE_STRING,
        .as.simple_str = str 
    };

    char *response = encode_resp_data(&data);
    send(client_fd, response, strlen(response), 0);
    free(response);
    return 0;
}

int send_bulk_string(int client_fd, char *str) {
    BlkStr value = {
        .length = strlen(str),
        .chars = str
    };
    RespData data = {
        .type = RESP_BULK_STRING,
        .as.blk_str = &value
    };

    char *response = encode_resp_data(&data);
    send(client_fd, response, strlen(response), 0);
    free(response);
    return 0;
}

int send_arr_of_bulk_string(int client_fd, char *strings[], int count) {
    char arr_len[10]; 
    sprintf(arr_len, "*%d\r\n");
    send(client_fd, arr_len, strlen(arr_len), 0);
    for (int i = 0; i < count; i++) {
        send_bulk_string(client_fd, strings[i]);
    }

    return 0;
}