//�ļ���: sip/sip.h
//
//����: �����ļ���������SIP���̵����ݽṹ�ͺ���  
//
//��������: 2015��

#ifndef NETWORK_H
#define NETWORK_H

//SIP����ʹ�������������ӵ�����SON���̵Ķ˿�SON_PORT
//�ɹ�ʱ��������������, ���򷵻�-1
int connectToSON();

//�����߳�ÿ��ROUTEUPDATE_INTERVALʱ���ͷ���һ��·�ɸ��±���
//�ڱ�ʵ����, �����߳�ֻ�㲥�յ�·�ɸ��±��ĸ������ھ�, 
//����ͨ������SIP�����ײ��е�dest_nodeIDΪBROADCAST_NODEID�����͹㲥
void* routeupdate_daemon(void* arg);

//�����̴߳�������SON���̵Ľ��뱨��
//��ͨ������son_recvpkt()��������SON���̵ı���
//�ڱ�ʵ����, �����߳��ڽ��յ����ĺ�, ֻ����ʾ�����ѽ��յ�����Ϣ, ����������
void* pkthandler(void* arg); 

//����������ֹSIP����, ��SIP�����յ��ź�SIGINTʱ�������������� 
//���ر���������, �ͷ����ж�̬�������ڴ�
void sip_stop();
//这个函数打开端口SIP_PORT并等待来自本地STCP进程的TCP连接.
//在连接建立后, 这个函数从STCP进程处持续接收包含段及其目的节点ID的sendseg_arg_t. 
//接收的段被封装进数据报(一个段在一个数据报中), 然后使用son_sendpkt发送该报文到下一跳. 下一跳节点ID提取自路由表.
//当本地STCP进程断开连接时, 这个函数等待下一个STCP进程的连接.
void waitSTCP();
#endif
