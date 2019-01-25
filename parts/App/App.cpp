/**
\file
\brief Untrusted code

This file contains elements of untrusted infrastructure of the Framework.
Mostly, this code spawns enclaves, workers and allocates shared objects.

*/

#include <stdio.h>
#include "Enclave_u.h"
#include "sgx_urts.h"
#include "sgx_utils/sgx_utils.h"

#include <sys/time.h>
#include <sys/stat.h> 
#include <fcntl.h>
#include <unistd.h>

#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>

#include "node.h"
#include "stack.h"

#include "config.h"


sgx_enclave_id_t eid[MAX_ENCLAVES]; ///< array for enclave's identificators
sgx_enclave_id_t eid_dyn[MAX_DYN_ENCLAVES]; ///< array for enclave's identificators


int eid_dyn_ctr = 0;


//staticaly allocated boxes
queue gboxes[MAX_GBOXES]; ///< message gboxes
queue gpool; ///< a global free memory pool

#ifdef V2
queue eboxes[MAX_ENCLAVES]; ///< message boxes for trusted system actors
#endif

//dynamicaly allocated boxes
queue gboxes_v2; ///< a second version of mbox assigning
queue gpool_v2[MAX_GBOXES];

//storae globals
void *gsp = NULL; //memory mapped store
int gsfd = -1;

//OCalls
void ocall_syncfs(int fd) {
	fd = gsfd; //for now
	syncfs(fd);
}

void ocall_usleep(int usec) {
	usleep(usec);
}

void ocall_exit(int ret) {
	printf("emergency exit was called\n");
	for (int i = 0; i < MAX_ENCLAVES; i++) {
		sgx_destroy_enclave(eid[i]);
	}
	exit(ret);
}

struct timeval pstart, pend;
void ocall_print(const char* str) {
	gettimeofday(&pend, NULL);
//	printf("%ld\t%s",((pend.tv_sec-pstart.tv_sec)*1000000 + pend.tv_usec - pstart.tv_usec),str);
	printf("%s",str);
	gettimeofday(&pstart, NULL);
}

//
#include "worker.h"
#include "aargs.h"
#include "eos.h"


#include "iworkers.cxx"
#include "systhreads.cxx"
#include "uactors.cxx"

////////
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

///////


/**
\brief a worker thread
*/
void *e_thread(void *arg)
{
	struct s_worker *me = (struct s_worker *)arg;
	int ret;
	int ptr;
	cpu_set_t cpuset;
	sgx_status_t status;

	int i;
	CPU_ZERO(&cpuset);
	for(i = 0; i < 8; i = i + 1) {
		if(me->cpus_mask & (1 << i))
			CPU_SET(i, &cpuset);
	}

	if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) < 0) {
		perror("pthread_setaffinity_np");
	}

	char my_enclaves[MAX_ENCLAVES];
	int total_enclaves = 0;
	for(i = 0; i < MAX_ENCLAVES; i++) {
		if(me->enclaves_mask & (1 << i)) {
			my_enclaves[total_enclaves++] = i;
		}
	}

	i = 0;

	printa("spawn worker %d with 0x%x enclaves\n", me->my_id, total_enclaves);

	if(total_enclaves == 0) {
		printf("total enclaved == 0, should never happens \n");
		while(1)
			sleep(1);
	}

	while(1) {
//place for an enclave switching policy
		ret = 0;
		ptr = 0;
		for( i = 0; i < total_enclaves; i++) {
			if(my_enclaves[i] == -1) //ugly workaround, but it is better than add 'done' state to actors
				continue;

			if(my_enclaves[i] != 0) {
				status = dispatcher(eid[my_enclaves[i]], &ptr, me->my_id, total_enclaves > 1);
				if (status != SGX_SUCCESS) {
					printf("[%d]\tecall error %x, i = %d\n", __LINE__, status, i);while(1);
				}
			} else {
//				printa("untrusted dispatcher\n");
				ptr = u_dispatcher(total_enclaves > 1);
			}

			if(ptr == 0)
				my_enclaves[i] = -1; //disable this enclave as soon all actors return zero

			ret |= ptr;
		}
		if (ret == 0)
			return NULL;
	}
}

int main(int argc, char const *argv[]) {
	int ret;
	sgx_status_t status;
	int ptr;


	int n_work, n_e;
	struct timeval pstart, pend;

	int w, i , j;

	assign_workers();

//init enclaves
	for(i = 1; i < MAX_ENCLAVES; i++) {
		eid[i]=i;
		char epath[50];
		char tpath[50];
		snprintf(epath, 50, "Enclave%d/enclave.signed.so", i);
		snprintf(tpath, 50, "Enclave%d/enclave.token", i);
		printf("creating enclave %s\n", epath);
		ret = initialize_enclave(&eid[i], tpath, epath);
		if(ret < 0) {
			printf("Fail to initialize enclave %d\n", i);
			return 1;
		}
	}

//init a store

//An EOS depends on stacks by design and cannot work with queue
	gsfd = open("./eos", O_RDWR | O_CREAT,  S_IRWXU | S_IRGRP | S_IROTH);
	if((gsp = mmap((void *)EOS_PTADDR, EOS_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,gsfd,0)) == MAP_FAILED || (unsigned long int) gsp!=EOS_PTADDR) {
		perror("Error in mmap, do not forget 'dd if=/dev/zero of=./eos bs=10M count=1' ");
		printf("gsp = %lx\n", (long unsigned int) gsp);
		while(1);
	}
	printf("gsp = %lx with size = %d\n", (unsigned long int) gsp, EOS_SIZE);

	eos_init(gsp, 1);

//init global pools and gboxes
	queue_init(&gpool);

	for(i = 0; i < MAX_NODES; i++) {
		node *tmp = (node *) malloc(sizeof(node));
		memset(tmp->payload, 0, PL_SIZE);
		push_front(&gpool, tmp);
	}

	for(i = 0; i < MAX_GBOXES; i++) {
		queue_init(&gboxes[i]);
#ifdef V2
		gboxes[i].id = i;
		gboxes[i].type = GLOBAL;
#endif
	}

#ifdef V2
	for(i = 0; i < MAX_ENCLAVES; i++) {
		queue_init(&eboxes[i]);
		eboxes[i].id = i;
		eboxes[i].type = GLOBAL;
	}
#endif

//gboxes version 2
//this interface can be used for dynamic allocation and use of mboxes
//However, static assignment will be used still. for some cases this way is not sutable, because two communicators should know each other
//for an 'avatar' 'chat' it is not the case, since all posible participants uses publish own mboxes and IDs globally.
//however #2. For a chat I can use also only dynamic mboxes event for networking
//in this case I will add in aargs_struct named fields for OPENNER, SENDER and etc
//which is in fact is the same using global alliases. 
	queue_init(&gboxes_v2);

	for(i = 0; i < MAX_GBOXES; i++) {
//init pools
		queue_init(&gpool_v2[i]);
		for(j = 0; j < 10; j++) {
			node *tmp = (node *) malloc(sizeof(node));
			memset(tmp->payload, 0, sizeof(tmp->payload));
			push_front(&gpool_v2[i], tmp);
		}
//init boxes
		node *tmp = (node *) malloc(sizeof(node));
		queue *tmp2 = (queue *) tmp->payload;
		queue_init(tmp2);
#ifdef V2
		tmp2->id = i;
		tmp2->type = GLOBAL;
#endif
		push_front(&gboxes_v2, tmp);
	}

	spawn_systhreads();

//construct actors
	while(1) {
		int stop = 0;
		for(w = 0; w < MAX_WORKERS; w++) {
			for(i = 0;i < MAX_ENCLAVES; i++) {
				for(j = 0; j < MAX_ACTORS; j++) {
//now we construct existing actors only
					if(!(workers[w].actors_mask[i] & (1<<j)))
						continue;
//
					struct aargs_s aargs;
					aargs.gpool = &gpool;
					aargs.gboxes = gboxes;
#ifdef V2
					aargs.eboxes = eboxes;
					aargs.my_box = &eboxes[i];
#endif
					aargs.gsp =  (struct eos_struct *) gsp;
					aargs.gboxes_v2 = &gboxes_v2;
					aargs.gpool_v2 = gpool_v2;

					if(i != 0) {
						status = constructor(eid[i], &ptr, i, j, (void *) &aargs);
						if (status != SGX_SUCCESS)
							printf("[%d]\tecall error %x\n", __LINE__, status);
					} else
						ptr = u_constructor(i*MAX_ACTORS+j, j, (void *) &aargs);
					stop |= ptr;
				}
			}
		}
		if(!stop)
		    break;
	}


//spawn workers
	for( i = 0; i < MAX_WORKERS; i++) {
		workers[i].my_id = i;
		if (pthread_create(&workers[i].id, NULL, e_thread,  &workers[i]) != 0)
		        perror("pthread_create");
	}

	for(i = 0; i < MAX_WORKERS; i++) {
		pthread_join(workers[i].id, NULL);
		printf("Static worker %d has stopped\n", i);
	}

	void *retval;

	while(1) {
		for(i = 0; i < MAX_DYN_WORKERS; i++) {
			if( (dyn_workers[i].id != 0) && pthread_tryjoin_np(dyn_workers[i].id, &retval) == EBUSY)
				break;
		}
		if(i == MAX_DYN_WORKERS)
		    break;
	}

	printf("All actors done, destroy enclaves\n");

	for (int i = 0; i < MAX_ENCLAVES; i++) {
		sgx_destroy_enclave(eid[i]);
	}

	return 0;
}
