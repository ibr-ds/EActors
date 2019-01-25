#ifndef EOS_H
#define EOS_H

#include "../config.h"
#include "node.h"
#include "packs.h"
#include "errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SEALED_KEY_SIZE
    #define SEALED_KEY_SIZE 576 ///< eos relies on PACK_CTR but for compatibility...
#endif

#define EOS_PTADDR	(0x7ffb00000000ll) ///< always mmap a eos file at this address
#define EOS_SIZE	(50*1024*1024) ///< Pre defined eos size
#define MAX_BASKETS	4096	///< number of baskets in a hash table
#define EOS_NODES	((EOS_SIZE - (MAX_BASKETS+1)*sizeof(queue) - sizeof(char) - sizeof(struct grace_struct) - 2*SEALED_KEY_SIZE)/sizeof(node) ) // it is very ugly

struct grace_struct {
	char grace_actors[MAX_ACTORS];	///< accesses counters
	unsigned long int grace_mask;	///< not all actors use the store
	queue grace;			///< todo
};

struct eos_struct {
	char sealed_key[SEALED_KEY_SIZE];
	char sealed_ctr0[SEALED_KEY_SIZE];
	char init;				///< an eos was inited previosly
	struct grace_struct grace;
	queue baskets[MAX_BASKETS];		///< an array of baskets
	queue pool;				///< a free objects pull
	node array[EOS_NODES];			///< an array of nodes
};

struct kvpair_struct {
	unsigned char key[64];
	unsigned char value[PL_SIZE-64];
};

//extern void eos_sync(int fd);

void drop_old(struct eos_struct *st, int basket);
void grace_register(struct eos_struct *st, int id);
void grace_update(struct eos_struct *st, int id);
void eos_init(void *, char force);
int putb(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value, int size);
int getb(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value, int size);
int put(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value);
int get(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value);

void retrive_eos_keys(struct eos_struct *st, struct ctx_ctr_s *);
void seal_eos_keys(struct eos_struct *st, struct ctx_ctr_s *);

#ifdef __cplusplus
}
#endif

#endif //EOS_H