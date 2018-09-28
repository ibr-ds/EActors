/**
\file
\brief A header file for a cargo

Cargo is an object produced by a socket
- Universal carrier
- Includes a node, shadow buffer and pointer to a socket
- Should be specially handled
- Can be, but should not be used for over multiple sockets 
- Should be returned
- Can be not created
*/

#ifndef CARGO_H
#define CARGO_H

#include "../config.h"
#include "socket.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
\brief a cargo object description
*/
struct cargo_s {
	char *data;		///< a data pointer
	node *node;		///< a node object
	struct socket_s *sock;	///< a pointer to a socket to which this cargo belongs
};

void hold_on_cargo(struct cargo_s *cargo);
int need_repack(struct socket_s *sockA, struct socket_s *sockB);
void free_cargo(struct cargo_s *cargo);
void return_cargo(struct cargo_s *cargo);
int recv_cargo_ks(struct socket_s *sock, struct cargo_s *cargo, unsigned int size);
void send_cargo_ks(struct cargo_s *cargo, unsigned int size);
int recv_cargo(struct socket_s *sock, struct cargo_s *cargo);
void send_cargo(struct cargo_s *cargo);
int create_cargo(struct socket_s *sock, struct cargo_s *cargo);


#ifdef __cplusplus
}
#endif

#endif
