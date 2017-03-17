#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "stcp_client.h"
#include <malloc.h>
#include "../common/constants.h"
#include "../topology/topology.h"

//下面定义client tcb
client_tcb_t** client_tcb_p;//tcb表指针,将在init中进行初始化
//son层的sockfd
int son_sockfd;
/*面向应用层的接口*/

//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: 当实现这些函数时, 你需要考虑FSM中所有可能的状态, 这可以使用switch语句来实现.
//
//  目标: 你的任务就是设计并实现下面的函数原型.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// stcp客户端初始化
//
// 这个函数初始化TCB表, 将所有条目标记为NULL.  
// 它还针对重叠网络TCP套接字描述符conn初始化一个STCP层的全局变量, 该变量作为sip_sendseg和sip_recvseg的输入参数.
// 最后, 这个函数启动seghandler线程来处理进入的STCP段. 客户端只有一个seghandler.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void stcp_client_init(int conn) {
	client_tcb_p = (client_tcb_t **)malloc(sizeof(client_tcb_t *) * MAX_TRANSPORT_CONNECTIONS);
	memset(client_tcb_p, 0, sizeof(client_tcb_t *) * MAX_TRANSPORT_CONNECTIONS);//将所有的表指针初始化为NULL
	son_sockfd = conn;
	pthread_t tid;
	pthread_create(&tid, NULL, seghandler, NULL);
	printf("client initial end\n");
	return;
}

// 创建一个客户端TCB条目, 返回套接字描述符
//
// 这个函数查找客户端TCB表以找到第一个NULL条目, 然后使用malloc()为该条目创建一个新的TCB条目.
// 该TCB中的所有字段都被初始化. 例如, TCB state被设置为CLOSED，客户端端口被设置为函数调用参数client_port. 
// TCB表中条目的索引号应作为客户端的新套接字ID被这个函数返回, 它用于标识客户端的连接. 
// 如果TCB表中没有条目可用, 这个函数返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_sock(unsigned int client_port) {
	int index = 0;
	int notfind = 1;
	for(; index < MAX_TRANSPORT_CONNECTIONS; index++){
		if(client_tcb_p[index] == NULL){
			notfind = 0;
			break;
		}
	}
	if(notfind == 1){
		printf("dont have enough tcb entry\n");
		return -1;
	}
	client_tcb_p[index] = (client_tcb_t*)malloc(sizeof(client_tcb_t));
	if(client_tcb_p[index] == NULL){//内存不足
		printf("dont have enough memory\n");
		fflush(stdout);
		return -1;
	}
	memset(client_tcb_p[index], 0, sizeof(client_tcb_t));
	client_tcb_p[index]->client_portNum = client_port;
	client_tcb_p[index]->state = CLOSED;
	client_tcb_p[index]->client_nodeID = topology_getMyNodeID();
	printf("client_tcb_p[%d], set closed\n", index);
	client_tcb_p[index]->bufMutex = (pthread_mutex_t*)malloc(sizeof(pthread_mutex_t));
	printf("client_tcb_p[%d], malloc mutex\n", index);
	pthread_mutex_init(client_tcb_p[index]->bufMutex, NULL);
	client_tcb_p[index]->recv_cond = (pthread_cond_t*)malloc(sizeof(pthread_cond_t));
	printf("client_tcb_p[%d], malloc cond\n", index);
	pthread_cond_init(client_tcb_p[index]->recv_cond, NULL);
	return index;
}

// 连接STCP服务器
//
// 这个函数用于连接服务器. 它以套接字ID和服务器的端口号作为输入参数. 套接字ID用于找到TCB条目.  
// 这个函数设置TCB的服务器端口号,  然后使用sip_sendseg()发送一个SYN段给服务器.  
// 在发送了SYN段之后, 一个定时器被启动. 如果在SYNSEG_TIMEOUT时间之内没有收到SYNACK, SYN 段将被重传. 
// 如果收到了, 就返回1. 否则, 如果重传SYN的次数大于SYN_MAX_RETRY, 就将state转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_connect(int sockfd, int server_nodeID, unsigned int server_port) {
	seg_t syn;
	int try_times = 0;
	struct timespec syntimeout;
	struct timeval now;
	memset(&syn, 0, sizeof(syn));
	if(client_tcb_p[sockfd] == NULL)
		return -1;
	syn.header.src_port = client_tcb_p[sockfd]->client_portNum;
	client_tcb_p[sockfd]->server_portNum = server_port;
	syn.header.dest_port = server_port;
	syn.header.type = SYN;
	client_tcb_p[sockfd]->next_seqNum = 0;
	while(1){
		pthread_mutex_lock(client_tcb_p[sockfd]->bufMutex);
		syn.header.seq_num = client_tcb_p[sockfd]->next_seqNum;//use seq	
		printf("try %d times, syn%d\n", try_times, syn.header.seq_num);
		sip_sendseg(son_sockfd, server_nodeID, &syn);
		printf("send syn\n");
		fflush(stdout);
		client_tcb_p[sockfd]->state = SYNSENT;
		gettimeofday(&now, NULL);
		syntimeout.tv_sec = now.tv_sec;
		syntimeout.tv_nsec = now.tv_usec * 1000 + SYN_TIMEOUT;
		if(syntimeout.tv_nsec >= ENINE){
			syntimeout.tv_nsec -= ENINE;
			syntimeout.tv_sec ++;
		}
	//	printf("get time \n");
	//	fflush(stdout);
		pthread_cond_timedwait(client_tcb_p[sockfd]->recv_cond, client_tcb_p[sockfd]->bufMutex, &syntimeout);
		switch(client_tcb_p[sockfd]->state){
			case CLOSED:
				pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
				return -1;
			case SYNSENT:
				try_times ++;
				if(try_times >= SYN_MAX_RETRY){
					printf("connect try times out, giveup\n");
					client_tcb_p[sockfd]->state = CLOSED;
					pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
					return -1;//retry times out
				}
				break;
			case CONNECTED:
				printf("connected successful \n");
				client_tcb_p[sockfd]->server_nodeID = server_nodeID;
				pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
				return 1;//connected
			case FINWAIT:
				pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
				return -1;
			default:;
		}
		pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
	}
	assert(0);//should not comes here
	return -1;
}

// 发送数据给STCP服务器
//
// 这个函数发送数据给STCP服务器. 你不需要在本实验中实现它。
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void  splitadd(client_tcb_t * tcb, char * data, unsigned int length){
	int rc = length;
	segBuf_t * tail = tcb->sendBufTail;
	if(tcb->sendBufHead == NULL){//there no seg to send and need ack
		pthread_t tid;
		printf("start sendBuf_timer\n");
		printf("throw arg:%ld\n", tcb);
		pthread_create(&tid, NULL, sendBuf_timer, tcb);//start timer
	}

	while(rc > MAX_SEG_LEN){
		if(tail == NULL){
			printf("add first\n");
			tail = malloc(sizeof(segBuf_t));
			tcb->sendBufHead = tail;
			tcb->sendBufunSent = tail;
		}
		else{
			printf("add one\n");
			tail->next = malloc(sizeof(segBuf_t));
			tail = tail->next;
		}
		memset(tail, 0, sizeof(segBuf_t));
		(tail->seg).header.src_port = tcb->client_portNum;
		(tail->seg).header.dest_port = tcb->server_portNum;
		(tail->seg).header.seq_num = tcb->next_seqNum;
		(tail->seg).header.length = MAX_SEG_LEN;
		printf("when add seq_num is %d\n", tcb->next_seqNum);
		(tail->seg).header.type = DATA;
		memcpy((tail->seg).data, data, MAX_SEG_LEN);
		data += MAX_SEG_LEN;
		tcb->next_seqNum += MAX_SEG_LEN;
		rc -= MAX_SEG_LEN;	
	}
	if(rc > 0){
		if(tail == NULL){
			printf("add first\n");
			tail = malloc(sizeof(segBuf_t));
			tcb->sendBufHead = tail;
			tcb->sendBufunSent = tail;
		}else{
			printf("add one\n");
			tail->next = malloc(sizeof(segBuf_t));
			tail = tail->next;
		}
		memset(tail, 0, sizeof(segBuf_t));
		(tail->seg).header.src_port = tcb->client_portNum;
		(tail->seg).header.dest_port = tcb->server_portNum;
		(tail->seg).header.seq_num = tcb->next_seqNum;
		(tail->seg).header.length = rc;
		printf("when add seq_num is %d\n", tcb->next_seqNum);
		(tail->seg).header.type = DATA;
		memcpy((tail->seg).data, data, rc);
		tcb->next_seqNum += rc;
		rc = 0;
	}
	tcb->sendBufTail = tail;
	printf("splitadd end\n");
	return;
}

int stcp_client_send(int sockfd, void* data, unsigned int length) {
	if(client_tcb_p[sockfd] == NULL)
		return -1;
	pthread_mutex_lock(client_tcb_p[sockfd]->bufMutex);
	printf("send get the lock @%d\n", pthread_self());
	if(client_tcb_p[sockfd]->state != CONNECTED){
		printf("send going to free the lock @%d\n", pthread_self());
		pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
		return -1;
	}
	splitadd(client_tcb_p[sockfd], data, length);//add to tail
	printf("add to tail\n");
	printf("send going to free the lock @%d\n", pthread_self());
	pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
	return 1;
}

// 断开到STCP服务器的连接
//
// 这个函数用于断开到服务器的连接. 它以套接字ID作为输入参数. 套接字ID用于找到TCB表中的条目.  
// 这个函数发送FIN segment给服务器. 在发送FIN之后, state将转换到FINWAIT, 并启动一个定时器.
// 如果在最终超时之前state转换到CLOSED, 则表明FINACK已被成功接收. 否则, 如果在经过FIN_MAX_RETRY次尝试之后,
// state仍然为FINWAIT, state将转换到CLOSED, 并返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_disconnect(int sockfd) {
	seg_t fin;
	int try_times = 0;
	struct timespec fintimeout;
	struct timeval now;
	memset(&fin, 0, sizeof(fin));
	if(client_tcb_p[sockfd] == NULL)
		return -1;
	while(client_tcb_p[sockfd]->sendBufHead != NULL){
		printf("wait end\n");
		fflush(stdout);
		usleep(1e5);//等待发送结束
	}
	fin.header.src_port = client_tcb_p[sockfd]->client_portNum;
	fin.header.dest_port = client_tcb_p[sockfd]->server_portNum;
	fin.header.type = FIN;
	while(1){
		pthread_mutex_lock(client_tcb_p[sockfd]->bufMutex);
		printf("disconnect get the lock @%d\n", pthread_self());
		fin.header.seq_num = client_tcb_p[sockfd]->next_seqNum;//use seq
	//	(client_tcb_p[sockfd]->next_seqNum)++;
		sip_sendseg(son_sockfd, client_tcb_p[sockfd]->server_nodeID,  &fin);
		client_tcb_p[sockfd]->state = FINWAIT;
		gettimeofday(&now, NULL);
		fintimeout.tv_sec = now.tv_sec;
		fintimeout.tv_nsec = now.tv_usec * 1000 + SYN_TIMEOUT;
		if(fintimeout.tv_nsec >= ENINE){
			fintimeout.tv_nsec -= ENINE;
			fintimeout.tv_sec ++;
		}
		pthread_cond_timedwait(client_tcb_p[sockfd]->recv_cond, client_tcb_p[sockfd]->bufMutex, &fintimeout);
		switch(client_tcb_p[sockfd]->state){
			case CLOSED:
				printf("closed successful~~~~~~\n");
				printf("disconnect going to free the lock @%d\n", pthread_self());
				pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
				return 1;//成功断开
			case SYNSENT:
				printf("closed , but something maybe wrong\n");
				client_tcb_p[sockfd]->state = CLOSED;
				printf("disconnect going to free the lock @%d\n", pthread_self());
				pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
				return -1;
			case CONNECTED:
				printf("closed , but something maybe wrong\n");
				client_tcb_p[sockfd]->state = CLOSED;
				(client_tcb_p[sockfd]->next_seqNum)++;
				printf("disconnect going to free the lock @%d\n", pthread_self());
				pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
				return -1;//should not come here
			case FINWAIT:
				try_times++;
				if(try_times >= FIN_MAX_RETRY){
					printf("FIN times out , closed\n");
					client_tcb_p[sockfd]->state = CLOSED;
					printf("disconnect going to free the lock @%d\n", pthread_self());
					pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
					return -1;
				}
				break;
			default:;
		}
		printf("disconnect going to free the lock @%d\n", pthread_self());
		pthread_mutex_unlock(client_tcb_p[sockfd]->bufMutex);
	}
	assert(0);//should not comes here
	return -1;
}

// 关闭STCP客户
//
// 这个函数调用free()释放TCB条目. 它将该条目标记为NULL, 成功时(即位于正确的状态)返回1,
// 失败时(即位于错误的状态)返回-1.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

int stcp_client_close(int sockfd) {
	client_tcb_t *tofree = client_tcb_p[sockfd];
	if(tofree == NULL)
		return -1;
	if(tofree->state != CLOSED)
		return -1;
	pthread_mutex_destroy(tofree->bufMutex);
	pthread_cond_destroy(tofree->recv_cond);
	free(tofree->bufMutex);
	free(tofree->recv_cond);
	free(tofree);
	return 1;
}

// 处理进入段的线程
//
// 这是由stcp_client_init()启动的线程. 它处理所有来自服务器的进入段. 
// seghandler被设计为一个调用sip_recvseg()的无穷循环. 如果sip_recvseg()失败, 则说明重叠网络连接已关闭,
// 线程将终止. 根据STCP段到达时连接所处的状态, 可以采取不同的动作. 请查看客户端FSM以了解更多细节.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

void *seghandler(void* arg) {
	seg_t seg_b;
	int srcnodeID = 0;
	while(1){
		int rc = sip_recvseg(son_sockfd, &srcnodeID,  &seg_b);
		printf("one loop\n");
		if(rc == -1){//sip_recv  返回负数表示son层关闭
			printf("sip recv error, will exit\n");
			fflush(stdout);
			exit(-1);
		}
		//正常受到一个包
		if(rc){//损坏.
			 printf("lost a seg, @thread:%d\n", (int)pthread_self());
			 fflush(stdout);
		}else{
			printf("recv a segment, @thread:%d\n", (int)pthread_self());
			printf("src_port:%u, dest_port:%u, ack_num:%u, type:%d\n",\
					seg_b.header.src_port, seg_b.header.dest_port, seg_b.header.ack_num, seg_b.header.type);
			fflush(stdout);
			//变量查找是给谁的.
			int i = 0;
			for(; i < MAX_TRANSPORT_CONNECTIONS; i++){
				if(client_tcb_p[i] != NULL && seg_b.header.dest_port == client_tcb_p[i]->client_portNum\
						&& seg_b.header.src_port == client_tcb_p[i]->server_portNum){
					printf("client find reciver id:%d\n", i);
					pthread_mutex_lock(client_tcb_p[i]->bufMutex);
			//		printf("handler get the lock @%d\n", pthread_self());
					switch(client_tcb_p[i]->state){
						case CLOSED:
							//just ignore
							break;
						case SYNSENT:
							if(seg_b.header.type == SYNACK){
								client_tcb_p[i]->state = CONNECTED;
								pthread_cond_broadcast(client_tcb_p[i]->recv_cond);//尝试唤醒条件变量上的线程
							}//其他的抱可以直接忽略
							break;
						case CONNECTED:
							if(seg_b.header.type == DATAACK){
								//visit delete
								if(client_tcb_p[i]->unAck_segNum > 0){
									
									segBuf_t * buf = client_tcb_p[i]->sendBufHead;
									while(buf != NULL){
										if((buf->seg).header.seq_num < seg_b.header.ack_num){
											printf("[seghandler>>]type is ack ,ack some data , ack num is :%d\n", seg_b.header.ack_num);
											client_tcb_p[i]->sendBufHead = buf->next;
											free(buf);
											buf = client_tcb_p[i]->sendBufHead;
											(client_tcb_p[i]->unAck_segNum) --;
										}
										else{
											printf("ack end \n");
											break;
										}
									}
									if(client_tcb_p[i]->sendBufHead == NULL){//发送缓冲区空
										client_tcb_p[i]->sendBufunSent = NULL;
										client_tcb_p[i]->sendBufTail = NULL;
										client_tcb_p[i]->unAck_segNum = 0;
									}
								}
							}//其他的可以忽略
							break;
						case FINWAIT:
							if(seg_b.header.type == FINACK){
								client_tcb_p[i]->state = CLOSED;
								pthread_cond_broadcast(client_tcb_p[i]->recv_cond);//唤醒
							}
							break;
						default:;
					}
			//		printf("handler going to free the lock @%d\n", pthread_self());
					pthread_mutex_unlock(client_tcb_p[i]->bufMutex);
				}
			}
		}
	}
}


// ÕâžöÏß³Ì³ÖÐøÂÖÑ¯·¢ËÍ»º³åÇøÒÔŽ¥·¢³¬Ê±ÊÂŒþ. Èç¹û·¢ËÍ»º³åÇø·Ç¿Õ, ËüÓŠÒ»Ö±ÔËÐÐ.
// Èç¹û(µ±Ç°Ê±Œä - µÚÒ»žöÒÑ·¢ËÍµ«ÎŽ±»È·ÈÏ¶ÎµÄ·¢ËÍÊ±Œä) > DATA_TIMEOUT, ŸÍ·¢ÉúÒ»ŽÎ³¬Ê±ÊÂŒþ.
// µ±³¬Ê±ÊÂŒþ·¢ÉúÊ±, ÖØÐÂ·¢ËÍËùÓÐÒÑ·¢ËÍµ«ÎŽ±»È·ÈÏ¶Î. µ±·¢ËÍ»º³åÇøÎª¿ÕÊ±, ÕâžöÏß³Ìœ«ÖÕÖ¹.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
void* sendBuf_timer(void* clienttcb)
{
	printf("sendBuf_timer running\n");
	fflush(stdout);
	client_tcb_t * tcb = (client_tcb_t*)clienttcb;
	struct timeval start, now, time;
	gettimeofday(&start, NULL);
//	printf("gettimeofday \n");
//	fflush(stdout);
	if(tcb == NULL){
		printf("senfbuf_timer tcb is NULL\n");
		pthread_exit(NULL);
	}
	printf("the sendbuf arg is :%ld\n, the unack_seqNum is :%d \n", clienttcb, tcb ->unAck_segNum);
	while(1){
		pthread_mutex_lock(tcb->bufMutex);
//		printf("sendBuf_timer get the lock @%d\n", (int)pthread_self());
		if(tcb ->sendBufHead == NULL){
			printf("send buf is null, then sendBuf_timer will stop\n");
			pthread_mutex_unlock(tcb->bufMutex);
//			printf("sendBuf_timer going to free the lock @%d\n", (int)pthread_self());
			pthread_exit(NULL);//exit
		}
		gettimeofday(&now, NULL);
		timeval_subtract(&time, &start, &now);
		if(tcb->unAck_segNum > 0 && (time.tv_sec * ENINE + time.tv_usec * 1000) >= SENDBUF_POLLING_INTERVAL){
			timeval_subtract(&time, &((tcb->sendBufHead)->sentTime), &now);	
			if((time.tv_sec * ENINE + time.tv_usec * 1000) > DATA_TIMEOUT){
				tcb->sendBufunSent = tcb->sendBufHead;//
				tcb->unAck_segNum = 0;
				printf("resend\n");
			}
			gettimeofday(&start, NULL);
		}
		if(tcb->unAck_segNum < GBN_WINDOW && tcb->sendBufunSent != NULL){
			printf("send a data seg, seq num is %d\n", (tcb->sendBufunSent)->seg.header.seq_num);
			sip_sendseg(son_sockfd, tcb->server_nodeID, &((tcb->sendBufunSent)->seg));
			gettimeofday(&((tcb->sendBufunSent)->sentTime), NULL);
			tcb->sendBufunSent = (tcb->sendBufunSent)->next;
			(tcb->unAck_segNum)++;
		}
//		printf("sendBuf_timer going to free the lock @%d\n", (int)pthread_self());
		pthread_mutex_unlock(tcb->bufMutex);
	}
}


int timeval_subtract(struct timeval *result, struct timeval* x, struct timeval *y)
{
	int nsec;
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




