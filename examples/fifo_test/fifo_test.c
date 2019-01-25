/**
\file
\brief A template
*/
#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);


struct fifo_pl {
	int n;
};

#define DEPTH	3

int test(struct actor_s *self) {
	queue test;
	node *tmp;
	int i;

	struct fifo_pl *pl;

	queue_init(&test);

	for(i = 0; i < DEPTH; i++) {
		tmp = (node *) malloc(sizeof(node));
		pl = (struct fifo_pl *) tmp->payload;
		pl->n = i+1;
		push_back(&test, tmp);
	}

	printa("-- init done -- \n");

	for(i = 0; i < 10; i++) {
		tmp = pop_front(&test);
		pl = (struct fifo_pl *) tmp->payload;
		printa("pl->n = %d\n", pl->n);
		push_back(&test, tmp);
	}

	printa("--  fifo 2 -- \n");

	for(i = 0; i < 10; i++) {
		tmp = pop_back(&test);
		pl = (struct fifo_pl *) tmp->payload;
		printa("pl->n = %d\n", pl->n);
		push_front(&test, tmp);
	}

	return 0;
}

int test_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	return 0;
}