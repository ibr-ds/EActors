/**
\file
\brief Template for trusted actors


*/

#include <string.h> /* for memcpy etc */
#include <stdlib.h> /* for malloc/free etc */

#include "Enclave_t.h"
#include "config.h"



#ifdef __cplusplus
extern "C" {
#endif
void printa(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#include <stdio.h>      /* vsnprintf */
void printa(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print(buf);
}


#include <fw.h>

#ifdef __cplusplus
extern "C" {
#endif

void asleep(int usec) {
	ocall_usleep(usec);
}

void eos_sync(int fd) {
	ocall_syncfs(fd);
}

int in_enclave(void) {
	return 1;
}

void aexit(int ret) {
	ocall_exit(ret);
}

#include "../App/iworkers.cxx"
#include "enclave_cfg.h"

extern int (*pf[])(struct actor_s *);
extern int (*cf[])(struct actor_s *, queue *, queue *, queue *, queue *);

static enum constructor_state {
	INIT = 0,
	NORMAL,
	RECOVERY
} ctr_state = INIT;
static int ctr_sub_state = 0;

#define	MAX_PRIVATE_BOXES	10	///< maximum number of a private boxes
#define	PPOOL_SIZE		10

queue private_boxes[MAX_PRIVATE_BOXES]; ///< per enclave array of private boxes
queue private_pool;			///< per enclave private object pool

#ifdef __cplusplus
}
#endif

unsigned long int callers[MAX_WORKERS] = {0};

/**
\brief A constructor ECAll, used to construct actors
\param[in] id An identifier of an actor
\param[in] aid Anotether identifier of an actor. In fact it is a number of an actor inside an enclave binary
\param[in] *aargs_p is a pointer in untrusted memory consists of argument structure (defined in aargs.h)
\param[out] ret: 0=done, 1=need more
*/
int constructor(unsigned int enclave_id, unsigned int aid, void *aargs_p) {
	int i;
	int ret = 1;
	node *tmp = NULL;

	struct aargs_s *aargs = (struct aargs_s *) aargs_p;
	switch(ctr_state) {
		case INIT:
			printa("Constructing enclave %d\n", enclave_id);

			memset(actors, 0, sizeof(actors));
			queue_init(&private_pool);

			for(i=0; i < PPOOL_SIZE; i++) {
				tmp = (node *) malloc(sizeof(node));
				if( tmp == NULL) {
					printa("no mem %d, die\n", __LINE__);
					while(1);
				}
				memset(tmp->payload, 0, PL_SIZE);
				push_front(&private_pool, tmp);
			}

			for(i=0; i < MAX_PRIVATE_BOXES; i++) {
				queue_init(&private_boxes[i]);
#ifdef V2
				private_boxes[i].id = i;
				private_boxes[i].type = PRIVATE;
#endif
			}

			assign_workers();
			for (int w = 0; w < MAX_WORKERS; w++) {
				for(int a = 0; a < MAX_ACTORS; a++) {
					if(workers[w].actors_mask[ENCLAVE_ID] & (1<<a))
						callers[w] |= (1 << a);
				}
			}

#ifdef V2
			gboxes = aargs->gboxes;
			gpool = aargs->gpool;
			gboxes_v2 = aargs->gboxes_v2;
			gpool_v2 = aargs->gpool_v2;

			eboxes = aargs->eboxes;
			my_box = aargs->my_box;

			tmp_timer = nalloc(gboxes_v2);

			timer_box = (queue *) tmp_timer->payload;
			tmp = nalloc(gpool);
			fr = (struct factory_request_s *) tmp->payload;
			fr->mbox_back = gpool; // > /dev/null
			fr->op = F_CREATE;
			fr->tp = F_TIMER;
			fr->fctx.port = 5000000; //each 5 sec
			fr->fctx.mbox = timer_box;
			push_back(&gboxes[FACTORY], tmp);

			tmp = pop_front(my_box);
			if(tmp == NULL) {
				ctr_state = NORMAL;
				break;
			}
#else
			ctr_state = NORMAL;
#endif
		case NORMAL:
			ret = 0;
			if(actors[aid].ps == NULL) {
				actors[aid].id = aid;
				actors[aid].body=pf[aid];
				actors[aid].ctr=cf[aid];
				actors[aid].gsp = aargs->gsp;
				actors[aid].ps = (char *)malloc(PRIVATE_STORE_SIZE);
				actors[aid].gboxes_v2 = aargs->gboxes_v2;
				actors[aid].gpool_v2 = aargs->gpool_v2;
//
#ifdef V2
				actors[aid].gboxes = aargs->gboxes;
				actors[aid].gpool = aargs->gpool;
				actors[aid].pboxes = private_boxes;
				actors[aid].ppool = &private_pool;
				actors[aid].sockets = sockets;
#endif
				memset(actors[aid].ps, 0, PRIVATE_STORE_SIZE);
			}

//			printa("[T] constructing enclave = %d, aid = %d\n", enclave_id,aid);
			if(actors[aid].ctr)
				ret = actors[aid].ctr(&actors[aid], aargs->gpool, &private_pool, aargs->gboxes, private_boxes);
			else {
				printa("[T] Intrusion detected in CTR: call for a non-existing consrtructor\n");
				while(1);
			}
//			printa("[T] [Enclave%d][actor%d]: %d\n", enclave_id, aid, ret);
			break;
	}

	return ret;
}

/**
\brief A Dispatcher ECAll, used to call actors code
\param[in] leave: should an worker leave this enclave after call of an actor
\return 0: all actors are done
*/
int dispatcher(unsigned int id, char leave) {
	int i;
	int ret = 0;
	node *tmp = NULL;

	while(1) {
		ret = 0;
		for(i = 0; i < MAX_ACTORS; i++) {
			if(callers[id] & (1ull<<i)) {
				if(actors[i].body)
					ret |= actors[i].body(&actors[i]);
			}
		}

		if(leave || !ret)
			break;
	}

	return ret;
}

