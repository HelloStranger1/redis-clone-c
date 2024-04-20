#ifndef redis_resp_handler_h
#define redis_resp_handler_h

#include "utils.h"
#include "data.h"
#include "parser.h"

int send_simple_string(int client_fd, char *str);
int send_bulk_string(int client_fd, char *str);
int send_arr_of_bulk_string(int client_fd, char *strings[], int count);

#endif