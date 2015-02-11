#include <stdio.h>
#include <stdlib.h>
#include "clientserver.h"

struct node {
	int value;
	char filename[512];
	char path[512];
	struct node *next;
};

struct node *head;
struct node *tail;

init(void) {
	head=(struct node *) malloc(sizeof *head);
	memset(head,0,sizeof *head);
	tail=(struct node *) malloc(sizeof *head);
	memset(tail,0,sizeof *tail);
	head->next = tail; 
	tail->next = tail;
	tail->value=-1;
}

bool empty(){
	bool retval = true;
	if(head->value <0)
	{
		retval = false;
	}
	return retval;
}

struct node *enqueue(int v) {
	struct node *ptr;
	struct node *t;
	ptr=head;
	while (ptr->next != tail) {
		ptr = ptr->next;
	}
	t=(struct node *) malloc(sizeof *t);
	t->value=v;
	t->next = tail;
	ptr->next = t;
	return ptr;
}

struct node *insert(int v, struct node *ptr) {
	struct node *t;
	t=(struct node *) malloc(sizeof *t);
	t->value=v;
	t->next = tail;
	ptr->next = t;
	return ptr;
}	

int dequeue(struct node *ptr) {
	struct node *t;
	int retval = 0;
	t = ptr->next;
	retval = t->value;
	ptr->next = ptr->next->next;
	free(t);
	return retval;
}