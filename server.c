#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netdb.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "system.c"

#define PORT "3490"
#define BACKLOG 10

void reap_dead_children(int signal) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void setup_sigchld_handling() {
    struct sigaction signal_action;
    signal_action.sa_handler = reap_dead_children;
    sigemptyset(&signal_action.sa_mask);
    
    signal_action.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &signal_action, NULL) == -1) {
        perror("sigaction failed");
        exit(1);
    }
}

void *get_socket_address(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int bind_socket(int sock_type, int opt_name, int *option) {
    int server_socket = -1, gai_rv = -1;
    struct addrinfo addr_info, *server_info, *p;
    memset(&addr_info, 0, sizeof addr_info);

    addr_info.ai_family = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    addr_info.ai_socktype = sock_type;      
    addr_info.ai_flags = AI_PASSIVE;        // fill in my IP for me

    if ((gai_rv = getaddrinfo(NULL, PORT, &addr_info, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(gai_rv));
        return 1;
    }

    // bind to socket
    for (p = server_info; p != NULL; p = p->ai_next) {
        if ((server_socket = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket error");
            continue;
        }
        
        if (setsockopt(server_socket, SOL_SOCKET, opt_name, option, sizeof(int)) == -1) {
            perror("server: setsockopt error");
            exit(1);
        }

        if (bind(server_socket, p->ai_addr, p->ai_addrlen) == -1) {
            close(server_socket);
            perror("server: couldn't bind to address");
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
        perror("server: failed to listen");
        exit(1);
    }

    return server_socket;
}

int main(void)
{
    int client_socket = -1, option = 1;
    socklen_t address_size = -1;
    struct sockaddr_storage client_address;

    char client_ip[INET6_ADDRSTRLEN];
    memset(client_ip, '\0', sizeof(client_ip));

    // tcp server socket
    // int server_socket = bind_socket(SOCK_STREAM, SO_REUSEADDR, &option);
    
    // udp server socket
    int server_socket = bind_socket(SOCK_DGRAM, SOCK_DGRAM, &option);

    setup_sigchld_handling();

    printf("server: waiting for connections...\n");

    while(1) {
        address_size = sizeof client_address;
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &address_size);

        if (client_socket == -1) {
            perror("failed to accept connection");
            continue;
        }

        inet_ntop(client_address.ss_family, get_socket_address((struct sockaddr *)&client_address), client_ip, sizeof client_ip);
        printf("server: got connection from %s\n", client_ip);

        if (fork() == 0) {
            // server stops listening
            close(server_socket);

            char admin_flag;
            recv(client_socket, &admin_flag, 1, 0);     // Receives the client's flag

            if (admin_flag == '1')
                serve_client_admin((struct sockaddr *)&client_address, &address_size, client_socket);
            else
                serve_client((struct sockaddr *)&client_address, &address_size, client_socket);
        }
        close(client_socket);
    }

    return 0;
}
