#include <sqlite3.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const int DEBUG = 1;
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
    //bigest id in db
    int n;
};

/* Communication with client */

void send_to_client(int connfd, const char *format, ...) {
    char message[100];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (DEBUG) {
        printf("%s", message);
    }

    // if (send(connfd, message, strlen(message), 0) == -1) {
    //     perror("send");
    // }
}

/* Interation with database */

void db_sql_check(int rc, sqlite3 *db) {
    if (rc != SQLITE_OK) {
        if (rc = sqlite3_errcode(db) != SQLITE_OK) {
            const char *error_msg = sqlite3_errmsg(db);
            fprintf(stderr, "Erro SQLite (%d): %s\n", rc, error_msg);
            sqlite3_close(db);
            exit(1);
        }
    }
}

void db_close_conn(sqlite3 *db) {
    if (sqlite3_close(db) != SQLITE_OK) {
        fprintf(stderr, "Erro ao encerrar banco de dados\n");
    }
}

int db_pull(struct Musics *musics) {
    char *sql;
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(rc, db);

    struct Music *current = musics->musics;
    while (current != NULL) {
        // Prepare SQL statement for insertion
        sqlite3_stmt *stmt = NULL;
        sql = "INSERT OR IGNORE INTO music (title, artist, language, type, chore, year) VALUES (?, ?, ?, ?, ?, ?)";
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        db_sql_check(rc, db);

        // Bind values from the current music structure
        sqlite3_bind_text(stmt, 1, current->title, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, current->artist, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, current->language, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, current->type, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, current->chore, -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 6, current->year);

        // Execute the statement and check for errors
        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            db_sql_check(rc, db);
        }

        // Finalize the statement
        sqlite3_finalize(stmt);
        current = current->next;
    }

    sqlite3_close(db);
    return 0;
}

void db_fetch(struct Musics *musics) {
    musics->musics = NULL;
    musics->n = 0;

    char *sql;
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(rc, db);

    sql = "SELECT * FROM music";
    sqlite3_stmt *stmt = NULL;
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    db_sql_check(rc, db);

    rc = sqlite3_step(stmt);

    while (rc == SQLITE_ROW) {
        // Create a new Music structure for each row
        struct Music *music = (struct Music *)malloc(sizeof(struct Music));
        if (music == NULL) {
            fprintf(stderr, "Erro ao alocar memoria para nova musica!\n");
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

        // Add the music to the musics linked list
        music->next = musics->musics;
        musics->musics = music;
        
        if (music->id > musics->n) {
            musics->n = music->id;
        }

        rc = sqlite3_step(stmt);
    }

    // Finalize the statement and close the database connection
    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

void db_initialize() {
    sqlite3 *db;
    int rc = sqlite3_open(DB_NAME, &db);
    db_sql_check(rc, db);

    char *sql = "CREATE TABLE IF NOT EXISTS music (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT, artist TEXT, language TEXT, type TEXT, chore TEXT, year INTEGER)";
    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    db_sql_check(rc, db);

    sqlite3_close(db);
}

/* Utils */

void screen_pause(int connfd) {
    // remove all \n from stdin
    while (getchar() != '\n');

    send_to_client(connfd, "Pressione ENTER para retornar ao menu principal...");
    while (getchar() != '\n');
}

/* Specified Functions */

void add_song(int connfd) {
    char answer;
    struct Musics musics;

    while (1) {
        struct Music *music = (struct Music *)malloc(sizeof(struct Music));
        if (music == NULL) {
            printf("Erro ao alocar memoria para nova musica!\n");
            break;
        }
        music->id = ++musics.n;

        send_to_client(connfd, "Preencha os seguintes campos para adicionar uma musica.\n");

        send_to_client(connfd, "Titulo: ");
        scanf(" %[^\n]", music->title);

        send_to_client(connfd, "Interprete: ");
        scanf(" %[^\n]", music->artist);

        send_to_client(connfd, "Idioma: ");
        scanf(" %[^\n]", music->language);

        send_to_client(connfd, "Genero musical: ");
        scanf(" %[^\n]", music->type);

        send_to_client(connfd, "Possui refrão? (s/n) ");
        scanf(" %c", &answer);

        if (answer == 's') {
            send_to_client(connfd, "Insira o Refrão: ");
            scanf(" %[^\n]", music->chore);
        } else {
            strcpy(music->chore, "Não possui");
        }

        send_to_client(connfd, "Ano de lancamento: ");
        scanf(" %d", &music->year);

        music->next = musics.musics;

        musics.musics = music;

        send_to_client(connfd, "Musica adicionada com sucesso! Deseja adicionar mais musicas? (s/n) ");
        scanf(" %c", &answer);

        if (answer == 'n') {
            break;
        }
    }

    db_pull(&musics);
};

void remove_song(int connfd) {
    char answer;
    int removeId;
    struct Musics musics;
    db_fetch(&musics);

    send_to_client(connfd, "Insira o ID da musica a ser removida: ");
    scanf(" %d", &removeId);

    while(musics.musics != NULL && musics.musics->id != removeId) {
        musics.musics = musics.musics->next;
    }

    if (musics.musics->id != removeId) {
        send_to_client(connfd, "\nMusica nao encontrada!\n\n");
        return;
    }

    // code
    //logica para remover musica do db
    printf("%d", removeId);


    send_to_client(connfd, "Musica removida com sucesso!\n\n");
};

void list_by_year(int connfd) {
    int yearList;

    struct Musics musics;
    db_fetch(&musics);

    send_to_client(connfd, "Insira o ano: ");
    scanf(" %d", &yearList);

    // code
    // logica pra listar por ano
    send_to_client(connfd, "%d", yearList);

    screen_pause(connfd);
};

void list_by_year_and_language(int connfd) {
    int yearList;
    char languageList[50];
    struct Musics musics;
    db_fetch(&musics);

    send_to_client(connfd, "Insira o ano: ");
    scanf(" %d", &yearList);
    send_to_client(connfd, "Insira o idioma: ");
    scanf(" %[^\n]", languageList);

    // code
    // logica pra listar por ano e idioma
    send_to_client(connfd, "%d %s", yearList, languageList);

    screen_pause(connfd);
};

void list_by_type(int connfd) {
    char typeList[50];
    struct Musics musics;
    db_fetch(&musics);

    send_to_client(connfd, "Insira o tipo: ");
    scanf(" %[^\n]", typeList);

    // code
    //logica pra listar por tipo
    send_to_client(connfd, "%s", typeList);

    screen_pause(connfd);
};

void search_by_id(int connfd) {
    int searchId;
    struct Musics musics;
    db_fetch(&musics);

    while (1) {
        send_to_client(connfd, "Insira o ID da musica: ");
        scanf(" %d", &searchId);

        if (searchId >= 0 && searchId <= musics.n) {
            break;
        }

        send_to_client(connfd, "ID invalido! Tente novamente.\n");
    }

    while(musics.musics != NULL && musics.musics->id != searchId) {
        musics.musics = musics.musics->next;
    }

    if (musics.musics->id != searchId) {
        send_to_client(connfd, "\nMusica nao encontrada!\n\n");
        return;
    }

    send_to_client(connfd, "Identificador Unico: %d\n", musics.musics->id);
    send_to_client(connfd, "Titulo: %s\n", musics.musics->title);
    send_to_client(connfd, "Interprete: %s\n", musics.musics->artist);
    send_to_client(connfd, "Idioma: %s\n", musics.musics->language);
    send_to_client(connfd, "Genero musical: %s\n", musics.musics->type);
    send_to_client(connfd, "Refrão: %s\n", musics.musics->chore);
    send_to_client(connfd, "Ano de lancamento: %d\n\n", musics.musics->year);

    screen_pause(connfd);
};

void list_all(int connfd) {
    struct Musics musics;
    db_fetch(&musics);

    send_to_client(connfd, "\nLista de todas as Musicas:\n");

    while (musics.musics != NULL) {
        send_to_client(connfd, "Identificador Unico: %d\n", musics.musics->id);
        send_to_client(connfd, "Titulo: %s\n", musics.musics->title);
        send_to_client(connfd, "Interprete: %s\n", musics.musics->artist);
        send_to_client(connfd, "Idioma: %s\n", musics.musics->language);
        send_to_client(connfd, "Genero musical: %s\n", musics.musics->type);
        send_to_client(connfd, "Refrão: %s\n", musics.musics->chore);
        send_to_client(connfd, "Ano de lancamento: %d\n\n", musics.musics->year);
        musics.musics = musics.musics->next;
    }

    screen_pause(connfd);
}

/* Server Initialization */

int serve_client(char *clientIp, int connfd) {
    db_initialize();
    int action = 0;

    while (1) {
        send_to_client(connfd, "\n - - - - - -  Socket Concurrent TCP  - - - - - -\n\n");
        send_to_client(connfd, "1. Adicionar uma musica\n");
        send_to_client(connfd, "2. Remover uma musica\n");
        send_to_client(connfd, "3. Listar musicas por ano\n");
        send_to_client(connfd, "4. Listar musicas por ano e idioma\n");
        send_to_client(connfd, "5. Listar musicas por tipo\n");
        send_to_client(connfd, "6. Consultar musica por ID\n");
        send_to_client(connfd, "7. Listar todas as musicas\n");
        send_to_client(connfd, "8. Encerrar\n\n");
        send_to_client(connfd, "Escolha uma ação (numero): ");
        scanf("%d", &action);

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
    }

    return 0;
}

// debug
int main() {
    serve_client("127.0.0.1", 8080);
}