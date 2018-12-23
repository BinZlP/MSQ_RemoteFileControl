#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Lpc.h"
#include "LpcStub.h"

#define PERMS 0666

#define REQ_SIZE (sizeof(LpcRequest)-sizeof(long))
#define RES_SIZE (sizeof(LpcResponse)-sizeof(long))

extern int rcv_msqid;
extern int snd_msqid;	
extern int gets_msqid;


void signalHandler(int signum);

void emptyHandler(int sig_no){}

int main(int argc,const char* argv[]){
	char pBuf[LPC_DATA_MAX]={0,};
	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	pthread_t tid;
	pthread_mutex_t mutex;

	signal(SIGINT,signalHandler);
	signal(SIGKILL,signalHandler);
	signal(SIGSEGV,signalHandler);

	Init();

	pthread_mutex_init(&mutex,NULL);

	//hile(msgrcv(rcv_msqid,req,REQ_SIZE,0,0)>=0);
	
	while(1){
		memset(req,0x00,sizeof(LpcRequest));
		//while(msgrcv(rcv_msqid,req,REQ_SIZE,0,IPC_NOWAIT)==-1);

		//while();
		while(1){
			if(msgrcv(rcv_msqid,req,REQ_SIZE,0,0)<1500){
				printf("Wrong message received, put back\n");
				msgsnd(snd_msqid,(LpcResponse*)req,RES_SIZE,0);
			}else{
				printf("Message received.\n");
				break;
			}
		}

		printf("Received request from %d - %d\n",req->pid,req->service);
		
		pthread_mutex_lock(&mutex);
		switch(req->service){
			case LPC_OPEN_FILE:

				OpenFile(req);
				break;

			case LPC_READ_FILE:
				ReadFile(req);
				break;

    		case LPC_WRITE_FILE:
				WriteFile(req);
    			break;

    		case LPC_CLOSE_FILE:
    			CloseFile(req);
    			break;

    		case LPC_MAKE_DIRECTORY:
    			MakeDirectory(req);
    			break;

    		case LPC_GET_STRING:
				GetString(req);
				break;
		}
		pthread_mutex_unlock(&mutex);
	}

	free(req);
	return 0;
}

void signalHandler(int signum){
	if(signum==SIGINT||signum==SIGKILL||signum==SIGSEGV){
		printf("SIGNAL: %d\n",signum);
		msgctl(rcv_msqid,IPC_RMID,NULL);
		msgctl(snd_msqid,IPC_RMID,NULL);
		msgctl(gets_msqid,IPC_RMID,NULL);
		exit(0);
	}
}