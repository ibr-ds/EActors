/**
\file
\brief An example of TCP-IP actor

This is a echo services. Just use following command to connect: 

telnet localhost 5001

and type something. This example requires system actors 'acceptor', 'writer', 'reader' and 'closer'.
This example supports multiple conqurent clients. 

*/

#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);

enum state_e {
	INIT,
	ACCEPTING,
	READY,
	WAIT_READING,
	WAIT_WRITING,
};

struct tsoc_desc {
	queue *queue;
	queue *accepter;
	queue *reader;
	queue *closer;
	queue *writer;
	queue *me;

	int sock;
	enum state_e state;
};

#define LOOP
#define DEBUG

int server(struct actor_s *self) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;
	node *tmp = NULL;
	struct tcp_sock *tcp_sock = NULL;
	struct req *to_read = NULL;
	struct req *to_write = NULL;
	char msg[512];

	switch(desc->state) {
		case INIT:
			tmp = pop_front(desc->queue);
			if (tmp == NULL)
				return 1;

			tcp_sock = (struct tcp_sock *) tmp->payload;
			tcp_sock->sockfd = -1;
			tcp_sock->mbox = desc->me;

			push_back(desc->accepter, tmp);
			desc->state = ACCEPTING;
			return 1;
		case ACCEPTING:
			tmp = pop_front(desc->me);
			if (tmp == NULL)
				return 1;

			tcp_sock = (struct tcp_sock *) tmp->payload;
			if(tcp_sock->sockfd == -1) {
				push_back(desc->queue, tmp);
				desc->state = INIT;
				return 1;
			}

			printa("got accept sock = %d\n", tcp_sock->sockfd);
			desc->sock = tcp_sock->sockfd;

			push_back(desc->queue, tmp);
			desc->state = READY;
			return 1;
		case READY:
			tmp = pop_front(desc->queue);
			if (tmp == NULL)
				return 1;

			to_read = (struct req *) tmp->payload;
			memset(to_read->data, 0 , sizeof(to_read->data));
			to_read->size = sizeof(to_read->data);
			to_read->sockfd = desc->sock;
			to_read->mbox = desc->me;

			push_back(desc->reader, tmp);
			desc->state = WAIT_READING;
			return 1;
		case WAIT_READING:
			tmp = pop_front(desc->me);
			if (tmp == NULL)
				return 1;

			to_read = (struct req *) tmp->payload;
			if (to_read->size == -2) {
				push_back(desc->queue, tmp);
				desc->state = READY;
				return 1;
			}
#ifdef DEBUG
			printa("incomming message '%s' size = %d\n", to_read->data, to_read->size);
#endif

#ifdef BASIC
			memcpy(msg, to_read->data, sizeof(msg));
			memset(to_read->data, 0, sizeof(to_read->data));
			snprintf(to_read->data, sizeof(to_read->data), "got following message: '%s'", msg); //todo sanity check
			to_read->size=strlen(to_read->data)+1;
#else
//nothing to do. we send back the initial message
#endif
			to_read->mbox = desc->me;
			to_read->sockfd = desc->sock;

			push_back(desc->writer, tmp);
			desc->state = WAIT_WRITING;
			return 1;
		case WAIT_WRITING:
			tmp = pop_front(desc->me);
			if (tmp == NULL)
				return 1;

			to_write = (struct req *) tmp->payload;
			if (to_write->size < 0)
				printa("something went wrong with write but we do not care\n");

#ifdef LOOP
			push_back(desc->queue, tmp);
			desc->state = READY;
#else
			push_back(desc->closer, tmp);
			desc->state = INIT;
#endif
			return 1;
	}
	return 1;
}

int server_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct tsoc_desc *desc = (struct tsoc_desc *) self->ps;

	desc->queue = gpool;
	desc->accepter = &gboxes[ACCEPTER];
	desc->writer = &gboxes[WRITER];
	desc->reader = &gboxes[READER];
	desc->closer = &gboxes[CLOSER];
	desc->me = &gboxes[0];

	desc->sock = -1;
	desc->state = INIT;

	return 0;
}
