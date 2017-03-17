//�ļ���: server/app_simple_server.c

//����: ���Ǽ򵥰汾�ķ�������������. ����������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص�������. Ȼ��������stcp_server_init()��ʼ��STCP������. 
//��ͨ�����ε���stcp_server_sock()��stcp_server_accept()����2���׽��ֲ��ȴ����Կͻ��˵�����. ������Ȼ�����������������ӵĿͻ��˷��͵Ķ��ַ���. 
//����, ������ͨ������stcp_server_close()�ر��׽���. �ص�������ͨ������son_stop()ֹͣ.

//��������: 2015��

//����: ��

//����: STCP������״̬

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#include "../common/constants.h"
#include "stcp_server.h"

//������������, һ��ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. ��һ��ʹ�ÿͻ��˶˿ں�89�ͷ������˿ں�90.
#define CLIENTPORT1 87
#define SERVERPORT1 88
#define CLIENTPORT2 89
#define SERVERPORT2 90
//�ڽ��յ��ַ�����, �ȴ�10��, Ȼ���ر�����.
#define WAITTIME 10
#define SERVER "127.0.0.1"
//��������ͨ���ڿͻ��ͷ�����֮�䴴��TCP�����������ص�������. ������TCP�׽���������, STCP��ʹ�ø����������Ͷ�. ����TCP����ʧ��, ����-1.
int connectToSIP() 
{
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

//��������ͨ���رտͻ��ͷ�����֮����TCP������ֹͣ�ص�������
void disconnectToSIP(int sip_conn)
{
	close(sip_conn);
}
int main() {
	//ÓÃÓÚ¶ª°üÂÊµÄËæ»úÊýÖÖ×Ó
	srand(time(NULL));

	//Á¬œÓµœSIPœø³Ì²¢»ñµÃTCPÌ×œÓ×ÖÃèÊö·û
	int sip_conn = connectToSIP();
	if(sip_conn<0) {
		printf("can not connect to the local SIP process\n");
	}

	//³õÊŒ»¯STCP·þÎñÆ÷
	stcp_server_init(sip_conn);

	//ÔÚ¶Ë¿ÚSERVERPORT1ÉÏŽŽœšSTCP·þÎñÆ÷Ì×œÓ×Ö 
	int sockfd= stcp_server_sock(SERVERPORT1);
	if(sockfd<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//ŒàÌý²¢œÓÊÜÀŽ×ÔSTCP¿Í»§¶ËµÄÁ¬œÓ 
	stcp_server_accept(sockfd);

	//ÔÚ¶Ë¿ÚSERVERPORT2ÉÏŽŽœšÁíÒ»žöSTCP·þÎñÆ÷Ì×œÓ×Ö
	int sockfd2= stcp_server_sock(SERVERPORT2);
	if(sockfd2<0) {
		printf("can't create stcp server\n");
		exit(1);
	}
	//ŒàÌý²¢œÓÊÜÀŽ×ÔSTCP¿Í»§¶ËµÄÁ¬œÓ 
	stcp_server_accept(sockfd2);

	char buf1[6];
	char buf2[7];
	int i;
	//œÓÊÕÀŽ×ÔµÚÒ»žöÁ¬œÓµÄ×Ö·ûŽ®
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd,buf1,6);
		printf("recv string: %s from connection 1\n",buf1);
	}
	//œÓÊÕÀŽ×ÔµÚ¶þžöÁ¬œÓµÄ×Ö·ûŽ®
	for(i=0;i<5;i++) {
		stcp_server_recv(sockfd2,buf2,7);
		printf("recv string: %s from connection 2\n",buf2);
	}

	sleep(WAITTIME);

	//¹Ø±ÕSTCP·þÎñÆ÷ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				
	if(stcp_server_close(sockfd2)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//¶Ï¿ªÓëSIPœø³ÌÖ®ŒäµÄÁ¬œÓ
	disconnectToSIP(sip_conn);
}
