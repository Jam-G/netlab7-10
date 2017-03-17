//
// 文件名: seg.c

// 描述: 这个文件包含用于发送和接收STCP段的接口sip_sendseg() and sip_rcvseg(), 及其支持函数的实现. 
//
// 创建日期: 2015年
//

#include "seg.h"
#include "stdio.h"
#include<assert.h>

//
//
//  用于客户端和服务器的SIP API 
//  =======================================
//
//  我们在下面提供了每个函数调用的原型定义和细节说明, 但这些只是指导性的, 你完全可以根据自己的想法来设计代码.
//
//  注意: sip_sendseg()和sip_recvseg()是由网络层提供的服务, 即SIP提供给STCP.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//

// 通过重叠网络(在本实验中，是一个TCP连接)发送STCP段. 因为TCP以字节流形式发送数据, 
// 为了通过重叠网络TCP连接发送STCP段, 你需要在传输STCP段时，在它的开头和结尾加上分隔符. 
// 即首先发送表明一个段开始的特殊字符"!&"; 然后发送seg_t; 最后发送表明一个段结束的特殊字符"!#".  
// 成功时返回1, 失败时返回-1. sip_sendseg()首先使用send()发送两个字符, 然后使用send()发送seg_t,
// 最后使用send()发送表明段结束的两个字符.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//
int sip_sendseg(int connection, int destnodeID,  seg_t* segPtr) {
//	printf("into sip_sendseg \n");
//	fflush(stdout);
	if(send(connection, "!&", 2, 0) < 0)
	{
	//	printf("into !&\n");
	//	fflush(stdout);
		return -1;
	}
//	printf("sip_send !&\n");
//	fflush(stdout);
	sendseg_arg_t pkt;
	pkt.nodeID = destnodeID;
	(segPtr->header).checksum = 0;
	(segPtr->header).checksum = checksum(segPtr);
	memcpy(&(pkt.seg), segPtr, segPtr->header.length + sizeof(stcp_hdr_t));
	if(send(connection, &pkt, sizeof(int) + (segPtr->header).length  + sizeof(stcp_hdr_t),0) < 0)
	{	
		printf("send seg_t error~\n");
		fflush(stdout);
		return -1;
	}
//	printf("sip_send seg_t\n");
	if(send(connection, "!#", 2, 0) < 0)
		return -1;
//	printf("sip_send !#\n");
	printf("[sip_sendseg>>]send a seg successful\n");
	return 1;
}

// 通过重叠网络(在本实验中，是一个TCP连接)接收STCP段. 我们建议你使用recv()一次接收一个字节.
// 你需要查找"!&", 然后是seg_t, 最后是"!#". 这实际上需要你实现一个搜索的FSM, 可以考虑使用如下所示的FSM.
// SEGSTART1 -- 起点 
// SEGSTART2 -- 接收到'!', 期待'&' 
// SEGRECV -- 接收到'&', 开始接收数据
// SEGSTOP1 -- 接收到'!', 期待'#'以结束数据的接收
// 这里的假设是"!&"和"!#"不会出现在段的数据部分(虽然相当受限, 但实现会简单很多).
// 你应该以字符的方式一次读取一个字节, 将数据部分拷贝到缓冲区中返回给调用者.
//
// 注意: 还有一种处理方式可以允许"!&"和"!#"出现在段首部或段的数据部分. 具体处理方式是首先确保读取到!&，然后
// 直接读取定长的STCP段首部, 不考虑其中的特殊字符, 然后按照首部中的长度读取段数据, 最后确保以!#结尾.
//
// 注意: 在你剖析了一个STCP段之后,  你需要调用seglost()来模拟网络中数据包的丢失. 
// 在sip_recvseg()的下面是seglost()的代码.
// 
// 如果段丢失了, 就返回1, 否则返回0.
//
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// 
int sip_recvseg(int connection, int *srcnodeID, seg_t* segPtr) {
	//采用解析stcp首部的方式,为了兼容数据内部出现!#
	//接收状态我们也使用FSM;
	//初始化FSM
	static seg_fsm_t state = SEGSTART1;
	char buf;//接收字符缓存
	int recvlen = 0;//记录已经接收到的长度
	int maxline = 0;
	int node = 0;
	char *contain = (char *)&node;
	while(1){//不停的接收,直到结束
		int rc = recv(connection, &buf, 1, 0);
		if(rc < 0) {
			perror("recv interrupted, will continue\n");
			continue;
		}else if(rc == 0) {
			perror("socket disconnect, will return -1, means son closed\n");
			state = SEGSTART1;
			return -1;
		}
		if(state == SEGSTART1){
			if(buf == '!')
				state = SEGSTART2;//状态转换
		}else if(state == SEGSTART2){
			if(buf == '&'){
				state = SEGRECV;
				recvlen = 0;
				contain = &node;
				maxline = sizeof(sendseg_arg_t) - sizeof(int) - sizeof(stcp_hdr_t);
			}
			else{
				state = SEGSTART1;
				return 1;//lost
			}
		}else if(state == SEGRECV){
			if(recvlen < sizeof(sendseg_arg_t))
				contain[recvlen++] = buf;
			else{
				state = SEGSTART1;
				return 1;//lost,超过缓存区大小
			}
			if(recvlen == sizeof(int)){//接收满了一个node
				*srcnodeID = node;
				printf("[STCP>>sip_recvseg]recv from node:%d\n", node);
				contain = (char *)segPtr;
				contain -= sizeof(int);
			}
			if(recvlen == sizeof(int) + sizeof(stcp_hdr_t)){
				maxline = (segPtr->header).length;
			}
			if((recvlen - sizeof(stcp_hdr_t) - sizeof(int)) == maxline){//接收完成整个数据
				state = SEGEND1;
			}
		}else if(state == SEGEND1){
			if(buf == '!')
				state = SEGEND2;
			else{
				state = SEGSTART1;
				return 1;//lost
			}
		}else if(state == SEGEND2){
			if(buf == '#'){
				state = SEGSTART1;// 完整的接收完一个数据保并找到后同步序列
				break;
			}
			else{
				state = SEGSTART1;
				return 1;//lost
			}
		}else{//error
			assert(0);//never should enter this entry
		}
	}
	if(seglost(segPtr)){//以一定的概论丢弃报文{
		return 1;
	}
	if(checkchecksum(segPtr) == -1){//check chencksum srror
		printf("check sum error, %d\n", checkchecksum(segPtr));
		return 1;
	}
	printf("recv a seg finish\n");
	return 0;
}

int seglost(seg_t* segPtr) {
	int random = rand()%100;
	if(random<PKT_LOSS_RATE*100) {
		//50%可能性丢失段
		if(rand()%2==0) {
			printf("seg lost!!!\n");
			 return 1;
		}
		//50%可能性是错误的校验和
		else {
			//获取数据长度
			int len = sizeof(stcp_hdr_t)+segPtr->header.length;
			//获取要反转的随机位
			int errorbit = rand()%(len*8);
			//反转该比特
			char* temp = (char*)segPtr;
			temp = temp + errorbit/8;
			*temp = *temp^(1<<(errorbit%8));
			return 0;
		}
	}
	return 0;
}

//这个函数计算指定段的校验和.
//校验和覆盖段首部和段数据. 你应该首先将段首部中的校验和字段清零, 
//如果数据长度为奇数, 添加一个全零的字节来计算校验和.
//校验和计算使用1的补码.
unsigned short checksum(seg_t* segment)
{
	unsigned short *buffer = (unsigned short *)segment;
	int size = segment->header.length + 24;
	unsigned long cksum = 0;
	while (size > 1) {
		cksum += *buffer++;
		size -= sizeof(unsigned short);
	}
	if (size) {
		cksum += *(unsigned char*)buffer * 256;
	}
	while(cksum >> 16) {
		cksum = (cksum & 0xffff) + (cksum >> 16);
	}
	return (unsigned short)(~cksum);
}

//这个函数检查段中的校验和, 正确时返回1, 错误时返回-1
int checkchecksum(seg_t* segment){
	unsigned short result = checksum(segment);
	printf("son :checkchecksum result:%d\n",result);
	if(result == 0) {
		return 1;
	}
	else {
		return -1;
	}
}
