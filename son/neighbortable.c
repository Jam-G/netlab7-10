//文件名: son/neighbortable.c
//
//描述: 这个文件实现用于邻居表的API
//
//创建日期: 2015年

#include "neighbortable.h"
#include<malloc.h>
#include"../common/constants.h"
#include"../topology/topology.h"

//这个函数首先动态创建一个邻居表. 然后解析文件topology/topology.dat, 填充所有条目中的nodeID和nodeIP字段, 将conn字段初始化为-1.
//返回创建的邻居表.
nbr_entry_t* nt_create()
{
	nbr_entry_t * nbr_table = (nbr_entry_t *)malloc(sizeof(nbr_entry_t) * MAX_NODE_NUM);
	int * arr = topology_getNbrArray();   //获得所有邻居节点id
	in_addr_t * iparr = topology_getNbrIpArray();
	//struct hostent *ho = gethostbyname(hostname);
	int length = topology_getNbrNum();
	int i = 0;
	for(;i < length;i++)
	{
		nbr_table[i].nodeID = arr[i];
		nbr_table[i].nodeIP = iparr[i];
		nbr_table[i].conn = -1;
	}
	printf("neighbour table initial successful, nbr num is %d\n", length);
	return nbr_table;
}

//这个函数删除一个邻居表. 它关闭所有连接, 释放所有动态分配的内存.
void nt_destroy(nbr_entry_t* nt)
{
	int i = 0;
	int num = topology_getNbrNum();
	for(i = 0; i < num; i++){
		if((nt[i].conn) >= 0)
		{
			close(nt[i].conn);
			nt[i].conn = -1;
		}
	}
	free(nt);
	printf("free the neighbour table\n");
	return;
}

//这个函数为邻居表中指定的邻居节点条目分配一个TCP连接. 如果分配成功, 返回1, 否则返回-1.
int nt_addconn(nbr_entry_t* nt, int nodeID, int conn)
{
	int i = 0;
	int flag = 0;
	int len = topology_getNbrNum();
	for(i = 0; i < len; i ++){
		if(nt[i].nodeID == nodeID)
		{
			nt[i].conn = conn;
			flag = 1;
			break;
		}
	}
	if(flag == 0)
		{
			printf("tcp for neighbour failed");
			return -1;
		}
	else
		{
			printf("tcp for neighbour successful");
			return 1;
		}
	return 0;
}
