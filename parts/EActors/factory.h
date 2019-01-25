#ifndef FACTORY_H
#define FACTORY_H

#include "../config.h"
#include "node.h"

//operation on the entity
enum fac_op {
	F_OKK = 0xaa,		//F_OK is used by someone else
	F_CREATE,
	F_DESTROY,
	F_FAIL
};


//type of the entity
enum fac_tp {
	F_SERVER,
	F_CLIENT,
	F_ENCLAVE,
	F_TIMER,
	F_CLOSER,
	F_READER,
	F_WRITER,
	F_PIPE_READER,
	F_PIPE_WRITER,
	F_ACCEPTER
};

struct factory_ctx_s {
	long unsigned int	id; //should be pthread_t
	int	port;
	int	clnt;
	queue	*mbox;
	int	init;
};

struct	factory_request_s {
	queue		*mbox_back;
	enum fac_op	op;
	enum fac_tp	tp;
//
	struct factory_ctx_s	fctx;
};


#ifdef __cplusplus
extern "C" {
#endif

void spawn_service(queue *gpool, queue *gboxes, enum fac_tp tp, int clnt, int port, queue *mbox);
void spawn_server(queue *gpool, queue *gboxes, int ip, int port, queue *mbox);
void spawn_client(queue *gpool, queue *gboxes, int ip, int port, queue *mbox);

#ifdef __cplusplus
}
#endif

#endif //FACTORY_H