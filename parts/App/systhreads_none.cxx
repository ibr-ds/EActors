#ifndef SYSTHREADS_CXX
#define SYSTHREADS_CXX

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <sys/mman.h>

#include <netdb.h>
#include <netinet/in.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "factory.h"
#include "tcp_sock.h"


#define SYS_WORKERS	7

struct s_worker clients[SYS_WORKERS];
struct s_worker servers[SYS_WORKERS];
struct s_worker	factory;



void spawn_systhreads(void) {

}

#endif //SYSTHREADS_CXX