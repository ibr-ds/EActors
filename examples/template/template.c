/**
\file
\brief A template
*/
#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);


int test(struct actor_s *self) {
	printa("hello world\n");

	return 1;
}

int test_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	return 0;
}
