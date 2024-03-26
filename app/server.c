#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_SIZE 1024

void *handle_client(void *arg);

int main() {
	char buffer[BUFFER_SIZE] = {0};
	int server_fd, client_addr_len, new_socket;
	struct sockaddr_in client_addr;
	pthread_t tid;
	// Disable output buffering
	setbuf(stdout, NULL);
	
	server_fd = socket(AF_INET, SOCK_STREAM, 0);

	if (server_fd == -1) {
		printf("Socket creation failed: %s...\n", strerror(errno));
		return 1;
	}
	
	// // Since the tester restarts your program quite often, setting SO_REUSEADDR
	// // ensures that we don't run into 'Address already in use' errors
	int reuse = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		printf("SO_REUSEADDR failed: %s \n", strerror(errno));
		return 1;
	}
	
	struct sockaddr_in serv_addr = { .sin_family = AF_INET ,
									 .sin_port = htons(6379),
									 .sin_addr = { htonl(INADDR_ANY) },
									};
	
	if (bind(server_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) != 0) {
		printf("Bind failed: %s \n", strerror(errno));
		return 1;
	}
	
	int connection_backlog = 5;
	if (listen(server_fd, connection_backlog) != 0) {
		printf("Listen failed: %s \n", strerror(errno));
		return 1;
	}
	
	printf("Waiting for a client to connect...\n");
	client_addr_len = sizeof(client_addr);
	
	for (;;) {
		if ((new_socket = accept(server_fd, (struct sockaddr *)&client_addr, (socklen_t*)&client_addr_len)) < 0) {
			perror("Accept failed.");
			exit(EXIT_FAILURE);
		}

		if (pthread_create(&tid, NULL, handle_client, (void *)&new_socket) != 0) {
			perror("Thread creation failed.");
			exit(EXIT_FAILURE);
		}

		pthread_detach(tid);
	}

	//Added comment
	
	close(server_fd);

	return 0;
}

void *handle_client(void *arg) {
	int client_socket = *((int *)arg);
	char buffer[BUFFER_SIZE] = {0};

	for (;;) {
		memset(buffer, 0, BUFFER_SIZE);
		if (read(client_socket, buffer, BUFFER_SIZE) == 0) {
			printf("Client disconnected\n");
			break;
		}
		printf("Recieved message: %s\n", buffer);
		char* responsePing = "+PONG\r\n";
		send(client_socket, responsePing, strlen(responsePing), 0);
	}

	close(client_socket);
	pthread_exit(NULL);
}
