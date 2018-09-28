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

void accepter(int gsock) {
}

void closer(struct actor_s *self) {
	node *tmp = pop_front(&gboxes[CLOSER]);
	if(tmp == NULL)
			return;

	struct req *to_close = (struct req *) tmp->payload;
	close(to_close->sockfd);

	push_front(&gpool, tmp);
}

void *client_thread(void *arg)
{
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;

	printf("I am connecting to %d on port %d\n", fctx->clnt, fctx->port);

	int rq_port = fctx->port;
	int rq_clnt = fctx->clnt;
	queue *rq_mbox = fctx->mbox;

	int ret;
	int ptr;
	cpu_set_t cpuset;
	sgx_status_t status;

	int i;
	CPU_ZERO(&cpuset);

	CPU_SET(0, &cpuset);
	CPU_SET(1, &cpuset);


	if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) <0) {
		perror("pthread_setaffinity_np");
	}

	int sockfd,  n;
	struct sockaddr_in serv_addr;
	struct hostent *server;

	sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0) 
		perror("ERROR opening socket");

	char ipaddr[16];
	sprintf(ipaddr, "10.30.1.%d", rq_clnt);
	server = gethostbyname(ipaddr);
	if (server == NULL) {
		fprintf(stderr,"ERROR, no such host\n");
		exit(0);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;

	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(rq_port);
	while (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)
		sleep(1);

	fctx->init = 1;

	while(1) {
		node *tmp = pop_front(rq_mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

#ifdef SOCKET_DEBUG
		printf("poped box with '%s', size = %d \n", tmp->payload, sizeof(tmp->payload));
#endif
		n = write(sockfd,tmp->payload,sizeof(tmp->payload));
		if (n < 0) 
			perror("ERROR write to socket");

		push_front(&gpool, tmp);
	}
}


/**
\brief server thread
*/
void *server_thread(void *arg)
{
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;

	int sockfd, newsockfd;
	unsigned int clilen;
	char buffer[256];
	struct sockaddr_in serv_addr, cli_addr;
	int  n, rc, on = 1;

	printf("I am server for %d on port %d\n", fctx->clnt, fctx->port);

	int rq_port = fctx->port;
	int rq_clnt = fctx->clnt;
	queue *rq_mbox = fctx->mbox;

	fctx->init = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd < 0) {
		perror("sock create\n");while(1);
	}

	bzero((char *) &serv_addr, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(rq_port);

// re-bind if a sock still open from previous run.
	while(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		sleep(1);
//sysctl -w net.core.somaxconn=2048

	while(1) {
		listen(sockfd, 2048);
		clilen = sizeof(cli_addr);

		unsigned int c_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (c_sockfd < 0) {
			perror("ERROR on accept");while(1);
		}

		while(1) {
			node *tmp = pop_front(&gpool);
			if(tmp == NULL) {
				usleep(1000);
				continue;
			}

			n = 0;
			while((n += read(c_sockfd, &tmp->payload[n], sizeof(tmp->payload) - n) ) != sizeof(tmp->payload));
//			if (n <= 0) {
//				printf("ERROR reading from socket");
//				break;
//			}

#ifdef SOCKET_DEBUG
			printf("received msg '%s', size = %d\n", tmp->payload, n);
#endif
			push_back(rq_mbox, tmp);
		}
	}

	return 0;
}

void *factory_thread(void *arg)
{
	struct s_worker *me=(struct s_worker *) arg;

	printf("hello, I am a network factory\n");

	while(1) {
		node *tmp = pop_front(&gboxes[FACTORY]);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}
		struct factory_request_s *fr = (struct factory_request_s *) tmp->payload;

		printf("[FACTORY] got request\n");
		printf("op = %d, tp = %d, port = %d, clnt = %d\n", fr->op, fr->tp, fr->fctx.port, fr->fctx.clnt);

		switch(fr->tp) {
			case F_SERVER:
//todo DESTROY
				fr->fctx.init = 0;
				if (pthread_create(&fr->fctx.id, NULL, server_thread,  &fr->fctx) != 0) {
					perror("pthread_create"); while(1);
				}

				while(fr->fctx.init == 0)
					usleep(100);

				fr->op = F_OKK;
				push_back(fr->mbox_back, tmp);
				break;
			case F_CLIENT:
//todo DESTROY
				fr->fctx.init = 0;
				if (pthread_create(&fr->fctx.id, NULL, client_thread,  &fr->fctx) != 0) {
					perror("pthread_create"); while(1);
				}

				while(fr->fctx.init == 0)
					usleep(100);

				fr->op = F_OKK;
				push_back(fr->mbox_back, tmp);
				break;
		}

	}
}



void spawn_systhreads(void) {
	int i;
	int fd;
	struct ifreq ifr;

	factory.my_id = 0;
	if (pthread_create(&factory.id, NULL, factory_thread,  &factory) != 0)
		        perror("pthread_create");

}

#endif //SYSTHREADS_CXX