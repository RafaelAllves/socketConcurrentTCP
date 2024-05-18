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
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int bind_socket(int sock_type, int opt_name, int *option) {
    int server_socket = -1, gai_rv = -1;
    struct addrinfo addr_init, *server_info, *curr_server_info;
    memset(&addr_init, '\0', sizeof addr_init);

    addr_init.ai_family = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    addr_init.ai_socktype = sock_type;      
    addr_init.ai_flags = AI_PASSIVE;        // fill in host IP

    if ((gai_rv = getaddrinfo(NULL, PORT, &addr_init, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(gai_rv));
        return 1;
    }

    // bind to socket
    for (curr_server_info = server_info; curr_server_info != NULL; curr_server_info = curr_server_info->ai_next) {
        if ((server_socket = socket(curr_server_info->ai_family, curr_server_info->ai_socktype, curr_server_info->ai_protocol)) == -1) {
            perror("server: socket error");
            continue;
        }
        
        if (setsockopt(server_socket, SOL_SOCKET, opt_name, option, sizeof(int)) == -1) {
            perror("server: setsockopt error");
            exit(1);
        }

        if (bind(server_socket, curr_server_info->ai_addr, curr_server_info->ai_addrlen) == -1) {
            close(server_socket);
            perror("server: couldn't bind to address");
            continue;
        }
        break;
    }

    freeaddrinfo(server_info);

    if (curr_server_info == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        exit(1);
    }

    if (listen(server_socket, BACKLOG) == -1) {
        perror("server: failed to listen");
        exit(1);
    }

    return server_socket;
}

int main(void) {
    int client_socket = -1, option = 1;
    socklen_t address_size = -1;
    struct sockaddr_storage client_address;

    char client_ip[INET6_ADDRSTRLEN];
    memset(client_ip, '\0', sizeof(client_ip));

    // tcp socket
    int server_socket = bind_socket(SOCK_STREAM, SO_REUSEADDR, &option);

    setup_sigchld_handling();

    printf("server: waiting for connections...\n");

    while(1) {
        address_size = sizeof client_address;
        client_socket = accept(server_socket, (struct sockaddr *)&client_address, &address_size);
        if (client_socket == -1) {
            perror("accept");
            continue;
        }

         // Show client's IP
        inet_ntop(client_address.ss_family, get_socket_address((struct sockaddr *)&client_address), client_ip, sizeof client_ip);
        printf("server: got connection from %s\n", client_ip);

        if (fork() == 0) {
            int admin_flag;

            if (recv(client_socket, &admin_flag, sizeof admin_flag, 0) <= 0) {
                perror("server: error receiving client's flag");
                close(client_socket);
                continue;
            }

            serve_client(admin_flag, (struct sockaddr *)&client_address, &address_size, client_socket);

            inet_ntop(client_address.ss_family, get_socket_address((struct sockaddr *)&client_address), client_ip, sizeof client_ip);
            printf("server: %s disconnected\n", client_ip);
        }
        close(client_socket);
    }

    close(server_socket);

    return 0;
}
