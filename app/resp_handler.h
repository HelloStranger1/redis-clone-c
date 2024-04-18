#ifndef redis_resp_handler_h
#define redis_resp_handler_h

int send_simple_string(int client_fd, char *str);
int send_bulk_string(int client_fd, char *str);

#endif