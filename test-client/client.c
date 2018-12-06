#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <pthread.h>
#include <fcntl.h>

#include "warn.h"

#define BUF_SIZE 1024
#define MAX_CLIENT 10

const char* messages_path = "client_messages.txt";

struct parameters
{
  int client_socket;
  int position;
};
 
/* getaddrinfo() renvoie une structure de liste d'adresse.
 * On essaie chaque adresse jusqu'à ce qu'on puisse se connecter au socket.
 * Si socket (ou connect echoue, on (ferme le socket et) essaie l'adresse suivante.
 * On renvoie l'addresse qui sera connectée au socket dans connect_addr.
 * Si un erreur survient connect_addr sera NULL et la fonciton renverra -1.
 */
int create_and_connect_socket(struct addrinfo* addr_list, struct sockaddr* connect_addr, socklen_t* connect_addrlen) {

    int sock_fd;
    int optval = 1;
    struct addrinfo* rp;

    connect_addr = NULL; 
    
    for (rp = addr_list; rp != NULL; rp = rp->ai_next) {
      
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if (sock_fd == -1)
            continue;

        if (connect(sock_fd, rp->ai_addr, rp->ai_addrlen) != -1) {
            connect_addr = rp->ai_addr;
            (*connect_addrlen) = rp->ai_addrlen;
            break;      /* Success */
        }

       close(sock_fd);
    }

    return connect_addr == NULL ? -1 : sock_fd;
}

void init_client(int client_socket){
  size_t line_size = 0;
  int res, count = 0, number_lines = 0;
  char* line = NULL;

  FILE *fp = fopen(messages_path,"a");
  while(1){

    //nombre de lignes dans le fichier
    res = recv(client_socket,&number_lines,sizeof(number_lines),0);
    if (res == -1)
    {
      continue;
    }
    if (number_lines > 0)
    {
      break;
    }
  }

  while(1){

    //envoie des lignes dans le fichier
    res = recv(client_socket,line_size,sizeof(line_size),0);
    if (res == -1)
    {
      continue;
    }

    res = recv(client_socket,line,line_size,0);
    if (res == -1)
    {
      continue;
    }
    printf("%s\n", line);

    if(line_size != 0 && line != NULL){
    //creation du fichier
    res = fputs(line, fp);
    if (res == EOF)
    {
      fprintf(stderr,"Erreur durant l'écriture du fichier \n");
      exit(1);
    }
    line_size = 0;line = NULL;count++;
    }
    if (count == number_lines)
    {
      break;
    }
  }
  free(line);
  fclose(fp);

  return;
}

void print_chat(struct parameters *p){
  
  //affichage du chat
  char* filename = "/data.txt";
  char buffer[BUF_SIZE];
  FILE *fp = fopen(filename,"w+");
  fseek(fp, 0, SEEK_END);
  int size = ftell(fp);
  fseek( fp, p->position, SEEK_SET );
  p->position = size;
  while(fgets(buffer, BUF_SIZE,(FILE *)fp) != NULL){
    printf("%s \n", buffer);
  }
  fclose(fp);
  return;
}

void* rcv_chat_update(void* parameters){
  struct parameters *p= (struct parameters *) parameters;

  while(1){
    printf("Update : ");

    ssize_t nread = 0;
    char update[BUF_SIZE];

    //reception d'un message
    nread = recv(p->client_socket,update,BUF_SIZE,0);
    if (nread == -1)
    {
      continue;
    }

    if(update != NULL){
      //reecriture du fichier data
      FILE *fp = fopen(messages_path,"a");
      nread = fputs(update, fp);
      if (nread == EOF)
      {
        fprintf(stderr,"Erreur durant la réecriture du fichier \n");
        exit(1);
      }
      fclose(fp);

      continue;
    }
  }

  print_chat(p);

  pthread_exit(NULL);
}

void* snd_chat_msg(void* parameters){
  struct parameters *p= (struct parameters *)parameters;

  while (1) {

        printf("Message : ");

        ssize_t nread = 0;
        size_t message_size = 0;
        char* message;

        nread = getline(&message, &message_size, stdin);
        if (nread == -1)
        {
          continue;
        }

        // printf("Envoie de la taille du message...\n");

        // nread = send(p->client_socket, message_size, sizeof(size_t), 0);
        // if (nread == -1)
        // {
        //   continue;
        // }

        printf("Envoie du message en cours...\n");

        nread = send(p->client_socket, message, message_size, 0);
        if (nread == -1)
        {
          continue;
        }

        printf("Attente d'une réponse du serveur...\n");

        nread = recv(p->client_socket, message, BUF_SIZE, 0);
        if (nread == -1)
        {
          continue;
        }
        
        // printf("┌[RECEPTION HOTE : %s, PORT : %s]\n", server_address, server_port);
        printf("└─▶ %s", message);

        free(message);
    }
  pthread_exit(NULL);
}

int main(int argc, char const *argv[]){

    PRINT_USAGE_IF(argc < 3, "Usage %s <HOTE> <PORT_>\n", argv[0]);
    
    const char* server_address = argv[1];
    const char* server_port = argv[2];

    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */

    printf("Allocation de la liste d'adresses de l'HOTE : %s, PORT : %s...\n", server_address, server_port);

    int error = getaddrinfo(server_address, server_port, &hints, &result); WARN_ERROR_GAI(error);

    printf("Création du socket et connexion sur une adresse de l'HOTE : %s, PORT : %s...\n", server_address, server_port);

    struct sockaddr server_addr;
    socklen_t server_addr_len;

    int client_socket = create_and_connect_socket(result, &server_addr, &server_addr_len); WARN_ERROR(client_socket);

    printf("Liberation de la liste d'adresses de l'HOTE : %s, PORT : %s...\n", server_address, server_port);

    freeaddrinfo(result);

    printf("Connexion en cours...\n");

    init_client(client_socket);


    struct parameters *p = (struct parameters *)malloc(2*sizeof(int));
    p->client_socket = client_socket;
    p->position = 0;
    pthread_t idT[2];

    //tread d'update des message
    int thread = pthread_create(&idT[0],NULL,rcv_chat_update,(void*)p);
    WARN_ERROR_PTHREAD(thread);

    //tread d'envoie de message
    thread = pthread_create(&idT[1],NULL,snd_chat_msg,(void*)p);
    WARN_ERROR_PTHREAD(thread);

    thread = pthread_join(idT[0],NULL);
    WARN_ERROR_PTHREAD(thread);
    thread = pthread_join(idT[1],NULL);  
    WARN_ERROR_PTHREAD(thread);

    close(client_socket);

    return 0;
}
