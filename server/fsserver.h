/*
 * fsserver.h: static file server
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define PORT 5123

char *respond(char *filename, int *len){
  FILE *fp;
  char filepath[128],
       extension[8],
       mime[32],
       *dot,
       *out;
  size_t sze;

  getcwd(filepath, sizeof(filepath)); /* TODO: Move this outside of response function */
  strcat(filepath, filename);
  if(strcmp(filename, "/") == 0){
    strcat(filepath, "index.html");
  }

  fp = fopen(filepath, "rb");
  if(fp == NULL){
    out = malloc(32);
    strcpy(out, "HTTP/1.1 404 Not Found");
    printf("\x1b[31m(404)\x1b[0m\n");
    return out;
  }

  dot = strrchr(filename, '.');
  if(!dot || dot == filename){
    extension[0] = '\0';
  } else {
    strcpy(extension, dot+1);
  }

  if(strcmp(extension, "js") == 0){
    strcpy(mime, "application/javascript");
  } else if(strcmp(extension, "html") == 0){
    strcpy(mime, "text/html");
  } else if(strcmp(extension, "css") == 0){
    strcpy(mime, "text/css");
  } else if(strcmp(extension, "wasm") == 0){
    strcpy(mime, "application/wasm");
  } else {
    strcpy(mime, "text/plain");
  }

  fseek(fp, 0, SEEK_END);
  out = malloc(128+(sze=ftell(fp)));
  fseek(fp, 0, SEEK_SET);

  sprintf(out, "HTTP/1.1 200 OK\r\nContent-Length: %li\r\nContext-Type: %s\r\n\r\n", sze, mime);
  *len = sze+strlen(out);
  fread(out+strlen(out), sze, 1, fp);
  fclose(fp);
  printf("\x1b[32m(200)\x1b[0m\n");

  return out;
}

void fsserver(){
  int server_fd, new_socket;
  struct sockaddr_in address;
  int opt = 1;
  int addrlen = sizeof(address),
      responselen;
  char buffer[1024] = {0},
       filename[64],
       *response;
   
  if((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0){
    perror("[fsserver] Failed to open socket");
    exit(EXIT_FAILURE);
  }
  
  if(setsockopt(
      server_fd, SOL_SOCKET,
      SO_REUSEADDR | SO_REUSEPORT,
      &opt, sizeof(opt))){
    perror("[fsserver] Failed to set socket options");
    exit(EXIT_FAILURE);
  }
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);
  
  if(bind(server_fd, (struct sockaddr*)&address, sizeof(address))<0){
    perror("[fsserver] Failed to bind socket");
    exit(EXIT_FAILURE);
  }

  printf("[fsserver] Static file server listening on port %i.\n", PORT);

loop:;
  if(listen(server_fd, 8) < 0){
    perror("[fsserver] Failed to listen");
    exit(EXIT_FAILURE);
  }
  if((new_socket =
       accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen))<0){
    perror("[fsserver] Failed to accept");
    exit(EXIT_FAILURE);
  }
/*  if(fork() > 0){ TODO: This creates too many headaches for the benefit it provides */
    read(new_socket, buffer, 1024);
    sscanf(buffer, "GET %s HTTP/1.1", filename);
    printf("[fsserver] \x1b[36mRequest: \x1b[33m%s \x1b[0m", filename);

    response = respond(filename, &responselen);
    send(new_socket, response, responselen, 0);
    free(response);

    shutdown(new_socket, 2);
/*  } else { */
    goto loop;
/*  } */
}
