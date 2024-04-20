#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "parser.h"
#include "data.h"
#include "resp_handler.h"
#include "table.h"
#include "server.h"

#define BUFFER_SIZE 1024
#define MAX_EVENTS 512
#define DEFAULT_PORT "6379"
#define HASH_TABLE_SIZE (uint32_t) 512


#define SET_CMD "SET"
#define GET_CMD "GET"
#define PING_CMD "PING"
#define INFO_CMD "INFO"
#define REPLCONF_CMD "replconf"
#define PSYNC_CMD "psync"
#define ECHO_CMD "ECHO"

int stepInHandshake = -1; // We hold were we are in the handshake

struct ServerMetadata meta_data;
struct MasterMetadata master_meta_data;
hash_table *ht;
int step_at_handshake = -1;

void run_command(int client_fd, BlkStr *command, DataArr* args);

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

int handle_client_connection(int client_fd) {
	/**
	 * We have data on the fd waiting to be read.
	 * We must consume it all because we are running in edge triggered mode.
	*/
	int done = 0;
	for (;;) {
		ssize_t count;
		char buffer[BUFFER_SIZE] = {0};

		count = read(client_fd, buffer, sizeof(buffer));
		if (count == -1) {
			if (errno != EAGAIN) {
				perror("read");
				done = 1;
			}
			break;
		} else if (count == 0) {
			break;
		}
		printf("Recieved message: %s\n", buffer);
		
		char* tmp = buffer;

		RespData* data = parse_resp_data(&tmp);
		
		if(data->type != RESP_ARRAY) {
			printf("Expected array");
			exit(EXIT_FAILURE);
		}

		RespData* command = data->as.arr->values[0];
		if (command->type != RESP_BULK_STRING) {
			printf("Command should be a bulk string");
			exit(EXIT_FAILURE);
		}

		printf("We parsed %s\n", AS_BLK_STR(command)->chars);
		if (!meta_data.is_replica && (strcasecmp(AS_BLK_STR(command)->chars, SET_CMD) == 0)) {
			// We propegate the command.
			for (int i = 0; i < meta_data.replica_count; i++) {
				send(meta_data.replicas_fd[i], buffer, count, 0);
			}
		}

		run_command(client_fd, AS_BLK_STR(command), AS_ARR(data));
		free_data(data);
	}

	return done;
	
}

int main(int argc, char *argv[]) {
	int server_fd, master_fd = -1;
	int s, efd;	
	struct epoll_event event;
	struct epoll_event *events;
	
	meta_data.port = DEFAULT_PORT;
	meta_data.is_replica = false;
	meta_data.replication_offset = 0;
	meta_data.replica_count = 0;

//	char *master_host = NULL;
	//int master_port = -1;

	rand_str(meta_data.replication_id, REPLICATION_ID_LEN + 1);

	ht = hash_table_create(HASH_TABLE_SIZE, hash_string, free_data);

	// Disable output buffering
	setbuf(stdout, NULL);

	// Handle command line arguments
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--port")) {
			meta_data.port = argv[++i];
			continue;
		}
		if (!strcmp(argv[i], "--replicaof")) {
			if (i + 2 >= argc) {
				die("Expected use of --replicaof: <MASTER_HOST> <MASTER_PORT> \n");
			}
			meta_data.is_replica = true;
			master_meta_data.master_host = argv[++i];
			master_meta_data.master_port = atoi(argv[++i]);
		}
	}

	server_fd = create_and_bind(meta_data.port);

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
		die("epoll_create");
	}

	event.data.fd = server_fd;
	event.events = EPOLLIN | EPOLLET;
	s = epoll_ctl(efd, EPOLL_CTL_ADD, server_fd, &event);
	if (s == -1) {
		die("epoll_ctl");
	}
	
	// Buffer where events are returned
	events = calloc(MAX_EVENTS, sizeof(event));

	if (meta_data.is_replica) {
		master_fd = connect_to_master(master_meta_data.master_host, master_meta_data.master_port);
		if (master_fd == -1) {
			die("Couldn't connect to master");
		}

		master_meta_data.master_fd = master_fd;

		s = make_socket_non_blocking(master_fd);
		if (s == -1) {
			printf("Failed to make socket not blocking\n");
			return 1;
		}

		event.data.fd = master_fd;
		event.events = EPOLLIN | EPOLLET;
		s = epoll_ctl(efd, EPOLL_CTL_ADD, master_fd, &event);
		if (s == -1) {
			perror("epoll_ctl");
			return 1;
		}
		char *command[] = {"ping"};
		send_arr_of_bulk_string(master_fd, command, 1);
	}

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
			} else if (meta_data.is_replica && master_meta_data.master_fd == events[i].data.fd) {
				
				int master_fd = events[i].data.fd;
				for (;;) {
					ssize_t count;
					char buffer[BUFFER_SIZE] = {0};
					count = read(master_fd, buffer, sizeof(buffer));
					if (count == -1) {
						if (errno != EAGAIN) {
							perror("read");
						}
						break;
					} else if (count == 0) {
						break;
					}

					char *ptr = buffer;
					// Master doesn't wait for response, so we might have more than one command here, so we keep parsing
					while (*ptr != '\0') {
						if (step_at_handshake == 4) {
							// We got the rdb.
							printf("We got the rdb. rdb is: \n %s \n", ptr);
							ptr++; // Skip the $
							char *prev = ptr;
							while(*ptr != '\r') {
								ptr++;
							}
							*ptr = '\0';
							int len = atoi(prev);
							*ptr = '\r';
							ptr += 2 + len;

							step_at_handshake = 5;
							continue;

						}
						char *prev = ptr;
						RespData *data = parse_resp_data(&ptr);

						if (data->type == RESP_SIMPLE_STRING) {
							// This is the handshake. Here the master waits for a response, so we can just exit after reading.
							if (!strcasecmp(data->as.simple_str, "pong")) { 
								step_at_handshake = 1;
								char *response[] = {REPLCONF_CMD, "listening-port", meta_data.port};
								send_arr_of_bulk_string(master_fd, response, 3);
								free_data(data);
								break;
							} else if (step_at_handshake == 1) {
								step_at_handshake = 2;
								char *response[] = {REPLCONF_CMD, "capa", "psync2"};
								send_arr_of_bulk_string(master_fd, response, 3);
								free_data(data);
								break;
							} else if (step_at_handshake == 2) {
								step_at_handshake = 3;
								char *response[] = {PSYNC_CMD, "?", "-1"};
								send_arr_of_bulk_string(master_fd, response, 3);
								free_data(data);
								break;
							} else if (step_at_handshake == 3) {
								// This is +FULLRESYNC <REPL_ID> 0\r\n
								sscanf(data->as.simple_str, "FULLRESYNC %s %d", master_meta_data.master_replication_id, &master_meta_data.master_offset);
								step_at_handshake = 4;
								free_data(data);
								continue;
							}
						}
						

						if(data->type != RESP_ARRAY) {
							printf("Expected array\n");
							break;
						}

						RespData* command = data->as.arr->values[0];

						if (command->type != RESP_BULK_STRING) {
							printf("Command should be a bulk string\n");
							break;
						}
						printf("We parsed %s\n", AS_BLK_STR(command)->chars);

						run_command(master_fd, AS_BLK_STR(command), AS_ARR(data));
						meta_data.replication_offset += (ptr - prev);
						free_data(data);
					}
					
				}
			} else {
				int done = handle_client_connection(events[i].data.fd);

				if (done == 1) {
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

void send_empty_rdb(int client_fd) {
	char empty_rdb_hex[] = "524544495330303131fa0972656469732d76657205372e322e30fa0a72656469732d62697473c040fa056374696d65c26d08bc65fa08757365642d6d656dc2b0c41000fa08616f662d62617365c000fff06e3bfec0ff5aa2";

	unsigned char *empty_rdb_bin;
	size_t rdb_bin_len = hexs2bin(empty_rdb_hex, &empty_rdb_bin);

	char prefix[32];
	size_t prefix_len = sprintf(prefix, "$%zu\r\n", rdb_bin_len);


	char *rdb_file_response = (char *) malloc((rdb_bin_len + prefix_len) * sizeof(char));
	strcpy(rdb_file_response, prefix);
	memcpy(rdb_file_response + prefix_len, empty_rdb_bin, rdb_bin_len);

	send(client_fd, rdb_file_response, prefix_len + rdb_bin_len, 0);

	free(rdb_file_response);
	free(empty_rdb_bin);
}

void run_set(int socket_fd, RespData *key, RespData *value, DataArr *args, bool should_responde) {
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
		if (should_responde) {
			send_simple_string(socket_fd, "OK");
		}
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
	
	if (should_responde) {
		send_simple_string(socket_fd, "OK");
	}
	
}

void run_info(int client_fd, DataArr *args) {
	int i = 1; // Start of args
	char rawResponse[512];
	rawResponse[0] = '\0';
	while (i < args->length) {
		if (!strcasecmp(AS_BLK_STR(args->values[i])->chars, "Replication")) {
			strcat(rawResponse, "# Replication");
			if (meta_data.is_replica) {
				strcat(rawResponse, "role:slave\n");
			} else {
				strcat(rawResponse, "role:master\n");
			}
			strcat(rawResponse, "master_replid:");
			strcat(rawResponse, meta_data.replication_id);

			strcat(rawResponse, "\nmaster_repl_offset:");
			
			sprintf(rawResponse + strlen(rawResponse), "%d", meta_data.replication_offset);
			// Here we add more values in replication
			i++;
		}
		// Here we add support for more section
	}
	
	send_bulk_string(client_fd, rawResponse);
	
	/*
	char encodedLen[10];
	int lenlen = sprintf(encodedLen, "%ld", strlen(rawResponse));
	send(client_fd, "$", 1, 0);
	send(client_fd, encodedLen, lenlen, 0);
	send(client_fd, "\r\n", 2, 0);
	send(client_fd, rawResponse, strlen(rawResponse), 0);
	send(client_fd, "\r\n", 2, 0);
	*/
}

void run_command(int client_fd, BlkStr *command, DataArr* args) {
	bool should_respond_back = !(meta_data.is_replica && client_fd == master_meta_data.master_fd);

	if(!strcasecmp(command->chars, PING_CMD)) {
		if (should_respond_back) {
			send_bulk_string(client_fd, "PONG");
		}
		return;
	}

	if (!strcasecmp(command->chars, ECHO_CMD)) {
		RespData *arg = args->values[1];
		
		if (should_respond_back) {
			char* response = encode_resp_data(arg);
			send(client_fd, response, strlen(response), 0);
			free(response);
		}
		return;
	}

	if (!strcasecmp(command->chars, SET_CMD)) {
		if (args->length < 3) {
			fprintf(stderr, "Not enough args for SET command. ");
			return;
		}
		RespData *key = args->values[1];
		RespData *value = args->values[2];

		run_set(client_fd, key, value, args, should_respond_back);
		return;
	}

	if (!strcasecmp(command->chars, GET_CMD)) {
		BlkStr* key = AS_BLK_STR(args->values[1]);
		printf("Trying to get with key %s", key->chars);
		void *value = hash_table_get(ht, key->chars);
		if (value == NULL) {
			printf("Got null from get");
			send(client_fd, "$-1\r\n", strlen("$-1\r\n"), 0);
			return;
		}
		char* response = encode_resp_data((RespData*)value);
		send(client_fd, response, strlen(response), 0);
		free(response);
		return;
	}

	if (!strcasecmp(command->chars, INFO_CMD)) {
		run_info(client_fd, args);
		return;
	}

	if (!strcasecmp(command->chars, REPLCONF_CMD)) {
		BlkStr* arg1 = AS_BLK_STR(args->values[1]);

		if (!strcasecmp(arg1->chars, "listening-port")) {
			BlkStr* arg2 = AS_BLK_STR(args->values[2]);

			meta_data.replicas_fd[meta_data.replica_count] = client_fd;
			meta_data.replica_count++;
			printf("Replica is listening on port %s\n", arg2->chars);
			send_simple_string(client_fd, "OK");
		} else if (!strcasecmp(arg1->chars, "capa")) {
			BlkStr* arg2 = AS_BLK_STR(args->values[2]);

			printf("Replica has capability %s\n", arg2->chars);
			send_simple_string(client_fd, "OK");
		} else if (!strcasecmp(arg1->chars, "GETACK")) {
			char replica_offset_str[10];
			sprintf(replica_offset_str, "%d", meta_data.replication_offset);
			char *response[3] = {REPLCONF_CMD, "ACK", replica_offset_str};
			send_arr_of_bulk_string(client_fd, response, 3);
		}
		return;
	}

	if (!strcasecmp(command->chars, PSYNC_CMD)) {
		BlkStr *arg_id = AS_BLK_STR(args->values[1]);
		BlkStr *arg_offset = AS_BLK_STR(args->values[2]);

		char response[REPLICATION_ID_LEN + 20];
		sprintf(response, "FULLRESYNC %s %d", meta_data.replication_id, meta_data.replication_offset);
		send_simple_string(client_fd, response);

		send_empty_rdb(client_fd);
		return;

	}

	fprintf(stderr, "-ERR unkown command '%s'", command->chars);
	

}

