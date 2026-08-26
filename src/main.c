#include <stdlib.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <limits.h>

#define BUFFER_SIZE 6168

#define DEFAULT_DIR "www"
#define DEFAULT_PORT 8080


//path traversal check
int is_safe_path(const char *base_dir, const char *req_path, char *safe_path, size_t safe_len) {
    char raw_path[PATH_MAX];
    char resolved_base[PATH_MAX];
    char resolved_file[PATH_MAX];

    if (realpath(base_dir, resolved_base) == NULL) {
        return 0; 
    }

    if (strcmp(req_path, "/") == 0) {
        snprintf(raw_path, sizeof(raw_path), "%s/index.html", base_dir);
    } else {
        snprintf(raw_path, sizeof(raw_path), "%s%s", base_dir, req_path);
    }

    if (realpath(raw_path, resolved_file) == NULL) {
        return 0; 
    }

    size_t base_len = strlen(resolved_base);
    if (strncmp(resolved_base, resolved_file, base_len) != 0) {
        return 0; 
    }

    strncpy(safe_path, resolved_file, safe_len - 1);
    safe_path[safe_len - 1] = '\0';

    return 1; 
}


const char* get_file_info(const char* path)
{
	// search for the last point in the path
	const char *ext = strrchr(path, '.');

	if(!ext) return "text/plain";

	if(strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) return "text/html; Charset=UTF-8";
	if(strcmp(ext, ".css") == 0 ) return "text/css";
	if(strcmp(ext, ".js") == 0 ) return "application/javascript";
	if(strcmp(ext, ".png") == 0 ) return "image/png";
	if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
	if(strcmp(ext, ".ico") == 0 ) return "image/x-icon";

	//if the file type isn't here
	return "text/plain";
}


void print_usage(void)
{
	printf("\nusage ./file -p <port number> -f <root site directory>\n");
	printf("	-h,    show this \n\n");
}

int main(int argc, char *argv[])
{



	int port = DEFAULT_PORT;
	char directory[512] = {0};
	strncpy(directory, DEFAULT_DIR, sizeof(directory) - 1);
	directory[sizeof(directory) -1] = '\0';


	if(argc > 1 && strcmp(argv[1], "-h")== 0){
		print_usage();
		return EXIT_SUCCESS;
	}
	

	for(int i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "-p") == 0){
			if (i + 1 < argc) {
				char *endptr;
				long val = strtol(argv[i + 1], &endptr, 10);

				// Controlla se la conversione è fallita o se la porta non è valida
				if (*endptr != '\0' || val <= 0 || val > 65535) {
					fprintf(stderr, "[!] error: the port '%s' needs to be from 1 to 65535\n", argv[i + 1]);
					return EXIT_FAILURE;
				}
			
				port = (int)val;
			}
			else {
				fprintf(stderr, "[!] error: -p flag requires a port number\n");
				return EXIT_FAILURE;
			}
	   }
	
		if(strcmp(argv[i], "-f") == 0)
		{
			if(i + 1 < argc){

				DIR *dir = opendir(argv[i+1]);
				if(dir == NULL){
					fprintf(stderr, "[!] error: the file path '%s' doesn't exist\n", argv[i + 1]);
					return EXIT_FAILURE;
				}

				closedir(dir);
				
				strncpy(directory, argv[i + 1], sizeof(directory) -1);
				directory[sizeof(directory) -1] = '\0';
			}
			else {
				fprintf(stderr, "[!] error: -f flag requires a directory path\n");
				return EXIT_FAILURE;
			}


		}

	}

	int server_sock;
	struct sockaddr_in address;

	//creating an tcp socket using ipv4
	printf("[+] creating socket\n");
	if((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("[!] error: socket opening");
		return EXIT_FAILURE;

	}
	
	//makeing port reusable
	int opt = 1;
	if(setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR,&opt,sizeof(opt)) < 0){
		perror("[!] error: setsockopt");
		close(server_sock);
		return EXIT_FAILURE;
	}



	//assing to value to the strcut for the socket
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);


	printf("[+] bindig socket\n");
	if((bind(server_sock, (struct sockaddr*)&address, sizeof(address)) < 0)){
		perror("error: socket binding");
		close(server_sock);
		return EXIT_FAILURE;

	}

	// use SOMAXCONN if u want the max 10 is fine
	// for a test server
	printf("[+] making socket listen...\n");
	if(listen(server_sock, 10) < 0)
	{
		perror("[!] error: listen");
		close(server_sock);
		return EXIT_FAILURE;
	}

	printf("[+] waiting for connection on http://localhost:%d...\n",port);
	printf("[+] root dir: %s\n", directory);

	//select stuff
	
	// FD_ZERO rm garbage form current sockets
	// FD_SET put out current socket in
	int max_fd = server_sock;
	fd_set current_sockets, ready_socket; 

	FD_ZERO(&current_sockets); 
	FD_SET(server_sock, &current_sockets);


	while(1) {
	
		// select is destructive and it need a copy
		ready_socket = current_sockets;
		int client_sock;
	
		if(select(max_fd + 1, &ready_socket,NULL, NULL, NULL) < 0)
		{
			perror("[!] error: select");
			close(server_sock);
			return EXIT_FAILURE;
		}

		for(int i=0; i <= max_fd; i++)
		{
			if(FD_ISSET(i, &ready_socket)){

				if(i == server_sock)
				{
					struct sockaddr_in client_address;
					socklen_t client_addr_len = sizeof(client_address);

					//get a new connection to server
					client_sock = accept(server_sock, (struct sockaddr*)&client_address, &client_addr_len);
					if(client_sock < 0){
						perror("[!] error: accept");
						close(server_sock);
						return EXIT_FAILURE;
					}

					printf("[+] new connection to server\n");
					FD_SET(client_sock, &current_sockets);

					if(client_sock > max_fd){
						max_fd = client_sock;
					}

				}

				// get data parse http
				else
				{
					//buffer for the request
					char buffer[BUFFER_SIZE] = {0};
	                ssize_t buffer_read = recv(i, buffer, sizeof(buffer) - 1, 0);

					if(buffer_read <= 0)
					{
						// check if it's an error or a close connection
						if(buffer_read == 0){
							printf("[-] client (fd %d) close the connection\n", i);
						}

						else{
							perror("[!] error: recv");
						}

						FD_CLR(i, &current_sockets);
						close(i);
					}
					else{

						char method[16];
						char path[512];
						
						//sscanf do the parsing for Es. GET /index.html
						sscanf(buffer, "%15s %511s", method, path);
						printf("[+] %s request at: %s\n", method, path);

						//we are olnly going to allow the GET request
						if(strcmp(method, "GET") == 0)
						{


							char safe_file_path[1024] = {0};

							if(!is_safe_path(directory, path, safe_file_path, sizeof(safe_file_path)))
							{
								fprintf(stderr,"[!] error: the file path doesn't exist\n");

								const char *body = "<h1>404 File Not Found</h1>";
								char not_found[512];

								snprintf(not_found, sizeof(not_found),
										"HTTP/1.1 404 Not Found\r\n"
										"Content-Type: text/html\r\n"
										"Content-Length: %zu\r\n"   
										"Connection: close\r\n"
										"\r\n"
										"%s",
										strlen(body), body);

								send(i, not_found, strlen(not_found), 0);

							}
							else
							{

								FILE *file = fopen(safe_file_path, "rb");

								if(file != NULL){
									fseek(file ,0,SEEK_END);
									long file_size = ftell(file);
									fseek(file ,0,SEEK_SET);



									//HTTP RESPONSE//

									//buffer fort the http response
									char http_header[BUFFER_SIZE];

									snprintf(http_header, sizeof(http_header), 
											"HTTP/1.1 200 OK\r\n"
											"Content-Type: %s\r\n"
											"Content-Length: %ld\r\n" 
											"Connection: close\r\n"
											"\r\n"                   
											,get_file_info(safe_file_path),file_size);

									//send the header first
									send(i,http_header, strlen(http_header), 0);

									char GET_file_buffer[BUFFER_SIZE];

									ssize_t read_bytes;

									while((read_bytes = fread(GET_file_buffer, 1, sizeof(GET_file_buffer), file)) > 0)
									{
										send(i, GET_file_buffer, read_bytes, 0);
									}
									fclose(file);
								}
								else {
									//if the file is valid but the server can't open it
                                    const char *error_500 = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                                    send(i, error_500, strlen(error_500), 0);
                                }
							}
						}
						else{
							//since only GET is allow if the requesst
							//methon is something else this will show

							const char *not_allowed_verb =  "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
							send(i, not_allowed_verb, strlen(not_allowed_verb), 0);
						}


						FD_CLR(i, &current_sockets);
						close(i);
					}

				}
				
			}

		}
		


	}

	

	close(server_sock);
	return EXIT_SUCCESS;
}
