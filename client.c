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
const int udp_buffer_size = 64000;
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
            // send file
            return 3;
        } else if (strstr(buffer, "\ufeff\ufeff\n") != NULL) {
            // request file
            return 2;
        } else if (strstr(buffer, "\ufeff\n") != NULL) {
            // send text
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
    send_to_server(sockfd, "bytes serão transferidos");

    // Recebe porta udp, tamanho do arquivo e seu nome por tcp
    char tcp_buffer[buffer_size];
    memset(tcp_buffer, '\0', sizeof(tcp_buffer));

    if (recv(sockfd, tcp_buffer, sizeof(tcp_buffer), 0) <= 0) {
        perror("recv error");
        exit(EXIT_FAILURE);
    }
    
    // Desempacota porta, tamanho e nome da musica
    char filename[100];
    long file_size;
    int port;
    memset(filename, '\0', sizeof(filename));
    
    char *token = strtok(tcp_buffer, "|");
    if (token) {
        port = atoi(token);

        token = strtok(NULL, "|");
        if (token) {
            file_size = atol(token);

            token = strtok(NULL, "|");
            if (token) {
                strcpy(filename, token);
            } else {
                perror("Erro ao receber nome do arquivo");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Erro ao receber tamanho do arquivo");
            exit(EXIT_FAILURE);
        }
    } else {
        perror("Erro ao receber porta do servidor");
        exit(EXIT_FAILURE);
    }

    // Abre o arquivo
    FILE *file;
    char savepath[150];
    memset(savepath, '\0', sizeof(savepath));

    sprintf(savepath, "%s/%s", filepath, filename); 
    if ((file = fopen(savepath, "wb")) == NULL) {
        perror("Não foi possível abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    // Aloca memória para receber o arquivo
    int num_chunks = ceil((double)file_size / (double)udp_buffer_size);
    printf("Recebendo arquivo: %s em %d parte(s)\n", filename, num_chunks);
    
    unsigned char** chunks = (unsigned char**)malloc(sizeof(unsigned char*) * num_chunks);
    if (chunks == NULL) {
        perror("Chunks memory allocation failed");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < num_chunks; i++) {
        chunks[i] = (unsigned char*)malloc(sizeof(unsigned char) * (udp_buffer_size-1));
        if (chunks[i] == NULL) {
            perror("Chunks position memory allocation failed");
            exit(EXIT_FAILURE);
        }
        memset(chunks[i], '\0', sizeof(chunks[i]));
    }

    // Cria o socket udp do cliente
    int udp_socket;
    struct sockaddr_in client_addr;

    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Erro ao criar o socket udp");
        fclose(file);

        for (int i = 0; i < num_chunks; i++)
            free(chunks[i]);

        free(chunks);
        exit(EXIT_FAILURE);
    }

    client_addr.sin_family = AF_INET;   // Bind socket allows server to identify client's source address
    client_addr.sin_port = 0;       // permite que o SO escolha a porta
    client_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(udp_socket, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("bind failed");
        fclose(file);
        close(udp_socket);

        for (int i = 0; i < num_chunks; i++)
            free(chunks[i]);

        free(chunks);
        exit(EXIT_FAILURE);
    }

    // Inicializa endereco udp do servidor
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    
    if (server_addr.sin_addr.s_addr == INADDR_NONE) {
        perror("Invalid server IP address");
        fclose(file);
        close(udp_socket);

        for (int i = 0; i < num_chunks; i++)
            free(chunks[i]);

        free(chunks);
        exit(EXIT_FAILURE);
    }

    socklen_t server_address_len = sizeof(server_addr);
    char sync_msg[] = "Iniciando transferencia...\ufeff\n";

    if (sendto(udp_socket, sync_msg, sizeof(sync_msg), 0, (struct sockaddr *) &server_addr, server_address_len) == -1) {
        perror("Erro ao sincronizar inicio da transferência");
        fclose(file);
        close(udp_socket);

        for (int i = 0; i < num_chunks; i++)
            free(chunks[i]);

        free(chunks);
        exit(EXIT_FAILURE);
    }

    // Recebe os dados do socket e escreve no arquivo
    unsigned char buffer[udp_buffer_size];
    memset(buffer, '\0', sizeof(buffer));

    while (1) {
        ssize_t bytes_received = recvfrom(udp_socket, &buffer, sizeof(buffer), 0, (struct sockaddr *) &server_addr, &server_address_len);
   
        int piece_n = buffer[sizeof(buffer)-1];

        if (buffer[0] == '\0') {
            printf("Transferência concluida.\n");
            break;
        }

        // printf("Buffer recebido: ");
        // for (long i = 0; i < bytes_received; i++) {
        //     printf("%d", buffer[i]);
        // }
        // printf("\n");

        memcpy(chunks[piece_n], buffer, sizeof(buffer)-1);

        printf("Na memoria %d: ", piece_n);
        for (int i = 0; i < sizeof(unsigned char) * (udp_buffer_size-1); i++) {
            printf("%d", chunks[piece_n][i]);
        }
        printf("\n");

        if (bytes_received < 0) {
            perror("Erro ao receber dados");
            close(udp_socket);
            fclose(file);

            for (int i = 0; i < num_chunks; i++)
                free(chunks[i]);
            
            free(chunks);
            exit(EXIT_FAILURE);
        } else if (bytes_received < buffer_size && piece_n != num_chunks-1) {
            printf("Pacote numero %d incompleto\n", piece_n);
        }
    }

    
    // fwrite(chunks[i], 1, buff_len, file)
    long empty_packets = 0;
    int buff_len = 0;

    for (int i = 0; i < num_chunks; i++) {
        // buff_len = strlen(chunks[i]);
        // printf("Pacote %d len %d\n", i, buff_len);

        // // for (int j = 0; j < buff_len; j++) {
        // //     printf("%c", chunks[i][j]);
        // // }
        // // printf("\n");

        // if (buff_len != udp_buffer_size-1) {
        //     printf("Pacote %d incompleto\n", i);
        // }

        if (chunks[i][0] != '\0') {
            fwrite(chunks[i], 1, buff_len, file);
            //printf("Pacote %d: escrito como %s\n", i, chunks[i]);
            printf("Pacote %d enchido\n", i);
            for (int j = 0; j < sizeof(unsigned char) * (udp_buffer_size-1); j++) {
                printf("%d", chunks[i][j]);
            }
            printf("\n");
        } else {
            printf("Pacote %d vazio\n", i);
            for (int j = 0; j < sizeof(unsigned char) * (udp_buffer_size-1); j++) {
                printf("%d", chunks[i][j]);
            }
            printf("\n");
            empty_packets++;
        }
    }

    //fclose(file);

    // if ((file = fopen(savepath, "rb")) == NULL) {
    //     perror("Não foi possível abrir o arquivo");
    //     exit(EXIT_FAILURE);
    // }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("Erro ao percorrer arquivo");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    long final_size;

    if ((final_size = ftell(file)) < 0) {
        perror("Erro ao adquirir tamanho do arquivo");
        fclose(file);
        exit(EXIT_FAILURE);
    }    

    if (final_size != file_size)
        printf("Arquivo corrompido %ld/%ld. %ld pacotes vazios\n", final_size, file_size, empty_packets);
    else
        printf("O arquivo foi recebido com sucesso.\n");

    for (int j = 0; j < num_chunks; j++)
        free(chunks[j]);

    free(chunks);
    fclose(file);
    close(udp_socket);
}

int connect_to_server(char * server_addr, int sock_type, int opt_name) {
    int sockfd = -1, gai_rv = -1;
    char server_ip[INET6_ADDRSTRLEN];
    struct addrinfo addr_init, *server_info, *curr_server_info;
    memset(&addr_init, '\0', sizeof addr_init);

    addr_init.ai_family = AF_UNSPEC;        // use IPv4 or IPv6, whichever
    addr_init.ai_socktype = sock_type;      // sets protocol TCP=SOCK_STREAM

    if ((gai_rv = getaddrinfo(server_addr, PORT, &addr_init, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai_rv));
        exit(EXIT_FAILURE);
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
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);

    return sockfd;
}

int main(int argc, char *argv[]) {
    char *server_alias = "127.0.0.1";

    // Manage user input of where the server is
    if (argc > 2 && strcmp(argv[1], "-i") == 0)
        server_alias = argv[2];
    
    // tcp socket
    int sockfd = connect_to_server(server_alias, SOCK_STREAM, SO_REUSEADDR);
    
    while(1) {
        // show a message from server
        int op = receive_from_server(sockfd);
        char message_to_server[100];

        if (op == 2) {      // upload a music to server by tcp
            char filename[100];
            memset(filename, '\0', sizeof(filename));
            fgets(filename, sizeof(filename), stdin);
            filename[strcspn(filename, "\n")] = 0;

            send_to_server(sockfd, "%s", filename);
            upload_file(sockfd, filename);

        } else if (op == 3) {       // download a music from server by udp
            receive_file(sockfd, server_alias);

        }else {     // normal operation, send a message to server
            memset(message_to_server, '\0', sizeof(message_to_server));

            fgets(message_to_server, sizeof(message_to_server), stdin);
            send_to_server(sockfd, "%s", message_to_server);
        }

    }

    close(sockfd);

    return 0;
}
