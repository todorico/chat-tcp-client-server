//////////
//CLIENT//
//////////

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <arpa/inet.h>

#include "warn.h"

struct parametres
{
	int dS;
	int position;
};

void init_client(char* username,int dS){
	int size = sizeof(username);
	int taille;
	
	while(1){
	//demande de connexion tcp 1 <- taille username
	int res = send(dS,&size,sizeof(int),0);
	WARN_ERROR(res);

	//demande de connexion tcp 2 <- username
	res = send(dS,username,size,0);
	WARN_ERROR(res);
	
	//attente de reponse de connexion tcp 1 -> taille de data
	res = recv(dS,&taille,sizeof(int),MSG_WAITALL);
	WARN_ERROR(res);

	//attente de reponse de connexion tcp 2 <- data
	char contenu[taille];
	res = recv(dS,contenu,sizeof(contenu),MSG_WAITALL);
	WARN_ERROR(res);

	if(sizeof(contenu) == taille){
		//creation du fichier
		char* filename = "/data.txt";
		FILE *fp = fopen(filename,"w+");
		res = fputs(contenu, fp);
		if (res == EOF)
		{
			fprintf(stderr,"Erreur durant l'écriture du fichier \n");
			exit(1);
		}
		fclose(fp);
		break;
	}

	sleep(1);
	}


	return;
}

void print_chat(void* parametres){
	struct parametres *p= (struct parametres *)parametres;
	//affichage du chat
	char* filename = "/data.txt";
	char buffer[255];
	FILE *fp = fopen(filename,"w+");
	fseek(fp, 0, SEEK_END);
	int size = ftell(fp);
	fseek( fp, p->position, SEEK_SET );
	p->position = size;
	while(fgets(buffer, 255,(FILE *)fp) != NULL){
		printf("%s \n", buffer);
	}
	fclose(fp);
	return;
}

void* rcv_chat_update(void* parametres){
	struct parametres *p= (struct parametres *)parametres;
	int size;
	int res;

	while(1){
	//attente de reponse de connexion tcp 1 -> taille
	res = recv(p->dS,&size,sizeof(int),0);
	WARN_ERROR(res);

	//attente de reponse de connexion tcp 2 -> data
	char contenu[size];
	res = recv(p->dS,contenu,sizeof(contenu),0);
	WARN_ERROR(res);

	if(sizeof(contenu) == size){
		//reecriture du fichier data
		char* filename = "/data.txt";
		FILE *fp = fopen(filename,"w+");
		res = fputs(contenu, fp);
		if (res == EOF)
		{
			fprintf(stderr,"Erreur durant la réecriture du fichier \n");
			exit(1);
		}
		fclose(fp);
		break;
	}
	}

	print_chat(p);

  	pthread_exit(NULL);
}

void* snd_chat_msg(void* parametres){
	struct parametres *p= (struct parametres *)parametres;
	char message[255];
	scanf("%s",message);
	int size = sizeof(message);

	while(1){
	//envoie de message tcp 1 <- taille message
	int res = send(p->dS,&size,sizeof(int),0);
	WARN_ERROR(res);

	//envoie de message tcp 2 <- message
	res = send(p->dS,message,size,0);
	WARN_ERROR(res);
	}
	
	pthread_exit(NULL);
}

int main(int argc, char const *argv[])
{
	PRINT_USAGE_IF(argc != 3, "Usage %s <IP_Serveur> <NUM_PORT>", argv[0]);

	//connexion au serveur
	int dS = socket(PF_INET,SOCK_STREAM,0);
	struct sockaddr_in adServ;
	adServ.sin_family = AF_INET;
	int port = atoi(argv[2]);
	adServ.sin_port = htons(port);
	int res = inet_pton(AF_INET,argv[1],&(adServ.sin_addr));
	WARN_ERROR(res);
	res = connect(dS,(struct sockaddr *)&adServ,sizeof(struct sockaddr_in));
	WARN_ERROR(res);


	//saisis du nom de l'utilisateur
	char* username = malloc(sizeof(char));
	printf("Saisir votre Username : \n");
	scanf("%s",username);

	//innitialisation
	init_client(username,dS);

	struct parametres *p = (struct parametres *)malloc(2*sizeof(int));
	p->dS = dS;
	p->position = 0;
	pthread_t idT[2];
	while(1){

		//tread d'update des message
		int res = pthread_create(&idT[0],NULL,rcv_chat_update,(void*)p);
		WARN_ERROR_PTHREAD(res);
		res = pthread_join(idT[1],NULL);
		WARN_ERROR_PTHREAD(res);

		//tread d'envoie de message
	  	res = pthread_create(&idT[1],NULL,snd_chat_msg,(void*)p);
		WARN_ERROR_PTHREAD(res);
		res = pthread_join(idT[1],NULL);
		WARN_ERROR_PTHREAD(res);
	}

	close(dS);
	return 0;
}