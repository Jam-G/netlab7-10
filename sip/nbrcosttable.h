//文件名: sip/nbrcosttable.h
//
//描述: 这个文件定义用于邻居代价表的数据结构和函数.  
//
//创建日期: 2015年

#ifndef NBRCOSTTABLE_H 
#define NBRCOSTTABLE_H 
#include <arpa/inet.h>
#include<sys/time.h>
#include"dvtable.h"
#include"routingtable.h"
//邻居代价表条目定义
typedef struct neighborcostentry {
	unsigned int nodeID;	//邻居的节点ID
	unsigned int cost;	    //到该邻居的直接链路代价
	struct timeval lasttime;//last update time
} nbr_cost_entry_t;

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat.
nbr_cost_entry_t* nbrcosttable_create();

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct);

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID);
void upgradenbrcost(nbr_cost_entry_t *nct, dv_t *dv, routingtable_t *rttable);
//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct); 
int nbrcosttable_setcost(nbr_cost_entry_t *nct, int nodeID, int cost);
int timesub(struct timeval *t1, struct timeval *t2);
#endif
