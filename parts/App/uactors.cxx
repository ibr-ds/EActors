#include <string.h> /* for memcpy etc */
#include <stdlib.h> /* for malloc/free etc */

#include "socket.h"
#include "actor.h"

#ifdef __cplusplus
extern "C" {
#endif
void printa(const char *fmt, ...);

#ifdef __cplusplus
}
#endif

extern void ocall_print(const char* str);

#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
void printa(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print(buf);
}

#ifdef __cplusplus
extern "C" {
#endif

void build_SEAL_master_socket(struct socket_s *sock) {
	if(!is_enc(sock))
		return;

	printf("ERROR: you are calling function %s in untrusted environment which (1) is not supported and (2) has no sense\n", __func__);
	while(1);
}

void build_LARSA_master_socket(struct socket_s *sock) {
	if(!is_enc(sock))
		return;

	printf("ERROR: you are calling function %s in untrusted environment which (1) is not supported and (2) has no sense\n", __func__);
	while(1);
}

void build_SEAL_slave_socket(struct socket_s *sock) {
	if(!is_enc(sock))
		return;

	printf("ERROR: you are calling function %s in untrusted environment which (1) is not supported and (2) has no sense\n", __func__);
	while(1);
}

void build_LARSA_slave_socket(struct socket_s *sock) {
	if(!is_enc(sock))
		return;

	printf("ERROR: you are calling function %s in untrusted environment which (1) is not supported and (2) has no sense\n", __func__);
	while(1);
}

sgx_status_t sgx_unseal_data(const sgx_sealed_data_t *p_sealed_data,
				uint8_t *p_additional_MACtext,
				uint32_t *p_additional_MACtext_length,
				uint8_t *p_decrypted_text,
				uint32_t *p_decrypted_text_length) {
}

sgx_status_t sgx_seal_data(const uint32_t additional_MACtext_length,
				const uint8_t *p_additional_MACtext,
				const uint32_t text2encrypt_length,
				const uint8_t *p_text2encrypt,
				const uint32_t sealed_data_size,
				sgx_sealed_data_t *p_sealed_data) {
	printf("ERROR: function %s should never be called by untrusted actor\n", __func__);
	while(1);
}

sgx_status_t sgx_read_rand(unsigned char *rand, size_t length_in_bytes) {
	printf("ERROR: function %s should never be called by untrusted actor\n", __func__);
	while(1);
}

void asleep(int usec) {
	ocall_usleep(usec);
}

#if 0
void storage_sync(int fd) {
	ocall_syncfs(fd);
}
#endif

int in_enclave(void) {
	return 0;
}

void aexit(int ret) {
	ocall_exit(ret);
}

void replica(int destroy) {
	printf("ERROR: you cannot call 'replica()' by an untrusted actor, die\n");while(1);
}


#include "enclave_cfg.h"

extern int (*pf[])(struct actor_s *);
extern int (*cf[])(struct actor_s *, queue *, queue *, queue *, queue *);

//there is a problem with linking of low-level sgx functions like sgx_rijndael128GCM_decrypt, I do not have time to investigate
static void * (* const volatile __memset_vp)(void *, int, size_t)
    = (memset);

#undef memset_s /* in case it was defined as a macro */

int memset_s(void *s, size_t smax, int c, size_t n)
{
    int err = 0;

    if (s == NULL) {
        err = EINVAL;
        goto out;
    }

    if (n > smax) {
        err = EOVERFLOW;
        n = smax;
    }

    /* Calling through a volatile pointer should never be optimised away. */
    (*__memset_vp)(s, c, n);

    out:
    if (err == 0)
        return 0;
    else {
        errno = err;
        /* XXX call runtime-constraint handler */
        return err;
    }
}

int
consttime_memequal(const void *b1, const void *b2, size_t len)
{
	const unsigned char *c1 = (const unsigned char *)b1, *c2 = (const unsigned char *) b2;
	unsigned int res = 0;

	while (len--)
		res |= *c1++ ^ *c2++;

	/*
	 * Map 0 to 1 and [1, 256) to 0 using only constant-time
	 * arithmetic.
	 *
	 * This is not simply `!res' because although many CPUs support
	 * branchless conditional moves and many compilers will take
	 * advantage of them, certain compilers generate branches on
	 * certain CPUs for `!res'.
	 */
	return (1 & ((res - 1) >> 8));
}


#ifdef __cplusplus
}
#endif




#define MAX_PRIVATE_BOXES	10			///< maximum number of a private boxes
static queue private_boxes[MAX_PRIVATE_BOXES];		///< per enclave array of private boxes
static queue private_pool;				///< per enclave private object pool
static struct socket_s sockets[MAX_SOCKETS] = {0};
#define PPOOL_SIZE	10

int priv_log[MAX_ACTORS][MAX_ACTORS];
int pub_log[MAX_ACTORS][MAX_ACTORS];


static int inited = 0;

int u_constructor(unsigned int id, unsigned int aid, void *aargs_p) {
	int i;
	int ret = 0;
	struct aargs_s *aargs = (struct aargs_s *) aargs_p;
//workaround. in future I will add here independant enclave entry for dynamic configuration on_boot
	if(!inited) {
		queue_init(&private_pool);

		for(i=0; i < PPOOL_SIZE; i++) {
			node *tmp = (node *) malloc(sizeof(node));
			if( tmp == NULL) {
				printf("no mem %d, die\n", __LINE__);
				while(1);
			}
			memset(tmp->payload, 0, PL_SIZE);
			push_front(&private_pool, tmp);
		}

		for(i=0; i < MAX_PRIVATE_BOXES; i++) {
			queue_init(&private_boxes[i]);
#ifdef V2
			private_boxes[i].id = i;
			private_boxes[i].type = GLOBAL;
#endif
		}

		inited = 1;
	}

	printa("[U] constructing id = %d, aid = %d\n", id,aid);
	actors[aid].id = id;
	actors[aid].body=pf[aid];
	actors[aid].ctr=cf[aid];
	actors[aid].gsp = aargs->gsp;
	actors[aid].ps = (char *)malloc(PRIVATE_STORE_SIZE);
	actors[aid].gboxes_v2 = aargs->gboxes_v2;
	actors[aid].gpool_v2 = aargs->gpool_v2;
//
#ifdef V2
	actors[aid].gboxes = aargs->gboxes;
	actors[aid].gpool = aargs->gpool;
	actors[aid].pboxes = private_boxes;
	actors[aid].ppool = &private_pool;
//
	actors[aid].sockets = sockets;
#endif
	if(actors[aid].ctr)
		ret = actors[aid].ctr(&actors[aid], aargs->gpool, &private_pool, aargs->gboxes, private_boxes);
	else {
		printa("[U] Intrusion detected: call for a non-existing consrtructor\n");
		while(1);
	}

	printa("[U] ctr%d: %d\n", id, ret);
	return ret;
}

int u_dispatcher(char leave) {
	int i;
	int ret = 0;

	while(1) {
		ret = 0;
//place for a dispatch policy
		for(i=0; i< MAX_ACTORS; i++) {
			if(ACTORS_MASK & (1<<i)) {
				if(actors[i].body)
					ret |= actors[i].body(&actors[i]);
				else {
					printa("Intrusion detected: call for a non-existing consrtructor\n");
					while(1);
				}
			}
		}
		if(leave || !ret)
			break;
	}
	return ret;
}



