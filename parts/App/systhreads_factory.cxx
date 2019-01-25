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

//#define DEBUG_SOCKETS
#define NON_BLOCKED 1
///

struct s_worker	factory;
/**
\brief 
*/
void *accepter_thread(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;
	queue *mbox = fctx->mbox;

	printf("creating NEW accepter, mbox = %p, port = %d \n", mbox, fctx->port);

	int sockfd, newsockfd, portno, clilen;
	struct sockaddr_in serv_addr, cli_addr;
	int  n, rc, on = 1;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if( sockfd < 0) {
		perror("sock create\n");while(1);
	}
#ifdef NON_BLOCKED
	rc = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));
	if (rc < 0) {
		perror("setsockopt() failed");
		while(1);
	}

	rc = ioctl(sockfd, FIONBIO, (char *)&on);
	if (rc < 0) {
		perror("ioctl() failed");
		while(1);
	}
#endif

	bzero((char *) &serv_addr, sizeof(serv_addr));
	portno = fctx->port;

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

// re-bind if a sock is openned still from previous run.
	while(bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0);
//sysctl -w net.core.somaxconn=2048
	listen(sockfd, 2048);
	clilen = sizeof(cli_addr);

	fctx->init = 1;

	while(1) {
		node *tmp = pop_front(mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

		struct tcp_sock *tcp_sock = (struct tcp_sock *) tmp->payload;
		tcp_sock->sockfd = accept(sockfd, (struct sockaddr *)&tcp_sock->cli_addr, (socklen_t *)&tcp_sock->clilen);

		int rc, on = 1;
		if(tcp_sock->sockfd != -1) {
#ifdef NON_BLOCKED
			rc = setsockopt(tcp_sock->sockfd, SOL_SOCKET,  SO_REUSEADDR, (char *)&on, sizeof(on));
			if (rc < 0) {
				perror("setsockopt() failed");
				while(1);
			}

			rc = ioctl(tcp_sock->sockfd, FIONBIO, (char *)&on);
			if (rc < 0) {
				perror("ioctl() failed");
				while(1);
			}
#endif
		}

		push_front(tcp_sock->mbox, tmp);
	}

	return 0;
}

void *pipe_reader_thread(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;
	queue *mbox = fctx->mbox;

	printf("creating NEW PIPE reader, mbox = %p \n", mbox);

	fctx->init = 1;
	while(1) {
		node *tmp = pop_front(mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

		struct req *to_read = (struct req *) tmp->payload;
		int will_read = to_read->size;
//			printf("READER: sock = %d, data = %p, size = %d, mbox=%p\n", to_read->sockfd, to_read->data, to_read->size, to_read->mbox);

		char myfifo[] = "/tmp/myfifo"; 
		mkfifo(myfifo, 0666); 
		int fd = open(myfifo, O_RDONLY); 

		to_read->size = read(fd, to_read->data, to_read->size);

		push_back(to_read->mbox, tmp);

		close(fd);
	}
}

void *pipe_writer_thread(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;
	queue *mbox = fctx->mbox;

	printf("creating NEW PIPE writer, mbox = %p \n", mbox);

	fctx->init = 1;

	while(1) {
		node *tmp = pop_front(mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

		int written = 0;
		struct req *to_write = (struct req *) tmp->payload;

		char myfifo[] = "/tmp/myfifo"; 
		mkfifo(myfifo, 0666); 
		int fd = open(myfifo, O_WRONLY); 

		int ret = 0;
		while(1) {
			ret = write(fd, &to_write->data[written], to_write->size - written);
			if(ret <= 0) {
				perror("write:");
			}

			written += ret;

			if(written == to_write->size)
				break;

		}

		to_write->size = written;

		push_back(to_write->mbox, tmp);

		close(fd);
	}
}


void *reader_thread(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;
	queue *mbox = fctx->mbox;

	printf("creating NEW reader, mbox = %p \n", mbox);

	fctx->init = 1;
	while(1) {
		node *tmp = pop_front(mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

		struct req *to_read = (struct req *) tmp->payload;
		int will_read = to_read->size;
//			printf("READER: sock = %d, data = %p, size = %d, mbox=%p\n", to_read->sockfd, to_read->data, to_read->size, to_read->mbox);

		if(to_read->sockfd < 0) {
			printf("read sock is wrong\n");while(1);
		}

		to_read->size = read(to_read->sockfd, to_read->data, to_read->size);
#ifdef DEBUG_SOCKETS
		if(to_read->size >= 0)
			printf("Here is the message: %s, %d\n",to_read->data, to_read->size);
		else {
			printf("READER: %d bytes read\n", to_read->size);perror("reader\n");
		}
#endif
//probably errno would be enough
		if(errno == EAGAIN && to_read->size == -1)
			to_read->size = -2;

		push_back(to_read->mbox, tmp);
	}
}


void *writer_thread(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;
	queue *mbox = fctx->mbox;

	printf("creating NEW writer, mbox = %p \n", mbox);

	fctx->init = 1;

	while(1) {
		node *tmp = pop_front(mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

		int written = 0;
		struct req *to_write = (struct req *) tmp->payload;

		if(to_write->sockfd < 0) {
			printf("write sock is wrong\n");while(1);
		}

#ifdef DEBUG_SOCKETS
		printf("to_write->data = %s, sock = %d, size = %d\n", to_write->data, to_write->sockfd, to_write->size);
#endif
		int ret = 0;
		while(1) {
			ret = send(to_write->sockfd, &to_write->data[written], to_write->size - written, MSG_NOSIGNAL);
			if(ret == -1) {
				if(errno != EAGAIN) {
					written = -1;
					printf("written -1\n");
					break;
				} else
					written = 0;
			}

			written += ret;

			if(written == to_write->size)
				break;

		}

		to_write->size = written;

		push_back(to_write->mbox, tmp);
	}
}


void *closer_thread(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;

	queue *mbox = fctx->mbox;

	printf("creating NEW closer, mbox = %p \n", mbox);

	fctx->init = 1;

	while(1) {
		node *tmp = pop_front(mbox);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}

		struct req *to_close = (struct req *) tmp->payload;
#ifdef DEBUG_SOCKETS
		printf("closing sock %d\n", to_close->sockfd);
#endif
		close(to_close->sockfd);

		push_front(&gpool, tmp);
	}
}


/**
\brief 
*/
void *timer(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;

	unsigned long int delay = fctx->port;
	queue *mbox = fctx->mbox;

	printf("creating NEW timer for %ld usec, mbox = %p \n", delay, mbox);

	fctx->init = 1;

	while(1) {
		node *tmp = nalloc(&gpool);
		if(tmp == NULL) {
			usleep(1000000);
			continue;
		}

		usleep(delay);
		push_front(mbox, tmp); // insta
	}
}


/**
\brief server thread
*/
void *new_worker(void *arg) {
	struct factory_ctx_s *fctx = (struct factory_ctx_s *) arg;

	char epath[50];
	char tpath[50];
	int ret;

	int me = MAX_ENCLAVES + eid_dyn_ctr;	// this id is used to distinct different enclaves 
	int eid = eid_dyn_ctr;			//this is an offset inside data structures
	int enclave_dir = fctx->port;

	snprintf(epath, 50, "Enclave%d/enclave.signed.so", enclave_dir);
	snprintf(tpath, 50, "Enclave%d/enclave.token", enclave_dir);

	printf("creating NEW enclave %s\n", epath);

	queue *my_box = fctx->mbox;

	eid_dyn[eid] = me;

	ret = initialize_enclave(&eid_dyn[eid], tpath, epath);
	if(ret < 0) {
		printf("Fail to initialize enclave %d\n", enclave_dir);while(1);
	}

	while(1) {
		int stop = 0;
		int ptr = 0;
		for(int j = 0; j < MAX_ACTORS; j++) {
			struct aargs_s aargs;
			aargs.gpool = &gpool;
			aargs.gboxes = gboxes;
#ifdef V2
			aargs.eboxes = eboxes;
			aargs.my_box = my_box;
#endif
			aargs.gsp =  (struct eos_struct *) gsp;
			aargs.gboxes_v2 = &gboxes_v2;
			aargs.gpool_v2 = gpool_v2;

			int status = constructor(eid_dyn[eid], &ptr, me, j, (void *) &aargs);
			if (status != SGX_SUCCESS)
				printf("[%d]\tecall error %x\n", __LINE__, status);
			stop |= ptr;
		}
		if(!stop)
		    break;
	}
	fctx->init = 1;

	int ptr = 0;
	int status = dispatcher(eid_dyn[eid], &ptr, fctx->clnt, 0);
	if (status != SGX_SUCCESS)
		printf("[%d]\tecall error %x\n", __LINE__, status);

	sgx_destroy_enclave(eid_dyn[eid]);

	printf("Dynamic Enclave%d (eid %d, me %d) done\n", enclave_dir, eid, me);

	dyn_workers[eid].id = 0;
}


void *client_thread(void *arg) {
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
void *server_thread(void *arg) {
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

void *factory_thread(void *arg) {
	struct s_worker *me=(struct s_worker *) arg;

	extern sgx_enclave_id_t eid_dyn[MAX_DYN_ENCLAVES];
	extern int eid_dyn_ctr;

	int i = 0;

	printf("hello, I am factory\n");

	while(1) {
		node *tmp = pop_front(&gboxes[FACTORY]);
		if(tmp == NULL) {
			usleep(1000);
			continue;
		}
		struct factory_request_s *fr = (struct factory_request_s *) tmp->payload;

		printf("[FACTORY] got request in message %p\n", tmp);
//		printf("op = %d, tp = %d, port = %d, clnt = %d\n", fr->op, fr->tp, fr->fctx.port, fr->fctx.clnt);

		fr->fctx.init = 0;
		fr->op = F_FAIL;
		do {
			for(i = 0; i < MAX_DYN_WORKERS; i++)
				if(dyn_workers[i].id == 0) {
					eid_dyn_ctr = i;
					break;
			}

			if(i == MAX_DYN_WORKERS) {
				printf(" MAX_DYN_WORKERS = %d reached\n", MAX_DYN_WORKERS);
				break;
			}

			int ret;

			switch(fr->tp) {
				case F_CLOSER:
					ret = pthread_create(&fr->fctx.id, NULL, closer_thread,  &fr->fctx);
					break;
				case F_READER:
					ret = pthread_create(&fr->fctx.id, NULL, reader_thread,  &fr->fctx);
					break;
				case F_WRITER:
					ret = pthread_create(&fr->fctx.id, NULL, writer_thread,  &fr->fctx);
					break;
				case F_ACCEPTER:
					ret = pthread_create(&fr->fctx.id, NULL, accepter_thread,  &fr->fctx);
					break;
				case F_PIPE_READER:
					ret = pthread_create(&fr->fctx.id, NULL, pipe_reader_thread,  &fr->fctx);
					break;
				case F_PIPE_WRITER:
					ret = pthread_create(&fr->fctx.id, NULL, pipe_writer_thread,  &fr->fctx);
					break;
// not tested:
				case F_ENCLAVE:
					ret = pthread_create(&fr->fctx.id, NULL, new_worker,  &fr->fctx);
					break;
				case F_TIMER:
					ret = pthread_create(&fr->fctx.id, NULL, timer,  &fr->fctx);
					break;
				case F_SERVER:
					ret = pthread_create(&fr->fctx.id, NULL, server_thread,  &fr->fctx);
					break;
				case F_CLIENT:
					ret = pthread_create(&fr->fctx.id, NULL, client_thread,  &fr->fctx);
					break;
			}

			if (ret != 0) {
				perror("pthread_create"); while(1);
			}

			dyn_workers[i].id = fr->fctx.id;

			while(fr->fctx.init == 0)
				usleep(100);

			eid_dyn_ctr++;
			fr->op = F_OKK;

		} while(0);

		push_back(fr->mbox_back, tmp);
	}
}

void spawn_systhreads(void) {
	int i;
	int fd;
	struct ifreq ifr;

	factory.my_id = 0;
	if (pthread_create(&factory.id, NULL, factory_thread,  &factory) != 0)
		        perror("pthread_create");

	if(TCP_PORT != 0) {
//spawn legacy system actors
		spawn_service(&gpool, gboxes, F_CLOSER, 0, 0, &gboxes[CLOSER]);
		spawn_service(&gpool, gboxes, F_READER, 0, 0, &gboxes[READER]);
		spawn_service(&gpool, gboxes, F_WRITER, 0, 0, &gboxes[WRITER]);
		spawn_service(&gpool, gboxes, F_ACCEPTER, 0, TCP_PORT, &gboxes[ACCEPTER]);
	}
}


#endif //SYSTHREADS_CXX