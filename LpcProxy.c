#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "Lpc.h"
#include "LpcProxy.h"

#define SND_MSG_QUEUE_KEY "/tmp/responseq" // server send
#define SND_MSG_QUEUE_NO 123
#define RCV_MSG_QUEUE_KEY "/tmp/receiveq" // server receive
#define RCV_MSG_QUEUE_NO 456
#define GETS_MSG_QUEUE_KEY "/tmp/getstringq" // GetString queue
#define GETS_MSG_QUEUE_NO 789

#define PERMS 0666

#define REQ_SIZE (sizeof(LpcRequest)-sizeof(long))
#define RES_SIZE (sizeof(LpcResponse)-sizeof(long))

void wait_for_response(void* (*callback)(char*));

int snd_msqid, rcv_msqid, gets_msqid; 
	// snd_msqid == client receive/server send, rcv_msqid == client send/server receive

void emptyHandler(int sig_no){

}

void Init(void){
	key_t mk1, mk2;
	mk1=ftok(RCV_MSG_QUEUE_KEY,RCV_MSG_QUEUE_NO); // Client Send
	snd_msqid=msgget(mk1,IPC_CREAT|PERMS);
	mk2=ftok(SND_MSG_QUEUE_KEY,SND_MSG_QUEUE_NO); // Client Receive
	rcv_msqid=msgget(mk2,IPC_CREAT|PERMS);
	key_t mk_get=ftok(GETS_MSG_QUEUE_KEY, GETS_MSG_QUEUE_NO); // GetString Queue
	gets_msqid=msgget(mk_get,IPC_CREAT|PERMS);

	signal(SIGUSR1,emptyHandler);

	printf("Message Queue Ready: (rcv)%d (snd)%d (gets)%d \n",rcv_msqid,snd_msqid,gets_msqid);
}


int OpenFile(char* path, int flags){
	int fd;
	printf("OpenFile called\n");

	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(req,0x00,sizeof(LpcRequest));
	memset(res,0x00,sizeof(LpcResponse));

	req->pid=getpid();
	req->service=LPC_OPEN_FILE;
	req->numArg=2;
	req->lpcArgs[0].argSize=strlen(path);
	strcpy(&req->lpcArgs[0].argData,path);
	req->lpcArgs[1].argSize=sizeof(flags);
	memcpy(&req->lpcArgs[1].argData,&flags,sizeof(int));

	printf("msgsnd: %d\n",msgsnd(rcv_msqid,req,REQ_SIZE,0));
	printf("OpenFile sent request.\n");
	while(msgrcv(snd_msqid,res,RES_SIZE,req->pid,0)<500);
	//printf("msgrcv: %d\n",msgrcv(snd_msqid,res,RES_SIZE,getpid(),0));

	memcpy(&fd,&(res->responseData),sizeof(int));
	printf("OpenFile received message. FD: %d\n",fd);

	free(req);
	free(res);

	return fd;
}

int ReadFile(int fd, void* pBuf, int size){
	int rSize;

	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(req,0x00,sizeof(LpcRequest));
	memset(res,0x00,sizeof(LpcResponse));

	req->pid=getpid();
	req->service=LPC_READ_FILE;
	req->numArg=2; // [0] => fd, [2] => size
	req->lpcArgs[0].argSize=sizeof(fd);
	memcpy(&req->lpcArgs[0].argData,&fd,sizeof(int));
	req->lpcArgs[2].argSize=sizeof(size);
	memcpy(&req->lpcArgs[2].argData,&size,sizeof(int));

	msgsnd(rcv_msqid,req,REQ_SIZE,0);
	printf("ReadFile sent request.\n");

	while(msgrcv(snd_msqid,res,RES_SIZE,req->pid,0)<500);

	memcpy(pBuf,&res->responseData,size);

	if(res->errorno) rSize=res->errorno;
	else rSize=res->responseSize;

	printf("ReadFile received message. Size: %d\n",rSize);


	free(req);
	free(res);

	return rSize;
}


int WriteFile(int fd, void* pBuf, int size){
	printf("WriteFile start. FD: %d, Size: %d\n",fd,size);
	int wSize;

	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(req,0x00,sizeof(LpcRequest));
	memset(res,0x00,sizeof(LpcResponse));

	req->pid=getpid();
	req->service=LPC_WRITE_FILE;
	req->numArg=3; // [0] => fd, [1] => pBuf, [2] => size
	req->lpcArgs[0].argSize=sizeof(fd);
	memcpy(&req->lpcArgs[0].argData,&fd,sizeof(int));
	req->lpcArgs[1].argSize=size;
	memcpy(&req->lpcArgs[1].argData,pBuf,size);
	req->lpcArgs[2].argSize=sizeof(size);
	memcpy(&req->lpcArgs[2].argData,&size,sizeof(int));

	msgsnd(rcv_msqid,req,REQ_SIZE,0);
	printf("WriteFile sent request.\n");
	while(msgrcv(snd_msqid,res,RES_SIZE,req->pid,0)<500);

	if(res->errorno) wSize=res->errorno;
	else memcpy(&wSize,&res->responseData,sizeof(int));

	printf("WriteFile received message. Size: %d\n",wSize);

	free(req);
	free(res);

	return wSize;
}


int CloseFile(int fd){
	int errorno;

	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(req,0x00,sizeof(LpcRequest));
	memset(res,0x00,sizeof(LpcResponse));

	req->pid=getpid();
	req->service=LPC_CLOSE_FILE;
	req->numArg=1;
	req->lpcArgs[0].argSize=sizeof(fd);
	memcpy(&req->lpcArgs[0].argData,&fd,sizeof(int));

	msgsnd(rcv_msqid,req,REQ_SIZE,0);
	printf("CloseFile sent request.\n");
	while(msgrcv(snd_msqid,res,RES_SIZE,req->pid,0)<500);

	errorno=res->errorno;
	printf("CloseFile received message: %d\n",errorno);

	free(req);
	free(res);

	return errorno;
}

int MakeDirectory(char* path, int mode){
	int errorno;

	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(req,0x00,sizeof(LpcRequest));
	memset(res,0x00,sizeof(LpcResponse));

	req->pid=getpid();
	req->service=LPC_MAKE_DIRECTORY;
	req->numArg=2; // [0] => fd [1] => mode
	req->lpcArgs[0].argSize=strlen(path);
	strcpy(&req->lpcArgs[0].argData,path);
	req->lpcArgs[1].argSize=sizeof(mode);
	memcpy(&req->lpcArgs[1].argData,&mode,sizeof(int));

	msgsnd(rcv_msqid,req,REQ_SIZE,0);
	printf("MakeDirectory sent request.\n");
	while(msgrcv(snd_msqid,res,RES_SIZE,req->pid,0)<500);

	errorno=res->errorno;
	printf("MakeDirectory received message: %d\n",errorno);

	free(req);
	free(res);

	return errorno;
}

int GetString(void* (*callback)(char*)){
	char buf[LPC_DATA_MAX]={0,};
	pthread_t tid;
	LpcResponse res;

	LpcRequest* req=(LpcRequest*)malloc(sizeof(LpcRequest));
	memset(req,0x00,sizeof(LpcRequest));

	req->pid=getpid();
	req->service=LPC_GET_STRING;
	req->numArg=0;

	msgsnd(rcv_msqid,req,REQ_SIZE,0);
	printf("GetString sent request.\n");
	while(msgrcv(gets_msqid,&res,RES_SIZE,getpid(),0)<500);

	callback(res.responseData);

	free(req);
	return 0;
}