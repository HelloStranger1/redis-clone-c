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
#include <arpa/inet.h>
#include <fcntl.h>

#include "parser.h"
#include "data.h"
#include "table.h"
#include "common.h"

#define BUFFER_SIZE 1024
#define MAX_EVENTS 64
#define DEFAULT_PORT 6379
#define HASH_TABLE_SIZE (uint32_t) 512
#define REPLICATION_ID_LEN 40


#define SET_CMD "SET"
#define GET_CMD "GET"
#define PING_CMD "PING"
#define ECHO_CMD "ECHO"

bool isReplica = false;
char replication_id[REPLICATION_ID_LEN + 1];
int offset = 0;

hash_table *ht;
void run_command(int client_fd, BlkStr *command, DataArr* args);

void rand_str(char *dest, size_t length) {
    char charset[] = "0123456789"
                     "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while (length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

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
static int create_and_bind (char *port) {
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	int s, sfd;

	memset (&hints, 0, sizeof (struct addrinfo));
	hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
	hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
	hints.ai_flags = AI_PASSIVE;     /* All interfaces */
	s = getaddrinfo (NULL, port, &hints, &result);
	if (s != 0) {
		fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
		return -1;
	}

	for (rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sfd == -1)
			continue;

		int reuse = 1;
		if (setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
			printf("SO_REUSEADDR failed: %s \n", strerror(errno));
			return -1;
		}

		s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
		if (s == 0) {
			/* We managed to bind successfully! */
			break;
		}

		close (sfd);
	}

	if (rp == NULL) {
		fprintf (stderr, "Could not bind\n");
		return -1;
	}

	freeaddrinfo (result);

	return sfd;
}

int connect_to_master(char *host,  int port) {
	int master_fd;
	struct sockaddr_in master_addr;

	master_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (master_fd == -1) {
		fprintf(stderr, "Could open socket to master");
		return -1;
	}

	memset(&master_addr, 0, sizeof(master_addr));
	master_addr.sin_family = AF_INET;
	master_addr.sin_port = htons(port);
	if (inet_pton(AF_INET, host, &master_addr.sin_addr) > 0) {
		printf("Using IPv4 address: %s\n", host);
	} else {
		// We try to resolve host
		struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_INET; 
        hints.ai_socktype = SOCK_STREAM; 

        int ret = getaddrinfo(host, NULL, &hints, &result);
        if (ret != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in *resolved_addr = (struct sockaddr_in *)result->ai_addr;
        memcpy(&master_addr.sin_addr, &resolved_addr->sin_addr, sizeof(struct in_addr));
        freeaddrinfo(result);
	}

	if (connect(master_fd, (struct sockaddr *)&master_addr, sizeof(master_addr)) < 0) {
		fprintf(stderr, "Error connecting to master");
		return -1;
	}

	return master_fd;
}

int main(int argc, char *argv[]) {
	int server_fd, master_fd = -1;
	int s, efd;	
	struct epoll_event event;
	struct epoll_event *events;

	char *port = "6379";
	char *master_host = NULL;
	int master_port = -1;

	rand_str(replication_id, REPLICATION_ID_LEN + 1);

	ht = hash_table_create(HASH_TABLE_SIZE, hash_string, free_data);

	// Disable output buffering
	setbuf(stdout, NULL);
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--port")) {
			port = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--replicaof")) {
			if (i + 2 >= argc) {
				fprintf(stderr, "Expected use of --replicaof: <MASTER_HOST> <MASTER_PORT> \n");
				return -1;
			}
			master_host = argv[++i];
			master_port = atoi(argv[++i]);
		}
	}

	server_fd = create_and_bind(port);

	if (master_host != NULL && master_port != -1) {
		isReplica = true;
		master_fd = connect_to_master(master_host, master_port);
		if (master_fd == -1) {
			fprintf(stderr, "Couldn't connect to master.");
			exit(EXIT_FAILURE);
		}
		char *ping = "*1\r\n$4\r\nping\r\n";
		send(master_fd, ping, strlen(ping), 0);
	}

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
			} else if (master_fd == events[i].data.fd) {
				/**
				 * We got a msg from master
				*/
			} else {
				/* We have data on the fd waiting to be read.
				 We must consume it all because we are running in edge triggered mode.
				*/
				int done = 0;

				for (;;) {
					ssize_t count;
					char buffer[BUFFER_SIZE] = {0};

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
					if (!strcmp(buffer, "*1\r\n$4\r\nquit\r\n")) {
						printf("Closed connection on descriptor %d\n", events[i].data.fd);
						close(events[i].data.fd);
						goto cleanup;
					}
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

					printf("We parsed %s\n", AS_BLK_STR(command)->chars);

					run_command(events[i].data.fd, AS_BLK_STR(command), AS_ARR(data));
					freeData(data);
				}
				if (done) {
					printf("Closed connection on descriptor %d\n", events[i].data.fd);
					close(events[i].data.fd);
				}
			}
				
		}
	}
cleanup:
	free(events);
	hash_table_free(ht);
	close(server_fd);

	return 0;
}

void run_set(int socket_fd, RespData *key, RespData *value, DataArr *args) {
	int i = 3; // Start of arguments
	/**
	 * Args go in this order: [NX | XX] [GET] [PX | EXAT | KEEPTTL ...]
	*/
	if (args->length == 3) {
		void *inserted = hash_table_insert(ht, AS_BLK_STR(key)->chars, copy_data(value), -1);
		if (inserted == NULL) {
			// We inserted a new value
			printf("New value inserted with key %s\n", AS_BLK_STR(key)->chars);
		} else {
			// We over wrote one. We could add functionality for the GET arg here
			printf("Overwrote data: %s\n", AS_BLK_STR((RespData*)inserted)->chars);
			free_data(inserted);
		}
		char* ok = "+OK\r\n";
		send(socket_fd, ok, strlen(ok), 0);
		return;
	}
	// We have args. We check one by one. 
	if (!strcasecmp(AS_BLK_STR(args->values[i])->chars, "NX")) {
		fprintf(stderr, "We haven't implemented command 'NX'");
		exit(1);	
	} else if (!strcasecmp(AS_BLK_STR(args->values[i])->chars, "XX")) {
		fprintf(stderr, "We haven't implemented command 'XX'");
		exit(1);
	}

	if (!strcasecmp(AS_BLK_STR(args->values[i])->chars, "GET")) {
		fprintf(stderr, "We haven't implemented command 'GET'");
		exit(1);
	}

	if (!strcasecmp(AS_BLK_STR(args->values[i])->chars, "PX")) {
		long long expiryMilli = atoi(AS_BLK_STR(args->values[++i])->chars);
		hash_table_insert(ht, AS_BLK_STR(key)->chars, copy_data(value), current_timestamp() + expiryMilli);
	}
	char* ok = "+OK\r\n";
	send(socket_fd, ok, strlen(ok), 0);
	
}

void run_info(int client_fd, DataArr *args) {
	int i = 1; // Start of args
	char rawResponse[512];
	rawResponse[0] = '\0';
	while (i < args->length) {
		if (!strcasecmp(AS_BLK_STR(args->values[i])->chars, "Replication")) {
			strcat(rawResponse, "# Replication");
			if (isReplica) {
				strcat(rawResponse, "role:slave");
			} else {
				strcat(rawResponse, "role:master");
			}

			strcat(rawResponse, "\n");
			strcat(rawResponse, "master_replid:");
			strcat(rawResponse, replication_id);

			strcat(rawResponse, "\n");
			strcat(rawResponse, "master_repl_offset:");
			char repl_offset[24];
			sprintf(repl_offset, "%d", offset);
			strcat(rawResponse, repl_offset);
			
			// Here we add more values in replication
			i++;
		}
		// Here we add support for more section
	}
	
	char encodedLen[10];
	int lenlen = sprintf(encodedLen, "%ld", strlen(rawResponse));
	send(client_fd, "$", 1, 0);
	send(client_fd, encodedLen, lenlen, 0);
	send(client_fd, "\r\n", 2, 0);
	send(client_fd, rawResponse, strlen(rawResponse), 0);
	send(client_fd, "\r\n", 2, 0);
}

void run_command(int client_fd, BlkStr *command, DataArr* args) {
	if(!strcasecmp(command->chars, PING_CMD)) {
		send(client_fd, "+PONG\r\n", strlen("+PONG\r\n"), 0);
		return;
	}

	if (!strcasecmp(command->chars, ECHO_CMD)) {
		RespData *arg = args->values[1];
		char* response = convert_data_to_blk(arg);
		send(client_fd, response, strlen(response), 0);
		free(response);
		return;
	}

	if (!strcasecmp(command->chars, SET_CMD)) {
		if (args->length < 3) {
			fprintf(stderr, "Not enough args for SET command. ");
			return;
		}
		RespData *key = args->values[1];
		RespData *value = args->values[2];

		run_set(client_fd, key, value, args);
		return;
	}

	if (!strcasecmp(command->chars, GET_CMD)) {
		BlkStr* key = AS_BLK_STR(args->values[1]);
		void *value = hash_table_get(ht, key->chars);
		if (value == NULL) {
			send(client_fd, "$-1\r\n", strlen("$-1\r\n"), 0);
			return;
		}
		char* response = convert_data_to_blk((RespData*)value);
		send(client_fd, response, strlen(response), 0);
		free(response);
		return;
	}

	if (!strcasecmp(command->chars, "INFO")) {
		run_info(client_fd, args);
		return;
	}
	fprintf(stderr, "-ERR unkown command '%s'", command->chars);
	

}

