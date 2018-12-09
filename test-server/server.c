#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include <pthread.h>
#include <fcntl.h>

#include "warn.h"
#include "chat.h"

#define SHM_CUSTOM_KEY 1235
#define SEM_CLIENT_CUSTOM_KEY 1236
#define SEM_SHARED_CUSTOM_KEY 1237

void* handle_client_messages(void* handler_param) {
    int error;

    CHAT_MESSAGE message;
    CHAT_MESSAGE_init(&message);

    CHAT_HANDLER_PARAM* param = (CHAT_HANDLER_PARAM*)handler_param;

    int shared_semid = semget(SEM_SHARED_CUSTOM_KEY, 1, 0666); WARN_ERROR(shared_semid);

    int client_semid = semget(SEM_CLIENT_CUSTOM_KEY, NB_MAX_CLIENT, 0666); WARN_ERROR(client_semid);

    struct sembuf opp;
    opp.sem_num = 0;
    opp.sem_op = -1;
    opp.sem_flg = SEM_UNDO;

    struct sembuf opv;
    opv.sem_num = 0;
    opv.sem_op = +1;
    opv.sem_flg = SEM_UNDO;

    while (1) {

        CHAT_MESSAGE_recv(param->sockfd, &message);
        
        printf("Message reçu :\n");
        CHAT_MESSAGE_print(&message);

        error = semop(shared_semid, &opp, 1); WARN_ERROR(error); // verrouille l'acces au segment de mémoire

        CHAT_SHARED_add_message(param->shared, &message); // Ajoute le message dans le segment de mémoire

        error = semop(shared_semid, &opv, 1); WARN_ERROR(error); // deverrouille l'acces au segment de mémoire
        
        printf("Reveille de tous les semaphores client en attente...\n");
        CHAT_SHARED_broadcast(param->shared, client_semid);
    }
}

void* handle_shared_messages(void* handler_param) {
    int error;

    CHAT_MESSAGE* message = NULL;
    CHAT_HANDLER_PARAM* param = (CHAT_HANDLER_PARAM*)handler_param;

    int semid = semget(SEM_CLIENT_CUSTOM_KEY, NB_MAX_CLIENT, 0666); WARN_ERROR(semid);

    int semnum = CHAT_SHARED_semaphore_num(param->shared, param->sockfd); WARN_ERROR(semnum);

    struct sembuf opp;
    opp.sem_num = semnum;
    opp.sem_op = -1;
    opp.sem_flg = SEM_UNDO;

    while (1) {
        
        printf("Semaphore client N°%d en attente...\n", semnum);
        error = semop(semid, &opp, 1); WARN_ERROR(error); // Attend jusqu'a ce que le semaphore semnum soit reveillé
        
        printf("Semaphore client N°%d reveillé !\n", semnum);
        message = CHAT_SHARED_last_message(param->shared);
        //error = semop(semid, &opv, NB_MAX_CLIENT); WARN_ERROR(error); // verrouille l'acces au segment de mémoire
        
        printf("Envoie du dernier message enregistré...\n");
        CHAT_MESSAGE_print(message);

        CHAT_MESSAGE_send(param->sockfd, message);
    }
}

/* getaddrinfo() renvoie une structure de liste d'adresse.
 * On essaie chaque adresse jusqu'à ce qu'on puisse en liée une au socket.
 * Si socket (ou bind echoue, on (ferme le socket et) essaie l'adresse suivante.
 * On renvoie l'adresse qui sera liée au socket dans bind_addr.
 * Si un erreur survient bind_addr sera NULL et la foncton renverra -1.
 */
int create_named_socket(const char* host, const char* service, struct addrinfo* hints) {

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

        if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;                  /* Success */

        close(socket_fd);
    }

    freeaddrinfo(result);

    return socket_fd;
}

pid_t fork_process(void){
    pid_t pid;

    do{
        pid = fork();
    } 
    while((pid == -1) && errno == EAGAIN);

    return pid;
}

// renvoie les message au client
void handle_client_connection(int client_socket, const char* host, const char* service){
    int error;

    printf("La session d'ID : %d s'attache au segment de mémoire...\n", client_socket);
    int shmid = shmget(SHM_CUSTOM_KEY, 0, 0666); WARN_ERROR(shmid);

    CHAT_HANDLER_PARAM handler_param;
    handler_param.sockfd = client_socket;
    handler_param.shared = shmat(shmid, NULL, 0); WARN_ERROR_IF(handler_param.shared == (void*)-1);

    printf("La session d'ID : %d recupère le semaphore pour modifier le segment de mémoire...\n", client_socket);
    int semid = semget(SEM_SHARED_CUSTOM_KEY, 1, 0666); WARN_ERROR(semid);

    struct sembuf opp;
    opp.sem_num = 0;
    opp.sem_op = -1;
    opp.sem_flg = SEM_UNDO;

    struct sembuf opv;
    opv.sem_num = 0;
    opv.sem_op = 1;
    opv.sem_flg = SEM_UNDO;

    printf("La session d'ID : %d demande l'accès au segment mémoire...\n", client_socket);
    error = semop(semid, &opp, 1); WARN_ERROR(error); // verrouille l'acces au segment de mémoire
    printf("La session d'ID : %d ajoute sont indentifiant dans le segment de mémoire...\n", client_socket);
    CHAT_SHARED_add_socket(handler_param.shared, client_socket);
    error = semop(semid, &opv, 1); WARN_ERROR(error); // deverrouille l'acces au segment de mémoire

    printf("La session d'ID : %d ce synchronise avec le client...\n", client_socket);
    CHAT_SHARED_send(client_socket, handler_param.shared);

    pthread_t thread_ids[2];

    printf("La session d'ID : %d créér les threads de reception et de synchronisation avec le client...\n", client_socket);
    error = pthread_create(&thread_ids[0], NULL, handle_client_messages, &handler_param); WARN_ERROR_PTHREAD(error);
    error = pthread_create(&thread_ids[1], NULL, handle_shared_messages, &handler_param); WARN_ERROR_PTHREAD(error);

    error = pthread_join(thread_ids[0], NULL); WARN_ERROR_PTHREAD(error);
    error = pthread_join(thread_ids[1], NULL); WARN_ERROR_PTHREAD(error);
    
    error = semop(SEM_SHARED_CUSTOM_KEY, &opp, 1); WARN_ERROR(error); // verrouille l'acces au segment de mémoire
    CHAT_SHARED_remove_socket(handler_param.shared, client_socket);
    error = semop(SEM_SHARED_CUSTOM_KEY, &opv, 1); WARN_ERROR(error); // deverrouille l'acces au segment de mémoire

    error = shmdt(handler_param.shared); WARN_ERROR(error);

    printf("La session d'ID : %d s'est terminée proprement...\n", client_socket);
}

void clean_exit(int num) {

}

int main(int argc, char const *argv[]) {   
    int error;

    PRINT_USAGE_IF(argc < 2, "Usage %s <PORT>\n", argv[0]);

    const char* server_port = argv[1];

    union semun  {
        int val;
        struct semid_ds* buf;
        ushort* array;
    } sem_arg;

    printf("Création et initialisation du segment de mémoire partagé...\n");
    int shmid = shmget(SHM_CUSTOM_KEY, sizeof(CHAT_SHARED), IPC_CREAT | 0666); WARN_ERROR(shmid);

    CHAT_SHARED* shared = shmat(shmid, NULL, 0); WARN_ERROR_IF(shared == (void*)-1);
    CHAT_SHARED_init(shared);

    printf("Création d'un sémaphore pour la modification du segment de mémoire...\n");
    int shared_semid = semget(SEM_SHARED_CUSTOM_KEY, 1, 0666 | IPC_CREAT); WARN_ERROR(shared_semid);

    sem_arg.val = 1; // Initialise le semaphore segment mémoire à 1 pour crééer un verrou

    error = semctl(shared_semid, 0, SETVAL, sem_arg);

    printf("Création des sémaphores pour la synchronisation des clients...\n");
    int client_semid = semget(SEM_CLIENT_CUSTOM_KEY, NB_MAX_CLIENT, 0666 | IPC_CREAT); WARN_ERROR(client_semid);

    sem_arg.val = 0; // Initialise les sémaphores client a 0 pour créér une barrière

    for (int i = 0; i < NB_MAX_CLIENT; ++i) {
        error = semctl(client_semid, i, SETVAL, sem_arg); WARN_ERROR(error);
    }

    printf("Création et nommage du socket serveur...\n");
    struct addrinfo hints;
    
    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;     // Autorise IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; // Communication TCP
    hints.ai_flags = AI_PASSIVE;     // For wildcard IP address

    int server_socket = create_named_socket(NULL, server_port, &hints); WARN_ERROR(server_socket);

    printf("Activation du mode connecté pouvant gerer jusqu'à %d client(s)...\n", NB_MAX_CLIENT);
    error = listen(server_socket, NB_MAX_CLIENT); WARN_ERROR(error);

    int client_socket;
    struct sockaddr_storage client_addr; // Peut contenir à la fois des adresses IPv4 et IPv6
    socklen_t client_addr_len = sizeof(client_addr);

    printf("Serveur en attente de connexions...\n");
    printf("IP : %s, PORT : %s\n", "localhost", server_port);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*) &client_addr, &client_addr_len); WARN_ERROR(client_socket);

        char client_address_str[NI_MAXHOST] = "";
        char client_port_str[NI_MAXSERV] = "";

        error = getnameinfo((struct sockaddr*) &client_addr, client_addr_len, client_address_str, sizeof(client_address_str), client_port_str, sizeof(client_port_str), NI_NUMERICHOST|NI_NUMERICSERV); WARN_ERROR_GAI(error);
        
        printf("Creation d'un processus dedié au client d'IP : %s et de PORT : %s\n", client_address_str, client_port_str);
        pid_t pid = fork_process(); WARN_ERROR(pid);

        if (pid == 0) {
            close(server_socket);
            handle_client_connection(client_socket, client_address_str, client_port_str);
            close(client_socket);
            exit(EXIT_SUCCESS);
        }
    }

    printf("Nettoyage des structures allouées...\n");
    CHAT_SHARED_close_sockets(shared);
    shmdt(shared);
    
    close(server_socket);

    shmctl(SHM_CUSTOM_KEY, 0, IPC_RMID);
    semctl(SEM_CLIENT_CUSTOM_KEY, 0, IPC_RMID);
    semctl(SEM_SHARED_CUSTOM_KEY, 0, IPC_RMID);

    printf("le serveur s'est terminé proprement\n");

    return 0;
}