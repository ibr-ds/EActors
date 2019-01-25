/**
\file
\brief an include file for sockets
*/


#ifndef SOCKET_H
#define SOCKET_H

#include <string.h>
#include <stdlib.h>

#include "../config.h"
#include "node.h"
#include "packs.h"
#include "rsa.h"

#ifdef __cplusplus
extern "C" {
#endif

enum etype {
	EMPTY = 0,
	MEMCPY = 1,
	CTR,
	GCM,
};

/**
\brief A socket structure
*/
struct socket_s {
	queue		*pool;		///< pool of free objects of a socket
	queue		*out;		///< a link to an outgoing queue
	queue		*in;		///< a link to an incomming queue

	int		cross;		///< 1 if a socket bridges actors in different enclaves
	int		enc;		///< 1 if a socket bridges actors in different enclaves
	enum	etype	type;		///< a type of a socket
	void 		*ctx;		///< pointer to an encryption context. should be casted

	struct rsa_kp_struct rsa_master;///< all sockets include 2 RSA pairs even if they are SEAL-based
	struct rsa_kp_struct rsa_slave;	///< A socket connects two participants always
	int		state;		///< key-exchange phase

#ifdef V2
	unsigned long int	who;	///< who uses this socket
	unsigned long int	whom;	///< with whom

	int		count;		///< send statistic
#endif
	int 		mid;		///< This is a termporary workaround for LA. Master use this value to idenitfy the proper MEASURE of the slave
};

char is_cross(struct socket_s *sock);
char is_enc(struct socket_s *sock);
char *get_content(node *);
node *alloc_pkg(struct socket_s *sock);
void return_empty(struct socket_s *sock, node *pkg);
void return_pkg(struct socket_s *sock, node *pkg, char *msg);
void send_pkg_unpacked_myself(struct socket_s *sock, node *pkg, char *src);
void send_pkg_unpacked(struct socket_s *sock, node *pkg);
void send_pkg(struct socket_s *sock, node *pkg, char *src, int size);
node *recv_pkg_unpacked(struct socket_s *sock);
node *recv_pkg_no_allocate(struct socket_s *sock, char *dst, int size);
node *recv_pkg(struct socket_s *sock, char **dst, int size);
node *create_pkg(struct socket_s *sock, char **pl);

void setup_socket(struct socket_s *sock, int enc, int cross, enum etype type);
void unpack(struct socket_s *sock, char *dst, char *src, int size, void *ctx, char *mac);
void pack(struct socket_s *sock, char *dst, char *src, int size, void *ctx, char *mac);
int pack_get_size(struct socket_s *sock);

#ifdef __cplusplus
}
#endif

#endif //ENC_H