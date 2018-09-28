#ifndef STORAGE_H
#define STORAGE_H

#include "../config.h"
#include "node.h"
#include "packs.h"
#include "errors.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SEALED_KEY_SIZE
    #define SEALED_KEY_SIZE 576 ///< storage relies on PACK_CTR but for compatibility...
#endif

#define STORAGE_PTADDR	(0x7ffb00000000ll) ///< always mmap a storage file at this address
#define STORAGE_SIZE	(100*1024*1024) ///< Pre defined storage size -- 100MB
#define STORAGE_NODES	((STORAGE_SIZE - (MAX_BASKETS+1)*sizeof(stack) - sizeof(char) - sizeof(struct grace_struct) - 2*SEALED_KEY_SIZE)/sizeof(node) ) // it is very ugly
#define MAX_BASKETS	32	///< number of baskets in a hash table

struct grace_struct {
	char grace_actors[MAX_ACTORS];	///< accesses counters
	unsigned long int grace_mask;	///< not all actors use the store
	stack grace;			///< todo
};

struct storage_struct {
	char sealed_key[SEALED_KEY_SIZE];
	char sealed_ctr0[SEALED_KEY_SIZE];
	char init;				///< a storage was inited previosly
	struct grace_struct grace;
	stack baskets[MAX_BASKETS];		///< an array of baskets
	stack pool;				///< a free objects pull
	node array[STORAGE_NODES];		///< an array of nodes
};

struct kvpair_struct {
	unsigned char key[64];
	unsigned char value[64];
};

extern void storage_sync(int fd);

void drop_old(struct storage_struct *st, int basket);
void grace_register(struct storage_struct *st, int id);
void grace_update(struct storage_struct *st, int id);
void storage_init(void *, char force);
int putb(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value, int size);
int getb(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value, int size);
int put(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value);
int get(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value);

void retrive_storage_keys(struct storage_struct *st, struct ectx_s *);
void seal_storage_keys(struct storage_struct *st, struct ectx_s *);

#ifdef __cplusplus
}
#endif

#endif //STORAGE_H