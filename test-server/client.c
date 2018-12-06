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

void recv_and_save_messages(int sockfd, FILE* messages_stream) {

    long messages_count = 0;

    printf("Reception du nombre de messages...\n");
    
    ssize_t nread = recv(sockfd, &messages_count, sizeof(messages_count), 0); WARN_ERROR(nread);

    printf("Reception du contenu des messages\n");

    char* line = NULL;
    size_t line_size = 0;

    for (int i = 0; i < messages_count; ++i) {
        nread = recv(sockfd, &line_size, sizeof(line_size), 0); WARN_ERROR(nread);
        line = (char*) malloc(line_size);
        nread = recv(sockfd, line, line_size, 0); WARN_ERROR(nread);
        fprintf(messages_stream, "%s", line);

        free(line);
    }
}

void print_file(FILE* stream) {

    fseek(stream, 0, SEEK_SET);

    char c = fgetc(stream); 

    while (c != EOF) { 
        printf ("%c", c); 
        c = fgetc(stream); 
    }  
}

const char* messages_path = "client_messages.txt";

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
 
    FILE* messages_stream = fopen(messages_path, "w+"); WARN_ERROR_IF(messages_stream == NULL);
    
    printf("Reception des messages...\n");

    recv_and_save_messages(client_socket, messages_stream);

    printf("Affichage des messages...\n");

    print_file(messages_stream);

    fclose(messages_stream);

    while (1) {

        printf("Message : ");

        ssize_t nread = 0;
        size_t message_size = 0;
        char* message;

        nread = getline(&message, &message_size, stdin); WARN_ERROR(nread);

        printf("Envoie du message en cours...\n");

        nread = send(client_socket, message, BUF_SIZE, 0); WARN_ERROR(nread);

        printf("Attente d'une réponse du serveur...\n");

        nread = recv(client_socket, message, BUF_SIZE, 0); WARN_ERROR(nread);
        
        printf("┌[RECEPTION HOTE : %s, PORT : %s]\n", server_address, server_port);
        printf("└─▶ %s", message);

        free(message);
    }

    close(client_socket);

    return 0;
}

/*

           if (len + 1 > BUF_SIZE) {
               fprintf(stderr,
                       "Ignoring long message in argument %d\n", j);
               continue;
           }

           if (write(sfd, argv[j], len) != len) {
               fprintf(stderr, "partial/failed write\n");
               exit(EXIT_FAILURE);
           }

           nread = read(sfd, buf, BUF_SIZE);
           if (nread == -1) {
               perror("read");
               exit(EXIT_FAILURE);
           }

           printf("Received %zd bytes: %s\n", nread, buf);
           }

 */
/*
           for (;;) {
               peer_addr_len = sizeof(struct sockaddr_storage);
               nread = recvfrom(sfd, buf, BUF_SIZE, 0,
                       (struct sockaddr *) &peer_addr, &peer_addr_len);
               if (nread == -1)
                   continue;              

               char host[NI_MAXHOST], service[NI_MAXSERV];

               s = getnameinfo((struct sockaddr *) &peer_addr,
                               peer_addr_len, host, NI_MAXHOST,
                               service, NI_MAXSERV, NI_NUMERICSERV);
               if (s == 0)
                   printf("Received %zd bytes from %s:%s\n",
                           nread, host, service);
               else
                   fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

               if (sendto(sfd, buf, nread, 0,
                           (struct sockaddr *) &peer_addr,
                           peer_addr_len) != nread)
                   fprintf(stderr, "Error sending response\n");
           }

*/