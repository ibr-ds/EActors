/**
\file
\brief Description of actors
*/

#ifndef ACTOR_H
#define ACTOR_H

#include "../config.h"
#include "node.h"
#include "socket.h"

#ifdef __cplusplus
extern "C" {
#endif

int in_enclave(void);

typedef int (*body_t)(struct actor_s *); 					///< typedef for a body function callback
typedef int (*ctr_t)(struct actor_s *, queue *, queue *, queue *, queue *);	///< typedef for a construction function callback

/**
\struct actor_s
\brief A representation of an actor
*/
struct actor_s {
	unsigned int	id;							///< An Identifier, not acturally used
	body_t		body;							///< the body function of an actor
	ctr_t		ctr;							///< the constructor callback of an actor
	char		*ps;							///< the private sore of an actor
	struct 		eos_struct *gsp;					///< a pointer to the memory-mapped EOS
	queue		*gboxes_v2;						///< an array of boxes
	queue		*gpool_v2;						///< an array of pools
#ifndef V2
	struct 		socket_s	sockets[MAX_SOCKETS];			///< an array of sockets
#else
	struct 		socket_s	*sockets;				///< a pointer to sockets TODO: tests with sockets will not work anymore
	queue		*gpool;							///< global pool
	queue		*ppool;							///< private pool
	queue		*gboxes;						///< global boxes
	queue		*pboxes;						///< private boxes
#endif
} actors[MAX_ACTORS];

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))	///< a sanity check at the compilation time

#ifdef __cplusplus
}
#endif

#endif
