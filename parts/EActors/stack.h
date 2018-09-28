/**
\file
\brief communication primitives

\deprecated
\note CAS-based atomic stac will be removed and should not be used. HLE is better
*/

#ifndef STACK_H
#define STACK_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "node.h"

struct  __attribute__((aligned(64)))   stack_s {
	node	*top;		///< a top of the stack
	node	*bottom;	//unused
	char	lock;		//unused
};

typedef struct stack_s	stack;


void stack_init(stack *s);
int spush_one(stack *s, node *n);
void spush(stack *s, node *n);
node *spop(stack *s);
void spush_bottom(stack *s, node *n);

//node *nalloc(stack *s);
//void nfree(node *n);


#ifdef __cplusplus
}
#endif


#endif