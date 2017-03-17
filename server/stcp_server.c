#include <stdlib.h>
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include "stcp_server.h"
#include "../common/constants.h"
#include "../common/seg.h"
#include "../topology/topology.h"

server_tcb_t ** server_tcb_p;
int connection;
/*面向应用层的接口*/

//
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//

// stcp服务器初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL. 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量,
// 该变量作为sip_sendseg和sip_recvseg的输入参数. 最后, 这个函数启动seghandler线程来处理进入的STCP段.
// 服务器只有一个seghandler.
//

void stcp_server_init(int conn) 
{
	server_tcb_p = (server_tcb_t **)malloc(sizeof(server_tcb_t *) * MAX_TRANSPORT_CONNECTIONS);
	memset(server_tcb_p, 0, sizeof(server_tcb_t *) * MAX_TRANSPORT_CONNECTIONS);//将所有的表指针初始化为NULL
	connection = conn;
	pthread_t tid;
	pthread_create(&tid,NULL,closetimer,NULL);
	pthread_create(&tid, NULL, seghandler, NULL);
  return;
}

// 创建服务器套接字
//
// 这个函数查找服务器TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化, 例如, TCB state被设置为CLOSED, 服务器端口被设置为函数调用参数server_port. 
// TCB表中条目的索引应作为服务器的新套接字ID被这个函数返回, 它用于标识服务器的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.

int stcp_server_sock(unsigned int server_port) 
{
//	server_tcb_t* server_p; 
	int i = 0;
	int flag = -1;
	for(;i < MAX_TRANSPORT_CONNECTIONS;i++)
	{
		if(server_tcb_p[i] == NULL)
		{
			flag = 1;
			break;
		}
	}
	if(flag == -1)
	{
		return -1;
	}
	else
	{
		server_tcb_p[i] = (server_tcb_t*)malloc(sizeof(server_tcb_t));
		memset(server_tcb_p[i],0,sizeof(server_tcb_t));
		server_tcb_p[i] -> state = CLOSED;
		server_tcb_p[i] -> server_portNum = server_port;
		server_tcb_p[i]->server_nodeID = topology_getMyNodeID();
		server_tcb_p[i] -> bufMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
		pthread_mutex_init(server_tcb_p[i] -> bufMutex,NULL);
		server_tcb_p[i] -> wait_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
		pthread_cond_init(server_tcb_p[i] -> wait_cond,NULL);
		server_tcb_p[i]->recvBuf = malloc(RECEIVE_BUF_SIZE);
		return i;
	}
}

// 接受来自STCP客户端的连接
//
// 这个函数使用sockfd获得TCB指针, 并将连接的state转换为LISTENING. 它然后进入忙等待(busy wait)直到TCB状态转换为CONNECTED 
// (当收到SYN时, seghandler会进行状态的转换). 该函数在一个无穷循环中等待TCB的state转换为CONNECTED,  
// 当发生了转换时, 该函数返回1. 你可以使用不同的方法来实现这种阻塞等待.
//

int stcp_server_accept(int sockfd)
 {
 	pthread_mutex_lock(server_tcb_p[sockfd]->bufMutex);
	server_tcb_p[sockfd] -> state = LISTENING;
	printf("server_state LISTENING,prepare to wait\n");
	pthread_cond_wait(server_tcb_p[sockfd]->wait_cond, server_tcb_p[sockfd]->bufMutex);
	printf("server is awake\n");
	if(server_tcb_p[sockfd] -> state == CONNECTED){
			pthread_mutex_unlock(server_tcb_p[sockfd]->bufMutex);
			printf("connect success\n");
			return 1;
	}
	pthread_mutex_unlock(server_tcb_p[sockfd]->bufMutex);
	return 0;
}

// 接收来自STCP客户端的数据
//
// 这个函数接收来自STCP客户端的数据. 你不需要在本实验中实现它.
//
int getminlen(int a, int b){
	if(a < b)
		return a;
	return b;
}
int stcp_server_recv(int sockfd, void* buf, unsigned int length) 
{
	if(server_tcb_p[sockfd] == NULL || server_tcb_p[sockfd]->state != CONNECTED)
		return -1;
	char movebuf[RECEIVE_BUF_SIZE];
	int rc = length;
	while(rc > 0){
		pthread_mutex_lock(server_tcb_p[sockfd]->bufMutex);
		if(server_tcb_p[sockfd] -> usedBufLen > 0)
		{
			int getlen = getminlen(rc, server_tcb_p[sockfd]->usedBufLen);
			printf("bufed data is enough begin to move %d\n", getlen);
			memcpy(buf,server_tcb_p[sockfd] -> recvBuf, getlen);
			//data shift to top
			memcpy(movebuf, &(server_tcb_p[sockfd] -> recvBuf[getlen]), server_tcb_p[sockfd]->usedBufLen - getlen);
			memcpy(server_tcb_p[sockfd] -> recvBuf, movebuf, server_tcb_p[sockfd]->usedBufLen - getlen);
			server_tcb_p[sockfd] -> usedBufLen -= getlen;
			buf += getlen;
			rc -= getlen;
//			pthread_mutex_unlock(server_tcb_p[sockfd]->bufMutex);
		}
		pthread_mutex_unlock(server_tcb_p[sockfd]->bufMutex);
		usleep(RECVBUF_POLLING_INTERVAL * 1000000);
		printf("sleep RECVBUF_ROLLING_INTERVAL then check\n");
	}
	return 1;
}

// 关闭STCP服务器
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//

int stcp_server_close(int sockfd)
{
	server_tcb_t *tofree = server_tcb_p[sockfd];
	if(tofree == NULL)
		return -1;
	int try = 0;
	while(1){
		if(tofree->state != CLOSED){
			sleep(1);
			printf("wait to closed\n");
			try ++;	
			if(try > 4)
				return -1;
		}
	}
	pthread_mutex_destroy(tofree->bufMutex);
	pthread_cond_destroy(tofree->wait_cond);
	free(tofree->bufMutex);
	free(tofree->wait_cond);
	free(tofree->recvBuf);
	free(tofree);
	return 1;
}

// 处理进入段的线程
//
// 这是由stcp_server_init()启动的线程. 它处理所有来自客户端的进入数据. seghandler被设计为一个调用sip_recvseg()的无穷循环, 
// 如果sip_recvseg()失败, 则说明重叠网络连接已关闭, 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作.
// 请查看服务端FSM以了解更多细节.
//

void *seghandler(void* arg) 
{

	seg_t seg;
	seg_t ack;
	int srcnodeID;
	while(1)
	{
		memset(&seg, 0, sizeof(seg_t));
		int los = sip_recvseg(connection, &srcnodeID, &seg);
		if(los == 1){
			printf("[seghandler>>sip_recvseg]los a seg\n");
			continue;//seg lost
		}
		else if(los == -1){
			printf("son stoped, now handler stop\n");
			exit(-1);
		}
//		printf("server_recvseg success\n");
		int i = 0;
		int flag = 0;
		for(;i < MAX_TRANSPORT_CONNECTIONS;i++)
		{
			if(server_tcb_p[i] == NULL){
				continue;
			}
			if(server_tcb_p[i]->server_portNum == seg.header.dest_port)
			{
				flag = 1;
				printf("server_find receiver%d\n",i);
				break;
			}
		}
		if(flag)
		{
			pthread_mutex_lock(server_tcb_p[i]->bufMutex);
			switch(server_tcb_p[i] -> state)
			{
				case CLOSED:
				{
					break;
				}
				case LISTENING:
				{
					if(seg.header.type == SYN)
					{
						printf("server_segtype is SYN\n");
						memset(&ack, 0, sizeof(seg_t));
						ack.header.src_port = server_tcb_p[i]->server_portNum;
						ack.header.dest_port = seg.header.src_port;
						printf("src_pot%d\tdest_port%d\n",seg.header.src_port, seg.header.dest_port);
						ack.header.type = SYNACK;
						sip_sendseg(connection, srcnodeID, &ack);
						printf("server_sendSYNACK\n");
						server_tcb_p[i] -> state = CONNECTED;
						server_tcb_p[i]->client_nodeID = srcnodeID;
						pthread_cond_broadcast(server_tcb_p[i]->wait_cond);
					}
					break;
				}
				case CONNECTED:
				{
				//	printf("recv type is:%d, SYN:%d, FIN:%d, DATA:%d\n");
					if(seg.header.type == SYN)
					{
						memset(&ack, 0, sizeof(seg_t));
						ack.header.src_port = server_tcb_p[i]->server_portNum;
						ack.header.dest_port = seg.header.src_port;
						ack.header.type = SYNACK;
						server_tcb_p[i] -> expect_seqNum = seg.header.seq_num;									//初始化expect_seqnum
						sip_sendseg(connection,server_tcb_p[i]->client_nodeID, &ack);
					}
					else if(seg.header.type == FIN)
					{
						gettimeofday(&(server_tcb_p[i]->start),0);
						memset(&ack, 0, sizeof(seg_t));
						ack.header.src_port = server_tcb_p[i]->server_portNum;
						ack.header.dest_port = seg.header.src_port;
						ack.header.type = FINACK;
						server_tcb_p[i] -> state = CLOSEWAIT;
						sip_sendseg(connection, server_tcb_p[i]->client_nodeID, &ack);
					}
					else if(seg.header.type == DATA)
					{
					//	printf("receive a DATA seg, seq num is %d\n", seg.header.seq_num);
					//	int bufflag = 0;
						if((server_tcb_p[i] -> usedBufLen) + seg.header.length > RECEIVE_BUF_SIZE)
						{
							printf("over the maxbufsize");//直接悄悄丢弃
						}
						else if(seg.header.seq_num == server_tcb_p[i] -> expect_seqNum)
						{	
							printf("server got what i expect\n");
							memcpy(&(server_tcb_p[i]->recvBuf[server_tcb_p[i]->usedBufLen]), seg.data, seg.header.length);
						//	printf("copy finished\n");
							(server_tcb_p[i] -> usedBufLen) += seg.header.length;
							(server_tcb_p[i] -> expect_seqNum) += seg.header.length;
							printf("data length is %d\n", seg.header.length);
							memset(&ack, 0, sizeof(seg_t));
							ack.header.src_port = server_tcb_p[i]->server_portNum;
							ack.header.dest_port = seg.header.src_port;
							ack.header.type = DATAACK;
							ack.header.ack_num = server_tcb_p[i] -> expect_seqNum;
							printf("after ack a seg the expect num change to %d\n", server_tcb_p[i]->expect_seqNum);
							sip_sendseg(connection, server_tcb_p[i]->client_nodeID, &ack);
						}
						else if(seg.header.seq_num != server_tcb_p[i] -> expect_seqNum)
						{
						//	printf("server don't got what i expect\n");
							printf("want seq %d, but receive:%d\n", server_tcb_p[i]->expect_seqNum, seg.header.seq_num);
							memset(&ack, 0, sizeof(seg_t));
							ack.header.src_port = server_tcb_p[i]->server_portNum;
							ack.header.dest_port = seg.header.src_port;
							ack.header.type = DATAACK;
							ack.header.ack_num = server_tcb_p[i] -> expect_seqNum;
							sip_sendseg(connection, server_tcb_p[i]->client_nodeID, &ack);						
						}

					}
					break;
				}
				case CLOSEWAIT:
				{
					if(seg.header.type == FIN)
					{
						memset(&ack, 0, sizeof(seg_t));
						ack.header.src_port = server_tcb_p[i]->server_portNum;
						ack.header.dest_port = seg.header.src_port;
						ack.header.type = FINACK;
						sip_sendseg(connection, server_tcb_p[i]->client_nodeID, &ack);
					}
					break;
				}
				default:;
			}
			pthread_mutex_unlock(server_tcb_p[i]->bufMutex);
		}
	}
	return 0;
}

void *closetimer(void * arg)
{
	int i = 0;
	struct timeval stop,diff;
	while(1)
	{
		for(i = 0;i < MAX_TRANSPORT_CONNECTIONS;i++)
		{
			if(server_tcb_p[i] != NULL && server_tcb_p[i]->state == CLOSEWAIT)
			{
				gettimeofday(&stop, 0);
				timeval_subtract(&diff, &(server_tcb_p[i]->start), &stop);
				if((diff.tv_sec * (double)ENINE + diff.tv_usec * 1000.0) > (CLOSEWAIT_TIMEOUT *(double) ENINE))
					server_tcb_p[i] ->state = CLOSED;
			}
		}
	}
	return NULL;
}

int timeval_subtract(struct timeval *result, struct timeval* x, struct timeval *y)
{
//	int nsec;
	if(x->tv_sec>y->tv_sec)
		return -1;
	if(x->tv_sec==y->tv_sec && x->tv_usec>y->tv_usec)
		return -1;
	result->tv_sec = y->tv_sec-x->tv_sec;
	result->tv_usec = y->tv_usec-x->tv_usec;
	if(result->tv_usec < 0)
	{
		result->tv_sec--;
		result->tv_usec+=1000000;
	}
	return 0;
}

































