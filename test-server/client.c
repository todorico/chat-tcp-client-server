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
#include "chat.h"

#define BUF_SIZE 1024
#define MAX_CLIENT 10
 
/* getaddrinfo() renvoie une structure de liste d'adresse.
 * On essaie chaque adresse jusqu'à ce qu'on puisse se connecter au socket.
 * Si socket (ou connect echoue, on (ferme le socket et) essaie l'adresse suivante.
 * On renvoie l'addresse qui sera connectée au socket dans connect_addr.
 * Si un erreur survient connect_addr sera NULL et la fonciton renverra -1.
 */
int create_connected_socket(const char* host, const char* service, struct addrinfo* hints) {
	int error;
    struct addrinfo *result;

    //printf("Obtention d'informations sur l'HOTE : %s au PORT : %s...\n", host, service);

    error = getaddrinfo(host, service, hints, &result); WARN_ERROR_GAI(error);

    //printf("Création et nommage de la socket...\n");

    int socket_fd;
    struct addrinfo* rp;
    
    int optval = 1;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (socket_fd == -1)
            continue;

        // Permet de réutilliser le port directement
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if (connect(socket_fd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;                  /* Success */

        close(socket_fd);
    }

    freeaddrinfo(result);

    return socket_fd;
}

void* handle_messages_sending(void* sockfd) {
	int sock_fd = *(int*)sockfd;

    CHAT_MESSAGE message;
    CHAT_MESSAGE_init(&message);

    char date[STR_BUF_SIZE] = "0 av JC";
    char sender[STR_BUF_SIZE] = "Jesus-christ";

    strcpy(message.date, date);
    strcpy(message.sender, sender);

    while (1) {

        ssize_t nread = 0;
        size_t msg_size = 0;
        char* msg;

        printf("Entrez un message à envoyer :\n");
        nread = getline(&msg, &msg_size, stdin); WARN_ERROR(nread);

        strcpy(message.data, msg);

        CHAT_MESSAGE_send(sock_fd, &message);

        free(msg);
    }
}

void* handle_messages_reception(void* sockfd) {
	int sock_fd = *(int*)sockfd;

    CHAT_MESSAGE message;
    CHAT_MESSAGE_init(&message);

    while (1) {
        CHAT_MESSAGE_recv(sock_fd, &message);
        
        printf("Message reçu :\n");
        CHAT_MESSAGE_print(&message);
    }
}

int main(int argc, char const *argv[]){

    PRINT_USAGE_IF(argc < 3, "Usage %s <HOTE> <PORT>\n", argv[0]);
    
    const char* server_address = argv[1];
    const char* server_port = argv[2];

    int error;

    printf("Création et connection du socket à l'HOTE : %s, PORT : %s...\n", server_address, server_port);
    struct addrinfo hints;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;	 // Autorise IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; // Communication TCP

    int client_socket = create_connected_socket(server_address, server_port, &hints); WARN_ERROR(client_socket);

    printf("Synchronisation avec le serveur...\n");
    CHAT_SHARED shared;
    CHAT_SHARED_recv(client_socket, &shared);

    CHAT_SHARED_print_messages(&shared);
    
    printf("Creation des threads d'envoi et de reception de message...\n");
    pthread_t thread_ids[2];

    error = pthread_create(&thread_ids[0], NULL, handle_messages_sending, &client_socket); WARN_ERROR_PTHREAD(error);
    error = pthread_create(&thread_ids[1], NULL, handle_messages_reception, &client_socket); WARN_ERROR_PTHREAD(error);

    error = pthread_join(thread_ids[0], NULL); WARN_ERROR_PTHREAD(error);
    error = pthread_join(thread_ids[1], NULL); WARN_ERROR_PTHREAD(error);

    printf("Nettoyage des structures allouées...\n");
    close(client_socket);

    printf("Le client s'est terminé proprement...\n");

    return 0;
}