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
#include <unistd.h>
#include <math.h>

#define PORT "3490"     // Port for connecting to the server

const int admin_flag = 1;       // 1 for admin, 0 for normal user
const int buffer_size = 2048;
const char filepath[] = "./musicas";

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

int receive_from_server(int sockfd) {
    char buffer[buffer_size];
    int total_bytes_received = 0;
    int bytes_received = -1;

    while (1) {
        memset(buffer, '\0', sizeof(buffer));        
        bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);

        if (bytes_received == 0) {
            printf("Sessão encerrada.\n");
            exit(0);
        } else if (bytes_received < 0) {
            perror("recv error");
            return -1;
        }

        total_bytes_received += bytes_received;
        buffer[total_bytes_received] = '\0';
        printf("%s", buffer);

        if (strstr(buffer, "\ufeff\ufeff\ufeff\n") != NULL) {
            // File send
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

void upload_file(int sockfd, char *filename) {
    long file_size;
    size_t bytes_read;
    FILE *fp;
    char buffer[buffer_size], loadpath[150];
    memset(loadpath, '\0', sizeof(loadpath));
    sprintf(loadpath, "%s/%s", filepath, filename);
    
    printf("Carregando arquivo: '%s'\n", filename);

    if ((fp = fopen(loadpath, "rb")) == NULL) {
        perror("Erro ao carregar arquivo");
        exit(EXIT_FAILURE);
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        perror("Erro ao percorrer arquivo");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    if ((file_size = ftell(fp)) < 0) {
        perror("Erro ao adquirir tamanho do arquivo");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    
    if (fseek(fp, 0, SEEK_SET) != 0) {
        perror("Erro ao percorrer arquivo");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    send_to_server(sockfd, "%ld", file_size);
    receive_from_server(sockfd);

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        if (send(sockfd, buffer, bytes_read, 0) == -1) {
            perror("Erro ao enviar o arquivo.");
            fclose(fp);
            exit(EXIT_FAILURE);
        }
    }
    
    fclose(fp);
    receive_from_server(sockfd);
}

void receive_file(int sockfd, char * server_ip) {
    char buffer[buffer_size], savepath[150], filename[100], raw_port[100];
    memset(buffer, '\0', sizeof(buffer));
    memset(savepath, '\0', sizeof(savepath));
    memset(filename, '\0', sizeof(filename));
    memset(raw_port, '\0', sizeof(raw_port));
    int port, res, udp_socket;
    long file_size;
    FILE *file;

    // inicializar socket e receber porta udp
    if ((res = recv(sockfd, raw_port, sizeof(raw_port), 0)) <= 0) {
        if (res == 0) {
            printf("Sessão encerrada.\n");
            exit(0);
        } else if (res < 0) {
            perror("recv error");
            exit(1);
        }
    }

    port = strtol(raw_port, NULL, 10);

    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar o socket udp");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // envia endereco ao servidor
    if (sendto(udp_socket, '\0', sizeof(char), 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Erro ao estabelecer conexao UDP");
        close(udp_socket);
        exit(EXIT_FAILURE);
    }

    // Variáveis para armazenar o endereço do cliente
    struct sockaddr_storage client_address;
    socklen_t client_address_len = sizeof(client_address);

    // Recebe o tamanho do arquivo
    if ((res = recvfrom(udp_socket, &file_size, sizeof(file_size), 0, (struct sockaddr *) &client_address, &client_address_len)) <= 0) {
        if (res == 0) {
            printf("Sessão encerrada.\n");
            exit(0);
        } else if (res < 0) {
            perror("recv error");
            exit(1);
        }
    }

    // Recebe nome do arquivo
    if ((res = recvfrom(udp_socket, &filename, sizeof(filename), 0, (struct sockaddr *) &client_address, &client_address_len)) <= 0) {
        if (res == 0) {
            printf("Sessão encerrada.\n");
            exit(0);
        } else if (res < 0) {
            perror("recv error");
            exit(1);
        }
    }
    
    // Abre o arquivo
    sprintf(savepath, "%s/%s", filepath, filename);
    
    if ((file = fopen(savepath, "wb")) == NULL) {
        perror("Não foi possível abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    // Aloca memória para receber o arquivo
    int num_chunks = ceil(file_size / (double)buffer_size);

    printf("Recebendo arquivo: %s com %ld bytes\n", filename, file_size);
    
    char** chunks = (char**)malloc(sizeof(char*) * num_chunks);
    if (chunks == NULL) {
        perror("Chunks memory allocation failed");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_chunks; i++) {
        chunks[i] = (char*)malloc(sizeof(char) * buffer_size-4);
        if (chunks[i] == NULL) {
            perror("Chunks position memory allocation failed");
            break;
        }
        chunks[i][0] = '\0';
    }

    // Sinaliza servidor que socket esta pronto para receber dados
    char *sync_msg = "Iniciando transferencia...\ufeff\n";

    if (sendto(udp_socket, sync_msg, sizeof(sync_msg), 0, (struct sockaddr *) &client_address, client_address_len) == -1) {
        perror("Erro ao sincronizar inicio da transferência");
        exit(EXIT_FAILURE);
    }

    // Recebe os dados do socket e escreve no arquivo
    long total_bytes_received = 0;
    char raw_pos[5];

    while (1) {
        memset(buffer, '\0', sizeof(buffer));

        char bytes_received = recvfrom(udp_socket, buffer, buffer_size, 0, (struct sockaddr *) &client_address, &client_address_len);
        
        // Sinal do servidor para parar de esperar por novos dados
        if (buffer[0] == '\0') {
            printf("Transferência completa.\n");
            break;
        }

        strncpy(raw_pos, buffer, 4);
        raw_pos[4] = '\0';
        int pos = atoi(raw_pos);

        // debug
        printf("Chunk recebido: %d\n", pos);

        if (bytes_received < 0) {
            perror("Erro ao receber dados do socket");
            exit(EXIT_FAILURE);
        } else if (bytes_received == 0) {
            printf("Transferência encerrada prematuramente.\n");
            break;
        } else if (bytes_received < buffer_size && pos != num_chunks-1) {
            printf("Pacote numero %d incompleto\n", pos);
        }

        strcpy(chunks[pos], buffer + 4);

        total_bytes_received += bytes_received;
    }

    printf("Foram recebidos %ld bytes\n", total_bytes_received);

    fwrite(chunks, 1, sizeof chunks, file);

    if (total_bytes_received != file_size)
        printf("Transferência incompleta. O arquivo foi corrompido.\n");
    else
        printf("Transferência concluída com sucesso.\n");

    for (int i = 0; i < num_chunks; i++)
        free(chunks[i]);
    free(chunks);

    fclose(file);
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

int main(int argc, char *argv[]) {
    char *server_alias = "localhost";

    // Manage user input of where the server is
    if (argc > 2 && strcmp(argv[1], "-i") == 0)
        server_alias = argv[2];
    
    // tcp socket
    int sockfd = connect_to_server(server_alias, SOCK_STREAM, SO_REUSEADDR);
    
    while(1) {
        int op = receive_from_server(sockfd);       // Wait for the message from the server
        char message_to_server[100];

        // If the server asks for the song file, send the file
        if (op == 2) {      // '2' for file, '1' for message
            char filename[100];
            memset(filename, '\0', sizeof(filename));
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

            send_to_server(sockfd, "%s", filename);     // Send the music name
            upload_file(sockfd, filename);

        } else if (op == 3) { // '3' para música // Se o servidor envia a música, receba o arquivo
            receive_file(sockfd, server_alias);
        }else {
            memset(message_to_server, '\0', sizeof(message_to_server));

            fgets(message_to_server, sizeof(message_to_server), stdin);
            send_to_server(sockfd, "%s", message_to_server);
        }

    }

    close(sockfd);

    return 0;
}
