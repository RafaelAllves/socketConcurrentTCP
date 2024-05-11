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
    char message[100] = "\0";
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
    }
    memset(message, 0, sizeof(message));
}

void send_file(int sockfd, char *filename) {
    char buffer[1024];
    char filepath[1024] = "./";
    filename[strcspn(filename, "\n")] = '\0'; // Adicione esta linha
    strcat(filepath, filename);
    printf("Tentando abrir o arquivo: '%s'\n", filename);
    FILE *fp = fopen(filepath, "rb");
    if (fp == NULL) {
        perror("Erro ao abrir o arquivo");
        return;
    }
    printf("Arquivo aberto com sucesso.\n");
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes_read, 0) == -1) {
            perror("Erro ao enviar o arquivo.");
            exit(1);
        }
        // printf("Enviando: %s\n", buffer);
    }
    printf("Arquivo enviado com sucesso.\n");
    fclose(fp);
}

int receive_from_server(int sockfd) {
    char buffer[2048] = "\0"; // Buffer size
    int total_bytes_received = 0;
    int bytes_received = -1;

    while (1) {
        memset(buffer, 0, sizeof(buffer));        
        bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);

        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                printf("Conexão encerrada pelo servidor.\n");
                exit(0);
            } else {
                perror("recv");
            }
            return -1;
        }
        total_bytes_received += bytes_received;
        buffer[total_bytes_received] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, ":::\n") != NULL) {
            // File request
            return 2;
        } else if (strstr(buffer, "::\n") != NULL) {
            // Stops reading messages from the server when it encounters '::\n'
            memset(buffer, 0, sizeof(buffer));
            return 1;
            break;
        }
    }
    memset(buffer, 0, sizeof(buffer));

}

int main(int argc, char *argv[]) {
    int sockfd = -1;
    struct addrinfo hints, *servinfo, *p;
    int rv = -1;
    char s[INET6_ADDRSTRLEN] = "\0";
    char *server_ip = "localhost";

    if (argc > 2 && strcmp(argv[1], "-i") == 0) {
        server_ip = argv[2];
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(server_ip, PORT, &hints, &servinfo)) != 0) {
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

        char admin_flag = '1'; // '1' for admin, '0' for normal user
        if (send(sockfd, &admin_flag, 1, 0) == -1) {
            perror("send");
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
        int op = receive_from_server(sockfd); // Wait for the message from the server

        char message_to_server[100] = "\0";
        fgets(message_to_server, sizeof(message_to_server), stdin);

        // If the server asks for the song file, send the file
        if (op == 2) { // '2' for filer, '1' for message
            printf("Repita o nome do arquivo\n"); //Fixme: Na primeira vez q digita o caminho nao funciona, precisa digitar 2 vezes
            char filename[100];
            fgets(filename, sizeof(filename), stdin);
            send_file(sockfd, filename);
        } else {
            send_to_server(sockfd, "%s", message_to_server);
        }

        memset(message_to_server, 0, sizeof(message_to_server));
    }

    close(sockfd);

    return 0;
}
