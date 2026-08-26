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

#define BUFFER_SIZE 6168

#define DEFAULT_DIR "www"
#define DEFAULT_PORT 8080

// TODO: put default port and directory + use args and get files

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
	char directory[512] = DEFAULT_DIR;


	if(argc == 1){
		printf("\ndo ./file -h to fort app usage\n\n");
		return EXIT_FAILURE;
	}

	if(argc > 1 && strcmp(argv[1], "-h")== 0){
		print_usage();
		return EXIT_SUCCESS;
	}
	

	for(int i = 0; i < argc; i++)
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


	//da cambiare

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
	printf("[+] root dir: %s", directory);

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

				// Caso 1: Nuova connessione in ingresso sul server
				if(i == server_sock)// questo vuol dire che nel socket del server principale c'é una richiesta di connessione
				{
					//servono solo per accept
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
						sscanf(buffer, "%s %s", method, path);

						printf("[+] %s request at: %s\n", method, path);

						//we are olnly going to allow the GET request
						if(strcmp(method, "GET") == 0)
						{

							//html body on the browser
							// da cambiare con  file reading 
							// const char *html_body = 
							// 	"<!DOCTYPE html>"
							// 	"<html>"
							// 	"<head><title>Mio Server C</title></head>"
							// 	"<body style='font-family: sans-serif; text-align: center; margin-top: 50px;'>"
							// 	"  <h1 style='color: #2b5797;'>HTML Servito con Successo dal Server C!</h1>"
							// 	"  <p>Hai effettuato una richiesta <b>GET</b> sulla rotta: <i>%s</i></p>"
							// 	"</body>"
							// 	"</html>";
							//
							
							//buffer fort the http response
							char http_response[BUFFER_SIZE * 2];

							snprintf(http_response, sizeof(http_response), 
								"HTTP/1.1 200 OK\r\n"
								"Content-Type: text/html; charset=UTF-8\r\n"
								"Content-Length: %zu\r\n" 
								"Connection: close\r\n"
								"\r\n"                   
								"%s"
								,strlen(html_body), html_body);

							//send the response back to the client
							send(i,http_response, strlen(http_response), 0);
							
						}

						else
						{
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
