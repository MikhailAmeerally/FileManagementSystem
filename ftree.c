#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <libgen.h>
#include <dirent.h>
#include "hash.h"
#include "ftree.h"



#define MAX_BACKLOG 100
#define MAX_CONNECTIONS 50
#define BUF_SIZE 128




struct sockname 
{
	int sock_fd;
	struct fileinfo file_info;
	size_t bytes_read;
	int type;
};

/* Accept a connection. Note that a new file descriptor is created for
 * communication with the client. The initial socket descriptor is used
 * to accept connections, but the new socket is used to communicate.
 * Return the new client's file descriptor or -1 on error.
 */
int accept_connection(int fd, struct sockname *usernames)
{
    int user_index = 0; 
    while (user_index < MAX_CONNECTIONS && usernames[user_index].sock_fd != -1) {
        user_index++;
    }
	int client_fd = accept(fd, NULL, NULL);
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    if (user_index == MAX_CONNECTIONS) {
        fprintf(stderr, "server: max concurrent connections\n");
        close(client_fd);
        return -1;
    }
    usernames[user_index].sock_fd = client_fd;
	usernames[user_index].bytes_read = 0;
	usernames[user_index].type = -1;
	usernames[user_index].file_info.hash[0] = '\0';
	usernames[user_index].file_info.path[0] = '\0';
	usernames[user_index].file_info.mode = 0;
	usernames[user_index].file_info.size = -1;
    return client_fd;
}


int read_from(int client_index, struct sockname *sockets)
{
    int fd = sockets[client_index].sock_fd;

    char buf_path[BUF_SIZE + 1], buf_hash[BLOCKSIZE+1];
    int size, type, num_read = 0;
    mode_t buf_mode;
    size_t ns;

    //Get the type.
    if(sockets[client_index].type == -1) 
    {
	    num_read = read(fd, &type, sizeof(int));
	    sockets[client_index].type = type;
	    if(num_read == -1)
	    {
	    	sockets[client_index].sock_fd = -1;
	    	return fd;
	    }
	} 
	else if(sockets[client_index].file_info.path[0] == '\0') 
	{
	    //Save the path.
	    num_read = read(fd, &buf_path, MAXPATH);
	    buf_path[num_read] = '\0';
	    strcpy(sockets[client_index].file_info.path, buf_path);

	    if (num_read == -1)
	    {
	        sockets[client_index].sock_fd = -1;
	        return fd;
	    }
	} 
	else if(sockets[client_index].file_info.mode == 0) 
	{
		//Get the mode.
	    num_read = read(fd, &buf_mode, sizeof(mode_t));
	    sockets[client_index].file_info.mode = buf_mode;
	    if (num_read == -1)
	    {
	    	sockets[client_index].sock_fd = -1;
	    	return fd;
	    }
	} 
	else if(sockets[client_index].file_info.hash[0] == '\0' && S_ISREG(sockets[client_index].file_info.mode)) 
	{
	   //Get the hash.
	    num_read = read(fd, &buf_hash, BLOCKSIZE);
	    buf_hash[num_read] = '\0';
	    for(int i = 0; i < 8; i++)
	    {
	    	sockets[client_index].file_info.hash[i] = buf_hash[i];
	    }
	    if(num_read == -1)
	    {
	    	sockets[client_index].sock_fd = -1;
	    	return fd;
	    }
	} 
	else if(sockets[client_index].file_info.size == -1) //CHANGE HERE
	{
		//Get the size.
	    num_read = read(fd, &ns, sizeof(size_t));
	    sockets[client_index].file_info.size = ns;
	    if(num_read == -1)
	    {
	    	sockets[client_index].sock_fd = -1;
	    	return fd;
	    }

	    if(sockets[client_index].type == CHECKER_CLIENT) 
	    {
	    	//same file.
			int response;

			struct stat whatever;
			if(lstat(sockets[client_index].file_info.path, &whatever) == -1)
			{
				if(S_ISDIR(sockets[client_index].file_info.mode))
				{
					if(mkdir(sockets[client_index].file_info.path, sockets[client_index].file_info.mode) == -1) 
					{
						perror("mkdir");
						response = MATCH_ERROR;
					}
					else
					{
						response = MATCH;
					}
				} 
				else 
				{
					perror("lstat");
					response = MISMATCH;

				}
			}
			else
			{
				if(S_ISDIR(whatever.st_mode))
				{
					if(sockets[client_index].file_info.mode != whatever.st_mode) 
					{
						if(chmod(sockets[client_index].file_info.path, whatever.st_mode) == -1)
						{
							perror("chmod");
							response = MATCH_ERROR;
						} 
						else 
						{
							response = MATCH;
						}
					} 
					else 
					{
						response = MATCH;
					}
				}
		    	else if(sockets[client_index].file_info.mode != whatever.st_mode)
		    	{
		    		if(chmod(sockets[client_index].file_info.path, whatever.st_mode) == -1)
					{
						perror("chmod");
						response = MATCH_ERROR;
					}

		    	}

		    	else if(sockets[client_index].file_info.size != whatever.st_size)
		    	{
		    		response = MISMATCH;
		    	}
		    	else
		    	{
		    		FILE* file_dest = fopen(sockets[client_index].file_info.path, "rb");
			    	if(file_dest == NULL)
			    	{
			    		perror("fopen");
			    		response = MATCH_ERROR;
			    	}
			    	char* hash_of_dest = hash(file_dest);
			    	response = MATCH;
			    	for(int i = 0; i < 8; i++)
			    	{ 	
			    	 	if(sockets[client_index].file_info.hash[i] != hash_of_dest[i])
			    	 	{
			    	 		fclose(file_dest);
			    	 		response = MISMATCH;
			    	 	}
			    	}

			    	fclose(file_dest);
		    	}
		    }

			int send = write(sockets[client_index].sock_fd, &response, sizeof(int));

			strcpy(sockets[client_index].file_info.hash, "");
			strcpy(sockets[client_index].file_info.path, "");
			sockets[client_index].file_info.mode = 0;
			sockets[client_index].file_info.size = -1;
	    }
	}
	else if(sockets[client_index].type == SENDER_CLIENT && sockets[client_index].bytes_read != sockets[client_index].file_info.size) 
	{
		FILE* f_write;
		if(sockets[client_index].bytes_read == 0) 
		{
			f_write = fopen(sockets[client_index].file_info.path, "wb");
		} 
		else 
		{
			f_write = fopen(sockets[client_index].file_info.path, "ab");
		}

		int something = sockets[client_index].file_info.size - sockets[client_index].bytes_read;
	
		if(something > MAXDATA)
		{
			something = MAXDATA;
		}

		char data[something];
		num_read = read(sockets[client_index].sock_fd, &data, something);
		fwrite(data, 1, something, f_write);
		sockets[client_index].bytes_read += something;
		fclose(f_write);
	}

	if(num_read == 0) 
		return sockets[client_index].sock_fd;

    return 0;
}

int check_file(int client_index, struct sockname* sockets)
{
	struct stat chk_file;

	if (lstat(sockets[client_index].file_info.path,&chk_file) == -1) // Get lstat info for src
    {
        perror("lstat");
        sockets[client_index].sock_fd = -1;
        // RESET SOCKET
    }

    if(sockets[client_index].file_info.size == chk_file.st_size &&
       sockets[client_index].file_info.mode == chk_file.st_mode)

    {
    	FILE* file_dest = fopen(sockets[client_index].file_info.path, "rb");
    	if(file_dest == NULL)
    	{
    		perror("fopen");
    		sockets[client_index].sock_fd = -1;
    	}
    	char* hash_of_dest = hash(file_dest);
    	for(int i = 0; i < 8; i++)
    	{ 	
    	 	if(sockets[client_index].file_info.hash[i] != hash_of_dest[i])
    	 	{
    	 		fclose(file_dest);
    	 		return -1;
    	 	}
    	}
    	fclose(file_dest);
    	return 0;

    }
    return -1;

}

void copy_dir(int sock_fd, char *src_path, char *dest_path, char *host_ip, int port) 
{
	struct stat f_check;


	if(lstat(src_path, &f_check) == -1)
	{
		perror("lstat");
		close(sock_fd);
		exit(1);
	}

	char real_src[MAXPATH];
	realpath(src_path, real_src);
	char* base = basename(real_src);
	char real_path[MAXPATH];
	realpath(dest_path, real_path);

	struct fileinfo fi;
	fi.size = f_check.st_size;
	strcpy(fi.path, real_path);
	fi.mode = f_check.st_mode;

	printf("SRC: %s\n", src_path);
    printf("DEST: %s\n", dest_path);
    printf("IP: %s\n", host_ip);
    printf("PORT: %d\n", port);

    strcat(fi.path,"/");
	strcat(fi.path,base);
	write(sock_fd, fi.path, MAXPATH);
	write(sock_fd, &fi.mode, sizeof(mode_t));
	write(sock_fd, &fi.size, sizeof(size_t));

	int response;
	int waiting = read(sock_fd, &response, sizeof(int));

	if(response == MATCH)
	{
		DIR* new_dir = opendir(real_src);
		struct dirent* dir_file;
		struct stat dir_stat;
		char new_src[MAXPATH];
		while( (dir_file = readdir(new_dir)) != NULL )
		{
			if(dir_file->d_name[0] != '.')
			{
				strcpy(new_src, src_path);
				strcat(new_src, "/");
				strcat(new_src, dir_file->d_name);
				if(lstat(new_src, &dir_stat) == -1)
				{
					perror("lstat");
					exit(1);
				}
				else
				{
					if(S_ISDIR(dir_stat.st_mode))
					{
						copy_dir(sock_fd, new_src, fi.path, host_ip, port);
					}
					else if(S_ISREG(dir_stat.st_mode))
					{
						FILE* f = fopen(new_src, "rb");
				    	if(f == NULL)
				    	{
				    		perror("fopen");
				    		close(sock_fd);
				    		exit(1);
				    	}

				    	struct fileinfo fi2;
				    	strcpy(fi2.path, fi.path);
				    	fi2.mode = dir_stat.st_mode;
				    	char* hash_something = hash(f);
				    	for(int i = 0; i < 8; i++)
				    	{ 	
				    	 	fi2.hash[i] = hash_something[i]; 
				    	}
				    	fi2.size = dir_stat.st_size;
				    	fclose(f);



				    	char *base = basename(new_src);
				    	strcat(fi2.path,"/");
				    	strcat(fi2.path,base);
				    	write(sock_fd, fi2.path, MAXPATH);
				    	write(sock_fd, &fi2.mode, sizeof(mode_t));
				    	write(sock_fd, fi2.hash, BLOCKSIZE);
				    	write(sock_fd, &fi2.size, sizeof(size_t));
				    	
				    	//Wait for a response.
				    	int response;
				    	int waiting = read(sock_fd, &response, sizeof(int));


				    	if(response == MISMATCH)
				    	{
				    		//Need to send file information to socket...
				    		
				    		int child = fork();
				    		if(child == -1)
				    		{
				    			perror("fork");
				    			exit(1);
				    		}
				    		else if(child == 0)
				    		{
				    			int sock_fd2 = socket(AF_INET, SOCK_STREAM, 0);
							    if (sock_fd2 < 0) 
							    {
							     	perror("client: socket");
							     	exit(1);
								}

								// Set the IP and port of the server to connect to.
							    struct sockaddr_in server2;
							    server2.sin_family = AF_INET;
							    server2.sin_port = htons(port);
							    if (inet_pton(AF_INET, host_ip, &server2.sin_addr) < 1) 
							    {
							        perror("client: inet_pton");
							        close(sock_fd2);
							        exit(1);
							    }

							    //Connect to server
							    if (connect(sock_fd2, (struct sockaddr *)&server2, sizeof(server2)) == -1) 
							    {
						        perror("client: connect");
						        close(sock_fd2);
						        exit(1);
						    	}


				    			int type = SENDER_CLIENT;
					    		write(sock_fd2, &type, sizeof(int));
						    	write(sock_fd2, fi2.path, MAXPATH);
						    	write(sock_fd2, &fi2.mode, sizeof(mode_t));
						    	write(sock_fd2, fi2.hash, BLOCKSIZE);
						    	write(sock_fd2, &fi2.size, sizeof(size_t));

						    	FILE* f_pass = fopen(new_src, "rb");
						    	if(f_pass == NULL)
						    	{
						    		perror("fopen");
						    		exit(1);
						    	}

						    	int size = fi2.size;
						    	int bytes = MAXDATA;
						    	char data[MAXDATA];
						    	if(fi2.size < MAXDATA)
						    		bytes = fi2.size;
						    	while(fread(data, sizeof(char), bytes, f_pass) != 0)
						    	{
						    		write(sock_fd2, data, bytes);
						    		size -= bytes;
						    		if(size < MAXDATA)
						    		{
						    			bytes = size;
						    		}
						    	}
						    	fclose(f_pass);

						    	int response2;
						    	read(sock_fd2, &response2, sizeof(int));
						    	close(sock_fd2);
						    	exit(response2);

				    		}

				    	}
				    	else if(response == MATCH_ERROR)
				    	{
				    		printf("Match error\n");
				    	}
					}
				}
			}
		}
	}
	else if(response == MATCH_ERROR)
	{
		printf("Match error\n");
	}

}

int rcopy_client(char *src_path, char *dest_path, char *host_ip, int port) {
     printf("SRC: %s\n", src_path);
     printf("DEST: %s\n", dest_path);
     printf("IP: %s\n", host_ip);
     printf("PORT: %d\n", port);

     /*if(strcmp(src_path, dest_path) == 0)
     {
     	//src_path is same as dest_path
     	return 0;
     }*/

     //create a socket.
     int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
     if (sock_fd < 0) 
     	{
     		perror("client: socket");
     		exit(1);
	    }

	    // Set the IP and port of the server to connect to.
	    struct sockaddr_in server;
	    server.sin_family = AF_INET;
	    server.sin_port = htons(port);
	    if (inet_pton(AF_INET, host_ip, &server.sin_addr) < 1) 
	    {
	        perror("client: inet_pton");
	        close(sock_fd);
	        exit(1);
	    }

	    //Connect to server
	    if (connect(sock_fd, (struct sockaddr *)&server, sizeof(server)) == -1) 
	    {
        perror("client: connect");
        close(sock_fd);
        exit(1);
    	}


	    int type = CHECKER_CLIENT;
	    write(sock_fd, &type, sizeof(int));

    	//Pay attention to the src_path. For now, just test with FILES. No checks needed.
    	struct stat f_check;


    	if(lstat(src_path, &f_check) == -1)
    	{
    		perror("lstat");
    		close(sock_fd);
    		return 1;
    	}

    	if(S_ISREG(f_check.st_mode))
    	{
    	
	    	FILE* f = fopen(src_path, "rb");
	    	if(f == NULL)
	    	{
	    		perror("fopen");
	    		close(sock_fd);
	    		return 1;
	    	}
	    	char real_src[MAXPATH];
	    	realpath(src_path, real_src);
	    	char* base = basename(real_src);
	    	char real_path[MAXPATH];
	    	realpath(dest_path, real_path);

	    	struct fileinfo fi;
	    	strcpy(fi.path, real_path);
	    	fi.mode = f_check.st_mode;
	    	char* hash_something = hash(f);
	    	for(int i = 0; i < 8; i++)
	    	{ 	
	    	 	fi.hash[i] = hash_something[i]; 
	    	}
	    	fi.size = f_check.st_size;
	    	fclose(f);

	    	strcat(fi.path,"/");
	    	strcat(fi.path,base);
	    	write(sock_fd, fi.path, MAXPATH);
	    	write(sock_fd, &fi.mode, sizeof(mode_t));
	    	write(sock_fd, fi.hash, BLOCKSIZE);
	    	write(sock_fd, &fi.size, sizeof(size_t));
	    	
	    	//Wait for a response.
	    	int response;
	    	int waiting = read(sock_fd, &response, sizeof(int));


	    	if(response == MISMATCH)
	    	{
	    		//Need to send file information to socket...
	    		int child = fork();
	    		if(child == -1)
	    		{
	    			perror("fork");
	    			exit(1);
	    		}
	    		else if(child == 0)
	    		{
	    			int sock_fd2 = socket(AF_INET, SOCK_STREAM, 0);
				    if (sock_fd2 < 0) 
				    {
				     	perror("client: socket");
				     	exit(1);
					}

					// Set the IP and port of the server to connect to.
				    struct sockaddr_in server2;
				    server2.sin_family = AF_INET;
				    server2.sin_port = htons(port);
				    if (inet_pton(AF_INET, host_ip, &server2.sin_addr) < 1) 
				    {
				        perror("client: inet_pton");
				        close(sock_fd2);
				        exit(1);
				    }

				    //Connect to server
				    if (connect(sock_fd2, (struct sockaddr *)&server2, sizeof(server2)) == -1) 
				    {
			        perror("client: connect");
			        close(sock_fd2);
			        exit(1);
			    	}


	    			type = SENDER_CLIENT;
		    		write(sock_fd2, &type, sizeof(int));
			    	write(sock_fd2, fi.path, MAXPATH);
			    	write(sock_fd2, &fi.mode, sizeof(mode_t));
			    	write(sock_fd2, fi.hash, BLOCKSIZE);
			    	write(sock_fd2, &fi.size, sizeof(size_t));

			    	FILE* f_pass = fopen(src_path, "rb");
			    	if(f_pass == NULL)
			    	{
			    		perror("fopen");
			    		exit(1);
			    	}

			    	int size = fi.size;
			    	int bytes = MAXDATA;
			    	char data[MAXDATA];
			    	if(fi.size < MAXDATA)
			    		bytes = fi.size;
			    	while(fread(data, sizeof(char), bytes, f_pass) != 0)
			    	{
			    		write(sock_fd2, data, bytes);
			    		size -= bytes;
			    		if(size < MAXDATA)
			    		{
			    			bytes = size;
			    		}
			    	}
			    	fclose(f_pass);

			    	int response2;
			    	read(sock_fd2, &response2, sizeof(int));
			    	close(sock_fd2);
			    	exit(response2);

	    		}

	    		

	    	}
	    	else if(response == MATCH_ERROR)
	    	{
	    		printf("Match error\n");
	    	}
	    }
	    else if(S_ISDIR(f_check.st_mode))
	    {
	    	copy_dir(sock_fd, src_path, dest_path, host_ip, port);
	    }

    	return 0;

}

void rcopy_server(int port) 
{
	struct sockname sockets[MAX_CONNECTIONS];
	for(int i = 0; i < MAX_CONNECTIONS; i++)
	{
		sockets[i].sock_fd = -1;
		sockets[i].bytes_read = 0;
		sockets[i].type = -1;
		sockets[i].file_info.hash[0] = '\0';
		sockets[i].file_info.path[0] = '\0';
		sockets[i].file_info.mode = 0;
		sockets[i].file_info.size = -1;
	}

	int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(sock_fd < 0)
	{
		perror("socket");
		close(sock_fd);
	}

	struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    memset(&server.sin_zero, 0, 8);

    // Bind the selected port to the socket.
    if (bind(sock_fd, (struct sockaddr *)&server, sizeof(server)) < 0) 
    {
        perror("server: bind");
        close(sock_fd);
    }

    //Listen
    if (listen(sock_fd, MAX_BACKLOG) < 0) {
        perror("server: listen");
        close(sock_fd);
        exit(1);
    }

    //Prepare to loop
    int max_fd = sock_fd;
    fd_set all_fds, listen_fds;
    FD_ZERO(&all_fds);
    FD_SET(sock_fd, &all_fds);
    //char buf[BUF_SIZE];

    while (1) 
    {
    	listen_fds = all_fds;
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        if (nready == -1) 
        {
            perror("server: select");
            exit(1);
        }

        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock_fd, &listen_fds)) 
        {
            int client_fd = accept_connection(sock_fd, sockets);
            if (client_fd > max_fd) 
            {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            printf("Accepted connection\n");
        }

        //Check clients.
		for (int index = 0; index < MAX_CONNECTIONS; index++) 
		{
            if (sockets[index].sock_fd > -1 && FD_ISSET(sockets[index].sock_fd, &listen_fds)) 
            {
                // Note: never reduces max_fd
                int client_closed = read_from(index, sockets);
                if (client_closed > 0) {
                    FD_CLR(client_closed, &all_fds);
                    printf("Client %d disconnected\n", client_closed);
                }
                
            }
        }

    }   
}






