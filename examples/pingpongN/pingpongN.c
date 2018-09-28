/**
\file
\brief Ping-pong example based on cargos

*/

#include <fw.h>
#include "payload.h"
extern void printa(const char *fmt, ...);

/**
\brief a private structure for communication

cache and encryption friendly alignment
*/
struct   mbox_struct {
//	char msg[PL_SIZE]; ///< a message
	char msg[16]; ///< a message
}; 

/**
\brief a structure describes a content of a per actor private store
*/
struct ps_struct {
	int once; ///< ping flag
	int cnt; ///< maximum number of pings
};
#define TEST
#define ADEBUG

#ifdef TEST
//    #define PAPER
#endif

/**
\brief A ping actor.

- Creates a cargo
- packs message
- sends cargo
- checks a responce
- receives pong
- returns a cargo
*/
void aping(struct actor_s *self) {
	struct socket_s *sockA = &self->sockets[0];
	struct mbox_struct *msg = NULL;
	struct cargo_s cargo;
	struct ps_struct *ps = (struct ps_struct *)self->ps;
	int ret;

	if(ps->once) {
		ret = create_cargo(sockA, &cargo);
		if(ret == 1)
			return;

		msg = (struct mbox_struct *) cargo.data;
#ifndef PAPER
		memcpy(msg->msg, "ping", sizeof("ping"));
#else
		memset(msg->msg, 'i', sizeof(msg->msg));
		msg->msg[sizeof(msg->msg)-1] = '\0';
#endif
#ifdef ADEBUG
		printa("1. aping sends message '%s'\n", msg->msg);
#endif
#ifdef TEST
		send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#else
		send_cargo(&cargo);
#endif
		ps->once = 0;

	} else {
		ret = recv_cargo_ks(sockA, &cargo, sizeof(struct mbox_struct));
		if( ret == 1)
			return;

		msg = (struct mbox_struct *) cargo.data;
#ifdef ADEBUG
		printa("4. aping got message '%s'\n", msg->msg);
#endif
		return_cargo(&cargo);

		if(ps->cnt--)
			ps->once = 1;
		else {
			printa("end\n",0);
			aexit(0);
		}
	}
}


/**
\brief A pong actor

- checks incomming messages
- on success:
    - unpack a message
    - print a content
    - pack new message and send

\note pong reuses a cargo
*/
void apong(struct actor_s *self) {
	struct socket_s *sockB = &self->sockets[0];
	struct mbox_struct *msg;
	struct cargo_s	cargo;
	int ret;
#ifdef TEST
	ret = recv_cargo_ks(sockB, &cargo, sizeof(struct mbox_struct));
#else
	ret = recv_cargo(sockB, &cargo);
#endif
	if( ret == 1)
		return;

	msg = (struct mbox_struct *) cargo.data;

#ifdef ADEBUG
	printa("2. apong got message '%s'\n", msg->msg);
#endif
#ifndef PAPER
	memcpy(msg->msg, "pong", sizeof("pong"));
#else
	memset(msg->msg, 'o', sizeof(msg->msg));
	msg->msg[sizeof(msg->msg)-1] = '\0';
#endif
#ifdef ADEBUG
	printa("3. apong sends message '%s'\n", msg->msg);
#endif
#ifdef TEST
	send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#else
	send_cargo(&cargo);
#endif
}

// constructors for pingpongN-1.xml configuration file

int aping_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	int ret = 0;

// we need to check size of mbox and a private store. both of them should less or equal of sizes of corresponding structures
	BUILD_BUG_ON( sizeof(((struct actor_s *)0)->ps) < sizeof(struct ps_struct));
	BUILD_BUG_ON( sizeof(((struct socket_s *)0)->pool->top->payload) < sizeof(struct mbox_struct));

	ps->once = 1;
	ps->cnt = 1000000;

	sockA->pool = gpool;
	sockA->in = &gboxes[0];
	sockA->out = &gboxes[1];

	spawn_server(gpool, gboxes, 6, 5106, sockA->in);
	spawn_client(gpool, gboxes, 6, 5105, sockA->out);

	setup_socket(sockA, 0, 1, MEMCPY);

	return 0;
}

int apong_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
//
	sockA->pool = gpool;
	sockA->in = &gboxes[0];
	sockA->out = &gboxes[1];

	spawn_server(gpool, gboxes, 5, 5105, sockA->in);
	spawn_client(gpool, gboxes, 5, 5106, sockA->out);

	setup_socket(sockA, 0, 1, MEMCPY);

	return 0;
}
