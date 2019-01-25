#ifndef NODE_H
#define NODE_H

#include "../config.h"

#include <stdio.h>
#include <sgx_tseal.h>

#ifdef __cplusplus
extern "C" {
#endif

struct __attribute__((aligned(64))) header_s {
	struct node_s *next;			///< pointer to the next
	struct node_s *prev;			///< pointer to the prev
	void *pool;				///< pool where return
	int		who;			///< developments only

	unsigned char mac[SGX_AESGCM_MAC_SIZE];	///<each node has MAC even it does not use it. Because of because.
};

#define BLOCK_SIZE	(1024*4)						///< Size of an element, should be evenly divisable by 128
#define HEADER_SIZE	(sizeof(struct header_s))
#define PL_SIZE		((BLOCK_SIZE - sizeof(struct header_s)) & 0xFFFFFFC0)	///<payload size should be rounded down to be a multiple of 64

struct __attribute__((aligned(64))) node_s {
	struct header_s header;
	char payload[PL_SIZE];
};

typedef struct node_s	node;

enum queue_etype {
	GLOBAL = 0,
	PRIVATE
};

struct __attribute__((aligned(64))) queue_s {
	node	*top;
	node	*bottom;
	char lock;				///< HLE lock
#ifdef V2
	int	id;				///< developments only
	enum queue_etype	type;		///< developments only
#endif
};

typedef struct queue_s	queue;


void queue_init(queue *q);
int how_long(queue *q);
int is_empty(queue *q);
void push_back(queue *q, node *new_node);
void push_front(queue *q, node *new_node);
node *pop_back(queue *q);
node *pop_front(queue *q);

node *nalloc(queue *q);
void nfree(node *n);

//new API

void swap_nodes(queue *one, queue *two);

#ifdef __cplusplus
}
#endif

#endif //NODE_H