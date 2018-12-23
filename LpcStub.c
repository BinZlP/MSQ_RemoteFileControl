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
#include <errno.h>
#include "Lpc.h"
#include "LpcStub.h"

#define SND_MSG_QUEUE_KEY "/tmp/responseq" // server send
#define SND_MSG_QUEUE_NO 123
#define RCV_MSG_QUEUE_KEY "/tmp/receiveq" // server receive
#define RCV_MSG_QUEUE_NO 456
#define GETS_MSG_QUEUE_KEY "/tmp/getstringq" // GetString queue
#define GETS_MSG_QUEUE_NO 789

#define PERMS 0666

#define REQ_SIZE (sizeof(LpcRequest)-sizeof(long))
#define RES_SIZE (sizeof(LpcResponse)-sizeof(long))

int rcv_msqid, snd_msqid, gets_msqid;
int target_pid;

void Init(void){
	key_t mk=ftok(RCV_MSG_QUEUE_KEY, RCV_MSG_QUEUE_NO);
	rcv_msqid=msgget(mk,IPC_CREAT|PERMS);
	key_t mk_s=ftok(SND_MSG_QUEUE_KEY, SND_MSG_QUEUE_NO);
	snd_msqid=msgget(mk_s,IPC_CREAT|PERMS);
	key_t mk_get=ftok(GETS_MSG_QUEUE_KEY, GETS_MSG_QUEUE_NO);
	gets_msqid=msgget(mk_get,IPC_CREAT|PERMS);

	printf("Message Queue Ready: (rcv)%d (snd)%d (gets)%d \n",rcv_msqid,snd_msqid,gets_msqid);
}


int OpenFile(LpcRequest* pRequest){
	char path[LPC_DATA_MAX]={0,};
	memcpy(path,&(pRequest->lpcArgs[0].argData),pRequest->lpcArgs[0].argSize);
	int fd, flags;
	memcpy(&flags,&(pRequest->lpcArgs[1].argData),sizeof(int));
	printf("OpenFile received request. Filename: %s\n",path);

	fd=open(path,flags,PERMS);
	printf("OpenFile opened file. Filename: %s FD: %d\n",pRequest->lpcArgs[0].argData,fd);

	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(res,0x00,sizeof(LpcResponse));

	res->pid=pRequest->pid;
	if(fd<0){
		res->errorno=errno;
	}else{
		res->errorno=0;
	}
	res->responseSize=sizeof(int);

	memcpy(&(res->responseData),&fd,sizeof(int));
	memcpy(&flags,&(res->responseData),sizeof(int));
	printf("res fd: %d\n",flags);

	printf("msgsnd: %d\n",msgsnd(snd_msqid, res, RES_SIZE, 0));
	for(int i=0;i<100;i++) kill(res->pid,SIGUSR1);

	free(res);
	return fd;
}

int ReadFile(LpcRequest* pRequest){
	int rsize, fd, size;
	char pBuf[LPC_DATA_MAX]={0,};
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(res,0x00,sizeof(LpcResponse));
	memcpy(&fd,&pRequest->lpcArgs[0].argData,sizeof(int));
	memcpy(&size,&pRequest->lpcArgs[2].argData,sizeof(int));

	rsize=read(fd,pBuf,size);
	printf("Read %d characters\n",rsize);

	res->pid=pRequest->pid;
	if(rsize<0){
		res->errorno=errno;
		res->responseSize=0;
	}else{
		res->errorno=0;
		res->responseSize=sizeof(rsize);
	}
	memcpy(&res->responseData,pBuf,rsize);

	//msgsnd(snd_msqid, res, RES_SIZE, 0);
	printf("msgsnd: %d\n",msgsnd(snd_msqid, res, RES_SIZE, 0));
	for(int i=0;i<100;i++) kill(res->pid,SIGUSR1);

	free(res);
	return rsize;
}


int WriteFile(LpcRequest* pRequest){
	int fd,size,wsize;
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(res,0x00,sizeof(LpcResponse));
	char pBuf[LPC_DATA_MAX]={0,};

	memcpy(&fd,&pRequest->lpcArgs[0].argData,pRequest->lpcArgs[0].argSize);
	memcpy(&size,&pRequest->lpcArgs[2].argData,pRequest->lpcArgs[2].argSize);
	memcpy(pBuf,&pRequest->lpcArgs[1].argData,pRequest->lpcArgs[1].argSize);

	wsize=write(fd,pBuf,size);
	printf("Write in FD: %d, %s\n",fd,pBuf);

	res->pid=pRequest->pid;
	if(wsize<0){
		res->errorno=errno;
	}else{
		res->errorno=0;
	}
	res->responseSize=sizeof(int);
	memcpy(&(res->responseData),&wsize,sizeof(int));
	printf("Response to %d setting completed.\n", res->pid);

	if(msgsnd(snd_msqid, res, RES_SIZE, IPC_NOWAIT)<0) perror("msgsnd()");
	else {
		printf("Sent WriteFile response to %d\n",res->pid);
		for(int i=0;i<10;i++) kill(res->pid,SIGUSR1);

	}

	free(res);
	return wsize;
}


int CloseFile(LpcRequest* pRequest){
	int ret, fd;
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(res,0x00,sizeof(LpcResponse));
	memcpy(&fd,&pRequest->lpcArgs[0].argData,sizeof(int));
	ret=close(fd);

	res->pid=pRequest->pid;
	if(ret<0){
		res->errorno=errno;
	}else{
		res->errorno=0;
	}
	res->responseSize=sizeof(int);
	memcpy(&ret,&res->responseData,sizeof(int));

	//while(msgsnd(snd_msqid,res,RES_SIZE,0)==-1);
	printf("msgsnd: %d\n",msgsnd(snd_msqid, res, RES_SIZE, 0));
	for(int i=0;i<100;i++) kill(res->pid,SIGUSR1);

	printf("Sent CloseFile response to %d: %d\n",res->pid,ret);

	free(res);
	return ret;
}

int MakeDirectory(LpcRequest* pRequest){
	int ret, mode;
	char path[LPC_DATA_MAX]={0,};
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(res,0x00,sizeof(LpcResponse));
	memcpy(path,&pRequest->lpcArgs[0].argData,pRequest->lpcArgs[0].argSize);
	memcpy(&mode,&pRequest->lpcArgs[1].argData,sizeof(int));
	ret=mkdir(path,mode);

	res->pid=pRequest->pid;
	if(ret<0){
		res->errorno=errno;
	}else{
		res->errorno=0;
	}
	res->responseSize=sizeof(int);
	memcpy(&ret,&res->responseData,sizeof(int));

	//msgsnd(snd_msqid,res,RES_SIZE,0);
	//kill(res->pid,SIGUSR1);
	printf("msgsnd: %d\n",msgsnd(snd_msqid, res, RES_SIZE, 0));
	for(int i=0;i<10;i++) kill(res->pid,SIGUSR1);


	free(res);
	return ret;
}

void wait_for_string(void* rpid){
	int pid=target_pid;
	LpcResponse* res=(LpcResponse*)malloc(sizeof(LpcResponse));
	memset(res,0x00,sizeof(LpcResponse));
	char buf[LPC_DATA_MAX]={0,};
	printf("Enter string: ");
	gets(buf);

	res->pid=pid;
	res->errorno=0;
	res->responseSize=strlen(buf);
	//printf("Set pid, errno, rSize.\n");
	memcpy(&(res->responseData[0]),buf,strlen(buf));

	msgsnd(gets_msqid,res,RES_SIZE,0);
	printf("Sent GetString response to %d\n",res->pid);

	free(res);
	pthread_exit(NULL);
}

int GetString(LpcRequest* pRequest){
	pthread_t tid;
	target_pid=pRequest->pid;
	pthread_create(&tid,NULL,wait_for_string,NULL);
	pthread_detach(tid);
	
	return 0;
}
 