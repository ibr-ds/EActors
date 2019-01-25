/**
\file
\brief Tripong example.

Like ping but involves three actros in interactions:

1. Ping sends 'ping' to Ming
2. Ming reads 'ping' and sends 'ping from ming' to Pong
3. Pong reads 'ping from ming'
4. Pong sends 'pong' to Ming
5. Ming reads 'pong' and sends 'pong from ming' to Ping
6. Ping reads 'pong from ming' 

*/

#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);

/**
\brief a private structure for communication

cache and encryption friendly alignment
*/
struct   mbox_struct {
	char msg[PL_SIZE]; ///< a message
//	char msg[16]; ///< a message
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

/**
\brief A ping actor.

- Creates a cargo
- packs message
- sends cargo
- checks a responce
- receives pong
- returns a cargo
*/
int aping(struct actor_s *self) {
	struct socket_s *sockA = &self->sockets[0];
	struct mbox_struct *msg = NULL;
	struct cargo_s cargo;
	struct ps_struct *ps = (struct ps_struct *)self->ps;
	int ret;

	if(ps->once) {
		ret = create_cargo(sockA, &cargo);
		if(ret == 1)
			return 1;

		msg = (struct mbox_struct *) cargo.data;
		memcpy(msg->msg, "ping", sizeof("ping"));
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
			return 1;

		msg = (struct mbox_struct *) cargo.data;
#ifdef ADEBUG
		printa("6. aping got message '%s' [%d] \n", msg->msg, ps->cnt);
#endif
		return_cargo(&cargo);

		if(ps->cnt--)
			ps->once = 1;
		else {
			printa("end\n",0);
			aexit(0);
		}
	}
	return 1;
}

/**
\brief A ming actor -- middle man.

- checks incomming messages
- on success:
    - unpack a message
    - print a content
    - return a cargo if need to repack
    - allocate new cargo if need to repack
    - pack new message and send
*/
int aming(struct actor_s *self) {
	struct mbox_struct *msg;
	struct socket_s *sockA = &self->sockets[0];
	struct socket_s *sockB = &self->sockets[1];
	struct cargo_s	cargo;
	int ret;
#ifdef TEST
	ret = recv_cargo_ks(sockA, &cargo, sizeof(struct mbox_struct));
#else
	ret = recv_cargo(sockA, &cargo);
#endif
	if( ret == 0) {
		msg = (struct mbox_struct *) cargo.data;
#ifdef ADEBUG
		printa("2. aming got message '%s' from aping\n", msg->msg);
#endif

		if(need_repack(sockA, sockB) ) {
			return_cargo(&cargo);
			ret = create_cargo(sockB, &cargo);
			if(ret == 1) {
				printa("error at line %d\n", __LINE__);while(1);
				//2. what if we cannot create a new cargo? then we need to make a step back 
				//for that we need to put the first cargo into the incoming queue and restart the actor
				//we cannot do this since we have returned a cargo already 
				//thus, firstly we need to create a new cargo
				//then we need to return the first cargo
			}
			msg = (struct mbox_struct *) cargo.data;
		} else
			cargo.sock = sockB;

		memcpy(msg->msg, "ping from aming", sizeof("ping from aming"));
#ifdef TEST
		send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#else
		send_cargo(&cargo);
#endif
	} else {
#ifdef TEST
		ret = recv_cargo_ks(sockB, &cargo, sizeof(struct mbox_struct));
#else
		ret = recv_cargo(sockB, &cargo);
#endif
		if(ret == 1)
			return 1;

		msg = (struct mbox_struct *) cargo.data;
#ifdef ADEBUG
		printa("5. aming got message '%s' from apong\n", msg->msg);
#endif

		if(need_repack(sockA, sockB) ) {
			return_cargo(&cargo);
			ret = create_cargo(sockA, &cargo);
			if(ret == 1) {
				printa("error at line %d\n", __LINE__);while(1);
				//2. what if we cannot create a new cargo? then we need to make a step back 
				//for that we need to put the first cargo into the incoming queue and restart the actor
				//we cannot do this since we have returned a cargo already 
				//thus, firstly we need to create a new cargo
				//then we need to return the first cargo
			}
			msg = (struct mbox_struct *) cargo.data;
		} else
			cargo.sock = sockA;

		memcpy(msg->msg, "pong from aming", sizeof("pong from aming"));
#ifdef TEST
		send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#else
		send_cargo(&cargo);
#endif
	}

	return 1;
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
int apong(struct actor_s *self) {
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
		return 1;

	msg = (struct mbox_struct *) cargo.data;

#ifdef ADEBUG
	printa("3. apong got message '%s'\n", msg->msg);
#endif
	memcpy(msg->msg, "pong", sizeof("pong"));
#ifdef ADEBUG
	printa("4. apong sends message '%s'\n", msg->msg);
#endif
#ifdef TEST
	send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#else
	send_cargo(&cargo);
#endif
	return 1;
}

#if 0
// constructors for tripong1.xml configuration file
#define ENC1 	0
#define CROSS1 	1
#define STYPE1	MEMCPY

#define ENC2 	0
#define CROSS2 	1
#define STYPE2	MEMCPY
#endif

#if 0
// constructors for tripong2.xml configuration file
#define ENC1 	0
#define CROSS1 	1
#define STYPE1	MEMCPY

#define ENC2 	1
#define CROSS2 	1
#define STYPE2	GCM
#endif

#if 0
// constructors for tripong3.xml configuration file
#define ENC1 	1
#define CROSS1 	1
#define STYPE1	GCM

#define ENC2 	1
#define CROSS2 	1
#define STYPE2	GCM
#endif

#if 1
// constructors for tripong4.xml configuration file
#define ENC1 	0
#define CROSS1 	1
#define STYPE1	MEMCPY

#define ENC2 	0
#define CROSS2 	0
#define STYPE2	MEMCPY
#endif


int aping_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	int ret = 0;

// we need to check size of mbox and a private store. both of them should less or equal of sizes of corresponding structures
	BUILD_BUG_ON( sizeof(((struct actor_s *)0)->ps) < sizeof(struct ps_struct));
	BUILD_BUG_ON( sizeof(((struct socket_s *)0)->pool->top->payload) < sizeof(struct mbox_struct));

	ps->once = 1;
	ps->cnt = 1000000;

	if(CROSS1) {
		sockA->pool = gpool;
		sockA->in = &gboxes[0];
		sockA->out = &gboxes[1];
	} else {
		sockA->pool = ppool;
		sockA->in = &pboxes[0];
		sockA->out = &pboxes[1];
	}

	setup_socket(sockA, ENC1, CROSS1, STYPE1);

	ret |= build_LARSA_master_socket(sockA);

	return ret;
}

int aming_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	struct socket_s *sockB = &self->sockets[1];
	int ret = 0;
//
	if(CROSS1) {
		sockA->pool = gpool;
		sockA->in = &gboxes[1];
		sockA->out = &gboxes[0];
	} else {
		sockA->pool = ppool;
		sockA->in = &pboxes[1];
		sockA->out = &pboxes[0];
	}

	setup_socket(sockA, ENC1, CROSS1, STYPE1);

	ret |= build_LARSA_slave_socket(sockA);
//
	if(CROSS2) {
		sockB->pool = gpool;
		sockB->in = &gboxes[2];
		sockB->out = &gboxes[3];
	} else {
		sockB->pool = ppool;
		sockB->in = &pboxes[2];
		sockB->out = &pboxes[3];
	}

	setup_socket(sockB, ENC2, CROSS2, STYPE2);

	ret |= build_LARSA_master_socket(sockB);

	return ret;
}

int apong_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	int ret = 0;
//
	if(CROSS2) {
		sockA->pool = gpool;
		sockA->in = &gboxes[3];
		sockA->out = &gboxes[2];
	} else {
		sockA->pool = ppool;
		sockA->in = &pboxes[3];
		sockA->out = &pboxes[2];
	}

	setup_socket(sockA, ENC2, CROSS2, STYPE2);

	ret |= build_LARSA_slave_socket(sockA);

	return ret;
}


