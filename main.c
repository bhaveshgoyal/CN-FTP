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
#include <dirent.h>

// Constants
#define PORT		8888
#define MAXBUF		1024
#define BACKLOG		20

struct network_info {
	int socket_desc;
	struct sockaddr_in conn_addr;
} server, in_client, out_client;

char NICK[MAXBUF], UPLOAD_FOLDER[MAXBUF];

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
	printf("[%s-SERVER]: Upload folder path set to: %s\n", NICK, UPLOAD_FOLDER);
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
	DIR *dir;
	struct dirent *ent;
	char out_message[MAXBUF], client_message[MAXBUF], args[MAXBUF];
	printf("[%s-SERVER]: Client %s:%d connected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));

	// Send some messages to the client
//	message = "Greetings! I am your connection handler\n";
//	write(sock.socket_desc, message, strlen(message));
//	message = "Now type something and i shall repeat what you type\n";
//	write(sock.socket_desc, message, strlen(message));

	// Receive a message from client
	while((read_size = recv(sock.socket_desc, client_message, MAXBUF, 0)) > 0) {
		// End of string marker
		client_message[read_size] = '\0';
		printf("%s\n", client_message);
		if(client_message[0] == '1') {
			sscanf(client_message, "1:%s", args);
			if(strcmp(args, "longlist") == 0) {
				dir = opendir(UPLOAD_FOLDER);
				while((ent = readdir (dir)) != NULL) {
					printf("%s\n", ent->d_name);
					strcpy(out_message, ent->d_name);
					write(sock.socket_desc, out_message, strlen(out_message));
				}
				closedir (dir);
			}
		}
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
	char *out_message, args[MAXBUF], in_message[MAXBUF];
	int option, read_size;
	init_out_client();
	printf("[%s-CLIENT]: Connected to server: %s:%d\n", NICK, inet_ntoa(out_client.conn_addr.sin_addr), PORT);
	while(1) {
		printf("Chose one of the given options:\n");
		printf("1. IndexGet --flag (args)\n");
		printf("2. FileHash --flag (args)\n");
		printf("3. FileDownload --flag (args)\n");
		printf("4. FileUpload --flag (args)\n");
		printf("Enter option: ");
		scanf("%d%s", &option, args);
		if(option == 1) {
			if(strcmp(args, "--longlist") == 0) {
				out_message = "1:longlist";
			}
		}
		printf("writing to socket now\n");
		write(out_client.socket_desc, out_message, strlen(out_message));
		while((read_size = recv(out_client.socket_desc, in_message, MAXBUF, 0)) > 0) {
			printf("%s", in_message);
		}
	}
}

void change_to_absolute_path() {
	char temp_path[MAXBUF];
	int i, j;
	if(UPLOAD_FOLDER[0] == '~') {
		strcpy(temp_path, UPLOAD_FOLDER);
		strcpy(UPLOAD_FOLDER, getenv("HOME"));
		for(i = 1, j = strlen(UPLOAD_FOLDER); temp_path[i] != '\0'; i++, j++) {
			UPLOAD_FOLDER[j] = temp_path[i];
		}
		UPLOAD_FOLDER[j] = '\0';
	}
}

int main(int argc, char *argv[]) {
	int status, option, valid_folder = 0;
	char temp_path[MAXBUF];
	DIR* dir;
	pthread_t server_thread, client_thread;
	printf("Enter nick: ");
	scanf("%s", NICK);
	while(!valid_folder) {
		printf("Enter upload folder path: ");
		scanf("%s", UPLOAD_FOLDER);
		change_to_absolute_path();
		dir = opendir(UPLOAD_FOLDER);
		if(dir) {
			valid_folder = 1;
		}
	}
	pthread_create(&server_thread, NULL, handle_server, NULL);
	pthread_create(&client_thread, NULL, handle_out_client, NULL);
	pthread_join(server_thread, NULL);
	pthread_join(client_thread, NULL);
	return 0;
}
