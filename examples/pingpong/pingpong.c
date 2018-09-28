/**
\file
\brief Basic ping-pong application.

This version of ping-pong uses low level push and pop functions.
*/

#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);

/**
\brief this structure describes a message format. only 'msg' is involved
*/
struct mbox_struct {
	char msg[64];
} __attribute__ ((aligned (64)));

/**
\brief a structure describes a content of a per actor private store
*/
struct ps_struct {
	int once; ///< ping flag
	int cnt; ///< maximum number of pings

	queue *pool;
	queue *mbox;
};

/**
\brief A ping actor.
\param[in] a pointer to a structure that this describe actor

- if it is a ping time (once), the aping actor pops an empty node, fills it by 'ping' and pushes to mbox[1]
- Otherwise pops message from mbox[0], and on success checks counter and restarts once
*/
void aping(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	if(ps->once) {
		node *tmp = nalloc(ps->pool);
		struct mbox_struct *msg = (struct mbox_struct *) tmp->payload;
		memcpy(msg->msg, "ping", sizeof("ping"));
		printa("[PING] Send message '%s'\n", msg->msg);
		push_back(&ps->mbox[1], tmp);

		ps->once = 0;
	} else {
		node *tmp=pop_front(&ps->mbox[0]);
		if(tmp==NULL)
			return;

		struct mbox_struct *msg = (struct mbox_struct *) tmp->payload;
		printa("[PING] Got message '%s'\n", msg->msg);
		nfree(tmp);

		if(ps->cnt--)
			ps->once = 1;
		else {
			printa("end\n");
		}
	}

}

/**
\brief A pong actor.
\param[in] a pointer to a structure that this describe actor

- pops message from mbox[1] and sends 'pong' via mbox[0]
*/
void apong(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	node *tmp = pop_front(&ps->mbox[1]);
	if(tmp==NULL)
		return;

	struct mbox_struct *msg = (struct mbox_struct *) tmp->payload;

	printa("[PONG] Got message '%s'\n", msg->msg);
	memcpy(msg->msg, "pong", sizeof("pong"));

	push_back(&ps->mbox[0], tmp);
}

/**
\brief A pong actor.

- aping constructor, initializes initial variables
*/

int aping_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	ps->pool = gpool;
	ps->mbox = gboxes;

	ps->once = 1;
	ps->cnt = 1000000;

	return 0;
}

int apong_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	ps->pool = gpool;
	ps->mbox = gboxes;

	return 0;
}
