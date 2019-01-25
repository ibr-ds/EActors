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
	char msg[1024]; ///< a message
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
#define NPINGS	500

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
	printa("2. apong got message '%s'\n", msg->msg);
#endif
	memcpy(msg->msg, "pong", sizeof("pong"));
#ifdef ADEBUG
	printa("3. apong sends message '%s'\n", msg->msg);
#endif
#ifdef TEST
	send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#else
	send_cargo(&cargo);
#endif

	return 1;
}

#if 0
//pingpong2/pingpong2-1.xml
//pingpong2/pingpong2-2.xml
#define CROSS 1
#define ENC 0
#define STYPE	MEMCPY	//plays no role
#endif

#if 1
//pingpong2/pingpong2-3.xml
//pingpong2/pingpong2-4.xml
#define CROSS 1
#define ENC 1
#define STYPE	GCM	//CTR, GCM or MEMCPY
//#define SEAL		//LARSA is default
#endif

#if 0
//pingpong2/pingpong2-5.xml
#define CROSS 0
#define ENC 0
#define STYPE	MEMCPY	//plays no role
#endif


int aping_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	int ret = 0;

// we need to check size of mbox and a private store. both of them should less or equal of sizes of corresponding structures

	BUILD_BUG_ON( PRIVATE_STORE_SIZE < sizeof(struct ps_struct));
	BUILD_BUG_ON( sizeof(((struct socket_s *)0)->pool->top->payload) < sizeof(struct mbox_struct));

	ps->once = 1;
	ps->cnt = NPINGS;

	sockA->cross = ENC;
	if(CROSS) {
		sockA->pool = gpool;
		sockA->in = &gboxes[0];
		sockA->out = &gboxes[1];
	} else {
		sockA->pool = ppool;
		sockA->in = &pboxes[0];
		sockA->out = &pboxes[1];
	}

	setup_socket(sockA, ENC, CROSS, STYPE);

#ifdef SEAL
	ret |= build_SEAL_master_socket(sockA);
#else
	ret |= build_LARSA_master_socket(sockA);
#endif

	return ret;
}

int apong_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	int ret = 0;

	if(CROSS) {
		sockA->pool = gpool;
		sockA->in = &gboxes[1];
		sockA->out = &gboxes[0];
	} else {
		sockA->pool = ppool;
		sockA->in = &pboxes[1];
		sockA->out = &pboxes[0];
	}

	setup_socket(sockA, ENC, CROSS, STYPE);

#ifdef SEAL
	ret |= build_SEAL_slave_socket(sockA);
#else
	ret |= build_LARSA_slave_socket(sockA);
#endif

	return ret ;
}

