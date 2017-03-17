
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "nbrcosttable.h"
#include "../common/constants.h"
#include "../topology/topology.h"
#include <sys/time.h>
#include "dvtable.h"
#include "routingtable.h"

//这个函数动态创建邻居代价表并使用邻居节点ID和直接链路代价初始化该表.
//邻居的节点ID和直接链路代价提取自文件topology.dat. 
nbr_cost_entry_t* nbrcosttable_create()
{
	int nbcount = topology_getNbrNum();
	int *nbs = topology_getNbrArray();
	int *costs = topology_getCostArray();
	if(nbcount <= 0)
		return NULL;
	nbr_cost_entry_t *costentrytable = malloc(nbcount * sizeof(nbr_cost_entry_t));
	int i = 0;
	for(i = 0; i < nbcount; i ++){
		costentrytable[i].nodeID = nbs[i];
		costentrytable[i].cost = costs[i];
		gettimeofday(&(costentrytable[i].lasttime), NULL);
	}
	printf("[nbrcosttable_create>>]nbr cost table created\n");
	return costentrytable;
}

//这个函数删除邻居代价表.
//它释放所有用于邻居代价表的动态分配内存.
void nbrcosttable_destroy(nbr_cost_entry_t* nct)
{
	if(nct == NULL)
		return;
	free(nct);
}

//这个函数用于获取邻居的直接链路代价.
//如果邻居节点在表中发现,就返回直接链路代价.否则返回INFINITE_COST.
unsigned int nbrcosttable_getcost(nbr_cost_entry_t* nct, int nodeID)
{
	int i = 0;
	int len = topology_getNbrNum();
	for(i = 0; i < len; i ++){
		if(nct[i].nodeID == (unsigned int)nodeID)
			return nct[i].cost;
	}
	return INFINITE_COST;
}
int nbrcosttable_setcost(nbr_cost_entry_t *nct, int nodeID, int cost){
	int i = 0;
	int len = topology_getNbrNum();
	for(i = 0; i < len; i ++){
		if(nct[i].nodeID == (unsigned int)nodeID){
			nct[i].cost = cost;
			gettimeofday(&(nct[i].lasttime), NULL);
			return 1;
		}
	}
	return -1;
}

void upgradenbrcost(nbr_cost_entry_t *nct, dv_t * dv, routingtable_t *rttable){
	struct timeval now;
	gettimeofday(&now, NULL);
	int i = 0;
	int len = topology_getNbrNum();
	int allc = topology_getNodeNum();
	int myid = topology_getMyNodeID();
	for(i = 0; i < len; i ++){
		if(timesub(&now, &(nct[i].lasttime)) > 5){
			if(nct[i].cost < INFINITE_COST){
				//更新dv_table
				int k = 0;
				for(k = 0; k < allc; k ++){
					int destnodeid = dv[len].dvEntry[k].nodeID;
					unsigned int forknext = routingtable_getnextnode(rttable, destnodeid);
					if(forknext == nct[i].nodeID)
						dvtable_setcost(dv, myid, destnodeid, INFINITE_COST);
				}
			
			}
			nct[i].cost = INFINITE_COST;
			gettimeofday(&(nct[i].lasttime), NULL);
		}
	}
	
}
//这个函数打印邻居代价表的内容.
void nbrcosttable_print(nbr_cost_entry_t* nct)
{
	if(nct == NULL){
		printf("nbr cost table is NULL\n");
		return;
	}
	int i = 0;
	int len = topology_getNbrNum();
	for(i = 0; i < len; i ++){
		printf("destID:%d   cost:%d\n", nct[i].nodeID, nct[i].cost);
	}
}

int timesub(struct timeval *t1, struct timeval *t2){
	int sec = t1->tv_sec - t2->tv_sec;
	sec = sec < 0? -sec:sec;

	return sec;
}

