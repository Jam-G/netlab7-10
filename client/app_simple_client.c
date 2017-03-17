//ÎÄŒþÃû: client/app_simple_client.c
//
//ÃèÊö: ÕâÊÇŒòµ¥°æ±ŸµÄ¿Í»§¶Ë³ÌÐòŽúÂë. ¿Í»§¶ËÊ×ÏÈÍš¹ýÔÚ¿Í»§¶ËºÍ·þÎñÆ÷Ö®ŒäŽŽœšTCPÁ¬œÓ,Æô¶¯ÖØµþÍøÂç²ã. 
//È»ºóËüµ÷ÓÃstcp_client_init()³õÊŒ»¯STCP¿Í»§¶Ë. ËüÍš¹ýÁœŽÎµ÷ÓÃstcp_client_sock()ºÍstcp_client_connect()ŽŽœšÁœžöÌ×œÓ×Ö²¢Á¬œÓµœ·þÎñÆ÷.
//ËüÈ»ºóÍš¹ýÕâÁœžöÁ¬œÓ·¢ËÍÒ»¶Î¶ÌµÄ×Ö·ûŽ®žø·þÎñÆ÷. Ÿ­¹ýÒ»¶ÎÊ±ºòºó, ¿Í»§¶Ëµ÷ÓÃstcp_client_disconnect()¶Ï¿ªµœ·þÎñÆ÷µÄÁ¬œÓ.
//×îºó,¿Í»§¶Ëµ÷ÓÃstcp_client_close()¹Ø±ÕÌ×œÓ×Ö. ÖØµþÍøÂç²ãÍš¹ýµ÷ÓÃson_stop()Í£Ö¹.

//ŽŽœšÈÕÆÚ: 2015Äê

//ÊäÈë: ÎÞ

//Êä³ö: STCP¿Í»§¶Ë×ŽÌ¬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "../common/constants.h"
#include "stcp_client.h"

//ŽŽœšÁœžöÁ¬œÓ, Ò»žöÊ¹ÓÃ¿Í»§¶Ë¶Ë¿ÚºÅ87ºÍ·þÎñÆ÷¶Ë¿ÚºÅ88. ÁíÒ»žöÊ¹ÓÃ¿Í»§¶Ë¶Ë¿ÚºÅ89ºÍ·þÎñÆ÷¶Ë¿ÚºÅ90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90

#define STARTDELAY 1
//ÔÚ·¢ËÍ×Ö·ûŽ®ºó, µÈŽý5Ãë, È»ºó¹Ø±ÕÁ¬œÓ
#define WAITTIME 5

#define SERVER "127.0.0.1"

//这个函数通过在客户和服务器之间创建TCP连接来启动重叠网络层. 它返回TCP套接字描述符, STCP将使用该描述符发送段. 如果TCP连接失败, 返回-1. 
int connectToSIP() {
	int son_so = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in servaddr;
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family  = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(SERVER);
	servaddr.sin_port = htons(SIP_PORT);
	if(connect(son_so, (struct sockaddr *)&servaddr, sizeof(servaddr)))//if success return 0
		return -1;
	printf("[connectToSIP>>] sockfd:%d\n", son_so);	
	return son_so;
}

//这个函数通过关闭客户和服务器之间的TCP连接来停止重叠网络层
void disconnectToSIP(int sip_conn) {
	close(sip_conn);
}
int main() {
	//用于丢包率的随机数种子
	srand(time(NULL));

	//连接到SIP进程并获得TCP套接字描述符	
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("fail to connect to the local SIP process\n");
		exit(1);
	}

	//初始化stcp客户端
	stcp_client_init(sip_conn);
	sleep(STARTDELAY);

	char hostname[50];
	printf("Enter server name to connect:");
	scanf("%s",hostname);
	int server_nodeID = topology_getNodeIDfromname(hostname);
	if(server_nodeID == -1) {
		printf("host name error!\n");
		exit(1);
	} else {
		printf("connecting to node %d\n",server_nodeID);
	}

	//在端口87上创建STCP客户端套接字, 并连接到STCP服务器端口88
	int sockfd = stcp_client_sock(CLIENTPORT1);
	if(sockfd<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd,server_nodeID,SERVERPORT1)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT1,SERVERPORT1);
	
	//在端口89上创建STCP客户端套接字, 并连接到STCP服务器端口90
	int sockfd2 = stcp_client_sock(CLIENTPORT2);
	if(sockfd2<0) {
		printf("fail to create stcp client sock");
		exit(1);
	}
	if(stcp_client_connect(sockfd2,server_nodeID,SERVERPORT2)<0) {
		printf("fail to connect to stcp server\n");
		exit(1);
	}
	printf("client connected to server, client port:%d, server port %d\n",CLIENTPORT2, SERVERPORT2);

	//通过第一个连接发送字符串
    char mydata[6] = "hello";
	int i;
	for(i=0;i<5;i++){
      	stcp_client_send(sockfd, mydata, 6);
		printf("send string:%s to connection 1\n",mydata);	
      	}
	//通过第二个连接发送字符串
    char mydata2[7] = "byebye";
	for(i=0;i<5;i++){
      	stcp_client_send(sockfd2, mydata2, 7);
		printf("send string:%s to connection 2\n",mydata2);	
      	}

	//等待一段时间, 然后关闭连接
	sleep(WAITTIME);

	if(stcp_client_disconnect(sockfd)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}
	
	if(stcp_client_disconnect(sockfd2)<0) {
		printf("fail to disconnect from stcp server\n");
		exit(1);
	}
	if(stcp_client_close(sockfd2)<0) {
		printf("fail to close stcp client\n");
		exit(1);
	}

	//断开与SIP进程之间的连接
	disconnectToSIP(sip_conn);
}
