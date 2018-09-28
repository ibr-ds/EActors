/**
\file
\brief SMC example for the paper

bigger block size may be required:
#define BLOCK_SIZE     (1024*40)

*/

#include <fw.h>
#include "payload.h"

extern void printa(const char *fmt, ...);

#define UINT_MAX	0xffffffff
#define ADEBUG
#define VECTS	10
#define LOAD

/**
\brief a private structure for communication
*/
struct   mbox_struct {
	unsigned int word[VECTS];
}; 

/**
\brief a structure describes a content of a per actor private store
*/
struct ps_struct {
	int once; ///< ping flag
	int cnt; ///< maximum number of pings
	unsigned int secret[VECTS];
	int me;
	unsigned int rnd[VECTS];
};



void first(struct actor_s *self) {
	struct socket_s *sockA = &self->sockets[0];
	struct socket_s *sockB = &self->sockets[1];
	struct mbox_struct *msg = NULL;
	struct cargo_s cargo;
	struct ps_struct *ps = (struct ps_struct *)self->ps;
	unsigned int sums[VECTS];
	int ret, i;

	if(ps->once) {
		ret = create_cargo(sockA, &cargo);
		if(ret == 1)
			return;

		msg = (struct mbox_struct *) cargo.data;
		sgx_read_rand((unsigned char *)&ps->rnd, sizeof(ps->rnd));
		for(i = 0; i < VECTS; i++) {
			ps->rnd[i] = ps->rnd[i] % ( UINT_MAX / MAX_ACTORS);
			msg->word[i] = ps->secret[i] + ps->rnd[i];
		}
#ifdef ADEBUG
		printa("1. secret = %x, rnd = %x, send = %x\n", ps->secret[0], ps->rnd[0], msg->word[0]);
#endif

		send_cargo_ks(&cargo, sizeof(struct mbox_struct));
		ps->once = 0;
#ifdef LOAD
		for(i = 0; i < VECTS; i++) {
			unsigned int ttmp;
			sgx_read_rand((unsigned char *)&ttmp, sizeof(unsigned int));
			ps->secret[i] = (ps->secret[i] + ttmp ) % ( UINT_MAX / MAX_ACTORS);
		}
#endif

	} else {
		ret = recv_cargo_ks(sockB, &cargo, sizeof(struct mbox_struct));
		if( ret == 1) {
			return;
		}

		msg = (struct mbox_struct *) cargo.data;
		for(i = 0; i < VECTS; i++) 
			sums[i] = msg->word[i] - ps->rnd[i];
#ifdef ADEBUG
		printa("6. REQUESTER got sum: '%x'\n", msg->word[0] - ps->rnd[0]);
#endif
		return_cargo(&cargo);

		if(--ps->cnt)
			ps->once = 1;
		else {
			printa("end\n",0);
			aexit();
		}
	}
}

void keeper(struct actor_s *self) {
	struct mbox_struct *msg;
	struct socket_s *sockA = &self->sockets[0];
	struct socket_s *sockB = &self->sockets[1];
	struct ps_struct *ps = (struct ps_struct *)self->ps;
	struct cargo_s	cargo;
	int ret, i;

	ret = recv_cargo_ks(sockA, &cargo, sizeof(struct mbox_struct));

	if( ret == 0) {
		msg = (struct mbox_struct *) cargo.data;
#ifdef ADEBUG
		printa("KEEPER%d got '%x'\n", ps->me, msg->word[0]);
#endif

		if(need_repack(sockA, sockB)) {
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

		for(i = 0; i < VECTS; i++)
			msg->word[i]+=ps->secret[i];

#ifdef ADEBUG
		printa("KEEPER%d secret = %x, send '%x'\n", ps->me, ps->secret[0], msg->word[0]);
#endif

		send_cargo_ks(&cargo, sizeof(struct mbox_struct));
#ifdef LOAD
		for(i = 0; i < VECTS; i++) {
			unsigned int ttmp;
			sgx_read_rand((unsigned char *)&ttmp, sizeof(unsigned int));
			ps->secret[i] = (ps->secret[i] + ttmp )% ( UINT_MAX / MAX_ACTORS);
		}
#endif

	}
}

#define ENC 1
#define CROSS 1
#define STYPE GCM

int first_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct socket_s *sockA = &self->sockets[0];
	struct socket_s *sockB = &self->sockets[1];
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	int i;
	int ret = 0;

	BUILD_BUG_ON( PRIVATE_STORE_SIZE < sizeof(struct ps_struct));
	BUILD_BUG_ON( sizeof(((struct socket_s *)0)->pool->top->payload) < sizeof(struct mbox_struct));

	ps->once = 1;
	ps->cnt = 10000;
	ps->me = 0;

	sgx_read_rand((unsigned char *)&ps->secret, sizeof(ps->secret));
	for(i = 0; i < VECTS; i++)
		ps->secret[i] = ps->secret[i] % ( UINT_MAX / MAX_ACTORS); //to prevent overflow

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

	if(CROSS) {
		sockB->pool = gpool;
		sockB->in = &gboxes[2*MAX_ACTORS-1];
		sockB->out = &gboxes[2*MAX_ACTORS-2];
	} else {
		sockB->pool = ppool;
		sockB->in = &pboxes[2*MAX_ACTORS-1];
		sockB->out = &pboxes[2*MAX_ACTORS-2];
	}

	setup_socket(sockB, ENC, CROSS, STYPE);
	sockB->mid = 1;

	ret |= build_LARSA_master_socket(sockA);
	ret |= build_LARSA_master_socket(sockB);

	return ret;
}


#define SERVER(A) \
void keeper##A(struct actor_s *self) {												\
	keeper(self);														\
}																\
																\
int keeper##A##_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {				\
	struct socket_s *sockA = &self->sockets[0];										\
	struct socket_s *sockB = &self->sockets[1];										\
	struct ps_struct *ps = (struct ps_struct *) self->ps;									\
	int i;															\
	int ret = 0;														\
																\
	ps->me = A;														\
	sgx_read_rand((unsigned char *)&ps->secret, sizeof(ps->secret));							\
	for(i = 0;i < VECTS; i++)												\
		ps->secret[i] = ps->secret[i] % ( UINT_MAX / MAX_ACTORS);							\
																\
	if(CROSS) {														\
		sockA->pool = gpool;												\
		sockA->in = &gboxes[ps->me*2-1];										\
		sockA->out = &gboxes[ps->me*2-2];										\
	} else {														\
		sockA->pool = ppool;												\
		sockA->in = &pboxes[ps->me*2-1];										\
		sockA->out = &pboxes[ps->me*2-2];										\
	}															\
																\
	setup_socket(sockA, ENC, CROSS, STYPE);											\
																\
	ret |= build_LARSA_slave_socket(sockA);											\
																\
	if(CROSS) {														\
		sockB->pool = gpool;												\
		sockB->in = &gboxes[ps->me*2];											\
		sockB->out = &gboxes[ps->me*2+1];										\
	} else {														\
		sockB->pool = ppool;												\
		sockB->in = &pboxes[ps->me*2];											\
		sockB->out = &pboxes[ps->me*2+1];										\
	}															\
																\
	setup_socket(sockB, ENC, CROSS, STYPE);											\
																\
	if((ps->me+1) == MAX_ACTORS)												\
		ret |= build_LARSA_slave_socket(sockB);										\
	else															\
		ret |= build_LARSA_master_socket(sockB);									\
																\
	return ret;														\
}

SERVER(1)
SERVER(2)
SERVER(3)
SERVER(4)
SERVER(5)
SERVER(6)
SERVER(7)
