#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>

// Constants
#define PORT		8888
#define MAXBUF		1024
#define BACKLOG		20

struct network_info {
	int server_fd, client_fd;
	struct sockaddr_in server_addr, client_addr;
} self, other;

struct machine_info {
	int socket_desc;
	struct sockaddr_in conn_addr;
} machine_info;

char NICK[MAXBUF];

// Function declarations
void print_error_message(char *message);
void init_server();
void init_client();
void handle_server();
void handle_client();
void incoming_connection_handler(void *);

void print_error_message(char *message) {
	fprintf(stderr, message);
//	exit(EXIT_FAILURE);
}

void init_server() {
	int yes = 1;
	char error_message[MAXBUF];

	// Initialize IP address / port structure
	bzero(&self.server_addr, sizeof(self.server_addr));
	self.server_addr.sin_family = AF_INET;
	self.server_addr.sin_port = htons(PORT);
	self.server_addr.sin_addr.s_addr = INADDR_ANY;

	// Create the socket
	if((self.server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(error_message, "Error creating socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Reuse local address
	if(setsockopt(self.server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		sprintf(error_message, "Error in local address reuse: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Bind the socket
	if(bind(self.server_fd, (struct sockaddr*)&self.server_addr, sizeof(self.server_addr)) != 0) {
		sprintf(error_message, "Error binding socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Make it a listening socket
	if(listen(self.server_fd, BACKLOG) != 0) {
		sprintf(error_message, "Error in listening socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}
}

void handle_server() {
	char server_buffer[MAXBUF];
	socklen_t addr_len;
	pthread_t conn_thread;
	init_server();
	printf("[%s-SERVER]: Serving at: %s:%d\n", NICK, inet_ntoa(self.server_addr.sin_addr), PORT);
	addr_len = sizeof(other.client_fd);
	// Serve forever
	while(1) {
		other.client_fd = accept(self.server_fd, (struct sockaddr*)&other.client_addr, &addr_len);
		machine_info.socket_desc = other.client_fd;
		machine_info.conn_addr = other.client_addr;
		pthread_create(&conn_thread, NULL, incoming_connection_handler, (void *)&machine_info);
	}
	return;
}

void incoming_connection_handler(void *socket_info) {
	//Get the socket descriptor
	struct machine_info sock = *(struct machine_info *)socket_info;
	int read_size;
	char *message , client_message[MAXBUF];
	printf("[%s-SERVER]: Client %s:%d connected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));
	//Send some messages to the client
	message = "Greetings! I am your connection handler\n";
	write(sock.socket_desc, message, strlen(message));
	message = "Now type something and i shall repeat what you type \n";
	write(sock.socket_desc, message, strlen(message));
	//Receive a message from client
	while((read_size = recv(sock.socket_desc, client_message, MAXBUF, 0)) > 0) {
		//end of string marker
		client_message[read_size] = '\0';
		//Send the message back to client
		write(sock.socket_desc, client_message, strlen(client_message));
		//clear the message buffer
		memset(client_message, 0, MAXBUF);
	}
	if(read_size == 0) {
		printf("[%s-SERVER]: Client %s:%d disconnected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));
		fflush(stdout);
	}
	else if(read_size == -1) {
		perror("recv failed");
	}
}

void init_client() {
	int connected = 0, valid_server_ip = 0;
	char error_message[MAXBUF], server_ip[MAXBUF];
	// Initialize IP address / port structure
	bzero(&self.client_addr, sizeof(self.client_addr));
	self.client_addr.sin_family = AF_INET;
	self.client_addr.sin_port = htons(PORT);

	// Create the socket
	if((self.client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(error_message, "Error creating socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	while(!valid_server_ip || !connected) {
		printf("Enter server IP: ");
		scanf("%s", server_ip);
		if(inet_aton(server_ip, &self.client_addr.sin_addr) == 0) {
			valid_server_ip = 0;
			printf("Error: Not a valid IP\n");
		}
		else {
			valid_server_ip = 1;
			if(connect(self.client_fd, (struct sockaddr*)&self.client_addr, sizeof(self.client_addr)) == 0) {
				connected = 1;
			}
			else {
				connected = 0;
				printf("Error: Server seems to be down\n");
			}
		}
	}
}

void handle_client() {
	char client_buffer[MAXBUF], option[MAXBUF];
	init_client();
	printf("[%s-CLIENT]: Connected to server: %s:%d\n", NICK, inet_ntoa(self.client_addr.sin_addr), PORT);
	while(1) {
		printf("Enter option bro\n");
		scanf("%s", option);
	}
	close(self.client_fd);
}

int main(int argc, char *argv[]) {
	int status, option;
	pthread_t server_thread, client_thread;
	printf("Enter nick: ");
	scanf("%s", NICK);
	pthread_create(&server_thread, NULL, handle_server, NULL);
	pthread_create(&client_thread, NULL, handle_client, NULL);
	pthread_join(server_thread, NULL);
	pthread_join(client_thread, NULL);
	return 0;
}
