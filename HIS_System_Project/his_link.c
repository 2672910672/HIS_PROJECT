#include "his.h"

/*
 * 通用链表操作模块
 *   InitList / InsertNode / DeleteNode / FindNode
 *   TraverseList / FreeList
 *   所有模块共用此链表演，data 域为 void* 支持多类型
 */
LinkList* InitList() {
    LinkList* list = (LinkList*)malloc(sizeof(LinkList));
    if (!list) return NULL;
    list->head = NULL;
    list->length = 0;
    return list;
}

/* 插入节点 (index=0头插, -1尾插) */
int InsertNode(LinkList* list, int index, void* data, int data_size, const char* id) {
    if (!list || !data || !id) return -1;
    if (index < -1 || index > list->length) return -1;

    ListNode* new_node = (ListNode*)malloc(sizeof(ListNode));
    if (!new_node) return -1;

    new_node->data = malloc(data_size);
    if (!new_node->data) { free(new_node); return -1; }

    memcpy(new_node->data, data, data_size);
    new_node->data_size = data_size;
    strncpy(new_node->id, id, MAX_ID_LEN - 1);
    new_node->id[MAX_ID_LEN - 1] = '\0';
    new_node->next = NULL;

    if (index == 0 || list->length == 0) {
        new_node->next = list->head;
        list->head = new_node;
    }
    else if (index == -1) {
        ListNode* p = list->head;
        while (p->next) p = p->next;
        p->next = new_node;
    }
    else {
        ListNode* p = list->head;
        for (int i = 0; i < index - 1; i++) p = p->next;
        new_node->next = p->next;
        p->next = new_node;
    }
    list->length++;
    return 0;
}

/* 按ID删除节点 */
int DeleteNode(LinkList* list, const char* id) {
    if (!list || !list->head) return -1;

    ListNode* p = list->head;
    ListNode* prev = NULL;

    while (p != NULL) {
        if (strcmp(p->id, id) == 0) {
            if (prev == NULL) {
                list->head = p->next;
            }
            else {
                prev->next = p->next;
            }
            free(p->data);
            free(p);
            list->length--;
            return 0;
        }
        prev = p;
        p = p->next;
    }
    return -1;
}

/* 按ID查找节点 */
ListNode* FindNode(LinkList* list, const char* id) {
    ListNode* p = list->head;
    while (p != NULL) {
        if (strcmp(p->id, id) == 0) {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

/* 遍历链表，对每个节点调用 print_func */
void TraverseList(LinkList* list, void (*print_func)(void*)) {
    if (!list || !print_func) return;
    ListNode* p = list->head;
    while (p) {
        print_func(p->data);
        p = p->next;
    }
}

/* 释放链表所有节点 */
void FreeList(LinkList* list) {
    if (!list) return;
    ListNode* p = list->head, * tmp;
    while (p) {
        tmp = p;
        p = p->next;
        free(tmp->data);
        free(tmp);
    }
    free(list);
}

