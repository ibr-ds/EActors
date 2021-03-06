#ifndef TCP_SOCK_H
#define TCP_SOCK_H

#include "../config.h"
#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UNTRUSTED

/*! For compatibility */
struct in_addr {
    unsigned long s_addr;  ///< load with inet_aton()
};

/*! For compatibility */
struct sockaddr_in {
    short            sin_family;   ///< e.g. AF_INET
    unsigned short   sin_port;     ///< e.g. htons(3490)
    struct in_addr   sin_addr;     ///< see struct in_addr, below
    char             sin_zero[8];  ///< zero this if you want to
};

#else
#include <netinet/in.h>
#endif

struct tcp_sock {
	queue	*mbox;
	int	sockfd;
	int	clilen;
	struct sockaddr_in cli_addr;
};

struct batch_s {
	int sockfd;
	int size;
	queue *mbox;
	queue *pool;
};

#define NB	( (PL_SIZE - 2*sizeof(int))/sizeof(struct batch_s))

struct batched_req {
	int total;
	queue *mbox;
	struct batch_s batch[NB];
};

struct batched_head {
	int size;
	queue q;
};

#define REQ_SIZE	( (PL_SIZE - (8 + 12 ) ) & 0xffffff80)

struct req {
//todo: Two fields + cast
	queue *mbox;
	int size;
	int sockfd;
	int id;
	char data[REQ_SIZE];
};

#define ACCEPTER	199
#define CLOSER		198
#define READER		197
#define WRITER		196
#define FACTORY		195


#ifdef __cplusplus
}
#endif

#endif //TCP_SOCK_H