#ifndef CHAT_H
#define CHAT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <unistd.h>

#include "warn.h"

#define NB_MAX_CLIENT 10
#define NB_MAX_MESSAGE 10

#define STR_BUF_SIZE 512
#define MESSAGE_DATA_BUF_SIZE 1024

// CHAT_STRUCT

typedef struct {
    char date[STR_BUF_SIZE];
    char sender[STR_BUF_SIZE];
    char data[MESSAGE_DATA_BUF_SIZE];
} CHAT_MESSAGE;

typedef struct {
    int sockets_current_num;
    int sockets[NB_MAX_CLIENT];
    int semaphores_current_num;
    int semaphores[NB_MAX_CLIENT];
    int messages_current_index;
    CHAT_MESSAGE messages[NB_MAX_MESSAGE];
    int chat_shared_semaphore;
} CHAT_SHARED;

typedef struct {
    int sockfd;
    CHAT_SHARED* shared;
} CHAT_HANDLER_PARAM;

// CHAT_MESSAGE

void CHAT_MESSAGE_init(CHAT_MESSAGE* message) {
    strcpy(message->date, "");
    strcpy(message->sender, "");
    strcpy(message->data, "");
}

void CHAT_MESSAGE_send(int sockfd, CHAT_MESSAGE* message) {
    int error;
    
    error = send(sockfd, message->date, STR_BUF_SIZE, 0); WARN_ERROR(error);
    error = send(sockfd, message->sender, STR_BUF_SIZE, 0); WARN_ERROR(error);
    error = send(sockfd, message->data, MESSAGE_DATA_BUF_SIZE, 0); WARN_ERROR(error);
}

void CHAT_MESSAGE_recv(int sockfd, CHAT_MESSAGE* message) {  
    int error;
    
    error = recv(sockfd, message->date, STR_BUF_SIZE, 0); WARN_ERROR(error);
    error = recv(sockfd, message->sender, STR_BUF_SIZE, 0); WARN_ERROR(error);
    error = recv(sockfd, message->data, MESSAGE_DATA_BUF_SIZE, 0); WARN_ERROR(error);
}

void CHAT_MESSAGE_print(CHAT_MESSAGE* message) {
	printf("%s : %s : %s\n", message->date, message->sender, message->data);
}

// CHAT_SHARED

void CHAT_SHARED_init(CHAT_SHARED* shared) {
    shared->sockets_current_num = 0;

    for (int i = 0; i < NB_MAX_CLIENT; ++i)
        shared->sockets[i] = -1;

    shared->semaphores_current_num = 0;

    for (int i = 0; i < NB_MAX_CLIENT; ++i)
        shared->semaphores[i] = -1;

    shared->messages_current_index = 0;

    for (int i = 0; i < NB_MAX_MESSAGE; ++i)
        CHAT_MESSAGE_init(&shared->messages[i]);
}

void CHAT_SHARED_add_message(CHAT_SHARED* shared, CHAT_MESSAGE* message) {
    strcpy(shared->messages[shared->messages_current_index].date, message->date);
    strcpy(shared->messages[shared->messages_current_index].sender, message->sender);
    strcpy(shared->messages[shared->messages_current_index].data, message->data);

    shared->messages_current_index++;

    if (shared->messages_current_index == NB_MAX_MESSAGE)
        shared->messages_current_index = 0;
}

CHAT_MESSAGE* CHAT_SHARED_last_message(CHAT_SHARED* shared) {
    return shared->messages_current_index == 0 ? &shared->messages[NB_MAX_MESSAGE - 1] : &shared->messages[shared->messages_current_index - 1];
}

int CHAT_SHARED_add_socket(CHAT_SHARED* shared, int sockfd) {
    for (int i = 0; i < NB_MAX_CLIENT; ++i) {
        if (shared->sockets[i] == -1) {
            shared->sockets[i] = sockfd;
            return 1;
        }
    }
    return 0;
}

int CHAT_SHARED_remove_socket(CHAT_SHARED* shared, int sockfd) {
    for (int i = 0; i < NB_MAX_CLIENT; ++i) {
        if (shared->sockets[i] == sockfd) {
            shared->sockets[i] = -1;
            return 1;
        }
    }
    return 0;
}

int CHAT_SHARED_close_sockets(CHAT_SHARED* shared) {
	int error;
    for (int i = 0; i < NB_MAX_CLIENT; ++i)
        if (shared->sockets[i] != -1)
        	error = close(shared->sockets[i]); WARN_ERROR(error);
    return 0;
}

// Envoie le numero de sémaphore associé au socket sockfd
int CHAT_SHARED_semaphore_num(CHAT_SHARED* shared, int sockfd) {
    for (int i = 0; i < NB_MAX_CLIENT; ++i) {
        if (shared->sockets[i] == sockfd)
            return i;
    }
    return -1;
}

void CHAT_SHARED_send(int sockfd, CHAT_SHARED* shared) { 
	int error;

	error = send(sockfd, &shared->messages_current_index, sizeof(shared->messages_current_index), 0); WARN_ERROR(error);

    for (int i = 0; i < NB_MAX_MESSAGE; ++i) {
        CHAT_MESSAGE_send(sockfd, &shared->messages[i]);
    }
}

void CHAT_SHARED_recv(int sockfd, CHAT_SHARED* shared) { 
	int error;

	error = recv(sockfd, &shared->messages_current_index, sizeof(shared->messages_current_index), 0); WARN_ERROR(error);

    for (int i = 0; i < NB_MAX_MESSAGE; ++i) {
        CHAT_MESSAGE_recv(sockfd, &shared->messages[i]);
    }
}

void CHAT_SHARED_print_messages(CHAT_SHARED* shared) {
	int k = shared->messages_current_index;

	for (int i = 0; i < NB_MAX_MESSAGE; ++i) {
		if (strcmp(shared->messages[k].data, "") != 0)
			CHAT_MESSAGE_print(&shared->messages[k]);
		
		k++;

		if (k == NB_MAX_MESSAGE)
			k = 0;
	}
}

int CHAT_SHARED_broadcast(CHAT_SHARED* shared, int semid) {
	int error;

	struct sembuf opv;
    opv.sem_num = 0;
    opv.sem_op = +1;
    opv.sem_flg = SEM_UNDO;

    for (int i = 0; i < NB_MAX_CLIENT; ++i) {
        if (shared->sockets[i] != -1) {
        	opv.sem_num = i;
        	error = semop(semid, &opv, 1); WARN_ERROR(error);
        }
    }
    return 1;
}

#endif