#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <errno.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <openssl/md5.h>
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
	int read_size, len, i, option, reti, temp_size;
	DIR *dir;
	regex_t regex;
	struct dirent *ent;
	struct stat buf;
	time_t t1, t2, file_t;
	struct tm tm1, tm2;
	struct tm *timeptr, tm_file;
	char start_stamp[MAXBUF], end_stamp[MAXBUF], file_stamp[MAXBUF];
	char out_message[MAXBUF], client_message[MAXBUF], flag[MAXBUF], args[MAXBUF], filename[MAXBUF], *file_type, in_message[MAXBUF];
	printf("[%s-SERVER]: Client %s:%d connected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));

	// Receive a message from client
	while((read_size = recv(sock.socket_desc, client_message, MAXBUF, 0)) > 0) {
		// End of string marker
		client_message[read_size] = '\0';
//		printf("%s\n", client_message);
		for(i = 0, len = 1; client_message[i] != '\0'; i++) {
			if(client_message[i] == '#') {
				client_message[i] = ' ';
				len++;
			}
		}
		if(len == 2) {
			sscanf(client_message, "%d %s", &option, flag);
			printf("two\n");
		}
		// else if (len == 4){
		// sscanf(client_message, "%d %s %s %s", &option, flag, start_stamp, end_stamp);
		// 	printf("four\n");

		// }
		else {
			sscanf(client_message, "%d %s %99[^\n]", &option, flag, args);
			// printf("%s",args);
		}
		if(option == 1) {
			if(strcmp(flag, "longlist") == 0) {
				dir = opendir(UPLOAD_FOLDER);
				while((ent = readdir(dir)) != NULL) {
					if(ent->d_type == DT_REG)
						file_type = "regular";
					else if(ent->d_type == DT_DIR)
						file_type = "directory";
					else if(ent->d_type == DT_FIFO)
						file_type = "FIFO";
					else if(ent->d_type == DT_SOCK)
						file_type = "socket";
					else if(ent->d_type == DT_LNK)
						file_type = "symlink";
					else if(ent->d_type == DT_BLK)
						file_type = "block dev";
					else if(ent->d_type == DT_CHR)
						file_type = "char dev";
					else
						file_type = "???";
					sprintf(filename, "%s%s", UPLOAD_FOLDER, ent->d_name);
					stat(filename, &buf);
					memset(out_message, 0, sizeof(out_message));
					sprintf(out_message, "%s\t%lld\t%ld\t%s\n", ent->d_name, buf.st_size, buf.st_atime, file_type);
					send(sock.socket_desc, out_message, strlen(out_message), 0);
				}
				closedir(dir);
			}
			if (strcmp(flag, "shortlist") == 0){
					for(i = 0;args[i] != '\0'; i++) {
						start_stamp[i] = args[i];
						if((args[i] == 'P' && args[i+1] == 'M') || args[i] == 'A' && args[i+1] == 'M') {
						start_stamp[i+1] = 'M';
						break;
						}
					}
					int index = i + 3;
					int index_sub = 0;
					for(i = index;args[i] != '\0'; i++) {
						end_stamp[index_sub] = args[i];
						if((args[i] == 'P' && args[i+1] == 'M') || args[i] == 'A' && args[i+1] == 'M') {
						end_stamp[index_sub+1] = 'M';
						break;
						}
						index_sub += 1;
					}
					// printf("%s %s\n",start_stamp, end_stamp);
				if(strptime(start_stamp, "%b %d %Y %I:%M:%S %p",&tm1) == NULL)
            			sprintf(stderr,"\nString Parsing Failed\n");          
    			if(strptime(end_stamp, "%b %d %Y %I:%M:%S %p",&tm2) == NULL)
            			sprintf(stderr,"\nString PArsing Failed\n");

            	t1 = mktime(&tm1);
    			t2 = mktime(&tm2);
				dir = opendir(UPLOAD_FOLDER);

				while((ent = readdir(dir)) != NULL) {
				
					sprintf(filename, "%s%s", UPLOAD_FOLDER, ent->d_name);
					stat(filename, &buf);

    				timeptr = localtime(&buf.st_mtime);
            		strftime(file_stamp, sizeof(file_stamp),"%b %d %Y %I:%M:%S %p",timeptr);
            		strptime(file_stamp, "%b %d %Y %I:%M:%S %p",&tm_file);
            		file_t = mktime(&tm_file);
            
            		if (cmptime(file_t,t1) > 0 && cmptime(t2,file_t) > 0){
   						memset(out_message, 0, sizeof(out_message));
						sprintf(out_message, "%s\t%lld\t%s\n", ent->d_name, buf.st_size, file_stamp);
						send(sock.socket_desc, out_message, strlen(out_message), 0);
					}
				
				}
				closedir(dir);
			
			}
			if (strcmp(flag,"regex") == 0){

				dir = opendir(UPLOAD_FOLDER);
				while((ent = readdir(dir)) != NULL) {
					if(ent->d_type == DT_REG)
						file_type = "regular";
					else if(ent->d_type == DT_DIR)
						file_type = "directory";
					else if(ent->d_type == DT_FIFO)
						file_type = "FIFO";
					else if(ent->d_type == DT_SOCK)
						file_type = "socket";
					else if(ent->d_type == DT_LNK)
						file_type = "symlink";
					else if(ent->d_type == DT_BLK)
						file_type = "block dev";
					else if(ent->d_type == DT_CHR)
						file_type = "char dev";
					else
						file_type = "???";
					sprintf(filename, "%s%s", UPLOAD_FOLDER, ent->d_name);
					stat(filename, &buf);
					memset(out_message, 0, sizeof(out_message));
					

					/* Compile regular expression */
					reti = regcomp(&regex, args, 0);
					if (reti) {
    					fprintf(stderr, "Could not compile regex\n");
    					exit(1);
					}

					/* Execute regular expression */
					reti = regexec(&regex, ent->d_name, 0, NULL, 0);
					
					if (!reti) {
    					sprintf(out_message, "%s\t%lld\t%ld\t%s\n", ent->d_name, buf.st_size, buf.st_atime, file_type);
						send(sock.socket_desc, out_message, strlen(out_message), 0);
    				}
					
					regfree(&regex);
					
				}
				closedir(dir);
			}
		}
		else if(option == 2) {
			if(strcmp(flag, "verify") == 0) {
				char filename[MAXBUF];
				sprintf(filename, "%s%s", UPLOAD_FOLDER, args);
				unsigned char c[MD5_DIGEST_LENGTH];
				FILE *inFile = fopen(filename, "rb");
				MD5_CTX mdContext;
				int bytes, i;
				unsigned char data[MAXBUF];
				memset(out_message, 0, sizeof(out_message));
				memset(c, 0, sizeof(c));
				if(inFile == NULL) {
					sprintf(out_message, "no");
					send(sock.socket_desc, out_message, strlen(out_message), 0);
				}
				else {
					sprintf(out_message, "yes");
					printf("%s\n", out_message);
					send(sock.socket_desc, out_message, strlen(out_message), 0);
					MD5_Init(&mdContext);
					while((bytes = fread (data, 1, MAXBUF, inFile)) != 0)
						MD5_Update(&mdContext, data, bytes);
					MD5_Final(c,&mdContext);
					send(sock.socket_desc, c, strlen(c), 0);
				}
				fclose(inFile);
			}
		}
		else if(option == 3) {
			if(strcmp(flag, "tcp") == 0) {
				char filename[MAXBUF];
				sprintf(filename, "%s%s", UPLOAD_FOLDER, args);
				printf("%s\n", filename);
				FILE *fp = fopen(filename, "rb");
				memset(out_message, 0, sizeof(out_message));
				if(fp == NULL) {
					sprintf(out_message, "no");
					send(sock.socket_desc, out_message, strlen(out_message), 0);
				}
				else {
					sprintf(out_message, "yes#%s", args);
					send(sock.socket_desc, out_message, strlen(out_message), 0);
					memset(out_message, 0, sizeof(out_message));
					while(fgets(out_message, sizeof(out_message), fp)){
						send(sock.socket_desc, out_message, strlen(out_message), 0);
						memset(out_message, 0, sizeof(out_message));
					}
				}
				fclose(fp);
			}
		}
		else if(option == 4) {
			if(strcmp(flag, "tcp") == 0) {
				char choice[MAXBUF], filename[MAXBUF];
				printf("Want the file?(yes/no) :%s\n", args);
				scanf("%s", choice);
				memset(out_message, 0, sizeof(out_message));
				if(strcmp(choice, "yes") == 0) {
					sprintf(out_message, "yes");
					send(sock.socket_desc, out_message, strlen(out_message), 0);
					sprintf(filename, "%s%s", UPLOAD_FOLDER, args);
					FILE *fp = fopen(filename, "wb");
					printf("Started writing to file: %s\n", filename);
		/*			memset(in_message, 0, sizeof(in_message));
					while((temp_size = recv(out_client.socket_desc, client_message, MAXBUF, 0)) > 0) {
						printf("%s", in_message);
						fputs(in_message, fp);
						memset(in_message, 0, sizeof(in_message));
					}*/
					fclose(fp);
					printf("Finished writing to file: %s\n", filename);
				}
				else {
					sprintf(out_message, "no");
					send(sock.socket_desc, out_message, strlen(out_message), 0);
				}
			}
		}
	}
	printf("[%s-SERVER]: Client %s:%d disconnected\n", NICK, inet_ntoa(sock.conn_addr.sin_addr), ntohs(sock.conn_addr.sin_port));
}

void init_out_client() {
	int connected = 0, valid_server_ip = 0;
	char error_message[MAXBUF], server_ip[MAXBUF];
	struct timeval tv;
	// Initialize IP address / port structure
	bzero(&out_client.conn_addr, sizeof(struct sockaddr_in));
	out_client.conn_addr.sin_family = AF_INET;
	out_client.conn_addr.sin_port = htons(PORT);

	// Create the socket
	if((out_client.socket_desc = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		sprintf(error_message, "Error creating socket: %s\n", strerror(errno));
		print_error_message(error_message);
	}

	// Set timeout for recv
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(out_client.socket_desc, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

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
	char out_message[MAXBUF], flag[MAXBUF], args[MAXBUF], in_message[MAXBUF];
	char stamp[MAXBUF], regex_exp[MAXBUF];
	int option, read_size, i;
	init_out_client();
	printf("[%s-CLIENT]: Connected to server: %s:%d\n", NICK, inet_ntoa(out_client.conn_addr.sin_addr), PORT);
	while(1) {
		printf("Chose one of the given options:\n");
		printf("1. IndexGet --flag (args)\n");
		printf("2. FileHash --flag (args)\n");
		printf("3. FileDownload --flag (args)\n");
		printf("4. FileUpload --flag (args)\n");
		printf("5. Connect to different server\n");
		printf("Enter option: ");
		scanf("%d", &option);
		memset(out_message, 0, sizeof(out_message));
		memset(in_message, 0, sizeof(in_message));
		if(option == 1) {
			scanf("%s", flag);
			if(strcmp(flag, "--longlist") == 0) {
				sprintf(out_message, "1#longlist");
				printf("[%s-CLIENT]: Listing of the shared folder from: %s:%d\n", NICK, inet_ntoa(out_client.conn_addr.sin_addr), PORT);
				printf("File: Name\tSize\tTimestamp\tType\n");
			}
			// Time Stamp Format <Month Abbreviated> date yr <12hr time seperated by :> AM/PM
			if(strcmp(flag, "--shortlist") == 0){
				scanf("%99[^\n]",stamp);
				sprintf(out_message, "1#shortlist#%s",stamp);
				printf("Listing Based On Time Stamps %s\n" ,stamp);
			}
			if(strcmp(flag,"--regex") == 0){
				scanf("%99[^\n]",regex_exp);
				sprintf(out_message,"1#regex#%s",regex_exp);
				printf("Listing Based on Regex %s\n",regex_exp);
			}
			send(out_client.socket_desc, out_message, strlen(out_message), 0);
			while((read_size = recv(out_client.socket_desc, in_message, MAXBUF, 0)) > 0) {
				printf("%s", in_message);
				memset(in_message, 0, sizeof(in_message));
			}
		}
		else if(option == 2) {
			scanf("%s", flag);
			if(strcmp(flag, "--verify") == 0) {
				scanf("%s", args);
				sprintf(out_message, "2#verify#%s", args);
				printf("%s\n", out_message);
			}
			send(out_client.socket_desc, out_message, strlen(out_message), 0);
			read_size = recv(out_client.socket_desc, in_message, MAXBUF, 0);
			printf("%s\n", in_message);
			if(strcmp(in_message, "no") == 0) {
				printf("[%s-CLIENT]: Sorry, incorrect file name\n", NICK);
			}
			else if(strcmp(in_message, "yes") == 0) {
				printf("cool thing\n");
				memset(in_message, 0, sizeof(in_message));
				read_size = recv(out_client.socket_desc, in_message, MAXBUF, 0);

				char filename[MAXBUF];
				sprintf(filename, "%s%s", UPLOAD_FOLDER, args);
				unsigned char c[MD5_DIGEST_LENGTH];
				FILE *inFile = fopen(filename, "rb");
				MD5_CTX mdContext;
				int bytes, i;
				unsigned char data[MAXBUF];
				memset(c, 0, sizeof(c));
				if(inFile == NULL) {
					printf("[%s-CLIENT]: Sorry, file is not present on this machine\n", NICK);
				}
				else {
					MD5_Init(&mdContext);
					while((bytes = fread (data, 1, MAXBUF, inFile)) != 0)
						MD5_Update(&mdContext, data, bytes);
					MD5_Final(c,&mdContext);
					if(strcmp(c, in_message) == 0) {
						printf("[%s-CLIENT]: Files verified\n", NICK);
					}
					else {
						printf("[%s-CLIENT]: Files are not the same\n", NICK);
					}
				}
				fclose(inFile);

			}
		}
		else if(option == 3) {
			scanf("%s%s", flag, args);
			char filename[MAXBUF], temp[MAXBUF];
			if(strcmp(flag, "--tcp") == 0) {
				memset(out_message, 0, sizeof(out_message));
				memset(in_message, 0, sizeof(in_message));
				memset(filename, 0, sizeof(filename));
				memset(temp, 0, sizeof(temp));
				sprintf(out_message, "3#tcp#%s", args);
				send(out_client.socket_desc, out_message, strlen(out_message), 0);
				recv(out_client.socket_desc, in_message, MAXBUF, 0);
				if(strcmp(in_message, "no") == 0) {
					printf("[%s-CLIENT]: Sorry, file does not exist\n", NICK);
				}
				else {
					sscanf(in_message, "yes#%s", temp);
					sprintf(filename, "%s%s", UPLOAD_FOLDER, temp);
					printf("[%s-CLIENT]: Started writing to file: %s\n", NICK, filename);
					FILE *fp = fopen(filename, "wb");
					memset(in_message, 0, sizeof(in_message));
					while((read_size = recv(out_client.socket_desc, in_message, MAXBUF, 0)) > 0) {
						fputs(in_message, fp);
						memset(in_message, 0, sizeof(in_message));
					}
					fclose(fp);
					printf("[%s-CLIENT]: Finished writing to file: %s\n", NICK, filename);
				}
			}
		}
		else if(option == 4) {
			scanf("%s%s", flag, args);
			char filename[MAXBUF];
			if(strcmp(flag, "--tcp") == 0) {
				sprintf(filename, "%s%s", UPLOAD_FOLDER, args);
				FILE *fp = fopen(filename, "rb");
				if(fp == NULL) {
					printf("[%s-CLIENT]: Sorry, file does not exist: %s\n", NICK, filename);
				}
				else {
					memset(out_message, 0, sizeof(out_message));
					memset(in_message, 0, sizeof(in_message));
					sprintf(out_message, "4#tcp#%s", args);
					send(out_client.socket_desc, out_message, strlen(out_message), 0);
					printf("%s\n", out_message);
					while((read_size = recv(out_client.socket_desc, in_message, MAXBUF, 0)) <= 0) {
						memset(in_message, 0, sizeof(in_message));
					}
					printf("%s\n", in_message);
					if(strcmp(in_message, "yes") == 0) {
						printf("[%s-CLIENT]: File upload accepted: %s\n", NICK, filename);
						memset(out_message, 0, sizeof(out_message));
						while(fgets(out_message, sizeof(out_message), fp)){
							printf("%s", out_message);
					//		send(out_client.socket_desc, out_message, strlen(out_message), 0);
							memset(out_message, 0, sizeof(out_message));
						}
					}
					else {
						printf("[%s-CLIENT]: Sorry, file upload denied: %s\n", NICK, filename);
					}
				}
				fclose(fp);
			}
		}
		else if(option == 5) {
			init_out_client();
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
		if(UPLOAD_FOLDER[j-1] != '/') {
			UPLOAD_FOLDER[j++] = '/';
		}
		UPLOAD_FOLDER[j] = '\0';
	}
}

int cmptime(time_t time1,time_t time2){
 return difftime(time1,time2) > 0.0 ? 1 : -1; 
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
