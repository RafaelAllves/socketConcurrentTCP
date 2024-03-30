#include <stdio.h>
#include <stdlib.h>
#include <string.h>


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


void add_song(struct Musics *musics) {
  char answer;
  
  while (1) {
    struct Music music;
    music.id = musics->n;
    
    printf("Preencha os seguintes campos para adicionar uma musica.\n");
    
    printf("Titulo: ");
    scanf(" %[^\n]", music.title);
    
    printf("Interprete: ");
    scanf(" %[^\n]", music.artist);
    
    printf("Idioma: ");
    scanf(" %[^\n]", music.language);
    
    printf("Genero musical: ");
    scanf(" %[^\n]", music.type);
    
    printf("Possui refrão? [s/n] ");
    scanf(" %c", &answer);
    
    if (answer == 's') {
      printf("Insira o Refrão: ");
      scanf(" %[^\n]", music.chore);
    } else {
      strcpy(music.chore, "Não possui");
    }
    
    printf("Ano de lancamento: ");
    scanf(" %d", &music.year);

    musics -> musics[musics->n] = music;
    musics->n++;

    printf("Musica adicionada com sucesso! Deseja adicionar mais musicas? [s/n] ");
    scanf(" %c", &answer);
    
    if (answer == 'n') {
      break;
    }
  }  
};


void remove_song(struct Musics *musics) {
  char answer;
  int removeId;

  while (1) {
    printf("Insira o ID da musica a ser removida: ");
    scanf(" %d", &removeId);

    // code
    printf("%d", removeId);

    printf("Musica removida com sucesso! Deseja remover mais musicas? [s/n] ");
    scanf(" %c", &answer);

    if (answer == 'n') {
      break;
    }
  }
};


void list_by_year(struct Musics *musics) {
  int yearList;

  printf("Insira o ano: ");
  scanf(" %d", &yearList);

  // code
  printf("%d", yearList);

};


void list_by_year_and_language(struct Musics *musics) {
  int yearList;
  char languageList[50];

  printf("Insira o ano: ");
  scanf(" %d", &yearList);
  printf("Insira o idioma: ");
  scanf(" %[^\n]", languageList);

  // code
  printf("%d %s", yearList, languageList);
  
};


void list_by_type(struct Musics *musics) {
  char typeList[50];

  printf("Insira o tipo: ");
  scanf(" %[^\n]", typeList);

  // code
  printf("%s", typeList);

};


void search_by_id(struct Musics *musics) {
  int searchId;

  while(1) {
    printf("Insira o ID da musica: ");
    scanf(" %d", &searchId);

    if (searchId > 0 && searchId < musics->n){
      break;
    }
  }
  
  printf("Identificador Unico: %d\n", musics -> musics[searchId].id);
  printf("Titulo: %s\n", musics -> musics[searchId].title);
  printf("Interprete: %s\n", musics -> musics[searchId].artist);
  printf("Idioma: %s\n", musics -> musics[searchId].language);
  printf("Genero musical: %s\n", musics -> musics[searchId].type);
  printf("Refrão: %s\n", musics -> musics[searchId].chore);
  printf("Ano de lancamento: %d\n\n", musics -> musics[searchId].year);

};


void list_all(struct Musics *musics){
  printf("\nLista de todas as Musicas:\n");

  for (int i = 0; i < musics -> n; i++) {
      printf("Identificador Unico: %d\n", musics -> musics[i].id);
      printf("Titulo: %s\n", musics -> musics[i].title);
      printf("Interprete: %s\n", musics -> musics[i].artist);
      printf("Idioma: %s\n", musics -> musics[i].language);
      printf("Genero musical: %s\n", musics -> musics[i].type);
      printf("Refrão: %s\n", musics -> musics[i].chore);
      printf("Ano de lancamento: %d\n\n", musics -> musics[i].year);
  }
}


int main(void) {
  struct Musics musics;
  musics.n = 0;
  int action = 0;
  
  while(1) {
    printf("Socket Concurrent TCP\n");
    printf("1. Adicionar uma musica\n");
    printf("2. Remover uma musica\n");
    printf("3. Listar musicas por ano\n");
    printf("4. Listar musicas por ano e idioma\n");
    printf("5. Listar musicas por tipo\n");
    printf("6. Consultar musica por ID\n");
    printf("7. Listar todas as musicas\n");
    printf("8. Encerrar\n");
    printf("Escolha uma ação: [n]\n");
    scanf("%d", &action);

    switch (action) {
      case 1:
        add_song(&musics);
        break;
      case 2:
        remove_song(&musics);
        break;
      case 3:
        list_by_year(&musics);
        break;
      case 4:
        list_by_year_and_language(&musics);
        break;
      case 5:
        list_by_type(&musics);
        break;
      case 6:
        search_by_id(&musics);
        break;
      case 7:
        list_all(&musics);
        break;
      case 8:
        exit(0);
    }
  }
  
  return 0;
}