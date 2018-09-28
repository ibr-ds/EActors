/**
\file
\brief factory is a system actor which spawns network actors on request
*/

#include "factory.h"
#include "tcp_sock.h"

extern void printa(const char *fmt, ...);
extern void asleep(int usec);


void spawn_server(queue *gpool, queue *gboxes, int ip, int port, queue *mbox) {
	node *tmp = pop_front(gpool);
	struct factory_request_s *fr = (struct factory_request_s *) tmp->payload;
	fr->mbox_back = mbox;
	fr->op = F_CREATE;
	fr->tp = F_SERVER;
	fr->fctx.port = port;
	fr->fctx.clnt = ip;
	fr->fctx.mbox = mbox;

	push_back(&gboxes[FACTORY], tmp);
	while(( tmp = pop_front(mbox)) == NULL)
		asleep(1000);

	fr = (struct factory_request_s *) tmp->payload;
	if(fr->op != F_OKK) {
		printa("cannot spawn server, die\n");while(1);
	} else
		printa("spawned server ok\n");

	push_front(gpool, tmp);
}

void spawn_client(queue *gpool, queue *gboxes, int ip, int port, queue *mbox) {
	node *tmp = pop_front(gpool);
	struct factory_request_s *fr = (struct factory_request_s *) tmp->payload;

	fr->mbox_back = mbox;
	fr->op = F_CREATE;
	fr->tp = F_CLIENT;
	fr->fctx.port = port;
	fr->fctx.clnt = ip;
	fr->fctx.mbox = mbox;

	push_back(&gboxes[FACTORY], tmp);
	while(( tmp = pop_front(mbox)) == NULL)
		asleep(1000);

	fr = (struct factory_request_s *) tmp->payload;
	if(fr->op != F_OKK) {
		printa("cannot spawn client, die\n");while(1);
	} else
		printa("conncted\n");

	push_front(gpool, tmp);
}