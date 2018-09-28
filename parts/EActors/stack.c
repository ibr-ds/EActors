/**
\file
\brief communication primitives

\deprecated
\note CAS-based atomic stac will be removed and should not be used. HLE is better
*/


#include "stack.h"

/**
\param[in] *s a stack pointer
\brief init a top of a pool
*/
void stack_init(stack *s) {
	s->top=NULL;
}

/**
\brief push a node into an empty stack
\param[in] *s a stack pointer
\param[in] *n a node pointer
\return 1 if a stack is not free
\return 0 on success
\deprecated In fact this functions does not work, since a stack can be filled by someone between check of the head. Here we need to use a DCAS. please use push instead
*/
int spush_one(stack *s, node *n) {
	while(1) {
		if(s->top!=NULL)
			return 1;

		node *old_top = s->top;
		n->header.next = old_top;
		if(__sync_val_compare_and_swap(&s->top, old_top, n) == old_top)
			return 0;
	}
}

/**
\brief push a node into a stack
\param[in] *s a stack pointer
\param[in] *n a node pointer
Cant fail
*/
void spush(stack *s, node *n) {
	while(1) {
		node *old_top = s->top;
		n->header.next = old_top;
		if(__sync_val_compare_and_swap(&s->top, old_top, n) == old_top)
			return;
	}
}

/**
\brief push a node into a stack
\param[in] *s a stack pointer
\return NULL if a queue is empty
\return *node on success
*/
node *spop(stack *s) {
	while(1) {
		node *old_top = s->top;
		if(old_top == NULL)
			return NULL;

		node *new_top = old_top->header.next;
		if(__sync_val_compare_and_swap(&s->top, old_top, new_top) == old_top)
			return old_top;
	}
}

