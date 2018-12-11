#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

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

// DEFINITION DES STRUCTURES

typedef struct {
	int sockfd;
	char pseudo[STR_BUF_SIZE];
} MESSAGE_WRITER_PARAM;

// DECLARATION DES FONCTIONS

int create_connected_socket(const char* host, const char* service, struct addrinfo* hints);

void* handle_messages_sending(void* writer_param);
void* handle_messages_reception(void* sockfd);

void clean_exit(int signo);

// PROGRAMME SERVEUR

int main(int argc, char const *argv[]){

    PRINT_USAGE_IF(argc != 3, "Usage %s <HOTE> <PORT>\n", argv[0]);
    
    const char* server_host = argv[1];
    const char* server_port = argv[2];

    int error;

	printf("Mise en place du gestionnaire de signal...\n");

	struct sigaction signal;

	signal.sa_handler = &clean_exit;
	signal.sa_flags = 0;

	sigemptyset(&signal.sa_mask); // On ne bloque pas de signaux spécifiques.

	sigaction(SIGINT, &signal, 0);
	sigaction(SIGQUIT, &signal, 0);
	sigaction(SIGTERM, &signal, 0);

    printf("Création du socket coté client et connection sur le port %s de l'hote %s\n", server_port, server_host);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;	 // Autorise IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; // Communication TCP
    hints.ai_flags = AI_PASSIVE;     // For wildcard IP address

    int client_socket_fd = create_connected_socket(server_host, server_port, &hints); WARN_ERROR(client_socket_fd);

    int pseudo_min_len = 3;
    int pseudo_current_len = 0;

    printf("\nVeuillez choisir un pseudo d'au moins %d charactères :\n", pseudo_min_len);

    MESSAGE_WRITER_PARAM writer_param;
    memset(&writer_param, 0, sizeof(MESSAGE_WRITER_PARAM));
    writer_param.sockfd = client_socket_fd;

    while (pseudo_current_len < pseudo_min_len) {
    	fgets(writer_param.pseudo, STR_BUF_SIZE, stdin);//WARN_ERROR_IF(strcmp(writer_param.pseudo, "") == 0);

		pseudo_current_len = strlen(writer_param.pseudo) - 1; // -1 car strlen compte le \n

    	if (pseudo_current_len < pseudo_min_len)
    		printf("Vous devez rentrer un pseudo d'au moins %d charactères...\n", pseudo_min_len);
    }

    printf("\nSynchronisation avec le serveur...\n");
    
    CHAT_SHARED shared;
    CHAT_SHARED_init(&shared);

    CHAT_SHARED_recv(client_socket_fd, &shared);

    CHAT_SHARED_print_messages(&shared);
    
    printf("Creation des threads d'envoi et de reception de message...\n");

    pthread_t thread_ids[2];

    error = pthread_create(&thread_ids[0], NULL, handle_messages_sending, &writer_param); WARN_ERROR_PTHREAD(error);
    error = pthread_create(&thread_ids[1], NULL, handle_messages_reception, &client_socket_fd); WARN_ERROR_PTHREAD(error);

    error = pthread_join(thread_ids[0], NULL); WARN_ERROR_PTHREAD(error);
    error = pthread_join(thread_ids[1], NULL); WARN_ERROR_PTHREAD(error);

    printf("Nettoyage des structures allouées...\n");

    close(client_socket_fd);

    printf("Le client s'est terminé proprement...\n");

    return 0;
}

// DEFINITION DES FONCTIONS
 
int create_connected_socket(const char* host, const char* service, struct addrinfo* hints) {
	int error;
    struct addrinfo *result = NULL;

    error = getaddrinfo(host, service, hints, &result); WARN_ERROR_GAI(error);

    int socket_fd = -1;
    struct addrinfo* rp = NULL;
    int k = -1;
    
    int optval = 1;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        k++;
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (socket_fd == -1)
            continue;

        // Permet de réutilliser le port directement
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if ((error = connect(socket_fd, rp->ai_addr, rp->ai_addrlen)) != -1) {
        	printf("Creation et connection du socket (%d) reussie au bout de la tentative %d, connect returned %d\n", socket_fd, k, error);
        	WARN_IF(errno == EAGAIN);
        	WARN_IF(errno == EWOULDBLOCK);
            break;                  /* Success */
        }

        close(socket_fd);
    }

    freeaddrinfo(result);

    // Si rp est NULL alors la connection à échoué.

    return !rp ? -1 : socket_fd;
}

void* handle_messages_sending(void* writer_param) {
	MESSAGE_WRITER_PARAM* param = (MESSAGE_WRITER_PARAM*)writer_param;

    CHAT_MESSAGE message;
    CHAT_MESSAGE_init(&message);
    strcpy(message.sender, param->pseudo);

	time_t rawtime;
	struct tm* timeinfo = NULL;

    printf("\nVous pouvez commencer à ecrire des messages (Pour arrêter tapez Ctrl+C)\n\n");
    
    while (1) {
        fgets(message.data, MESSAGE_DATA_BUF_SIZE, stdin); WARN_ERROR_IF(strcmp(message.data, "") == 0);

        if (strcmp(message.data, "\n") != 0) {
        	time (&rawtime);
			timeinfo = localtime (&rawtime);
			strcpy(message.date, asctime(timeinfo)); // Convertie la date dans une chaine.

        	CHAT_MESSAGE_send(param->sockfd, &message); // Envoie du message si la chaine data n'est pas vide.
        }
    }
}

void* handle_messages_reception(void* sockfd) {
	int sock_fd = *(int*)sockfd;

    CHAT_MESSAGE message;
    CHAT_MESSAGE_init(&message);

    while (1) {
        CHAT_MESSAGE_recv(sock_fd, &message);
        
        printf("Message reçu :\n\n");
        CHAT_MESSAGE_print(&message);
    }
}

void clean_exit(int signo) {
    printf("Le signal numero %d à été intercepté...\n", signo);
    printf("Le client ce termine proprement...\n");
    exit(EXIT_SUCCESS);
}