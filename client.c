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

// Gets the client's IPv4 or IPv6 address
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET)
        return &(((struct sockaddr_in*)sa)->sin_addr);
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void send_to_server(int sockfd, const char *format, ...) {
    char message[100];
    memset(message, '\0', sizeof(message));
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (send(sockfd, message, strlen(message), 0) == -1) {
        perror("send");
    }
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
    char buffer[buffer_size];
    int total_bytes_received = 0;
    int bytes_received = -1;

    while (1) {
        memset(buffer, '\0', sizeof(buffer));        
        bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);

        if (bytes_received == 0) {
            printf("Conexão encerrada pelo servidor.\n");
            exit(0);
        } else if (bytes_received < 0) {
            perror("recv error");
            return -1;
        }

        total_bytes_received += bytes_received;
        buffer[total_bytes_received] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "\ufeff\ufeff\ufeff\n") != NULL) {
            // File request
            return 3;
        } else if (strstr(buffer, "\ufeff\ufeff\n") != NULL) {
            // File request
            return 2;
        } else if (strstr(buffer, "\ufeff\n") != NULL) {
            // Stops reading messages from the server when it encounters '\ufeff\n'
            return 1;
        }
    }

}

int connect_to_server(char * server_addr, int sock_type, int opt_name) {
    int sockfd = -1, gai_rv = -1;
    char server_ip[INET6_ADDRSTRLEN];
    struct addrinfo addr_init, *server_info, *curr_server_info;
    memset(&addr_init, '\0', sizeof addr_init);

    addr_init.ai_family = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    addr_init.ai_socktype = sock_type;      // sets protocol, TCP=SOCK_STREAM or UDP=SOCK_DGRAM

    if ((gai_rv = getaddrinfo(server_addr, PORT, &addr_init, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for (curr_server_info = server_info; curr_server_info != NULL; curr_server_info = curr_server_info->ai_next) {
        if ((sockfd = socket(curr_server_info->ai_family, curr_server_info->ai_socktype, curr_server_info->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }

        if (connect(sockfd, curr_server_info->ai_addr, curr_server_info->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }

        if (send(sockfd, &admin_flag, sizeof admin_flag, 0) == -1) {
            perror("error setting user type");
        }
        break;
    }

    if (curr_server_info == NULL)  {
        fprintf(stderr, "server: failed to connect to host\n");
        return 2;
    }

    // Show server's IP
    // inet_ntop(curr_server_info->ai_family, get_in_addr((struct sockaddr *)curr_server_info->ai_addr), server_ip, sizeof server_ip);
    // printf("client: conectado a %s\n", server_ip);

    freeaddrinfo(server_info);

    return sockfd;
}

void receive_music(int sockfd) {

    long total_size;
    recvfrom(sockfd, &total_size, sizeof(total_size), 0, NULL, NULL);

    FILE *fp = fopen("./musicas_recebidas/received_music.mp3", "wb");
    if (fp == NULL) {
        printf("\nErro ao abrir o arquivo!\n\n");
        return;
    }

    char buffer[1024];
    int bytes_received;
    long bytes_received_total = 0;
    struct sockaddr_in server_address;
    socklen_t address_len = sizeof(server_address);
    while ((bytes_received = recvfrom(sockfd, buffer, 1024, 0, (struct sockaddr *)&server_address, &address_len)) > 0) {
        // printf("bytes_received: %d", bytes_received);
        fwrite(buffer, sizeof(char), bytes_received, fp);
        bytes_received_total += bytes_received;
        if (bytes_received_total >= total_size) {
            break;
        }
        
    }
    memset(buffer, '\0', sizeof(buffer));
    // printf("bytes_received_total: %ld\n", bytes_received_total);

    fclose(fp);
    printf("\nArquivo recebido com sucesso!\n\n");
}

int main(int argc, char *argv[]) {
    char *server_alias = "localhost";

    // Manage user input of where the server is
    if (argc > 2 && strcmp(argv[1], "-i") == 0)
        server_alias = argv[2];
    
    // tcp socket
    int sockfd = connect_to_server(server_alias, SOCK_STREAM, SO_REUSEADDR);
    
    // udp socket
    // int sockfd = connect_to_server(server_alias, SOCK_DGRAM, SOCK_DGRAM);

    while(1) {
        int op = receive_from_server(sockfd);       // Wait for the message from the server
        char message_to_server[100];


        // If the server asks for the song file, send the file
        if (op == 3) { // '2' for filer, '1' for message
            receive_music(sockfd);
        } else if (op == 2) { // '2' for filer, '1' for message
            char filename[100];
            memset(filename, '\0', sizeof(filename));
            fgets(filename, sizeof(filename), stdin);
            send_file(sockfd, filename);
        } else {
            memset(message_to_server, '\0', sizeof(message_to_server));
            fgets(message_to_server, sizeof(message_to_server), stdin);
            send_to_server(sockfd, "%s", message_to_server);
        }

    }

    close(sockfd);

    return 0;
}
