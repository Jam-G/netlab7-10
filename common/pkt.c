// 文件名: common/pkt.c
// 创建日期: 2015年

#include "pkt.h"
#include"seg.h"
#include<errno.h>
#include<stdlib.h>
#include<stdio.h>
#include<pthread.h>
#define PKTSTART1 0
#define PKTSTART2 1
#define PKTRECV 2
#define PKTSTOP1 3
#define PKTSTOP2 4
// son_sendpkt()由SIP进程调用, 其作用是要求SON进程将报文发送到重叠网络中. SON进程和SIP进程通过一个本地TCP连接互连.
// 在son_sendpkt()中, 报文及其下一跳的节点ID被封装进数据结构sendpkt_arg_t, 并通过TCP连接发送给SON进程. 
// 参数son_conn是SIP进程和SON进程之间的TCP连接套接字描述符.
// 当通过SIP进程和SON进程之间的TCP连接发送数据结构sendpkt_arg_t时, 使用'!&'和'!#'作为分隔符, 按照'!& sendpkt_arg_t结构 !#'的顺序发送.
// 如果发送成功, 返回1, 否则返回-1.
pthread_mutex_t forwardMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t sippktmutex = PTHREAD_MUTEX_INITIALIZER;
int readn( int fd, char *bp, int len)
{
	int cnt;
	int rc;
	cnt = len;
	while ( cnt > 0 ){
		rc = recv( fd, bp, cnt, 0 );
		if ( rc < 0 )				/* read error? */
		{
			if ( errno == EINTR )	/* interrupted? */
				continue;			/* restart the read */
			return -1;				/* return error */
		}
		if ( rc == 0 )				/* EOF? */
			return len - cnt;		/* return short count */
		bp += rc;
		cnt -= rc;
	}
	return len;
}
int son_sendpkt(int nextNodeID, sip_pkt_t* pkt, int son_conn)
{
	sendpkt_arg_t packet;
	packet.nextNodeID = nextNodeID;
	int length = pkt->header.length + sizeof(sip_hdr_t);
	pthread_mutex_lock(&sippktmutex);
	memcpy(&(packet.pkt), pkt, length);
	if(send(son_conn, "!&", 2, 0) <= 0){
		printf("sip send !& error\n");
		pthread_mutex_unlock(&sippktmutex);
		return -1;
	}
	if(send(son_conn, &packet, sizeof(int) + length, 0) <= 0){
		printf("sip send pkt error\n");
		pthread_mutex_unlock(&sippktmutex);
		return -1;
	}
	if(send(son_conn, "!#", 2, 0) <=0){
		printf("sip send !# error\n");
		pthread_mutex_unlock(&sippktmutex);
		return -1;
	}
	pthread_mutex_unlock(&sippktmutex);
	 return 1;
}

// son_recvpkt()函数由SIP进程调用, 其作用是接收来自SON进程的报文. 
// 参数son_conn是SIP进程和SON进程之间TCP连接的套接字描述符. 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int son_recvpkt(sip_pkt_t* pkt, int son_conn)
{
	if(pkt == NULL)
		return -1;
	int state = PKTSTART1;
	int len = sizeof(sip_pkt_t);
	char *buf = (char *)pkt;
	char c;
	int recved = 0;
	memset(buf, 0, len);
	while(1){
		int rc = readn(son_conn, &c, 1);
		if(rc == 0){
			printf("son sip socket closed, will exit\n");
			pthread_exit(NULL);
		}else if(rc < 0){
			return -1;
		}
//		printf("recv char %c\n", c);
		switch(state){
			case PKTSTART1:
				if(c == '!')
					state = PKTSTART2;
				break;
			case PKTSTART2:
				if(c=='&'){
					recved = 0;
					len = sizeof(sip_pkt_t);
					state = PKTRECV;
				}
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
			case PKTRECV:
				buf[recved ++] = c;
				if(recved == sizeof(sip_hdr_t)){
					len = pkt->header.length +sizeof(sip_hdr_t);
			//		printf("recv a header , length is :%d, header length is :%d\n", len, sizeof(sip_hdr_t));
				}
				if(len > (int)sizeof(sip_pkt_t))
					state = PKTSTART1;
				if(recved == len)
					state = PKTSTOP1;
				break;
			case PKTSTOP1:
				if(c == '!')
					state = PKTSTOP2;
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
			case PKTSTOP2:
				if(c == '#'){
					state = PKTSTART1;
					return 1;
				}
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
		}
	}
}

// 这个函数由SON进程调用, 其作用是接收数据结构sendpkt_arg_t.
// 报文和下一跳的节点ID被封装进sendpkt_arg_t结构.
// 参数sip_conn是在SIP进程和SON进程之间的TCP连接的套接字描述符. 
// sendpkt_arg_t结构通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 如果成功接收sendpkt_arg_t结构, 返回1, 否则返回-1.
int getpktToSend(sip_pkt_t* pkt, int* nextNode,int sip_conn)
{
	int state = PKTSTART1;
	int len = sizeof(sendpkt_arg_t);
	sendpkt_arg_t recvbuf;
	char *buf = (char *)&recvbuf;
	char c;
	int recved = 0;
	memset(buf, 0, len);
	while(1){
		if(readn(sip_conn, &c, 1) != 1)
			return -1;
		switch(state){
			case PKTSTART1:
				if(c == '!')
					state = PKTSTART2;
				break;
			case PKTSTART2:
				if(c=='&'){
					recved = 0;
					len = sizeof(sendpkt_arg_t);
					state = PKTRECV;
				}
				else{
					state = PKTSTART1;
					printf("cant recived full !&, miss &, but recived: %c\n", c);
					return -1;
				}
				break;
			case PKTRECV:
				buf[recved ++] = c;
				if(recved == sizeof(int) + sizeof(sip_hdr_t))
					len = recvbuf.pkt.header.length +sizeof(sip_hdr_t) + sizeof(int);
				if(len > (int)sizeof(sendpkt_arg_t))
					state = PKTSTART1;
				if(recved == len)
					state = PKTSTOP1;
				break;
			case PKTSTOP1:
				if(c == '!')
					state = PKTSTOP2;
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
			case PKTSTOP2:
				if(c == '#'){
					state = PKTSTART1;
					*nextNode = recvbuf.nextNodeID;
					memcpy(pkt, &(recvbuf.pkt), len - sizeof(int));
					return 1;
				}
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
		}
	}

}

// forwardpktToSIP()函数是在SON进程接收到来自重叠网络中其邻居的报文后被调用的. 
// SON进程调用这个函数将报文转发给SIP进程. 
// 参数sip_conn是SIP进程和SON进程之间的TCP连接的套接字描述符. 
// 报文通过SIP进程和SON进程之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int forwardpktToSIP(sip_pkt_t* pkt, int sip_conn)
{
	if(pkt == NULL){
		return -1;
	}
	int length = pkt->header.length + sizeof(sip_hdr_t);
	pthread_mutex_lock(&forwardMutex);
	if(send(sip_conn, "!&", 2, 0) <= 0){
		pthread_mutex_unlock(&forwardMutex);
		return -1;
	}
	if(send(sip_conn, pkt, length, 0) <= 0){
		pthread_mutex_unlock(&forwardMutex);
		return -1;
	}
	if(send(sip_conn, "!#", 2, 0) <=0){
		pthread_mutex_unlock(&forwardMutex);
		return -1;
	}
	printf("send a pkt to sip\n");
	pthread_mutex_unlock(&forwardMutex);
	return 1;
}

// sendpkt()函数由SON进程调用, 其作用是将接收自SIP进程的报文发送给下一跳.
// 参数conn是到下一跳节点的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居节点之间的TCP连接发送, 使用分隔符!&和!#, 按照'!& 报文 !#'的顺序发送. 
// 如果报文发送成功, 返回1, 否则返回-1.
int sendpkt(sip_pkt_t* pkt, int conn)
{
	if(pkt == NULL)
		return -1;
	int length = pkt->header.length + sizeof(sip_hdr_t);
	if(send(conn, "!&", 2, 0) <= 0)
		return -1;
	if(send(conn, pkt, length, 0) <= 0)
		return -1;
	if(send(conn, "!#", 2, 0) <= 0)
		return -1;
	 return 1;
}

// recvpkt()函数由SON进程调用, 其作用是接收来自重叠网络中其邻居的报文.
// 参数conn是到其邻居的TCP连接的套接字描述符.
// 报文通过SON进程和其邻居之间的TCP连接发送, 使用分隔符!&和!#. 
// 为了接收报文, 这个函数使用一个简单的有限状态机FSM
// PKTSTART1 -- 起点 
// PKTSTART2 -- 接收到'!', 期待'&' 
// PKTRECV -- 接收到'&', 开始接收数据
// PKTSTOP1 -- 接收到'!', 期待'#'以结束数据的接收 
// 如果成功接收报文, 返回1, 否则返回-1.
int recvpkt(sip_pkt_t* pkt, int conn)
{
	int state = PKTSTART1;
	int len = sizeof(sip_pkt_t);
	char *buf = (char *)pkt;
	char c;
	int recved = 0;
	memset(buf, 0, len);
	while(1){
		int rc = readn(conn, &c, 1);
		if(rc < 0){
			printf("readn error, can't read one char, ret val :%d\n", rc);
			return -1;
		}else if(rc == 0){
			printf("connection closed, now at recvpkt, then to return to listen and try reconnect\n");
			return -2;	
		}
		//正常接收一个字符`
		switch(state){
			case PKTSTART1:
				if(c == '!')
					state = PKTSTART2;
				break;
			case PKTSTART2:
				if(c=='&'){
					recved = 0;
					len = sizeof(sip_pkt_t);
					state = PKTRECV;
				}
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
			case PKTRECV:
				buf[recved ++] = c;
				if(recved == sizeof(sip_hdr_t))
					len = pkt->header.length +sizeof(sip_hdr_t);
				if(len > sizeof(sip_pkt_t))
					state = PKTSTART1;
				if(recved == len)
					state = PKTSTOP1;
				break;
			case PKTSTOP1:
				if(c == '!')
					state = PKTSTOP2;
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
			case PKTSTOP2:
				if(c == '#'){
					state = PKTSTART1;
					return 1;
				}
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
		}
	}
}


int forwordsegToSTCP(int stcp_conn, int src_nodeID, seg_t* segPtr){
	sendseg_arg_t packet;
	packet.nodeID = src_nodeID;
	int length = segPtr->header.length + sizeof(stcp_hdr_t);
	memcpy(&(packet.seg), segPtr, length);
	if(send(stcp_conn, "!&", 2, 0) <= 0){
		printf("sip send !& error to stcp\n");
		return -1;
	}
	if(send(stcp_conn, &packet, sizeof(int) + length, 0) <= 0){
		printf("sip send seg error to stcp\n");
		return -1;
	}
	if(send(stcp_conn, "!#", 2, 0) <=0){
		printf("sip send !# error to stcp\n");
		return -1;
	}
	 return 1;
}
int getsegToSend(int stcp_conn, int* dest_nodeID, seg_t* segPtr){
  	int state = PKTSTART1;
	int len = sizeof(sendseg_arg_t);
	sendseg_arg_t recvbuf;
	char *buf = (char *)&recvbuf;
	char c;
	int recved = 0;
	memset(buf, 0, len);
	while(1){
		if(readn(stcp_conn, &c, 1) != 1)
			return -1;
		switch(state){
			case PKTSTART1:
				if(c == '!')
					state = PKTSTART2;
				break;
			case PKTSTART2:
				if(c=='&'){
					recved = 0;
					len = sizeof(sendseg_arg_t);
					state = PKTRECV;
				}
				else{
					state = PKTSTART1;
					printf("cant recived full !&, miss &, but recived: %c\n", c);
					return -1;
				}
				break;
			case PKTRECV:
				buf[recved ++] = c;
				if(recved == sizeof(int) + sizeof(stcp_hdr_t))
					len = recvbuf.seg.header.length +sizeof(stcp_hdr_t) + sizeof(int);
				if(len > (int)sizeof(sendseg_arg_t))
					state = PKTSTART1;
				if(recved == len)
					state = PKTSTOP1;
				break;
			case PKTSTOP1:
				if(c == '!')
					state = PKTSTOP2;
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
			case PKTSTOP2:
				if(c == '#'){
					state = PKTSTART1;
					*dest_nodeID = recvbuf.nodeID;
					memcpy(segPtr, &(recvbuf.seg), len - sizeof(int));
					return 1;
				}
				else{
					state = PKTSTART1;
					return -1;
				}
				break;
		}
	}

}
