#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

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

// DECLARATION DES FONCTIONS

int create_named_socket(const char* host, const char* service, struct addrinfo* hints);

pid_t fork_process(void);

void handle_client_session(int client_socket_fd);

void* handle_client_messages(void* handler_param);
void* handle_shared_messages(void* handler_param);

void clean_exit(int signo);

// PROGRAMME SERVEUR

int main(int argc, char const *argv[]) {
    int error;

    PRINT_USAGE_IF(argc != 2, "Usage %s <PORT>\n", argv[0]);

    const char* server_port = argv[1];

    union semun  {
        int val;
        struct semid_ds* buf;
        ushort* array;
    } sem_arg;

    printf("Mise en place du gestionnaire de signal...\n");

    struct sigaction signal;

    signal.sa_handler = &clean_exit;
    signal.sa_flags = 0;

    sigemptyset(&signal.sa_mask); // On ne bloque pas de signaux spécifiques.

    sigaction(SIGINT, &signal, 0);
    sigaction(SIGQUIT, &signal, 0);
    sigaction(SIGTERM, &signal, 0);

    printf("Nettoyage des structures IPCs existantes...\n");

    int shared_memory_id;
    int shared_sem_id;
    int client_sem_id;

    if ((shared_memory_id = shmget(SHM_CUSTOM_KEY, 0, 0666)) != -1) {
        printf("Suppression de l'ancien segment de mémoire partagée...\n");
        error = shmctl(shared_memory_id, 0, IPC_RMID); WARN_ERROR(error);
    }

    if ((shared_sem_id = semget(SEM_SHARED_CUSTOM_KEY, 0, 0666)) != -1) {
        printf("Suppression de l'ancien semaphore protegeant le segment de mémoire partagée...\n");
        semctl(shared_sem_id, 0, IPC_RMID);
    }

    if ((client_sem_id = semget(SEM_CLIENT_CUSTOM_KEY, 0, 0666)) != -1) {
        printf("Suppression de l'ancien semaphore synchronisant les clients...\n");
        semctl(client_sem_id, 0, IPC_RMID);
    }

    printf("Création et initialisation du segment de mémoire partagé...\n");

    shared_memory_id = shmget(SHM_CUSTOM_KEY, sizeof(CHAT_SHARED), 0666 | IPC_CREAT); WARN_ERROR(shared_memory_id);

    CHAT_SHARED* shared = (CHAT_SHARED*)shmat(shared_memory_id, NULL, 0); WARN_ERROR_IF(shared == (void*)-1);
    CHAT_SHARED_init(shared);

    printf("Création d'un sémaphore pour sécurisé la modification du segment de mémoire...\n");

    shared_sem_id = semget(SEM_SHARED_CUSTOM_KEY, 1, 0666 | IPC_CREAT); WARN_ERROR(shared_sem_id);

    sem_arg.val = 1; // Initialise le semaphore segment mémoire à 1 pour créer un verrou
    error = semctl(shared_sem_id, 0, SETVAL, sem_arg); WARN_ERROR(error);

    printf("Création de sémaphores pour synchroniser les clients...\n");

    client_sem_id = semget(SEM_CLIENT_CUSTOM_KEY, NB_MAX_CLIENT, 0666 | IPC_CREAT); WARN_ERROR(client_sem_id);

    sem_arg.val = 0; // Initialise les sémaphores à 0 pour créér une barrière

    for (int i = 0; i < NB_MAX_CLIENT; ++i)
        error = semctl(client_sem_id, i, SETVAL, sem_arg); WARN_ERROR(error);

    printf("Création et nommage du socket coté serveur...\n");

    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     // Autorise IPv4 ou IPv6
    hints.ai_socktype = SOCK_STREAM; // Communication TCP
    hints.ai_flags = AI_PASSIVE;     // For wildcard IP address

    int server_socket_fd = create_named_socket(NULL, server_port, &hints); WARN_ERROR(server_socket_fd);

    printf("Activation du mode connecté pouvant gerer jusqu'à %d client(s) à la fois...\n", NB_MAX_CLIENT);

    error = listen(server_socket_fd, NB_MAX_CLIENT); WARN_ERROR(error);

    int client_socket_fd;
    struct sockaddr_storage client_addr; // Peut contenir à la fois des adresses IPv4 et IPv6
    socklen_t client_addr_len = sizeof(client_addr);

    char hostname[STR_BUF_SIZE] = "";
    error = gethostname(hostname, STR_BUF_SIZE); WARN_ERROR(error); 

    printf("Serveur en attente de connexions...\n");
    printf("\nHOTE : %s, PORT : %s\n", hostname, server_port);

    char client_address_str[NI_MAXHOST] = "";
    char client_port_str[NI_MAXSERV] = "";

    while (1) {
        client_socket_fd = accept(server_socket_fd, (struct sockaddr*) &client_addr, &client_addr_len); WARN_ERROR(client_socket_fd);
        WARN_IF(client_socket_fd == 0);
        WARN_IF(client_socket_fd == EAGAIN);
        WARN_IF(client_socket_fd == EWOULDBLOCK);
        WARN_IF(errno == EAGAIN);
        WARN_IF(errno == EWOULDBLOCK);

        error = getnameinfo((struct sockaddr*) &client_addr, client_addr_len, client_address_str, sizeof(client_address_str), client_port_str, sizeof(client_port_str), NI_NUMERICHOST|NI_NUMERICSERV); WARN_ERROR_GAI(error);
        
        printf("\nCreation d'une session d'ID %d pour le client d'IP : %s et de PORT : %s\n\n", client_socket_fd, client_address_str, client_port_str);

        pid_t pid = fork_process(); WARN_ERROR(pid);

        if (pid == 0) {
            close(server_socket_fd);
            handle_client_session(client_socket_fd);
            close(client_socket_fd);
            exit(EXIT_SUCCESS);
        }

        strcpy(client_address_str, "");
        strcpy(client_port_str, "");
    }

    printf("Nettoyage des structures allouées...\n");

    CHAT_SHARED_close_sockets(shared);
    shmdt(shared);
    close(server_socket_fd);
    shmctl(shared_memory_id, 0, IPC_RMID);
    semctl(shared_sem_id, 0, IPC_RMID);
    semctl(client_sem_id, 0, IPC_RMID);

    printf("le serveur s'est terminé proprement\n");

    return 0;
}

// DEFINITION DES FONCTIONS

int create_named_socket(const char* host, const char* service, struct addrinfo* hints) {

    int error;
    struct addrinfo *result;

    error = getaddrinfo(host, service, hints, &result); WARN_ERROR_GAI(error);

    int socket_fd;
    struct addrinfo* rp;
    
    int optval = 1;
    int k = -1;

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        k++;
        socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        if (socket_fd == -1)
            continue;

        // Permet de réutilliser le port directement
        setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0){
            printf("Creation et nommage du socket (%d) reussie au bout de la tentative %d\n", socket_fd, k);
            break;                  /* Success */
        }

        close(socket_fd);
    }

    freeaddrinfo(result);

    // Si rp est NULL le nommage à échoué.

    return !rp ? -1 : socket_fd;
}

pid_t fork_process(void) {
    pid_t pid;

    do{
        pid = fork();
    }
    while((pid == -1) && errno == EAGAIN);

    return pid;
}

void handle_client_session(int client_socket_fd) {
    int error;

    printf("Session %d : Accède au segment de mémoire...\n", client_socket_fd);

    int shmid = shmget(SHM_CUSTOM_KEY, 0, 0666); WARN_ERROR(shmid);

    CHAT_HANDLER_PARAM handler_param;
    handler_param.sockfd = client_socket_fd;
    handler_param.shared = (CHAT_SHARED*)shmat(shmid, NULL, 0); WARN_ERROR_IF(handler_param.shared == (void*)-1);

    printf("Session %d : Recupère le semaphore pour modifier le segment de mémoire...\n", client_socket_fd);

    int semid = semget(SEM_SHARED_CUSTOM_KEY, 1, 0666); WARN_ERROR(semid);

    struct sembuf opp;
    opp.sem_num = 0;
    opp.sem_op = -1;
    opp.sem_flg = SEM_UNDO;

    struct sembuf opv;
    opv.sem_num = 0;
    opv.sem_op = 1;
    opv.sem_flg = SEM_UNDO;

    printf("Session %d : Demande l'accès du semaphore segment mémoire...\n", client_socket_fd);

    error = semop(semid, &opp, 1); WARN_ERROR(error); // verrouille l'acces au segment de mémoire
    printf("Session %d : Ajoute sont indentifiant dans le segment de mémoire...\n", client_socket_fd);

    CHAT_SHARED_add_socket(handler_param.shared, client_socket_fd);
    error = semop(semid, &opv, 1); WARN_ERROR(error); // deverrouille l'acces au segment de mémoire

    printf("Session %d : Synchronise le client...\n", client_socket_fd);

    CHAT_SHARED_send(client_socket_fd, handler_param.shared);

    pthread_t thread_ids[2];

    printf("Session %d : Créér les threads de reception et de synchronisation avec le client...\n", client_socket_fd);

    error = pthread_create(&thread_ids[0], NULL, handle_shared_messages, &handler_param); WARN_ERROR_PTHREAD(error);
    error = pthread_create(&thread_ids[1], NULL, handle_client_messages, &handler_param); WARN_ERROR_PTHREAD(error);

    error = pthread_join(thread_ids[0], NULL); WARN_ERROR_PTHREAD(error);
    error = pthread_join(thread_ids[1], NULL); WARN_ERROR_PTHREAD(error);

    printf("Session %d : Nettoyage ses structures allouées...\n", client_socket_fd);
    
    error = semop(SEM_SHARED_CUSTOM_KEY, &opp, 1); WARN_ERROR(error); // verrouille l'acces au segment de mémoire
    printf("Session %d : Enlève sont indentifiant du segment de mémoire...\n", client_socket_fd);

    CHAT_SHARED_remove_socket(handler_param.shared, client_socket_fd);
    error = semop(SEM_SHARED_CUSTOM_KEY, &opv, 1); WARN_ERROR(error); // deverrouille l'acces au segment de mémoire

    error = shmdt(handler_param.shared); WARN_ERROR(error);

    printf("Session %d : Se termine proprement...\n", client_socket_fd);
}

void* handle_client_messages(void* handler_param) {
    int error;

    CHAT_MESSAGE message;
    CHAT_MESSAGE_init(&message);

    CHAT_HANDLER_PARAM* param = (CHAT_HANDLER_PARAM*)handler_param;

    int shared_sem_id = semget(SEM_SHARED_CUSTOM_KEY, 1, 0666); WARN_ERROR(shared_sem_id);

    int client_sem_id = semget(SEM_CLIENT_CUSTOM_KEY, NB_MAX_CLIENT, 0666); WARN_ERROR(client_sem_id);

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
        
        printf("Session %d : Message reçu\n\n", param->sockfd);

        CHAT_MESSAGE_print(&message);

        error = semop(shared_sem_id, &opp, 1); WARN_ERROR(error); // verrouille l'acces au segment de mémoire

        CHAT_SHARED_add_message(param->shared, &message); // Ajoute le message dans le segment de mémoire

        error = semop(shared_sem_id, &opv, 1); WARN_ERROR(error); // deverrouille l'acces au segment de mémoire
        
        printf("Session %d : Reveille tous les semaphores client en attente...\n", param->sockfd);

        CHAT_SHARED_broadcast(param->shared, client_sem_id);
    }

    printf("Session %d : Fin du gestionnaire de reception de messages client\n", param->sockfd);
}

void* handle_shared_messages(void* handler_param) {
    int error;

    CHAT_MESSAGE* message = NULL;
    CHAT_HANDLER_PARAM* param = (CHAT_HANDLER_PARAM*)handler_param;

    int client_sem_id = semget(SEM_CLIENT_CUSTOM_KEY, NB_MAX_CLIENT, 0666); WARN_ERROR(client_sem_id);

    int client_sem_num = CHAT_SHARED_semaphore_num(param->shared, param->sockfd); WARN_ERROR(client_sem_num);

    struct sembuf opp;
    opp.sem_num = client_sem_num;
    opp.sem_op = -1;
    opp.sem_flg = SEM_UNDO;

    while (1) {
        printf("Session %d : Semaphore client en attente...\n", param->sockfd);

        error = semop(client_sem_id, &opp, 1); WARN_ERROR(error); // Attend jusqu'a ce que le semaphore client_sem_num soit reveillé
        
        printf("Session %d : Semaphore client reveillé !\n", param->sockfd);
        
        message = CHAT_SHARED_last_message(param->shared);
        //error = semop(client_sem_id, &opv, NB_MAX_CLIENT); WARN_ERROR(error); // verrouille l'acces au segment de mémoire
        
        printf("Session %d : Envoie du dernier message enregistré...\n", param->sockfd);
        //CHAT_MESSAGE_print(message);

        CHAT_MESSAGE_send(param->sockfd, message);
    }

    printf("Session %d : Fin du gestionnaire de synchronisation client...\n", param->sockfd);
}

void clean_exit(int signo) {
    printf("Le signal numero %d à été intercepté...\n", signo);
    printf("Le serveur ce termine proprement...\n");
    exit(EXIT_SUCCESS);
}