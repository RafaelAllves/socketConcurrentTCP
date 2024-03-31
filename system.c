#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

struct Music {
  int id;
  char title[100];
  char artist[100];
  char language[50];
  char type[50];
  char chore[255];
  int year;
};


struct Musics {
  struct Music musics[100];
  int n;
};

void send_to_client(int connfd, const char *format, ...) {
    char message[100];
    va_list args;

    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    if (send(connfd, message, strlen(message), 0) == -1) {
        perror("send");
    }
}


void add_song(int connfd, struct Musics *musics) {
  char answer;
  
  while (1) {
    struct Music music;
    music.id = musics->n;
    
    send_to_client(connfd, "Preencha os seguintes campos para adicionar uma musica.\n");
    
    send_to_client(connfd, "Titulo: ");
    scanf(" %[^\n]", music.title);
    
    send_to_client(connfd, "Interprete: ");
    scanf(" %[^\n]", music.artist);
    
    send_to_client(connfd, "Idioma: ");
    scanf(" %[^\n]", music.language);
    
    send_to_client(connfd, "Genero musical: ");
    scanf(" %[^\n]", music.type);
    
    send_to_client(connfd, "Possui refrão? [s/n] ");
    scanf(" %c", &answer);
    
    if (answer == 's') {
      send_to_client(connfd, "Insira o Refrão: ");
      scanf(" %[^\n]", music.chore);
    } else {
      strcpy(music.chore, "Não possui");
    }
    
    send_to_client(connfd, "Ano de lancamento: ");
    scanf(" %d", &music.year);

    musics -> musics[musics->n] = music;
    musics->n++;

    send_to_client(connfd, "Musica adicionada com sucesso! Deseja adicionar mais musicas? [s/n] ");
    scanf(" %c", &answer);
    
    if (answer == 'n') {
      break;
    }
  }  
};


void remove_song(int connfd, struct Musics *musics) {
  char answer;
  int removeId;

  while (1) {
    send_to_client(connfd, "Insira o ID da musica a ser removida: ");
    scanf(" %d", &removeId);

    // code
    send_to_client(connfd, "%d", removeId);

    send_to_client(connfd, "Musica removida com sucesso! Deseja remover mais musicas? [s/n] ");
    scanf(" %c", &answer);

    if (answer == 'n') {
      break;
    }
  }
};


void list_by_year(int connfd, struct Musics *musics) {
  int yearList;

  send_to_client(connfd, "Insira o ano: ");
  scanf(" %d", &yearList);

  // code
  send_to_client(connfd, "%d", yearList);

};


void list_by_year_and_language(int connfd, struct Musics *musics) {
  int yearList;
  char languageList[50];

  send_to_client(connfd, "Insira o ano: ");
  scanf(" %d", &yearList);
  send_to_client(connfd, "Insira o idioma: ");
  scanf(" %[^\n]", languageList);

  // code
  send_to_client(connfd, "%d %s", yearList, languageList);
  
};


void list_by_type(int connfd, struct Musics *musics) {
  char typeList[50];

  send_to_client(connfd, "Insira o tipo: ");
  scanf(" %[^\n]", typeList);

  // code
  send_to_client(connfd, "%s", typeList);

};


void search_by_id(int connfd, struct Musics *musics) {
  int searchId;

  while(1) {
    send_to_client(connfd, "Insira o ID da musica: ");
    scanf(" %d", &searchId);

    if (searchId > 0 && searchId < musics->n){
      break;
    }
  }
  
  send_to_client(connfd, "Identificador Unico: %d\n", musics -> musics[searchId].id);
  send_to_client(connfd, "Titulo: %s\n", musics -> musics[searchId].title);
  send_to_client(connfd, "Interprete: %s\n", musics -> musics[searchId].artist);
  send_to_client(connfd, "Idioma: %s\n", musics -> musics[searchId].language);
  send_to_client(connfd, "Genero musical: %s\n", musics -> musics[searchId].type);
  send_to_client(connfd, "Refrão: %s\n", musics -> musics[searchId].chore);
  send_to_client(connfd, "Ano de lancamento: %d\n\n", musics -> musics[searchId].year);

};


void list_all(int connfd, struct Musics *musics){
  send_to_client(connfd, "\nLista de todas as Musicas:\n");

  for (int i = 0; i < musics -> n; i++) {
      send_to_client(connfd, "Identificador Unico: %d\n", musics -> musics[i].id);
      send_to_client(connfd, "Titulo: %s\n", musics -> musics[i].title);
      send_to_client(connfd, "Interprete: %s\n", musics -> musics[i].artist);
      send_to_client(connfd, "Idioma: %s\n", musics -> musics[i].language);
      send_to_client(connfd, "Genero musical: %s\n", musics -> musics[i].type);
      send_to_client(connfd, "Refrão: %s\n", musics -> musics[i].chore);
      send_to_client(connfd, "Ano de lancamento: %d\n\n", musics -> musics[i].year);
  }
}


int serve_client(char* clientIp, int connfd) {
  struct Musics musics;
  musics.n = 0;
  int action = 0;
  
  while(1) {
    send_to_client(connfd, "Socket Concurrent TCP - Server\n\n");
    send_to_client(connfd, "1. Adicionar uma musica\n");
    send_to_client(connfd, "2. Remover uma musica\n");
    send_to_client(connfd, "3. Listar musicas por ano\n");
    send_to_client(connfd, "4. Listar musicas por ano e idioma\n");
    send_to_client(connfd, "5. Listar musicas por tipo\n");
    send_to_client(connfd, "6. Consultar musica por ID\n");
    send_to_client(connfd, "7. Listar todas as musicas\n");
    send_to_client(connfd, "8. Encerrar\n\n");
    send_to_client(connfd, "Escolha uma ação [n]: ");
    scanf("%d", &action);

    switch (action) {
      case 1:
        add_song(connfd, &musics);
        break;
      case 2:
        remove_song(connfd, &musics);
        break;
      case 3:
        list_by_year(connfd, &musics);
        break;
      case 4:
        list_by_year_and_language(connfd, &musics);
        break;
      case 5:
        list_by_type(connfd, &musics);
        break;
      case 6:
        search_by_id(connfd, &musics);
        break;
      case 7:
        list_all(connfd, &musics);
        break;
      case 8:
        exit(0);
    }
  }

  return 0;
}