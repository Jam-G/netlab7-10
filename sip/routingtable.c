
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../common/constants.h"
#include "../topology/topology.h"
#include "routingtable.h"
#include<assert.h>
//makehash()是由路由表使用的哈希函数.
//它将输入的目的节点ID作为哈希键,并返回针对这个目的节点ID的槽号作为哈希值.
int makehash(int node)
{
	return node % MAX_ROUTINGTABLE_SLOTS;
}

//这个函数动态创建路由表.表中的所有条目都被初始化为NULL指针.
//然后对有直接链路的邻居,使用邻居本身作为下一跳节点创建路由条目,并插入到路由表中.
//该函数返回动态创建的路由表结构.
routingtable_t* routingtable_create()
{
	routingtable_t *routingtable = (routingtable_t *)malloc(sizeof(routingtable_t));
	if(routingtable == NULL){
		printf("routingtable create error \n");
		exit(-1);
	}
	memset(routingtable, 0, sizeof(routingtable_t));
	printf("[routingtable_create>>]routingtable createing and set NULL\n");
	int *nbs = topology_getNbrArray();
	int nbcount = topology_getNbrNum();
	int i = 0;
	int index = 0;
	for(i = 0; i < nbcount; i ++){
		routingtable_entry_t *entry = (routingtable_entry_t *)malloc(sizeof(routingtable_entry_t));
		entry->destNodeID = nbs[i];
		entry->nextNodeID = nbs[i];
		entry->next = NULL;
		index = makehash(entry->destNodeID);
		if(routingtable->hash[index] == NULL){
			routingtable->hash[index] = entry;
		}else{
			entry->next = routingtable->hash[index]->next;
			routingtable->hash[index]->next = entry;
		}	
	}
	printf("[routingtable_create>>]routingtable created and initailzed \n");
	return routingtable;
}

//这个函数删除路由表.
//所有为路由表动态分配的数据结构将被释放.
void routingtable_destroy(routingtable_t* routingtable)
{
	int i = 0;
	for(i = 0; i < MAX_ROUTINGTABLE_SLOTS; i ++){
		routingtable_entry_t * it = routingtable->hash[i];
		while(it != NULL){
			routingtable_entry_t * temp = it->next;
			free(it);
			it = temp;
		}
		routingtable->hash[i] = NULL;
	}
	free(routingtable);
}

//这个函数使用给定的目的节点ID和下一跳节点ID更新路由表.
//如果给定目的节点的路由条目已经存在, 就更新已存在的路由条目.如果不存在, 就添加一条.
//路由表中的每个槽包含一个路由条目链表, 这是因为可能有冲突的哈希值存在(不同的哈希键, 即目的节点ID不同, 可能有相同的哈希值, 即槽号相同).
//为在哈希表中添加一个路由条目:
//首先使用哈希函数makehash()获得这个路由条目应被保存的槽号.
//然后将路由条目附加到该槽的链表中.
void routingtable_setnextnode(routingtable_t* routingtable, int destNodeID, int nextNodeID)
{
	int index = makehash(destNodeID);
	routingtable_entry_t * newentry = (routingtable_entry_t *)malloc(sizeof(routingtable_entry_t));
	newentry->next = NULL;
	newentry->destNodeID = destNodeID;
	newentry->nextNodeID = nextNodeID;
	if(routingtable->hash[index] == NULL){
		printf("new entry dest :%d~\n", destNodeID);
		routingtable->hash[index] = newentry;
		return;
	}
	routingtable_entry_t * insert = routingtable->hash[index];
	while(insert != NULL){
		if(insert->destNodeID == destNodeID){
			printf("change next node id from %d to %d\n", insert->destNodeID, nextNodeID);
			insert->nextNodeID = nextNodeID;
			free(newentry);
			newentry = NULL;
			return;
		}
		insert = insert->next;
	}
	assert(insert == NULL);
	assert(newentry != NULL);
	//insert to haed
	newentry->next = routingtable->hash[index]->next;
	routingtable->hash[index]->next = newentry;
}

//这个函数在路由表中查找指定的目标节点ID.
//为找到一个目的节点的路由条目, 你应该首先使用哈希函数makehash()获得槽号,
//然后遍历该槽中的链表以搜索路由条目.如果发现destNodeID, 就返回针对这个目的节点的下一跳节点ID, 否则返回-1.
int routingtable_getnextnode(routingtable_t* routingtable, int destNodeID)
{
	int index = makehash(destNodeID);
	routingtable_entry_t * want = routingtable->hash[index];
	while(want != NULL){
		if(want->destNodeID == destNodeID)
			return want->nextNodeID;
		want = want->next;
	}
	return -1;
}

//这个函数打印路由表的内容
void routingtable_print(routingtable_t* routingtable)
{
	printf("_________routingtable is asbelow_______:\n");
	int i = 0;
	routingtable_entry_t * todis;
	for(i = 0; i < MAX_ROUTINGTABLE_SLOTS; i ++){
		todis = routingtable->hash[i];
		while(todis != NULL){
			printf("dest:%d-------next:%d\n", todis->destNodeID, todis->nextNodeID);
			todis = todis->next;
		}
	}
}
