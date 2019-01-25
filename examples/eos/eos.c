/**
\file
\brief
*/
#include <fw.h>
#include "payload.h"



extern void printa(const char *fmt, ...);

int cleaner(struct actor_s *self) {
	drop_old(self->gsp, 0);
	asleep(1000);
	return 1;
}

struct ps_struct {
	struct ctx_ctr_s *ctx;
};


#define N	1000

int test(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	int i = 0, j;

	char key[4096];
	char value[4096];
	int miss = 0;

	memset(key, 0, sizeof(key));
	memset(value, 0, sizeof(value));

	printa("PUT: insert %d elements \n", N);
	for(i = 0; i < N; i++) {
		grace_update(self->gsp, self->id);
		snprintf(key, sizeof(key), "%6d", i);
		snprintf(value, sizeof(value), "%6d", N - i);
		while(put(self->gsp, NULL, key, value) == 1) {
			grace_update(self->gsp, self->id);
			miss++;
               }
	}
	printa("write done, miss count =  %d, now read\n", miss);
	printa("GET: retrive %d elements\n", N);
	for(i = 0; i < N; i++) {
		snprintf(key, sizeof(key), "%6d", i);

		char tmp_val[100];
		memset(tmp_val, 0, sizeof(tmp_val));
		snprintf(value, sizeof(value), "%6d", N - i);
		if(get(self->gsp, NULL, key, tmp_val) == 1) {
			printa("KV does not exist but should\n");while(1);
		}
		if(strcmp(tmp_val, value) != 0) {
			printa("got '%s', should be '%s'\n", tmp_val, value);
			while(1);
		} 
	}
	printa("get done, all KV matched\n\n");

	printa("PUTB non-encrypted: insert %d elements\n", N);

	miss = 0;
	for(i = 0; i < N; i++) {
		grace_update(self->gsp, self->id);
		memset(key, 0, sizeof(key));
		memcpy(key, &i, sizeof(int));
		int tmp_v = N - i;
		memcpy(value, &tmp_v, sizeof(int));
		while(putb(self->gsp, NULL, key, value, 100) == 1) {
			grace_update(self->gsp, self->id);
			miss++;
               }
	}
	printa("write done, miss count =  %d, now read\n", miss);
	printa("GETB non-encrypted: insert %d elements\n", N);
	for(i = 0; i < N; i++) {
		memset(key, 0, sizeof(key));
		memcpy(key, &i, sizeof(int));

		char tmp_val[100];
		if(getb(self->gsp, NULL, key, tmp_val, 100) == 1) {
			printa("KV does not exist but should\n");while(1);
		}

		int tmp_v;
		memcpy(&tmp_v, tmp_val, sizeof(int));
		if(tmp_v != (N - i) ) {
			printa("KVP does not match: got = %d, expected = %d\n", tmp_v, N - i);
			while(1);
		}
	}

	printa("getb done, all KV matched\n\n");

	printa("PUTB encrypted: insert %d elements\n", N);

	miss = 0;
	for(i = 0; i < N; i++) {
		grace_update(self->gsp, self->id);
		memset(key, 0, sizeof(key));
		memcpy(key, &i, sizeof(int));
		int tmp_v = N - i;
		memcpy(value, &tmp_v, sizeof(int));
		while(putb(self->gsp, ps->ctx, key, value, 100) == 1) {
			grace_update(self->gsp, self->id);
			miss++;
               }
	}
	printa("write done, miss count =  %d, now read\n", miss);
	printa("GETB encrypted: insert %d elements\n", N);
	for(i = 0; i < N; i++) {
		memset(key, 0, sizeof(key));
		memcpy(key, &i, sizeof(int));

		char tmp_val[100];
		if(getb(self->gsp, ps->ctx, key, tmp_val, 100) == 1) {
			printa("KV does not exist but should\n");while(1);
		}

		int tmp_v;
		memcpy(&tmp_v, tmp_val, sizeof(int));
		if(tmp_v != (N - i) ) {
			printa("KVP does not match: got = %d, expected = %d\n", tmp_v, N - i);
			while(1);
		}
	}

	printa("getb encrypted done, all KV matched\n");


	return 0;
}

int cleaner_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {

	return 0;
}

int test_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	grace_register(self->gsp, self->id);

	struct ps_struct *ps = (struct ps_struct *) self->ps;

	alloc_ctr(&ps->ctx);

	return 0;
}
