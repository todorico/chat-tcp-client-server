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
 * On essaie chaque adresse jusqu'à ce qu'on puisse en liée une au socket.
 * Si socket (ou bind echoue, on (ferme le socket et) essaie l'adresse suivante.
 * On renvoie l'adresse qui sera liée au socket dans bind_addr.
 * Si un erreur survient bind_addr sera NULL et la foncton renverra -1.
 */
int create_and_bind_socket(struct addrinfo* addr_list, struct sockaddr* bind_addr, socklen_t* bind_addrlen) {
    
    int sock_fd;
    int optval = 1;
    struct addrinfo* rp;

    bind_addr = NULL; 

    for (rp = addr_list; rp != NULL; rp = rp->ai_next) {
        
        sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);

        // Permet de réutilliser le port directement
        setsockopt(sock_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

        if (sock_fd == -1)
            continue;

        if (bind(sock_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            bind_addr = rp->ai_addr;
            (*bind_addrlen) = rp->ai_addrlen;
            break;                  /* Success */
        }

        close(sock_fd);
    }

    return bind_addr == NULL ? -1 : sock_fd;
}

pid_t fork_process(void){
    
    pid_t pid;

    do{
        pid = fork();
    } 
    while((pid == -1) && errno == EAGAIN);

    return pid;
}

int count_lines(FILE* stream) {

    char c;  // To store a character read from file 
    int count = 0;  // Line counter (result) 
  
    if (stream == NULL) 
        return -1;
  
    for (c = getc(stream); c != EOF; c = getc(stream)) 
        if (c == '\n') // Increment count if this character is newline 
            count = count + 1; 

    return count;
}

const char* messages_path = "server_messages.txt";


void read_and_send_messages(int sockfd, FILE* messages_stream) {

    int message_count = count_lines(messages_stream); WARN_ERROR(message_count);

    // retour au debut du fichier
    fseek(messages_stream, 0, SEEK_SET);
    
    printf("Envoie du nombre de %d messages...\n", message_count);
    
    ssize_t nread = send(sockfd, &message_count, sizeof(message_count), 0); WARN_ERROR(nread);

    printf("Envoie du contenu des messages...\n");

    char* line = NULL;
    size_t line_size = 0;

    while (getline(&line, &line_size, messages_stream) != -1) {
        // Envoie taille message
        nread = send(sockfd, &line_size, sizeof(line_size), 0); WARN_ERROR(nread);
        // Envoie contenu message
        nread = send(sockfd, line, line_size, 0); WARN_ERROR(nread);
    }

    free(line);
}

// renvoie les message au client
void manage_client_connection(int client_socket, const char* host, const char* serv){

    ssize_t nread = 0;
    
    FILE* messages_stream = fopen(messages_path, "r"); WARN_ERROR_IF(messages_stream == NULL);

    read_and_send_messages(client_socket, messages_stream);

    fclose(messages_stream);

    while (1) {

        printf("Attente d'un message du client...\n");

        char message[BUF_SIZE] = "";

        //nread = recvfrom(client_socket, message, sizeof(message), 0, &client_addr, &client_addrlen); WARN_ERROR(nread);
        nread = recv(client_socket, message, sizeof(message), 0); WARN_ERROR(nread);

        printf("┌[RECEPTION IP : %s, PORT : %s]\n", host, serv);
        printf("└─▶ %s", message);

        printf("Renvoie du message en cours...\n");

        //nread = sendto(client_socket, message, sizeof(message), 0, &client_addr, client_addrlen); WARN_ERROR(nread);
        nread = send(client_socket, message, sizeof(message), 0); WARN_ERROR(nread);
    }
}

void clean_exit(int num) {

}

int main(int argc, char const *argv[]){

    PRINT_USAGE_IF(argc < 2, "Usage %s <PORT>\n", argv[0]);

    FILE* messages_stream = fopen(messages_path, "w+"); WARN_ERROR_IF(messages_stream == NULL);

    fprintf(messages_stream, "bonjour !,\n");
    fprintf(messages_stream, "je suis le contenu du fichier !\n");
    fprintf(messages_stream, "interressant n'est-ce pas ?\n");
    fprintf(messages_stream, "dans le future je contiendrai des messages de la forme:\n");
    fprintf(messages_stream, "DATE_ENVOI:NOM_UTILISATEUR:MESSAGE\n");
    
    fclose(messages_stream);

    const char* server_port = argv[1];

    struct addrinfo hints;
    struct addrinfo *result;

    memset(&hints, 0, sizeof(struct addrinfo));

    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* TCP socket */
    hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */

    printf("Allocation de la liste d'adresses du serveur au PORT : %s...\n", server_port);

    int error = getaddrinfo(NULL, server_port, &hints, &result); WARN_ERROR_GAI(error);

    printf("Création du socket et liaison d'une adresse local...\n");

    struct sockaddr server_addr;
    socklen_t server_addrlen;

    int server_socket = create_and_bind_socket(result, &server_addr, &server_addrlen); WARN_ERROR(server_socket);

    char server_address[] = "localhost"; 

   // error = getnameinfo(&server_addr, server_addrlen, server_address, sizeof(server_address), NULL, 0, NI_NUMERICHOST); WARN_ERROR_GAI(error);

    printf("Liberation de la liste d'adresses du serveur au PORT : %s...\n", server_port);

    freeaddrinfo(result);

    printf("Assignation du mode connexion pour %d client(s) au socket...\n", MAX_CLIENT);

    error = listen(server_socket, MAX_CLIENT); WARN_ERROR(error);

    int client_socket;

    // struct sockaddr_storage est generic, il peut contenir une adresse IPv4 ou IPv6
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    printf("Serveur en attente de connexions...\n");
    printf("IP : %s, PORT : %s\n", server_address, server_port);


    while (1) {

        client_socket = accept(server_socket, (struct sockaddr*) &client_addr, &client_addr_len); WARN_ERROR(client_socket);

        char client_address_str[NI_MAXHOST] = "";
        char client_port_str[NI_MAXSERV] = "";

        error = getnameinfo((struct sockaddr*) &client_addr, client_addr_len, client_address_str, sizeof(client_address_str), client_port_str, sizeof(client_port_str), NI_NUMERICHOST|NI_NUMERICSERV); WARN_ERROR_GAI(error);
        
        printf("Creation d'un processus dedié au client d'IP : %s et de PORT : %s\n", client_address_str, client_port_str);

        pid_t pid = fork_process(); WARN_ERROR(pid);

        if (pid == 0) {
            manage_client_connection(client_socket, client_address_str, client_port_str);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);

    return 0;
}


/*

struct shared_segment {
    int socket_fds_size = MAX_CLIENT;
    int client_current_num = 0;
    int socket_fds[MAX_CLIENT] = {-1};  
};

#define MEM_SEG_KEY 12345

const char* msg_data_file_path = "msg_data.txt";

connection_request(server_sock);


void clean_exit(int num);
void send_group_message();
void send_direct_message();

int main(int argc, char const *argv[]) {
    
    PRINT_USAGE_IF(argc < 2, "Usage %s <PORT>", argv[0]);
    
    create_segment(){
    
    }

    create_socket()

    wait_client() {
        
        while(true){
            client_socket = accept(connection_sock))

            fork
            si fils 
                manage_client(client_socket);
            sinon
                continue
        }

    }

    manage_client(client_socket) {
    
        shared_data* = shmat();
            
        lock_data()
        client_id = get_client_id(shared_data);
        unlock_data();


        msg_data_size = strlen(msg_data);
        tcpsend()

        lock();
        remove_client_id(shared_data, client_id);
        unlock
    }

}


COMMUNICATIONS TCP/IP

# SERVEUR CONCURRENT:
- Gère l'accès des clients sur l'espace partagé.
- Informe les clients des modifications.

manage_client_update:
- envoie données partagées au client
- thread attente de modif, mise à jour espace partagée, diffusion nouvelle etat
- thread attente modif autre client (semaphore), diffusion etat

# CLIENT:
- Echange avec le serveur pour visualisé/modifé le contenu de l'espace partagé.

manage_server_update:
- reception données partagées du serveur
- thread modification de l'espace partagé
- thread attente mise à jour

date:username:msg


autre:
- structure partagé

server.c:

// renvoie l'id du segment
int create_segment(...)

int wait_client(...):
    wait_connection en tcp
    int manage_client(...)
    int check_update(...)
    int update_all(...)

client.c:

init_client():
    demande de connexion tcp 1 <- taille username
    demande de connexton tcp 2 <- username

    attente reponse de connexion tcp 1 -> taille data
    attente reponse de connexion tcp 2 -> data 
    
    creation fichier data;

print_chat(filepath):
    affiche le chat

threads:

void* rcv_chat_update(void*):
    attente reponse de connexion tcp 1 -> taille data
    attente reponse de connexion tcp 2 -> data 

    réécriture du fichier data

    print_chat(filepath)

void* snd_chat_msg(void*)
    getline()
    envoie message tcp 1 <- taille message
    envoie message tcp 2 <- message
*/