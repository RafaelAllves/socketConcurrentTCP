#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>

#include "system.c"

#define PORT "3490"
#define BACKLOG 10

void reap_dead_children(int signal)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void *get_socket_address(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(void)
{
    int server_socket, client_socket;
    struct addrinfo hints, *server_info, *p;
    struct sockaddr_storage client_address;
    socklen_t address_size;
    struct sigaction signal_action;
    int option = 1;
    char client_ip[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, PORT, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for(p = server_info; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("server: bind");
            continue;
        }
        break;
    }

    freeaddrinfo(server_info);

    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(server_socket, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    signal_action.sa_handler = reap_dead_children;
    sigemptyset(&signal_action.sa_mask);
    signal_action.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &signal_action, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("server: waiting for connections...\n");

    while(1) {
        address_size = sizeof client_address;
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &address_size);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

        inet_ntop(client_address.ss_family, get_socket_address((struct sockaddr *)&client_address), client_ip, sizeof client_ip);
        printf("server: got connection from %s\n", client_ip);

        if (!fork()) {
            close(server_socket);
            char admin_flag;
            recv(client_socket, &admin_flag, 1, 0); // Receives the client's flag

            if (admin_flag == '1') {
                serve_client_admin(client_ip, client_socket);
            } else {
                serve_client(client_ip, client_socket); 
            }

            close(client_socket);
            exit(0);
        }
        close(client_socket);
    }

    return 0;
}
