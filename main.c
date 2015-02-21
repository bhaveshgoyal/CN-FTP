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
	int socket_desc;
	struct sockaddr_in conn_addr;
} server, in_client, out_client;

char NICK[MAXBUF];

// Function declarations
void print_error_message(char *message);
void init_server();
void init_out_client();
void handle_server();
void handle_out_client();
void handle_in_client(void *);

void print_error_message(char *message) {
	fprintf(stderr, message);
//	exit(EXIT_FAILURE);
}

void init_server() {
	int yes = 1;
	char error_message[MAXBUF];

	// Initialize IP address / port structure
	bzero(&server.conn_addr, sizeof(struct sockaddr_in));
	server.conn_addr.sin_family = AF_INET;
	server.conn_addr.sin_port = htons(PORT);
	server.conn_addr.sin_addr.s_addr = INADDR_ANY;

	// Create the socket
	if((server.socket_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(error_message, "Error creating socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Reuse local address
	if(setsockopt(server.socket_desc, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
		sprintf(error_message, "Error in local address reuse: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Bind the socket
	if(bind(server.socket_desc, (struct sockaddr*)&server.conn_addr, sizeof(server.conn_addr)) != 0) {
		sprintf(error_message, "Error binding socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Make it a listening socket
	if(listen(server.socket_desc, BACKLOG) != 0) {
		sprintf(error_message, "Error in listening socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}
}

void handle_server() {
	char server_buffer[MAXBUF];
	socklen_t addr_len;
	pthread_t conn_thread;
	init_server();
	printf("[%s-SERVER]: Serving at: %s:%d\n", NICK, inet_ntoa(server.conn_addr.sin_addr), PORT);
	addr_len = sizeof(struct sockaddr_in);
	FILE *in_clients = fopen("incoming_clients", "w");
	// Serve forever
	while(1) {
		in_client.socket_desc = accept(server.socket_desc, (struct sockaddr*)&in_client.conn_addr, &addr_len);
		fprintf(in_clients, "%s:%d\n", inet_ntoa(in_client.conn_addr.sin_addr), ntohs(in_client.conn_addr.sin_port));
		fflush(in_clients);
		pthread_create(&conn_thread, NULL, handle_in_client, (void *)&in_client);
	}
	return;
}

void handle_in_client(void *socket_info) {
	// Get the socket descriptor
	struct network_info sock = *(struct network_info *)socket_info;
	int read_size;
	char *message , client_message[MAXBUF];
	printf("[%s-SERVER]: Client %s:%d connected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));

	// Send some messages to the client
	message = "Greetings! I am your connection handler\n";
	write(sock.socket_desc, message, strlen(message));
	message = "Now type something and i shall repeat what you type \n";
	write(sock.socket_desc, message, strlen(message));

	// Receive a message from client
	while((read_size = recv(sock.socket_desc, client_message, MAXBUF, 0)) > 0) {
		// End of string marker
		client_message[read_size] = '\0';
		// Send the message back to client
		write(sock.socket_desc, client_message, strlen(client_message));
		// Clear the message buffer
		memset(client_message, 0, MAXBUF);
	}
	printf("[%s-SERVER]: Client %s:%d disconnected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));
}

void init_out_client() {
	int connected = 0, valid_server_ip = 0;
	char error_message[MAXBUF], server_ip[MAXBUF];
	// Initialize IP address / port structure
	bzero(&out_client.conn_addr, sizeof(struct sockaddr_in));
	out_client.conn_addr.sin_family = AF_INET;
	out_client.conn_addr.sin_port = htons(PORT);

	// Create the socket
	if((out_client.socket_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(error_message, "Error creating socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	while(!valid_server_ip || !connected) {
		printf("Enter server IP: ");
		scanf("%s", server_ip);
		if(inet_aton(server_ip, &out_client.conn_addr.sin_addr) == 0) {
			valid_server_ip = 0;
			printf("Error: Not a valid IP\n");
		}
		else {
			valid_server_ip = 1;
			if(connect(out_client.socket_desc, (struct sockaddr*)&out_client.conn_addr, sizeof(struct sockaddr_in)) == 0) {
				connected = 1;
			}
			else {
				connected = 0;
				printf("Error: Server seems to be down\n");
			}
		}
	}
}

void handle_out_client() {
	char client_buffer[MAXBUF], option[MAXBUF];
	init_out_client();
	printf("[%s-CLIENT]: Connected to server: %s:%d\n", NICK, inet_ntoa(out_client.conn_addr.sin_addr), PORT);
	while(1) {
		printf("Enter option bro\n");
		scanf("%s", option);
	}
}

int main(int argc, char *argv[]) {
	int status, option;
	pthread_t server_thread, client_thread;
	printf("Enter nick: ");
	scanf("%s", NICK);
	pthread_create(&server_thread, NULL, handle_server, NULL);
	pthread_create(&client_thread, NULL, handle_out_client, NULL);
	pthread_join(server_thread, NULL);
	pthread_join(client_thread, NULL);
	return 0;
}
