//文件名: sip/sip.c
//
//描述: 这个文件实现SIP进程  
//
//创建日期: 2015年

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <assert.h>
#include <sys/utsname.h>
#include <pthread.h>
#include <unistd.h>

#include "../common/constants.h"
#include "../common/pkt.h"
#include "../topology/topology.h"
#include "sip.h"
#include "nbrcosttable.h"
#include "dvtable.h"
#include "routingtable.h"
#include<assert.h>
#include<errno.h>
#include"../common/seg.h"
#define SIP_WAITTIME 60
/**************************************************************/
//声明全局变量
/**************************************************************/
int son_conn; 			//到重叠网络的连接
int stcp_conn;			//到STCP的连接
nbr_cost_entry_t* nct;			//邻居代价表
dv_t* dv;				//距离矢量表
pthread_mutex_t* dv_mutex;		//距离矢量表互斥量
routingtable_t* routingtable;		//路由表
pthread_mutex_t* routingtable_mutex;	//路由表互斥量

/**************************************************************/
//实现SIP的函数
/**************************************************************/

//SIP进程使用这个函数连接到本地SON进程的端口SON_PORT
//成功时返回连接描述符, 否则返回-1
int connectToSON() { 
	int clisock;
	struct sockaddr_in nba;
	memset(&nba, 0, sizeof(nba));
	nba.sin_family = AF_INET;
	nba.sin_addr.s_addr = inet_addr("127.0.0.1");
	nba.sin_port = htons(SON_PORT);
	clisock = socket(AF_INET, SOCK_STREAM, 0);
	if(clisock < 0){
		perror("can't craeate socket to son layer\n");
		return -1;
	}
	if(connect(clisock, (struct sockaddr *)&nba, sizeof(nba)) < 0){
		printf("can't connect to son layer\n");
		return -1;
	}
	son_conn = clisock;
	printf("connect to son, socket is :%d\n", clisock);
	return son_conn;
}

//这个线程每隔ROUTEUPDATE_INTERVAL时间发送路由更新报文.路由更新报文包含这个节点
//的距离矢量.广播是通过设置SIP报文头中的dest_nodeID为BROADCAST_NODEID,并通过son_sendpkt()发送报文来完成的.
void* routeupdate_daemon(void* arg) {
	sip_pkt_t pkt;
	pkt_routeupdate_t *routeupdate = (pkt_routeupdate_t *)pkt.data;
	timetest_t *timecost = (timetest_t *)pkt.data;
	memset(&pkt, 0, sizeof(sip_pkt_t));
	int entrynum = topology_getNodeNum();
	int nbrnum = topology_getNbrNum();
	int myid = topology_getMyNodeID();
	if(dv[nbrnum].nodeID != myid)
		printf("dv[nbrnum].nodeId:%d, myid:%d\n", dv[nbrnum].nodeID, myid);
	assert(dv[nbrnum].nodeID == myid);
	dv_entry_t *mydv =dv[nbrnum].dvEntry;
//	printf("[routeupdate_daemon>>]get my entry\n");
	pkt.header.src_nodeID = myid;
	pkt.header.dest_nodeID = BROADCAST_NODEID;
	while(1){
		routeupdate->entryNum = entrynum;
		pkt.header.length = sizeof(int) + entrynum * sizeof(routeupdate_entry_t);
		pkt.header.type = ROUTE_UPDATE;
		pthread_mutex_lock(dv_mutex);
		int i = 0;
		for(i = 0; i < entrynum; i++){
			routeupdate->entry[i].nodeID = mydv[i].nodeID;
			routeupdate->entry[i].cost = mydv[i].cost;
		}
		pthread_mutex_unlock(dv_mutex);
		if(son_conn != -1 && son_sendpkt(BROADCAST_NODEID, &pkt, son_conn) > 0){
			printf("success send update routing msg\n");
		}
		else
			printf("send update rputing msg error\n");
		struct timeval start, poll;
		gettimeofday(&start, NULL);
		gettimeofday(&poll, NULL);
		while(timesub(&poll, &start) < ROUTEUPDATE_INTERVAL){
			gettimeofday(&poll, NULL);
			sleep(1);
			pkt.header.length = sizeof(timetest_t) ;
			pkt.header.type = TIMETEST;
			memset(timecost, 0, sizeof(timetest_t));
			gettimeofday(&(timecost->sendtime), NULL);
			if(son_conn != -1 && son_sendpkt(BROADCAST_NODEID, &pkt, son_conn) > 0){
			//	printf("[timetest>>ROUTE_UPDATE]sent time test\n");
			//
				;
			}else{
				printf("[timetest>>ROUTE_UPDATE] send time test error \n");
			}
			pthread_mutex_lock(dv_mutex);
			pthread_mutex_lock(routingtable_mutex);
			upgradenbrcost(nct, dv, routingtable);
			pthread_mutex_unlock(routingtable_mutex);
			pthread_mutex_unlock(dv_mutex);
		}
	}
	assert(0);
	pthread_exit(NULL);
}

//这个线程处理来自SON进程的进入报文. 它通过调用son_recvpkt()接收来自SON进程的报文.
//如果报文是SIP报文,并且目的节点就是本节点,就转发报文给STCP进程. 如果目的节点不是本节点,
//就根据路由表转发报文给下一跳.如果报文是路由更新报文,就更新距离矢量表和路由表.
void* pkthandler(void* arg) {
	sip_pkt_t pkt;
	int nbc = topology_getNbrNum();
	int allc = topology_getNodeNum();
	int myid = topology_getMyNodeID();
	while(1){
		if(son_recvpkt(&pkt,son_conn)>0){
			switch(pkt.header.type){
				case ROUTE_UPDATE:{
									  pkt_routeupdate_t *routeupdate = (pkt_routeupdate_t *)pkt.data;
									  int entrynum = routeupdate->entryNum;
									  routeupdate_entry_t * entry = routeupdate->entry;
								//	  printf("ROUTING: get update msg:from %d\n, entry num is %d\n", pkt.header.src_nodeID, entrynum);
									  int i = 0;
								//	  for(i = 0; i < entrynum; i++){
								//		  printf(">>update entry:%d node:%d newcost:%d\n", i, entry[i].nodeID, entry[i].cost);
								//	  }									 
									  pthread_mutex_lock(dv_mutex);
									  pthread_mutex_lock(routingtable_mutex);
									  for(i = 0; i < entrynum; i++){
										  dvtable_setcost(dv, pkt.header.src_nodeID, entry[i].nodeID, entry[i].cost);
									  }
									  assert(dv[nbc].nodeID == myid);
									  dv_entry_t *myentry = dv[nbc].dvEntry;
									  for(i = 0; i < allc; i++){
										  int dest = myentry[i].nodeID;
										  int oldcost = myentry[i].cost;
										  int nextnode = pkt.header.src_nodeID;
										  int cost1 = nbrcosttable_getcost(nct, nextnode);
										  int cost2 = dvtable_getcost(dv, nextnode, dest);
								/*		  printf("[updating>>]cost1(nbr %d):%d, cost2(%d to %d):%d, old(my:%d to dest:%d):%d\n", \
												  nextnode, cost1,\
												  nextnode, dest,cost2,  myid, dest, oldcost);*/
										  if(cost1 + cost2 < oldcost){
											  printf("[updating>>]cost1(nbr %d):%d, cost2(%d to %d):%d, old(my:%d to dest:%d):%d\n", \
													  nextnode, cost1,\
													  nextnode, dest,cost2,  myid, dest, oldcost);
											  printf("[pkthandler>>ROUTE_UPDATE]updateing routing table, dest :%d, next node from %d to %d\n", dest, routingtable_getnextnode(routingtable, dest), nextnode);
											  dvtable_setcost(dv, myid, dest, cost1 + cost2);
											  routingtable_setnextnode(routingtable, dest, nextnode);
											  routingtable_print(routingtable);
											  dvtable_print(dv);

										  }
									  }
									  pthread_mutex_unlock(routingtable_mutex);
									  pthread_mutex_unlock(dv_mutex);
									  break;
								  }
				case SIP:{
							 if(pkt.header.dest_nodeID == myid){
								 forwordsegToSTCP(stcp_conn, pkt.header.src_nodeID, (seg_t *)pkt.data);
								 printf("[pkt_handler>>]forword a seg to STCP\n");
							 }else{
								 int next = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
								 son_sendpkt(next, &pkt, son_conn);
								 printf("[pkt_handler>>]forword a seg to nbr, dest:%d, next:%d\n", pkt.header.dest_nodeID, next);
							 }
							 break;
						 }
				case TIMETEST:{
								  timetest_t *timetest = (timetest_t *)pkt.data;
							//	  printf("[TIMETEST>>]time test type, dest is :%d, BROADCAST_NODEID:%d, myid:%d\n", pkt.header.dest_nodeID, BROADCAST_NODEID, myid);
								  if(pkt.header.dest_nodeID == BROADCAST_NODEID){
								//	  printf("[timetest>>]recv a time test from %d\n", pkt.header.src_nodeID);
									//  gettimeofday(&(timetest->recvtime), NULL);
									//  echo as it comes, when recv will test time 
									  int next = pkt.header.src_nodeID;
									  pkt.header.dest_nodeID = next;
									  pkt.header.src_nodeID = myid;
									  pkt.header.type = TIMETEST;
									  if(son_sendpkt(next, &pkt, son_conn) < 0)
										  printf("[timetest>>]send ans [<error>] back to %d, next:%d\n", pkt.header.src_nodeID, next); 
								  }else if(pkt.header.dest_nodeID == myid){
									  gettimeofday(&(timetest->recvtime), NULL);
									  int sec = timetest->recvtime.tv_sec - timetest->sendtime.tv_sec;
									  int usec = timetest->recvtime.tv_usec - timetest->sendtime.tv_usec;
									  if(usec < 0){
										  usec += 1000000;
										  sec --;
									  }
									  long co = sec * 1000000 + usec;
									  co = co < 0?-co:co;
									  co *= 5;//, 更加均匀
									  while(co > 10){
										  co = co / 10;
									  }
								//	  printf("[timetest>>]get an ans of time test, the cost to %d is %ld\n", pkt.header.src_nodeID, co);
									  pthread_mutex_lock(routingtable_mutex);
									  nbrcosttable_setcost(nct, pkt.header.src_nodeID, co);
									  pthread_mutex_unlock(routingtable_mutex);
							//		  printf("[timetest>>UPDATE] update nbr table time stamp\n");
								  }else{
									  int next = routingtable_getnextnode(routingtable, pkt.header.dest_nodeID);
									  son_sendpkt(next, &pkt, son_conn);
						//			  printf("[timetest>>]forword a timetest to nbr, dest:%d, next:%d\n", pkt.header.dest_nodeID, next);
								  
								  }
								  break;
							  }
				default:;
			}
		}else{
			printf("[pkt_handler>>]recv error, will try again, now at pkthandler\n");
		}
	}
}

//这个函数终止SIP进程, 当SIP进程收到信号SIGINT时会调用这个函数. 
//它关闭所有连接, 释放所有动态分配的内存.
void sip_stop() {
	close(son_conn);
	son_conn = -1;
	//你需要编写这里的代码.
}

//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP() {
	//你需要编写这里的代码.
	int sockfd, stcpsockfd;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	memset(&serv_addr, 0, sizeof(serv_addr));
	memset(&cli_addr, 0, sizeof(cli_addr));
	clilen = sizeof(cli_addr);
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		printf("error creating socket\n");
		exit(-1);
	}
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(SIP_PORT);
	if(bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
		printf("bind error\n");
		exit(-1);
	}
	listen(sockfd, 20);
	printf("SIP waiting for STCP ~~\n");
	int myid = topology_getMyNodeID();
	while(1){
		printf("waitting for connect from stcp\n");
		stcpsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if(stcpsockfd < 0){
			if(errno == EINTR )
				continue;
			printf("error accept\n");
			exit(-1);
		}
		stcp_conn = stcpsockfd;
		printf("[waitSTCP>>]accept successful\n");
		seg_t seg;
		int dest = 0;
		int next = 0;
		sip_pkt_t tosend;
		while(1){
			int rc = getsegToSend(stcp_conn, &dest, &seg);
			if(rc == 0 || son_conn == -1)
				break;
			else if(rc < 0){
				printf("[waitSTCP>>] to accept another STCP\n");
				close(stcp_conn);
				break;
			}
			memset(&tosend, 0, sizeof(tosend));
			tosend.header.src_nodeID = myid;
			tosend.header.dest_nodeID = dest;
			tosend.header.length = sizeof(stcp_hdr_t) + seg.header.length;
			tosend.header.type = SIP;
			memcpy(tosend.data, &seg, tosend.header.length);
			pthread_mutex_lock(routingtable_mutex);
			next = routingtable_getnextnode(routingtable, dest);
			pthread_mutex_unlock(routingtable_mutex);
		//	printf("get a seg from stcp, now send to son, dest:%d, next:%d\n", dest, next);
			if(son_conn != -1 && son_sendpkt(next, &tosend, son_conn) > 0)
				printf("[waitSTCP>>]send pkt to son, next:%d, dest:%d\n", next, dest);
			else{
				close(son_conn);
				printf("[waitSTCP>>]send error ,return to accept another\n");
				son_conn = -1;
			}
		}
	}
	return;
}


int main(int argc, char *argv[]) {
	printf("SIP layer is starting, pls wait...\n");

	//初始化全局变量
	nct = nbrcosttable_create();
	dv = dvtable_create();
	dv_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(dv_mutex,NULL);
	routingtable = routingtable_create();
	routingtable_mutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	pthread_mutex_init(routingtable_mutex,NULL);
	son_conn = -1;
	stcp_conn = -1;

	nbrcosttable_print(nct);
	dvtable_print(dv);
	routingtable_print(routingtable);

	//注册用于终止进程的信号句柄
	signal(SIGINT, sip_stop);

	//连接到本地SON进程 
	son_conn = connectToSON();
	if(son_conn<0) {
		printf("can't connect to SON process\n");
		exit(1);		
	}
	
	//启动线程处理来自SON进程的进入报文 
	pthread_t pkt_handler_thread; 
	pthread_create(&pkt_handler_thread,NULL,pkthandler,(void*)0);

	//启动路由更新线程 
	pthread_t routeupdate_thread;
	pthread_create(&routeupdate_thread,NULL,routeupdate_daemon,(void*)0);	

	printf("SIP layer is started...\n");
	printf("waiting for routes to be established\n");
	sleep(SIP_WAITTIME);
	routingtable_print(routingtable);

	//等待来自STCP进程的连接
	printf("waiting for connection from STCP process\n");
	waitSTCP(); 
}


