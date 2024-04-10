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

#define PORT "3490" // Port for connecting to the server

// Gets the client's IPv4 or IPv6 address
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_to_server(int sockfd, const char *format, ...) {
    char message[100];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
    }
    memset(message, 0, sizeof(message));
}

void receive_from_server(int sockfd) {
    char buffer[1024]; // Buffer size
    int total_bytes_received = 0;
    int bytes_received;

    while (1) {
        bytes_received = recv(sockfd, buffer + total_bytes_received, sizeof(buffer) - total_bytes_received - 1, 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Conexão encerrada pelo servidor.\n");
                exit(0);
            } else {
                perror("recv");
            }
            return;
        }
        total_bytes_received += bytes_received;
        buffer[total_bytes_received] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "::\n") != NULL) {
            // Stops reading messages from the server when it encounters '::\n'
            memset(buffer, 0, sizeof(buffer));
            break;
        }
    }
    memset(buffer, 0, sizeof(buffer));

}


int main(void) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("localhost", PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
            s, sizeof s);
    printf("client: conectado a %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

    while(1) {
        receive_from_server(sockfd); // Wait for the message from the server

        char message_to_server[100];
        fgets(message_to_server, sizeof(message_to_server), stdin);

        send_to_server(sockfd, "%s", message_to_server);
        memset(message_to_server, 0, sizeof(message_to_server));

    }

    close(sockfd);

    return 0;
}
