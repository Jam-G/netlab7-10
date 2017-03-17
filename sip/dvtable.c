
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "dvtable.h"

//这个函数动态创建距离矢量表.
//距离矢量表包含n+1个条目, 其中n是这个节点的邻居数,剩下1个是这个节点本身.
//距离矢量表中的每个条目是一个dv_t结构,它包含一个源节点ID和一个有N个dv_entry_t结构的数组, 其中N是重叠网络中节点总数.
//每个dv_entry_t包含一个目的节点地址和从该源节点到该目的节点的链路代价.
//距离矢量表也在这个函数中初始化.从这个节点到其邻居的链路代价使用提取自topology.dat文件中的直接链路代价初始化.
//其他链路代价被初始化为INFINITE_COST.
//该函数返回动态创建的距离矢量表.
dv_t* dvtable_create()
{
	int nbcount = topology_getNbrNum();
	int *nbs = topology_getNbrArray();
//	int *costs = topology_getCostArray();
	int allnodeCount = topology_getNodeNum();
	int *allnodes = topology_getNodeArray();
	dv_t *dv_table = malloc((nbcount + 1) * sizeof(dv_t));
	printf("[dvtable_create>>]nbcount:%d, allcount:%d\n", nbcount, allnodeCount);
	if(dv_table == NULL){
		printf("malloc error\n");
		exit(-1);
	}
	int i = 0;
	for(i = 0; i < nbcount; i ++){
		dv_table[i].nodeID = nbs[i];
		dv_table[i].dvEntry = malloc(allnodeCount * sizeof(dv_entry_t));
		int j = 0;
		for(j = 0; j < allnodeCount; j ++){
			dv_table[i].dvEntry[j].nodeID = allnodes[j];
			dv_table[i].dvEntry[j].cost = INFINITE_COST;
		}
	}
	printf("[dvtable_create>>]nbrs dv initialzed\n");
	dv_table[nbcount].nodeID = topology_getMyNodeID();
	dv_table[nbcount].dvEntry = malloc(allnodeCount * sizeof(dv_entry_t));
	for(i = 0; i < allnodeCount; i ++){
		dv_table[nbcount].dvEntry[i].nodeID = allnodes[i];
		//if self-to-self topology_getCost return 0
		dv_table[nbcount].dvEntry[i].cost = topology_getCost(dv_table[nbcount].nodeID, allnodes[i]);
	}
	printf("[dvtable_create>>]dvtable created\n");
	return dv_table;
}

//这个函数删除距离矢量表.
//它释放所有为距离矢量表动态分配的内存.
void dvtable_destroy(dv_t* dvtable)
{
	if(dvtable == NULL)
		return;
	int i = 0;
	int len = topology_getNbrNum();
	for(i = 0; i <= len; i ++){
		free(dvtable[i].dvEntry);
	}
	free(dvtable);
}

//这个函数设置距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,并且链路代价也被成功设置了,就返回1,否则返回-1.
int dvtable_setcost(dv_t* dvtable,int fromNodeID,int toNodeID, unsigned int cost)
{
	int i = 0;
	int len = topology_getNbrNum() + 1;
	for(i = 0; i < len; i ++){
		int j = 0; 
		int all = topology_getNodeNum();
		if(dvtable[i].nodeID == fromNodeID){
			for(j = 0; j < all; j ++){
				if(dvtable[i].dvEntry[j].nodeID == toNodeID){
					dvtable[i].dvEntry[j].cost = cost;
					return 1;
				}
			}
			return -1;
		}
	}
	return -1;
}

//这个函数返回距离矢量表中2个节点之间的链路代价.
//如果这2个节点在表中发现了,就返回链路代价,否则返回INFINITE_COST.
unsigned int dvtable_getcost(dv_t* dvtable, int fromNodeID, int toNodeID)
{
	int i = 0;
	int len = topology_getNbrNum() + 1;
	for(i = 0; i < len; i ++){
		int j = 0; 
		int all = topology_getNodeNum();
		if(dvtable[i].nodeID == fromNodeID){
			for(j = 0; j < all; j ++){
				if(dvtable[i].dvEntry[j].nodeID == toNodeID){
					return dvtable[i].dvEntry[j].cost;
				}	
			}
			return INFINITE_COST;
		}
	}
	return INFINITE_COST;
}

//这个函数打印距离矢量表的内容.
void dvtable_print(dv_t* dvtable)
{
	int i = 0; 
	int j = 0;
	int tablelen = topology_getNbrNum() + 1;
	int entrylen = topology_getNodeNum();
	printf("route table is:\n");
	for(i = 0; i < tablelen; i ++){
		for(j = 0; j < entrylen; j ++){
			printf("from:%d ====>to:%d  cost: %d\n", dvtable[i].nodeID, dvtable[i].dvEntry[j].nodeID, dvtable[i].dvEntry[j].cost);
		}
	}
}
