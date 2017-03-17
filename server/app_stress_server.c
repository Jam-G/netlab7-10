//�ļ���: server/app_stress_server.c

//����: ����ѹ�����԰汾�ķ�������������. ����������ͨ���ڿͻ��˺ͷ�����֮�䴴��TCP����,�����ص�������. 
//Ȼ��������stcp_server_init()��ʼ��STCP������. ��ͨ������stcp_server_sock()��stcp_server_accept()����һ���׽��ֲ��ȴ����Կͻ��˵�����.
//��Ȼ�������ļ�����. ����֮��, ������һ�򻺳���, �����ļ����ݲ��������浽receivedtext.txt�ļ���. 
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

//����һ������, ʹ�ÿͻ��˶˿ں�87�ͷ������˿ں�88. 
#define CLIENTPORT1 87
#define SERVERPORT1 88
//�ڽ��յ��ļ����ݱ�������, �������ȴ�10��, Ȼ���ر�����.
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

	//Ê×ÏÈœÓÊÕÎÄŒþ³€¶È, È»ºóœÓÊÕÎÄŒþÊýŸÝ
	int fileLen;
	stcp_server_recv(sockfd,&fileLen,sizeof(int));
	char* buf = (char*) malloc(fileLen);
	stcp_server_recv(sockfd,buf,fileLen);

	//œ«œÓÊÕµœµÄÎÄŒþÊýŸÝ±£ŽæµœÎÄŒþreceivedtext.txtÖÐ
	FILE* f;
	f = fopen("receivedtext.txt","a");
	fwrite(buf,fileLen,1,f);
	fclose(f);
	free(buf);

	//µÈŽýÒ»»á¶ù
	sleep(WAITTIME);

	//¹Ø±ÕSTCP·þÎñÆ÷ 
	if(stcp_server_close(sockfd)<0) {
		printf("can't destroy stcp server\n");
		exit(1);
	}				

	//¶Ï¿ªÓëSIPœø³ÌÖ®ŒäµÄÁ¬œÓ
	disconnectToSIP(sip_conn);
}
