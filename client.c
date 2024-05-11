#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdarg.h>

#define PORT "3490"     // Port for connecting to the server

const int admin_flag = 1;       // 1 for admin, 0 for normal user
const int buffer_size = 2048;

struct addrinfo *client_info;

// Gets the client's IPv4 or IPv6 address
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_to_server(int client_socket, const char *format, ...) {
    char message[buffer_size];
    memset(message, '\0', sizeof(message));
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (send(client_socket, message, strlen(message), 0) == -1) {
        perror("send error");
    }
}

void recv_msg(int client_socket, char *buffer, int *total_bytes_received) {
    int buffer_size = sizeof(buffer), bytes_received = -1, flag = 0;
    memset(buffer, '\0', buffer_size);

    bytes_received = recv(client_socket, buffer, buffer_size, flag);

    if (bytes_received == 0) {
        printf("Conexão encerrada pelo servidor.\n");
        exit(0);
    } else if (bytes_received < 0) {
        perror("recv error");
        return;
    }
    
    *total_bytes_received += bytes_received;
    buffer[*total_bytes_received] = '\0';
}

void receive_from_server(int client_socket) {
    int total_bytes_received = 0;
    char buffer[buffer_size];

    while (1) {
        recv_msg(client_socket, buffer, &total_bytes_received);

        printf("%s", buffer);

        if (strstr(buffer, "\ufeff") != NULL) {
            return;
        }
    }
}


void initialize_client(int sock_type) {
    // sets global client_info
    int gai_rv = -1;
    char client_ip[INET6_ADDRSTRLEN];
    struct addrinfo addr_init;

    addr_init.ai_family = AF_UNSPEC;
    addr_init.ai_socktype = sock_type;
    addr_init.ai_flags = AI_PASSIVE;        // fill client IP

    if ((gai_rv = getaddrinfo(NULL, PORT, &addr_init, &client_info)) != 0) {
        fprintf(stderr, "getaddrinfo client error: %s\n", gai_strerror(gai_rv));
        return;
    }
    
    // Show client IP
    inet_ntop(client_info->ai_family, get_in_addr((struct sockaddr *)client_info->ai_addr), client_ip, sizeof client_ip);
    printf("client: conectado a %s\n", client_ip);
}


int connect_to_server(char * server_addr, int sock_type, int opt_name) {
    int server_socket = -1, gai_rv = -1;
    char server_ip[INET6_ADDRSTRLEN];
    struct addrinfo addr_init, *server_info, *curr_server_info;
    memset(&addr_init, 0, sizeof addr_init);

    initialize_client(sock_type);

    addr_init.ai_family = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    addr_init.ai_socktype = sock_type;      // sets protocol, TCP=SOCK_STREAM or UDP=SOCK_DGRAM

    if ((gai_rv = getaddrinfo(server_addr, PORT, &addr_init, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo server error: %s\n", gai_strerror(gai_rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (curr_server_info = server_info; curr_server_info != NULL; curr_server_info = curr_server_info->ai_next) {
        if ((server_socket = socket(curr_server_info->ai_family, curr_server_info->ai_socktype, curr_server_info->ai_protocol)) == -1) {
            perror("server: socket error");
            continue;
        }

        if (connect(server_socket, curr_server_info->ai_addr, curr_server_info->ai_addrlen) == -1) {
            close(server_socket);
            perror("client: connect");
            continue;
        }

        if (send(server_socket, &admin_flag, sizeof admin_flag, 0) == -1) {
            perror("error setting user type");
        }
        break;
    }

    freeaddrinfo(server_info);

    if (curr_server_info == NULL)  {
        fprintf(stderr, "server: failed to connect to host\n");
        exit(1);
    }

    // Show server's IP
    inet_ntop(curr_server_info->ai_family, get_in_addr((struct sockaddr *)curr_server_info->ai_addr), server_ip, sizeof server_ip);
    printf("client: conectado a %s\n", server_ip);

    return server_socket;
}


int main(int argc, char *argv[]) {
    char *server_alias = "localhost";

    // Manage user input of where the server is
    if (argc > 2 && strcmp(argv[1], "-i") == 0)
        server_alias = argv[2];
    
    // tcp socket
    int server_socket = connect_to_server(server_alias, SOCK_STREAM, SO_REUSEADDR);
    
    // udp socket
    // int server_socket = connect_to_server(server_alias, SOCK_DGRAM, SOCK_DGRAM);

    while(1) {
        receive_from_server(server_socket);     // Wait for message from the server

        char message_to_server[buffer_size];
        memset(message_to_server, '\0', sizeof(message_to_server));

        fgets(message_to_server, sizeof(message_to_server), stdin);
        
        send_to_server(server_socket, "%s", message_to_server);

    }

    freeaddrinfo(client_info);
    close(server_socket);

    return 0;
}
