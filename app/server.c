#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <netdb.h>
#include <fcntl.h>

#include "parser.h"
#include "data.h"
#include "table.h"
#include "common.h"

#define BUFFER_SIZE 1024
#define MAX_EVENTS 64
#define HASH_TABLE_SIZE (uint32_t) 512


static char pingCmd[] = "PING";
static char echoCmd[] = "ECHO";
static char setCmd[] = "SET";
static char getCmd[] = "GET";

hash_table *ht;

const char* run_command(BlkStr *command, DataArr* args);

static int make_socket_non_blocking(int sfd) {
	int flags, s;

	flags = fcntl(sfd, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl"); 
		return -1;
	}

	flags |= O_NONBLOCK;
	s = fcntl(sfd, F_SETFL, flags);
	if (s == -1) {
		perror("fcntl");
		return -1;
	}

	return 0;
}
static int create_and_bind(void) {
	int server_fd;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return -1;
	}

	// // Since the tester restarts your program quite often, setting SO_REUSEADDR
	// // ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return -1;
	}

	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(6379),
									 .sin_addr = { htonl(INADDR_ANY) },
									};

	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return -1;
	}

	return server_fd;
}



int main() {
	char buffer[BUFFER_SIZE] = {0};
	int server_fd, client_addr_len;
	int s, efd;
	struct sockaddr_in client_addr;
	
	struct epoll_event event;
	struct epoll_event *events;

	ht = hash_table_create(HASH_TABLE_SIZE, hash_string, freeData);

	// Disable output buffering
	setbuf(stdout, NULL);
	
	server_fd = create_and_bind();

	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	s = make_socket_non_blocking(server_fd);
	if (s == -1) {
		printf("Failed to make socket not blocking\n");
		return 1;
	}

	if ((s = listen(server_fd, SOMAXCONN)) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);

	efd = epoll_create1(0);
	if (efd == -1) {
		perror("epoll_create");
		return 1;
	}

	event.data.fd = server_fd;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(efd, EPOLL_CTL_ADD, server_fd, &event);
	if (s == -1) {
		perror("epoll_ctl");
		return 1;
	}
	
	// Buffer where events are returned
	events = calloc(MAX_EVENTS, sizeof(event));

	for (;;) {
		int n, i;

		n = epoll_wait(efd, events, MAX_EVENTS, -1);
		for (i = 0; i < n; i++) {
			if ((events[i].events & EPOLLERR) ||
			    (events[i].events & EPOLLHUP) ||
				(!(events[i].events & EPOLLIN))) {
	
				// An error has occured on this fd (or the socket wasn't ready for reading for some reason)
				fprintf(stderr, "epoll error\n");
				close(events[i].data.fd);
				continue;
			} else if (server_fd == events[i].data.fd) {
				// We got a notification on the listening socket, and that means another connection
				for (;;) {
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

					in_len = sizeof(in_addr);
					infd = accept(server_fd, &in_addr, &in_len);
					if (infd == -1) {
						if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
							// We have processed all incoming connections
							break;
						} else {
							perror("accept");
							break;
						}
					}

					s = getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

					if (s == 0) {
						printf("Accepted connection on descriptor %d (host=%s, port = %s)\n", infd, hbuf, sbuf);
					}

					s = make_socket_non_blocking(infd);
					if (s == -1) {
						return 1;
					}
					event.data.fd = infd;
					event.events = EPOLLIN | EPOLLET;
					s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
					if (s == -1) {
						perror("epoll_ctl");
						return 1;
					}

				}
				continue;
			} else {
				/* We have data on the fd waiting to be read.
				 We must consume it all because we are running in edge triggered mode.
				*/
				int done = 0;

				for (;;) {
					ssize_t count;
					char buffer[BUFFER_SIZE];

					count = read(events[i].data.fd, buffer, sizeof(buffer));
					if (count == -1) {
						if (errno != EAGAIN) {
							perror("read");
							done = 1;
						}
						break;
					} else if (count == 0) {
						done = 1;
						break;
					}
					printf("Recieved message: %s\n", buffer);
					char* tmp = buffer;
					RespData* data = parse_resp_data(&tmp);
					if(data->type != RESP_ARRAY) {
						printf("Expected array. wtf");
						exit(EXIT_FAILURE);
					}
					RespData* command = AS_ARR(data)->values[0];
					if (command->type != RESP_BULK_STRING) {
						printf("command should be a bulk string");
						exit(EXIT_FAILURE);
					}

					printf("We parsed %s", AS_BLK_STR(command)->chars);

					const char* response = run_command(command, data);
					send(events[i].data.fd, response, strlen(response), 0);
				}
				if (done) {
					printf("Closed connection on descriptor %d\n", events[i].data.fd);
					close(events[i].data.fd);
				}
			}
				
		}
	}
	free(events);
	hash_table_free(ht);
	close(server_fd);

	return 0;
}



const char* run_command(BlkStr *command, DataArr* args) {
	if(!strcasecmp(command->chars, pingCmd)) {
		return "+PONG\r\n";
	}

	if (!strcasecmp(command->chars, echoCmd)) {
		RespData *arg = args->values[1];
		return convert_data_to_blk(arg);
	}

	if (!strcasecmp(command->chars, setCmd)) {
		BlkStr *key = args->values[1];
		BlkStr *value = args->values[2];

		hash_table_insert(ht, strdup(key->chars), copy_data(value));
	}

	if (!strcasecmp(command->chars, getCmd)) {
		BlkStr* key = args->values[1];
		void *value = hash_table_get(ht, key->chars);
		if (value == NULL) {
			return "_\r\n";
		}

		return convert_data_to_blk(AS_BLK_STR( (RespData*)value));
	}

}

