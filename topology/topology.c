//文件名: topology/topology.c
//
//描述: 这个文件实现一些用于解析拓扑文件的辅助函数 
//
//创建日期: 2015年

#include "topology.h"
#include<stdlib.h>
#include<stdio.h>
#include<string.h>
#include "../common/constants.h"
//这个函数返回指定主机的节点ID.
//节点ID是节点IP地址最后8位表示的整数.
//例如, 一个节点的IP地址为202.119.32.12, 它的节点ID就是12.
//如果不能获取节点ID, 返回-1.
int *allIdList;
int allCount = 0;
int  *nbIdList;
int *costList;
in_addr_t *nbIpList;
int nbCount = 0;
int big = 0;
static int inited = 0;
int topology_getNodeIDfromname(char* hostname) 
{
	struct hostent *ho = gethostbyname(hostname);
	if(ho == NULL)
		return -1;
	return topology_getNodeIDfromip((struct in_addr *)(ho->h_addr_list[0]));
}

//这个函数返回指定的IP地址的节点ID.
//如果不能获取节点ID, 返回-1.
int topology_getNodeIDfromip(struct in_addr* addr)
{
	if(addr == NULL || addr->s_addr == 0) 
		return -1;
	unsigned int ipaddr = ntohl(addr->s_addr);
	unsigned char * p = (unsigned char *)&ipaddr;
	return *p;
}

//这个函数返回本机的节点ID
//如果不能获取本机的节点ID, 返回-1.
int topology_getMyNodeID()
{
	char myname[32];
	gethostname(myname, sizeof(myname));
	return topology_getNodeIDfromname(myname);
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回邻居数.
int topology_getNbrNum()
{
	init_topology();
	return nbCount;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回重叠网络中的总节点数.
int topology_getNodeNum()
{
	init_topology();
	return allCount;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含重叠网络中所有节点的ID. 
int* topology_getNodeArray()
{
	init_topology();
	return allIdList;
}

//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回一个动态分配的数组, 它包含所有邻居的节点ID.  
int* topology_getNbrArray()
{
	init_topology();
	return nbIdList; 
}

int* topology_getCostArray(){
	init_topology();
	return costList;
}

in_addr_t * topology_getNbrIpArray()
{
	init_topology();
	return nbIpList;
}
in_addr_t getIPfromname(char *hostname){
	struct hostent *ho = gethostbyname(hostname);
	if(ho == NULL)
		return -1;
	return ((struct in_addr *)ho->h_addr_list[0])->s_addr;
}
int getBigNum(){
	init_topology();
	return big;
}
int getSmallNum(){
	init_topology();
	return nbCount - big;
}
//这个函数解析保存在文件topology.dat中的拓扑信息.
//返回指定两个节点之间的直接链路代价. 
//如果指定两个节点之间没有直接链路, 返回INFINITE_COST.
unsigned int topology_getCost(int fromNodeID, int toNodeID)
{
	if(fromNodeID == toNodeID)
		return 0;
	FILE *tf = fopen("../topology/topology.dat", "r");
	char myname[32];
	gethostname(myname, sizeof(myname));
	char node1[32], node2[32];
	int cost;
	while(fscanf(tf, "%s %s %d", node1, node2, &cost) > 0){
		int id1 = topology_getNodeIDfromname(node1);
		int id2 = topology_getNodeIDfromname(node2);
		if((fromNodeID == id1 && toNodeID == id2) || (fromNodeID == id2 && toNodeID == id1)){
			return cost;
		}
	}
	fclose(tf);
	return INFINITE_COST;
}

void init_topology(){
	if(inited == 1)
		return;
	printf("read data\n");
	FILE *tf = fopen("../topology/topology.dat", "r");
	int line = 0;
	char buf[256];
	while(fgets(buf, sizeof(buf), tf) != NULL)
		line ++;
	printf("nb line is %d\n", line);
	if(line <= 0){
		printf("topology.dat error\n");
		exit(-1);
	}
	allIdList = malloc(line * 2 * sizeof(int));
	nbIdList = malloc(line * sizeof(int));
	nbIpList = malloc(line * sizeof(in_addr_t));
	costList = malloc(line * sizeof(int));
	char node1[32], node2[32];
	int cost;
	int myID = topology_getMyNodeID();
	fseek(tf,0,SEEK_SET);
	while(fscanf(tf, "%s %s %d", node1, node2, &cost) > 0){
		int id1 = topology_getNodeIDfromname(node1);
		int id2 = topology_getNodeIDfromname(node2);
		int i = 0;
		for(; i < allCount; i ++){
			if(id1 == allIdList[i])
				break;
		}
		if(i == allCount){
			allIdList[allCount++] = id1;
			printf("find a node %d\n", id1);
		}
		for(i = 0; i < allCount; i ++){
			if(id2 == allIdList[i])
				break;
		}
		if(i == allCount){
			allIdList[allCount++] = id2;
			printf("find a node %d\n", id2);
		}
		if(id1 == myID){
			if(id2 > id1)
				big ++;
			printf("find a nb :%d\n", id2);
			nbIpList[nbCount] = getIPfromname(node2);
			costList[nbCount] = cost;
			nbIdList[nbCount++] = id2;
		}
		if(id2 == myID){
			if(id1 > id2)
				big ++;
			printf("find a nb %d\n", id1);
			nbIpList[nbCount] = getIPfromname(node1);
			costList[nbCount] = cost;
			nbIdList[nbCount++] = id1;
		}
	}
	inited = 1;
	fclose(tf);
	return;
}
