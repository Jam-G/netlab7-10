//文件名: son/son.c
//
//描述: 这个文件实现SON进程 
//SON进程首先连接到所有邻居, 然后启动listen_to_neighbor线程, 每个该线程持续接收来自一个邻居的进入报文, 并将该报文转发给SIP进程. 
//然后SON进程等待来自SIP进程的连接. 在与SIP进程建立连接之后, SON进程持续接收来自SIP进程的sendpkt_arg_t结构, 并将接收到的报文发送到重叠网络中. 
//
//创建日期: 2015年

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <sys/utsname.h>
#include <assert.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "son.h"
#include "../topology/topology.h"
#include "neighbortable.h"
#include<errno.h>

//你应该在这个时间段内启动所有重叠网络节点上的SON进程
#define SON_START_DELAY 60

/**************************************************************/
//声明全局变量
/**************************************************************/
//将邻居表声明为一个全局变量 
nbr_entry_t* nt; 
//将与SIP进程之间的TCP连接声明为一个全局变量
int sip_conn = -1;

/**************************************************************/
//实现重叠网络函数
/**************************************************************/

// 这个线程打开TCP端口CONNECTION_PORT, 等待节点ID比自己大的所有邻居的进入连接,
// 在所有进入连接都建立后, 这个线程一直监听来自邻居的连接. 

void* waitNbrsAfter(void* arg) {
	printf("wait NBR thread  after is running...\n");
	int sockfd, nbsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("ERROR creating socket\n");
		exit(-1);
	}
//	int on = 1;
	//setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(CONNECTION_PORT);
	if(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
		perror("ERROR binding \n");
		exit(-1);
	}
	listen(sockfd, 10);
	while(1){
		memset(&cli_addr, 0, sizeof(cli_addr));
		clilen = sizeof(cli_addr);
		nbsockfd = accept(sockfd, (struct sockaddr *)&cli_addr,  &clilen);
		if(nbsockfd < 0){
			if(errno == EINTR)
				continue;
			printf("ERROR accepting %d\n", nbsockfd);
			pthread_exit(NULL);
		}
		printf("accept a new neighbor\n");
		struct in_addr *nbIp = &(cli_addr.sin_addr);
		int nbId = topology_getNodeIDfromip(nbIp);
//		pthread_mutex_lock(routingtable_mutex);
		if(nt_addconn(nt, nbId, nbsockfd) == -1)
			printf("can't add sock to new link\n");
//		pthread_mutex_unlock(toutingtable_mutex);
		printf("add sock to new link to neighbor:%d\n", nbId);
	}
	close(sockfd);
	printf("ERROR EXIT from listing to after \n");
	//你需要编写这里的代码.
	pthread_exit(NULL);
}
// 这个函数连接到节点ID比自己小的所有邻居.
// 在所有外出连接都建立后, 返回1, 否则返回-1.
int connectNbrs() {
	//你需要编写这里的代码.
	int myId = topology_getMyNodeID();
	int nbnum = topology_getNbrNum();
	int i = 0;
	for(i = 0; i < nbnum; i++){
		if(nt[i].nodeID<myId){
			int clisock;
			struct sockaddr_in nba;
			memset(&nba, 0, sizeof(nba));
			nba.sin_family = AF_INET;
			nba.sin_addr.s_addr = nt[i].nodeIP;
			nba.sin_port = htons(CONNECTION_PORT);
			printf("try to connect to is:%d\n", nt[i].nodeIP >> 24);
			clisock = socket(AF_INET, SOCK_STREAM, 0);
			if(clisock < 0){
				perror("can't craeate socket to nbr\n");
				return -1;
			}
			if(connect(clisock, (struct sockaddr *)&nba, sizeof(nba)) < 0){
				printf("can't connect to nbr %d, try next nbr\n", nt[i].nodeID);
				nt[i].conn = -1;
				continue;
			}
			nt[i].conn = clisock;
			printf("connect to nbr %d, socket is :%d\n", nt[i].nodeID, clisock);
		}
	}
	return 1;
}

//每个listen_to_neighbor线程持续接收来自一个邻居的报文. 它将接收到的报文转发给SIP进程.
//所有的listen_to_neighbor线程都是在到邻居的TCP连接全部建立之后启动的. 
void* listen_to_neighbor(void* arg) {
	//你需要编写这里的代码.
	int index = *(int *)arg;
	sip_pkt_t *recvpktbuf = malloc(sizeof(sip_pkt_t));
	int myid = topology_getMyNodeID();
	int clisock;
	struct sockaddr_in nba;
	memset(&nba, 0, sizeof(nba));
	nba.sin_family = AF_INET;
	nba.sin_addr.s_addr = nt[index].nodeIP;
	nba.sin_port = htons(CONNECTION_PORT);
//	printf("try to connect to is:%d\n", nt[index].nodeIP >> 24);
	clisock = socket(AF_INET, SOCK_STREAM, 0);
	if(clisock < 0){
		perror("can't craeate socket to nbr\n");
	}
	while(1){
		if(nt[index].conn < 0){
			sleep(1);
			if(nt[index].nodeID < myid){
				if(connect(clisock, (struct sockaddr *)&nba, sizeof(nba)) < 0){
					printf("[listen_to_neighbor>>]can't connect to nbr %d, try later\n", nt[index].nodeID);	
					continue;
				}
				nt[index].conn = clisock;
				printf("[listen_to_neighbor>>]connect to nbr %d, socket is :%d\n", nt[index].nodeID, clisock);
			}
			printf("[Debug>>]continue______means nbr didn't connect\n");
			continue;
		}
		memset(recvpktbuf, 0, sizeof(sip_pkt_t));
		int rc = recvpkt(recvpktbuf, nt[index].conn);
		if(rc > 0){
			printf("recvive a pkt from :%d\n", nt[index].nodeID);	
		}else{
			printf("read error from :%d, will try again\n", nt[index].nodeID);
			close(nt[index].conn);
			nt[index].conn = -1;
		}
		if(sip_conn >= 0){
			if(forwardpktToSIP(recvpktbuf, sip_conn) > 0){
				printf("forwardpktToSIP from %d\n", nt[index].nodeID);
			}else{
				printf("forwardpktToSIP error from %d\n", nt[index].nodeID);
				close(sip_conn);
				sip_conn = -1;
				//pthread_exit(NULL);
			}
		}else{
			printf("sip connection has closed ,now at listen_to_neighbor\n");
			//close(nt[index].conn);
		//	nt[index].conn = -1;
	//		pthread_exit(NULL);
		}
	}
}

//这个函数打开TCP端口SON_PORT, 等待来自本地SIP进程的进入连接. 
//在本地SIP进程连接之后, 这个函数持续接收来自SIP进程的sendpkt_arg_t结构, 并将报文发送到重叠网络中的下一跳. 
//如果下一跳的节点ID为BROADCAST_NODEID, 报文应发送到所有邻居节点.
void waitSIP() {
	//你需要编写这里的代码.
	int sockfd, sipsock;
	socklen_t clilen;
	struct sockaddr_in localaddr, sipaddr;
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("create sip socket error \n");
		return;
	}
//	int on = 1;
//	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	memset(&localaddr, 0, sizeof(localaddr));
	localaddr.sin_family = AF_INET;
	localaddr.sin_addr.s_addr = INADDR_ANY;
	localaddr.sin_port = htons(SON_PORT);
	if(bind(sockfd, (struct sockaddr *)&localaddr, sizeof(localaddr)) < 0){
		perror("binf error sip socket\n");
		return;
	}
	listen(sockfd, 1);
	clilen = sizeof(sipaddr);
	memset(&sipaddr, 0, sizeof(sipaddr));
	while(1){
		sipsock = accept(sockfd, (struct sockaddr *)&sipaddr, &clilen);
		sip_conn = sipsock;
		if(sip_conn < 0){
			perror("accpet error sip socket\n");
			return;
		}
		printf("sip now is connected ,~~~\n");
		sip_pkt_t  recvpktbuf;
		int nextNode = 0;
		while(1){
			if(sip_conn < 0){
				break;
			}
			if(getpktToSend(&recvpktbuf, &nextNode, sip_conn) > 0){
				printf("get a pkt from sip\n");
				int nbCount = topology_getNbrNum();
				int i = 0;
				for(; i < nbCount; i ++){
					if((nextNode == BROADCAST_NODEID || nextNode == nt[i].nodeID) ){
						if(nt[i].conn >=0){
							if(sendpkt(&recvpktbuf, nt[i].conn) > 0)
								printf("send pkt to id:%d\n", nt[i].nodeID);
							else
								printf("send pkt error to id :%d, socketfd is :%d\n", nt[i].nodeID, nt[i].conn);
						}else{
							printf("nt connection has closed, so can't send to id %d, conn is :%d\n", nt[i].nodeID, nt[i].conn);
						}
					}
				}
			}else{
				printf("get an error pkt from sip\n");
			}
		}
	}
}

//这个函数停止重叠网络, 当接收到信号SIGINT时, 该函数被调用.
//它关闭所有的连接, 释放所有动态分配的内存.
void son_stop() {
	//你需要编写这里的代码.
	close(sip_conn);
	sip_conn = -1;
	nt_destroy(nt);
}

int main() {
	//启动重叠网络初始化工作
	printf("Overlay network: Node %d initializing...\n",topology_getMyNodeID());	

	//创建一个邻居表
	nt = nt_create();
	//将sip_conn初始化为-1, 即还未与SIP进程连接
	sip_conn = -1;
	
	//注册一个信号句柄, 用于终止进程
	signal(SIGINT, son_stop);

	//打印所有邻居
	int nbrNum = topology_getNbrNum();
	int i;
	for(i=0;i<nbrNum;i++) {
		printf("Overlay network: neighbor %d:%d\n",i+1,nt[i].nodeID);
	}

	//启动waitNbrs线程, 等待节点ID比自己大的所有邻居的进入连接
	pthread_t waitNbrs_thread;
	pthread_create(&waitNbrs_thread,NULL,waitNbrsAfter,(void*)0);

	//等待其他节点启动
//	sleep(SON_START_DELAY);
	
	//连接到节点ID比自己小的所有邻居
	//connectNbrs();



	//此时, 所有与邻居之间的连接都建立好了
	
	//创建线程监听所有邻居
	for(i=0;i<nbrNum;i++) {
		int* idx = (int*)malloc(sizeof(int));
		*idx = i;
		pthread_t nbr_listen_thread;
		pthread_create(&nbr_listen_thread,NULL,listen_to_neighbor,(void*)idx);
	}
	printf("Overlay network: node initialized...\n");
	printf("Overlay network: waiting for connection from SIP process...\n");

	//等待来自SIP进程的连接
	waitSIP();
}
