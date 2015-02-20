#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>

// Constants
#define PORT		8888
#define MAXBUF		1024
#define BACKLOG		20

struct network_info {
	int server_fd, client_fd;
	struct sockaddr_in server_addr, client_addr;
} self, other;

char NICK[MAXBUF];

// Function declarations
void print_error_message(char *message);
void init_server();
void init_client();
void handle_server();
void handle_client();

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
	init_server();
	printf("[%s-SERVER]: Serving at: %s:%d\n", NICK, inet_ntoa(self.server_addr.sin_addr), PORT);
	// Serve forever
	while(1) {
		addr_len = sizeof(other.client_fd);
		other.client_fd = accept(self.server_fd, (struct sockaddr*)&other.client_addr, &addr_len);
		printf("[%s-SERVER]: Client %s:%d connected\n", NICK, inet_ntoa(other.client_addr.sin_addr), ntohs(other.client_addr.sin_port));
		/*---Echo back anything sent---*/
		send(other.client_fd, server_buffer, recv(other.client_fd, server_buffer, MAXBUF, 0), 0);
		/*---Close data connection---*/
		close(other.client_fd);
	}
	close(self.server_fd);
	return;
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
	char client_buffer[MAXBUF];
	init_client();
	printf("[%s-CLIENT]: Connected to server: %s:%d\n", NICK, inet_ntoa(self.client_addr.sin_addr), PORT);
	close(self.client_fd);
}

int main(int argc, char *argv[]) {
	int status, option;
	pid_t pid;
	printf("Enter nick: ");
	scanf("%s", NICK);
	if((pid = fork()) < 0) {
		print_error_message("Failed to fork process\n");
	}
	if(pid == 0) {
		handle_server();
	}
	else {
		handle_client();
	}
	wait(&status);
	return 0;
}
