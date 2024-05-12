#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

const int buffer_size = 2048;
const char DB_NAME[] = "dev.sqlite3";
const char filepath[] = "./musics";

/* Data types */

struct Music {
    int id;
    char title[100];
    char artist[100];
    char language[50];
    char type[50];
    char chore[255];
    int year;
    char file[100];  // campo para o nome do arquivo da música
    char *content;  // novo campo para o conteúdo da música
    struct Music *next;
};

struct Musics {
    struct Music *musics;
    int n;
};

struct Conn {
    int connfd;
    struct sockaddr *client_address;
    socklen_t *address_len;
};

/* Communication with client */

void send_to_client(struct Conn * conn, const char *format, ...) {
    va_list args;
    char message[buffer_size];
    memset(message, '\0', sizeof(message));

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    if (send(conn->connfd, message, strlen(message), 0) == -1) {
        perror("send");
    }
}

void listen_for_client(struct Conn * conn, const char *format, ...) {
    char buffer[256];
    memset(buffer, '\0', sizeof(buffer));
    va_list args;
    va_start(args, format);

    while(1) {
        fflush(stdin);

        if (recv(conn->connfd, buffer, sizeof(buffer), 0) == -1) {
            perror("msg recv error");
        }

        if (vsscanf(buffer, format, args) < 1) {
            send_to_client(conn, "\nValor invalido! Tente novamente\ufeff\n");
        } else {
            break;
        }
    }

    va_end(args);
}

/* Interation with database */

void db_sql_check(struct Conn * conn, int rc, sqlite3 *db) {
    if (rc != SQLITE_OK)
        if (rc = sqlite3_errcode(db) != SQLITE_OK) {
            const char *error_msg = sqlite3_errmsg(db);
            send_to_client(conn, "\nErro na interacao com o banco de dados (%d): %s\n\n", rc, error_msg);
            sqlite3_close(db);
            exit(1);
        }
}

int db_remove_by_id(struct Conn * conn, int id) {
    int success = 1;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(conn, rc, db);

    const char* sql = "DELETE FROM music WHERE id = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    db_sql_check(conn, rc, db);

    rc = sqlite3_bind_int(stmt, 1, id);
    db_sql_check(conn, rc, db);

    // Execute the DELETE statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        send_to_client(conn, "\nErro ao apagar musica: %s\n\n", sqlite3_errmsg(db));
        success = 0;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return success;
}

int db_pull(struct Conn * conn, struct Musics *musics) {
    char *sql;
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(conn, rc, db);

    struct Music *current = musics->musics;
    while (current != NULL) {
        sqlite3_stmt *stmt = NULL;
        sql = "INSERT OR IGNORE INTO music (title, artist, language, type, chore, year) VALUES (?, ?, ?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        db_sql_check(conn, rc, db);

        // Bind values from the current music structure
        sqlite3_bind_text(stmt, 1, current->title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, current->artist, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, current->language, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, current->type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, current->chore, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, current->year);

        // Execute the statement and check for errors
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
            db_sql_check(conn, rc, db);

        sqlite3_finalize(stmt);

        current = current->next;
    }

    sqlite3_close(db);
    return 0;
}

void db_fetch(struct Conn * conn, struct Musics *musics) {
    musics->musics = NULL;
    musics->n = 0;

    char *sql;
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(conn, rc, db);

    sql = "SELECT * FROM music";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    db_sql_check(conn, rc, db);

    rc = sqlite3_step(stmt);

    while (rc == SQLITE_ROW) {
        // Create a new Music structure for each row
        struct Music *music = (struct Music *)malloc(sizeof(struct Music));
        if (music == NULL) {
            send_to_client(conn, "\nErro ao alocar memoria para nova musica!\n\n");
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            exit(1);
        }

        // Extract values from the row and populate the Music structure
        music->id = sqlite3_column_int(stmt, 0);
        strcpy(music->title, (const char *)sqlite3_column_text(stmt, 1));
        strcpy(music->artist, (const char *)sqlite3_column_text(stmt, 2));
        strcpy(music->language, (const char *)sqlite3_column_text(stmt, 3));
        strcpy(music->type, (const char *)sqlite3_column_text(stmt, 4));
        strcpy(music->chore, (const char *)sqlite3_column_text(stmt, 5));
        music->year = sqlite3_column_int(stmt, 6);

        music->next = musics->musics;
        musics->musics = music;
        
        if (music->id > musics->n)
            musics->n = music->id;

        rc = sqlite3_step(stmt);
    }

    if (rc != SQLITE_DONE)
        db_sql_check(conn, rc, db);
    
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void db_initialize(struct Conn * conn) {
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(conn, rc, db);

    char *sql = "CREATE TABLE IF NOT EXISTS music (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, artist TEXT, language TEXT, type TEXT, chore TEXT, year INTEGER)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    db_sql_check(conn, rc, db);

    sqlite3_close(db);
}

/* Utils */

void screen_pause(struct Conn * conn) {
    char input[256];
    memset(input, '\0', sizeof(input));
    
    send_to_client(conn, "\nPressione 1 para retornar ao menu principal\ufeff\n");

    listen_for_client(conn, "%s", input);

}

void show_music_preview(struct Conn * conn, struct Music *music) {
    send_to_client(conn, "\nIdentificador Unico: %d\nTitulo: %s\nInterprete: %s\n", music->id, music->title, music->artist);

}

void show_music(struct Conn * conn, struct Music *music) {
    show_music_preview(conn, music);
    send_to_client(conn, "Idioma: %s\nGenero musical: %s\nRefrão: %s\nAno de lançamento: %d\n\n", music->language, music->type, music->chore, music->year);

}

/* Specified Functions */

void receive_file(struct Conn * conn, char *filename) {
    printf("Recebendo arquivo: %s\n", filename);

    char buffer[buffer_size], savepath[150];
    memset(buffer, '\0', sizeof(buffer));
    memset(savepath, '\0', sizeof(savepath));
    int bytesReceived = 0;
    FILE *fp;

    sprintf(savepath, "%s%s", filepath, filename);

    if((fp = fopen(savepath, "wb")) == NULL){
        printf("Erro ao salvar arquivo");
        return;
    }

    while((bytesReceived = read(conn->connfd, buffer, sizeof buffer)) > 0){

        printf("Bytes recebidos %d\n", bytesReceived);    
        fwrite(buffer, 1, bytesReceived, fp);

        if (bytesReceived < buffer_size)
            break;
    }

    if(bytesReceived < 0){
        printf("\n Erro de escrita \n");
    }

    fclose(fp);

    return;
}

void send_music_to_client(const char *client_ip, int client_port, struct sockaddr_in client_address, const char *filepath) {
    int udp_socket;
    socklen_t client_address_len = sizeof(client_address);
    char buffer[buffer_size];
    memset(buffer, '\0', sizeof(buffer));
    FILE *music_file;

    // Cria o socket UDP
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Não foi possível criar o socket");
        exit(EXIT_FAILURE);
    }

    // Configura o endereço do cliente
    client_address.sin_family = AF_INET;
    client_address.sin_addr.s_addr = inet_addr(client_ip);
    client_address.sin_port = htons(client_port);

    // Abre o arquivo de música
    music_file = fopen(filepath, "rb");
    if (music_file == NULL) {
        perror("Não foi possível abrir o arquivo");
        exit(EXIT_FAILURE);
    }

    // Envia o tamanho do arquivo para o cliente
    fseek(music_file, 0, SEEK_END);
    long file_size = ftell(music_file);
    fseek(music_file, 0, SEEK_SET);
    sendto(udp_socket, &file_size, sizeof(long), 0, (struct sockaddr *) &client_address, client_address_len);

    // Envia os dados do arquivo para o cliente
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, buffer_size, music_file)) > 0) {
        printf("Enviando %ld bytes\n", bytes_read);
        sendto(udp_socket, buffer, bytes_read, 0, (struct sockaddr *) &client_address, client_address_len);
        // usleep(10000);  // Adiciona um pequeno atraso para simular a transmissão real
    }

    // Fecha o arquivo e o socket
    fclose(music_file);
    close(udp_socket);

    printf("Arquivo enviado com sucesso.\n");
}

void add_song(struct Conn * conn) {
    char answer = '\0';
    char filename[100];
    struct Musics musics;
    musics.musics = NULL;
    musics.n = 0;

    while (1) {
        strcpy(&answer, "\0");

        struct Music *music = (struct Music *)malloc(sizeof(struct Music));
        if (music == NULL) {
            send_to_client(conn, "\nErro ao alocar memoria para nova musica!\n\n");
            break;
        }

        music->id = ++musics.n;

        send_to_client(conn, "\nPreencha os seguintes campos para adicionar uma musica.\n");

        send_to_client(conn, "Titulo:\ufeff\n");
        listen_for_client(conn, " %[^\n]", music->title);

        send_to_client(conn, "Interprete:\ufeff\n");
        listen_for_client(conn, " %[^\n]", music->artist);

        send_to_client(conn, "Idioma:\ufeff\n");
        listen_for_client(conn, " %[^\n]", music->language);

        send_to_client(conn, "Genero musical:\ufeff\n");
        listen_for_client(conn, " %[^\n]", music->type);

        send_to_client(conn, "Nome do arquivo da musica (ex: hello.mp3):\ufeff\ufeff\n");
        listen_for_client(conn, " %[^\n]", music->file);

        memset(filename, '\0', sizeof(filename));
        sprintf(filename, "%s", music->file);  
        receive_file(conn, filename);

        while (answer != 's' && answer != 'n') {
            send_to_client(conn, "Possui refrão? (s/n)\ufeff\n");
            listen_for_client(conn, " %c", &answer);

            if (answer == 's') {
                send_to_client(conn, "Insira o Refrão:\ufeff\n");
                listen_for_client(conn, " %[^\n]", music->chore);
            } else {
                strcpy(music->chore, "Não possui");
            }
        }        

        send_to_client(conn, "Ano de lancamento:\ufeff\n");
        listen_for_client(conn, " %d", &music->year);

        music->next = musics.musics;

        musics.musics = music;

        send_to_client(conn, "\nMusica adicionada com sucesso! Deseja adicionar mais musicas? (s/n)\ufeff\n");
        listen_for_client(conn, " %c", &answer);

        if (answer == 'n')
            break;
    }

    db_pull(conn, &musics);
};

void remove_song(struct Conn * conn) {
    char answer = '\0';
    int removeId = -1;
    struct Musics musics;
    db_fetch(conn, &musics);

    send_to_client(conn, "\nInsira o ID da musica a ser removida\ufeff\n");
    listen_for_client(conn, " %d", &removeId);
    
    while(musics.musics != NULL && musics.musics->id != removeId)
        musics.musics = musics.musics->next;
    
    if (musics.musics == NULL) {
        send_to_client(conn, "\nMusica nao encontrada!\n\n");
        return;
    }

    if (db_remove_by_id(conn, removeId))
        send_to_client(conn, "\nMusica removida com sucesso!\n\n");
};

void list_by_year(struct Conn * conn) {
    int found = 0;
    int yearList = -1;

    struct Musics musics;
    db_fetch(conn, &musics);

    send_to_client(conn, "\nInsira o ano de lancamento da musica\ufeff\n");
    listen_for_client(conn, " %d", &yearList);

    while(musics.musics != NULL) {
        if (musics.musics->year == yearList){
            found = 1;
            show_music_preview(conn, musics.musics);
        }

        musics.musics = musics.musics->next;
    }

    if (!found) {
        send_to_client(conn, "\nNao foram encontradas musicas com esse ano de lancamento!\n\n");
    }

    screen_pause(conn);
};

void list_by_year_and_language(struct Conn * conn) {
    int found = 0, yearList = -1;
    char languageList[50];
    memset(languageList, '\0', sizeof(languageList));

    struct Musics musics;
    db_fetch(conn, &musics);

    send_to_client(conn, "\nInsira o ano de lancamento da musica\ufeff\n");
    listen_for_client(conn, " %d", &yearList);

    send_to_client(conn, "\nInsira o idioma da musica\ufeff\n");
    listen_for_client(conn, " %[^\n]", languageList);

    while(musics.musics != NULL) {
        if (musics.musics->year == yearList && strcmp(musics.musics->language, languageList) == 0){
            found = 1;
            show_music_preview(conn, musics.musics);
        }

        musics.musics = musics.musics->next;
    }

    if (!found)
        send_to_client(conn, "\nNao foram encontradas musicas com esses criterios!\n\n");

    screen_pause(conn);
};

void list_by_type(struct Conn * conn) {
    int found = 0;
    char typeList[50];
    memset(typeList, '\0', sizeof(typeList));

    struct Musics musics;
    db_fetch(conn, &musics);

    send_to_client(conn, "\nInsira o genero musical\ufeff\n");
    listen_for_client(conn, " %[^\n]", typeList);

    while(musics.musics != NULL) {
        if (strcmp(musics.musics->type, typeList) == 0){
            found = 1;
            show_music_preview(conn, musics.musics);
        }

        musics.musics = musics.musics->next;
    }

    if (!found)
        send_to_client(conn, "\nNao foram encontradas musicas com esse genero musical!\n\n");

    screen_pause(conn);
};

void search_by_id(struct Conn * conn) {
    int searchId = -1;
    struct Musics musics;
    db_fetch(conn, &musics);

    while (1) {
        send_to_client(conn, "\nInsira o ID da musica\ufeff\n");
        listen_for_client(conn, " %d", &searchId);

        if (searchId >= 0 && searchId <= musics.n)
            break;

        send_to_client(conn, "\nID invalido! Tente novamente.\n\n");
    }

    while(musics.musics != NULL && musics.musics->id != searchId)
        musics.musics = musics.musics->next;

    if (musics.musics == NULL) {
        send_to_client(conn, "\nMusica nao encontrada!\n\n");
        return;
    }

    show_music(conn, musics.musics);
    screen_pause(conn);
};

void list_all(struct Conn * conn) {
    struct Musics musics;
    db_fetch(conn, &musics);

    send_to_client(conn, "\nLista de todas as Musicas\n");

    while (musics.musics != NULL) {
        show_music(conn, musics.musics);
        musics.musics = musics.musics->next;
    }

    screen_pause(conn);
}

void get_by_id(struct Conn * conn_tcp, char *clientIp, int connfd, struct sockaddr_storage client_address) {
    int searchId = -1;
    struct Musics musics;
    db_fetch(conn_tcp, &musics);

    while (1) {
        send_to_client(conn_tcp, "\nInsira o ID da musica a ser baixada: \ufeff\n");
        listen_for_client(conn_tcp, " %d", &searchId);

        if (searchId >= 0 && searchId <= musics.n)
            break;

        send_to_client(conn_tcp, "\nID invalido! Tente novamente.\n\n");
    }

    while(musics.musics != NULL && musics.musics->id != searchId)
        musics.musics = musics.musics->next;

    if (musics.musics == NULL) {
        send_to_client(conn_tcp, "\nMusica nao encontrada!\n\n");
        return;
    }

    send_to_client(conn_tcp, "\nBaixando Musica!\ufeff\ufeff\ufeff\n");
    //send_music_to_client(clientIp, connfd, (struct sockaddr_in)&client_address, "./musicas/1.mp3");

    
    screen_pause(conn_tcp);
}

/* Server Initialization */

void show_menu(struct Conn * conn, int admin_mode) {
    if (admin_mode) {
        send_to_client(conn, "\n - - - - - -  Socket Concurrent UDP  - - - - - -\n\n"
            "1. Adicionar uma musica\n"
            "2. Remover uma musica\n"
            "3. Listar musicas por ano\n"
            "4. Listar musicas por ano e idioma \n"
            "5. Listar musicas por genero \n"
            "6. Consultar musica por ID\n"
            "7. Listar todas as musicas\n"
            "8. Baixar musica\n"
            "9. Encerrar\n\n"
            "Escolha uma ação (numero): \ufeff\n"
        );
    } else {
        send_to_client(conn, "\n - - - - - -  Socket Concurrent UDP  - - - - - -\n\n"
            "1. Listar musicas por ano\n"
            "2. Listar musicas por ano e idioma\n"
            "3. Listar musicas por genero\n"
            "4. Consultar musica por ID\n"
            "5. Listar todas as musicas\n"
            "6. Baixar musica\n"
            "7. Encerrar\n\n"
            "Escolha uma ação (numero): \ufeff\n"
        );
    }
}

int call_menu_input_adm(struct Conn * conn) {
    int action = 0;

    listen_for_client(conn, " %d", &action);

    switch (action) {
        case 1:
            add_song(conn);
            break;
        case 2:
            remove_song(conn);
            break;
        case 3:
            list_by_year(conn);
            break;
        case 4:
            list_by_year_and_language(conn);
            break;
        case 5:
            list_by_type(conn);
            break;
        case 6:
            search_by_id(conn);
            break;
        case 7:
            list_all(conn);
            break;
        case 8:
            //get_by_id(conn);
            break;
        case 9:
            return 1;
    }

    return 0;
}

int call_menu_input(struct Conn * conn) {
    int action = 0;

    listen_for_client(conn, " %d", &action);

    switch (action) {
        case 1:
            list_by_year(conn);
            break;
        case 2:
            list_by_year_and_language(conn);
            break;
        case 3:
            list_by_type(conn);
            break;
        case 4:
            search_by_id(conn);
            break;
        case 5:
            list_all(conn);
            break;
        case 6:
            //get_by_id(conn);
            break;
        case 7:
            return 1;
    }

    return 0;
}

void serve_client(int admin_mode, struct sockaddr * client_address, socklen_t * address_len, int connfd) {
    struct Conn conn;
    conn.client_address = client_address;
    conn.address_len = address_len;
    conn.connfd = connfd;
    int exit = 0;

    db_initialize(&conn);

    while (!exit) {
        show_menu(&conn, admin_mode);

        if (admin_mode)
            exit = call_menu_input_adm(&conn);
        else
            exit = call_menu_input(&conn);
    }
}