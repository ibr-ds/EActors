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
#if 0
void storage_sync(int fd) {
	ocall_syncfs(fd);
}
#endif
int in_enclave(void) {
	return 1;
}

void aexit(int ret) {
	ocall_exit(ret);
}

extern void (*pf[])(struct actor_s *);
extern int (*cf[])(struct actor_s *, queue *, queue *, queue *, queue *);

#ifdef __cplusplus
}
#endif

#define MAX_PRIVATE_BOXES	10 ///< maximum number of a private boxes

queue private_boxes[MAX_PRIVATE_BOXES]; ///< per enclave array of private boxes
queue private_pool; ///< per enclave private object pool

#define PPOOL_SIZE	10

static int inited = 0;

/**
\brief A constructor ECAll, used to construct actors
\param[in] id An identifier of an actor
\param[in] aid Anotether identifier of an actor. In fact it is a number of an actor inside an enclave binary
\param[in] *aargs_p is a pointer in untrusted memory consists of argument structure (defined in aargs.h)
\param[out] ret: 0=done, 1=need more
*/
int constructor(unsigned int id, unsigned int aid, void *aargs_p) {
	int i;
	int ret = 0;
	struct aargs_s *aargs = (struct aargs_s *) aargs_p;
//workaround. in future I will add here independant enclave entry for dynamic configuration on_boot
	if(!inited) {
		memset(actors, 0, sizeof(actors));
		queue_init(&private_pool);

		for(i=0; i < PPOOL_SIZE; i++) {
			node *tmp = (node *) malloc(sizeof(node));
			if( tmp == NULL) {
				printa("no mem %d, die\n", __LINE__);
				while(1);
			}
			memset(tmp->payload, 0, PL_SIZE);
			push_front(&private_pool, tmp);
		}

		for(i=0; i < MAX_PRIVATE_BOXES; i++) {
			queue_init(&private_boxes[i]);
		}

		inited = 1;
	}

	printa("[T] constructing id = %d, aid = %d\n", id,aid);
	if(actors[aid].ps == NULL) {
		actors[aid].id = id;
		actors[aid].body=pf[aid];
		actors[aid].ctr=cf[aid];
		actors[aid].gsp = aargs->gsp;
		actors[aid].ps = (char *)malloc(PRIVATE_STORE_SIZE);
		actors[aid].gboxes_v2 = aargs->gboxes_v2;
		actors[aid].gpool_v2 = aargs->gpool_v2;

		memset(actors[aid].ps, 0, PRIVATE_STORE_SIZE);
	}

	if(actors[aid].ctr)
		ret = actors[aid].ctr(&actors[aid], aargs->gpool, &private_pool, aargs->gboxes, private_boxes);
	else {
		printa("[T] Intrusion detected: call for a non-existing consrtructor\n");
		while(1);
	}

	printa("[T] ctr%d: %d\n", id, ret);
	return ret;
}

/**
\brief A Dispatcher ECAll, used to call actors code
\param[in] actors_mask a mask of actors that will be called by this worker inside current enclave
\param[in] leave should an worker leave this enclave after call of an actor
*/
int dispatcher(unsigned long int actors_mask, char leave) {
	int i;
	while(1) {
//place for a dispatch policy
		for(i=0; i< MAX_ACTORS; i++) {
			if(actors_mask & (1<<i)) {
				if(actors[i].body)
					actors[i].body(&actors[i]);
				else {
					printa("Intrusion detected: call for a non-existing actor\n");
					while(1);
				}
			}
		}
		if(leave)
			break;
	}
	return 0;
}



    