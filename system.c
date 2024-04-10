#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int DEBUG = 0;
const char DB_NAME[] = "dev.db";

/* Data types */

struct Music {
    int id;
    char title[100];
    char artist[100];
    char language[50];
    char type[50];
    char chore[255];
    int year;
    struct Music *next;
};

struct Musics {
    struct Music *musics;
    int n;
};

/* Communication with client */

void send_to_client(int connfd, const char *format, ...) {
    char message[1024];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (DEBUG)
        printf("%s", message);
    
    // Comment for standalone
    if (send(connfd, message, strlen(message), 0) == -1) {
        perror("send");
    }
    memset(message, 0, sizeof(message));
    // Comment for standalone - END
}

void listen_for_client(int connfd, const char *format, ...) {
    char buffer[256];

    va_list args;
    va_start(args, format);

    while(1) {
        fflush(stdin);

        // Comment for standalone
        recv(connfd, buffer, sizeof(buffer), 0);
        // Comment for standalone - END

        if (DEBUG) {
            fgets(buffer, sizeof(buffer), stdin);
        }

        if (vsscanf(buffer, format, args) < 1) {
            send_to_client(connfd, "\nValor invalido! Tente novamente::\n");
        } else {
            break;
        }
    }
    memset(buffer, 0, sizeof(buffer));
}

/* Interation with database */

void db_sql_check(int connfd, int rc, sqlite3 *db) {
    if (rc != SQLITE_OK)
        if (rc = sqlite3_errcode(db) != SQLITE_OK) {
            const char *error_msg = sqlite3_errmsg(db);
            send_to_client(connfd, "\nErro na interacao com o banco de dados (%d): %s\n\n", rc, error_msg);
            sqlite3_close(db);
            exit(1);
        }
}

int db_remove_by_id(int connfd, int id) {
    int success = 1;
    sqlite3 *db = NULL;
    sqlite3_stmt *stmt = NULL;

    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(connfd, rc, db);

    const char* sql = "DELETE FROM music WHERE id = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    db_sql_check(connfd, rc, db);

    rc = sqlite3_bind_int(stmt, 1, id);
    db_sql_check(connfd, rc, db);

    // Execute the DELETE statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        send_to_client(connfd, "\nErro ao apagar musica: %s\n\n", sqlite3_errmsg(db));
        success = 0;
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return success;
}

int db_pull(int connfd, struct Musics *musics) {
    char *sql;
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(connfd, rc, db);

    struct Music *current = musics->musics;
    while (current != NULL) {
        sqlite3_stmt *stmt = NULL;
        sql = "INSERT OR IGNORE INTO music (title, artist, language, type, chore, year) VALUES (?, ?, ?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        db_sql_check(connfd, rc, db);

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
            db_sql_check(connfd, rc, db);

        sqlite3_finalize(stmt);

        current = current->next;
    }

    sqlite3_close(db);
    return 0;
}

void db_fetch(int connfd, struct Musics *musics) {
    musics->musics = NULL;
    musics->n = 0;

    char *sql;
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(connfd, rc, db);

    sql = "SELECT * FROM music";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    db_sql_check(connfd, rc, db);

    rc = sqlite3_step(stmt);

    while (rc == SQLITE_ROW) {
        // Create a new Music structure for each row
        struct Music *music = (struct Music *)malloc(sizeof(struct Music));
        if (music == NULL) {
            send_to_client(connfd, "\nErro ao alocar memoria para nova musica!\n\n");
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

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void db_initialize(int connfd) {
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(connfd, rc, db);

    char *sql = "CREATE TABLE IF NOT EXISTS music (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, artist TEXT, language TEXT, type TEXT, chore TEXT, year INTEGER)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    db_sql_check(connfd, rc, db);

    sqlite3_close(db);
}

/* Utils */

void screen_pause(int connfd) {
    char input[256];
    
    send_to_client(connfd, "\nPressione 1 para retornar ao menu principal::\n");

    listen_for_client(connfd, "%s", input);
    memset(input, 0, sizeof(input));

}

void show_music_preview(int connfd, struct Music *music) {
    send_to_client(connfd, "\nIdentificador Unico: %d\nTitulo: %s\nInterprete: %s\n", music->id, music->title, music->artist);

}

void show_music(int connfd, struct Music *music) {
    show_music_preview(connfd, music);
    send_to_client(connfd, "Idioma: %s\nGenero musical: %s\nRefrão: %s\nAno de lançamento: %d\n\n", music->language, music->type, music->chore, music->year);

}

/* Specified Functions */

void add_song(int connfd) {
    char answer;
    struct Musics musics;
    musics.musics = NULL;
    musics.n = 0;

    while (1) {
        strcpy(&answer, "\0");

        struct Music *music = (struct Music *)malloc(sizeof(struct Music));
        if (music == NULL) {
            send_to_client(connfd, "\nErro ao alocar memoria para nova musica!\n\n");
            break;
        }

        music->id = ++musics.n;

        send_to_client(connfd, "\nPreencha os seguintes campos para adicionar uma musica.\n");

        send_to_client(connfd, "Titulo::\n");
        listen_for_client(connfd, " %[^\n]", music->title);

        send_to_client(connfd, "Interprete::\n");
        listen_for_client(connfd, " %[^\n]", music->artist);

        send_to_client(connfd, "Idioma::\n");
        listen_for_client(connfd, " %[^\n]", music->language);

        send_to_client(connfd, "Genero musical::\n");
        listen_for_client(connfd, " %[^\n]", music->type);

        while (answer != 's' && answer != 'n') {
            send_to_client(connfd, "Possui refrão? (s/n)::\n");
            listen_for_client(connfd, " %c", &answer);

            if (answer == 's') {
                send_to_client(connfd, "Insira o Refrão::\n");
                listen_for_client(connfd, " %[^\n]", music->chore);
            } else {
                strcpy(music->chore, "Não possui");
            }
        }        

        send_to_client(connfd, "Ano de lancamento::\n");
        listen_for_client(connfd, " %d", &music->year);

        music->next = musics.musics;

        musics.musics = music;

        send_to_client(connfd, "\nMusica adicionada com sucesso! Deseja adicionar mais musicas? (s/n)::\n");
        listen_for_client(connfd, " %c", &answer);

        if (answer == 'n')
            break;
    }

    db_pull(connfd, &musics);
};

void remove_song(int connfd) {
    char answer;
    int removeId;
    struct Musics musics;
    db_fetch(connfd, &musics);

    send_to_client(connfd, "\nInsira o ID da musica a ser removida::\n");
    listen_for_client(connfd, " %d", &removeId);
    
    while(musics.musics != NULL && musics.musics->id != removeId)
        musics.musics = musics.musics->next;
    
    if (musics.musics == NULL) {
        send_to_client(connfd, "\nMusica nao encontrada!\n\n");
        return;
    }

    if (db_remove_by_id(connfd, removeId))
        send_to_client(connfd, "\nMusica removida com sucesso!\n\n");
};

void list_by_year(int connfd) {
    int found = 0;
    int yearList;

    struct Musics musics;
    db_fetch(connfd, &musics);

    send_to_client(connfd, "\nInsira o ano de lancamento da musica::\n");
    listen_for_client(connfd, " %d", &yearList);

    while(musics.musics != NULL) {
        if (musics.musics->year == yearList){
            found = 1;
            show_music_preview(connfd, musics.musics);
        }

        musics.musics = musics.musics->next;
    }

    if (!found) {
        send_to_client(connfd, "\nNao foram encontradas musicas com esse ano de lancamento!\n\n");
    }

    screen_pause(connfd);
};

void list_by_year_and_language(int connfd) {
    int found = 0;
    int yearList;
    char languageList[50];
    struct Musics musics;
    db_fetch(connfd, &musics);

    send_to_client(connfd, "\nInsira o ano de lancamento da musica::\n");
    listen_for_client(connfd, " %d", &yearList);
    send_to_client(connfd, "\nInsira o idioma da musica::\n");
    listen_for_client(connfd, " %[^\n]", languageList);

    while(musics.musics != NULL) {
        if (musics.musics->year == yearList && strcmp(musics.musics->language, languageList) == 0){
            found = 1;
            show_music_preview(connfd, musics.musics);
        }

        musics.musics = musics.musics->next;
    }

    if (!found)
        send_to_client(connfd, "\nNao foram encontradas musicas com esses criterios!\n\n");

    screen_pause(connfd);
};

void list_by_type(int connfd) {
    int found = 0;
    char typeList[50];
    struct Musics musics;
    db_fetch(connfd, &musics);

    send_to_client(connfd, "\nInsira o genero musical::\n");
    listen_for_client(connfd, " %[^\n]", typeList);

    while(musics.musics != NULL) {
        if (strcmp(musics.musics->type, typeList) == 0){
            found = 1;
            show_music_preview(connfd, musics.musics);
        }

        musics.musics = musics.musics->next;
    }

    if (!found)
        send_to_client(connfd, "\nNao foram encontradas musicas com esse genero musical!\n\n");

    screen_pause(connfd);
};

void search_by_id(int connfd) {
    int searchId;
    struct Musics musics;
    db_fetch(connfd, &musics);

    while (1) {
        send_to_client(connfd, "\nInsira o ID da musica::\n");
        listen_for_client(connfd, " %d", &searchId);

        if (searchId >= 0 && searchId <= musics.n)
            break;

        send_to_client(connfd, "\nID invalido! Tente novamente.\n\n");
    }

    while(musics.musics != NULL && musics.musics->id != searchId)
        musics.musics = musics.musics->next;

    if (musics.musics == NULL) {
        send_to_client(connfd, "\nMusica nao encontrada!\n\n");
        return;
    }

    show_music(connfd, musics.musics);
    screen_pause(connfd);
};

void list_all(int connfd) {
    struct Musics musics;
    db_fetch(connfd, &musics);

    // send_to_client(connfd, "\nLista de todas as Musicas\n");

    while (musics.musics != NULL) {
        show_music(connfd, musics.musics);
        musics.musics = musics.musics->next;
    }

    screen_pause(connfd);
}

/* Server Initialization */

int serve_client(char *clientIp, int connfd) {
    db_initialize(connfd);
    int action = 0;

    while (1) {
        send_to_client(connfd, "\n - - - - - -  Socket Concurrent TCP  - - - - - -\n\n"
                        "1. Listar musicas por ano\n"
                        "2. Listar musicas por ano e idioma\n"
                        "3. Listar musicas por genero\n"
                        "4. Consultar musica por ID\n"
                        "5. Listar todas as musicas\n"
                        "6. Encerrar\n\n"
                        "Escolha uma ação (numero)::\n");

        
        listen_for_client(connfd, " %d", &action);

        switch (action) {
            case 1:
                list_by_year(connfd);
                break;
            case 2:
                list_by_year_and_language(connfd);
                break;
            case 3:
                list_by_type(connfd);
                break;
            case 4:
                search_by_id(connfd);
                break;
            case 5:
                list_all(connfd);
                break;
            case 6:
                exit(0);
        }
        memset(&action, 0, sizeof(action));
    }

    return 0;
}


int serve_client_admin(char *clientIp, int connfd) {
    db_initialize(connfd);
    int action = 0;

    while (1) {
        send_to_client(connfd, "\n - - - - - -  Socket Concurrent TCP  - - - - - -\n\n"
                        "1. Adicionar uma musica\n"
                        "2. Remover uma musica\n"
                        "3. Listar musicas por ano\n"
                        "4. Listar musicas por ano e idioma\n"
                        "5. Listar musicas por genero\n"
                        "6. Consultar musica por ID\n"
                        "7. Listar todas as musicas\n"
                        "8. Encerrar\n\n"
                        "Escolha uma ação (numero)::\n");

        
        listen_for_client(connfd, " %d", &action);

        switch (action) {
            case 1:
                add_song(connfd);
                break;
            case 2:
                remove_song(connfd);
                break;
            case 3:
                list_by_year(connfd);
                break;
            case 4:
                list_by_year_and_language(connfd);
                break;
            case 5:
                list_by_type(connfd);
                break;
            case 6:
                search_by_id(connfd);
                break;
            case 7:
                list_all(connfd);
                break;
            case 8:
                exit(0);
        }
        memset(&action, 0, sizeof(action));
    }

    return 0;
}
