/**
\file
\brief A persistent storage implementation

\deprecated

Storage:
- Key-Value object store
- Implemented on top of atomic stack
- Uses memory mapped file as a backend
- System arctor -- removes old nodes from a storage
- Node can be removed after grace period

Pros/cons:
- Worst case: o(n) get, o(n) put
- Best case: o(1) put, o(1) get
- Increasing of basket numbers reduces a search time
*/

#include "storage.h"
#include <string.h>

extern void printa(const char *fmt, ...);

void drop_old(struct storage_struct *st, int basket) {
	stack drop;
	node *tmp = st->grace.grace.top; //fix top

	if(tmp == NULL)
		return;

	drop.top = tmp->next;
	tmp->next = NULL; //cut top

	int i;
	char start[MAX_ACTORS];
	unsigned long int mask = st->grace.grace_mask;

	memcpy(start, st->grace.grace_actors, sizeof(st->grace.grace_actors));
	while(mask != 0) {
		for(i = 0; i < MAX_ACTORS * MAX_ENCLAVES; i++) {
			if(mask & (1 << i)) {
				if(start[i]!=st->grace.grace_actors[i])
					mask &= ~(1<<i);
			}
		}
	}
//a grace period has been ended and all actors will work with fresh values of object. now we can drop all of them

//because of stack we cannot drop items without reordering.
//assuming D->C->B->A, and we need to drop B and C. B is older than C, and thus, in stack we will have have DC (left is the top).
//If we firstly drop next(D), i.e. C, then we will not find B which is a next(C), because C will be out of the game.
	stack rdrop;
	queue_init(&rdrop);

	tmp = pop(&drop);
	while(tmp!=NULL) {
		push(&rdrop, tmp);
		tmp = pop(&drop);
	}

	tmp = pop(&rdrop);
	while(tmp!=NULL) {
		stack *single_stack = (stack *) tmp->payload;
#ifdef DEBUG
printa("top of drop = %x\n", single_stack->top);
#endif
		node *del = single_stack->top->next;
		single_stack->top->next = single_stack->top->next->next;
#ifdef DEBUG
printa("dropping node %x\n", del);
#endif
		push(&st->pool, del);
		push(&st->pool, tmp);
		tmp = pop(&rdrop);
	}
}

void grace_register(struct storage_struct *st, int id) {
	st->grace.grace_mask |= (1 << id);
}


void grace_update(struct storage_struct *st, int id) {
	st->grace.grace_actors[id]++;
}

void storage_init(void *gsp, char force) {
	struct storage_struct *storage = (struct storage_struct *) gsp;
	if(storage->init && !force) {
		printa("open inited storage\n");
		return;
	}

	memset(gsp, 0x0, STORAGE_SIZE);
	printa("Purge and init the storage\n");

	int i;

	for(i = 0; i < MAX_BASKETS; i++) {
		queue_init(&storage->baskets[i]);
	}

	queue_init(&storage->pool);
	for (i = STORAGE_NODES - 1; i >= 0; i--) {
		node *tmp = &storage->array[i];
		memset(tmp->payload, 0, PL_SIZE);
		push(&storage->pool, tmp);
	}

	queue_init(&storage->grace.grace);

	storage->init = 1;
}

static int st_hash(char *key) {
	unsigned long hash = 5381;
	int c;
	
	while(c = *key++)
		hash = ((hash << 5) + hash ) + c;

	return hash % MAX_BASKETS;
}

/**

I do not know how good this function for a binary data
*/
static int st_hashb(char *key, int len) {
	unsigned long hash = 5381;
	int c;

	for(c = 0; c < len; c++)
		hash = ((hash << 5) + hash ) + key[c];

	return hash % MAX_BASKETS;
}

void add_to_grace(struct storage_struct *st, node *next) {
	node *tmp = pop(&st->pool);
	if(tmp == NULL) {
		printa("no space for a grace object. todo\n"); while(1);
	}

	stack *single_stack = (stack *) tmp->payload;
	single_stack->top = next;

	push(&st->grace.grace, tmp);
}

/**

size is not used in encrypted mode
*/
int putb(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value, int size) {
	node *tmp = pop(&st->pool);
	if(tmp == NULL)
		return 1;

	struct kvpair_struct *kvp = (struct kvpair_struct *) tmp->payload;
//todo sanity check
	memset(kvp->key, 0, sizeof(kvp->key));
	memset(kvp->value, 0, sizeof(kvp->value));

	if(ctx == NULL) {
		memcpy(kvp->key, key, strlen(key));
		memcpy(kvp->value, value, size);
	} else {
		ctr_pack(kvp->key, key, sizeof(kvp->key), ctx);
		ctr_pack(kvp->value, value, sizeof(kvp->value), ctx);
		key = kvp->key;
	}
	push(&st->baskets[st_hashb(kvp->key, sizeof(kvp->key))], tmp);
//we need to find a previous key and add it into a grace list
//NB: while we have pushed tmp and reloaded a kvp, the key still points to an encrypted key
//this node cannot be corrupted, because of grace period and grace flag, which cannot be updated until we will not leave this function
//this is a bad writing style, but I do not want add here one more memcpy
	while(tmp->next != NULL) {
		kvp = (struct kvpair_struct *) tmp->next->payload;
		if( memcmp(key, kvp->key, sizeof(kvp->key)) == 0 )
			break;
		tmp = tmp->next;
	}

	if(tmp->next == NULL)
		return 0;

// now tmp points to a top of prevous object
	add_to_grace(st, tmp);

	return 0;
}

/**
size not used for a decryption
*/
int getb(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value, int size) {
	struct kvpair_struct *kvp;
	node *tmp;
//because we cannot and do not want to decrypt all keys, let we just encrypt a key and find an encrypted form of the key
//NB: I have no idea about AES counter here. Maybe another encryption algorithm should be used

	unsigned char enc_key[64]; //sizeof(kvp->key);
	if( ctx!= NULL) {
		ctr_pack(enc_key, key, sizeof(kvp->key), ctx);
		key = enc_key; //see the commend about memcpy above
	} 
	tmp = st->baskets[st_hashb(key, sizeof(kvp->key))].top;
	while(tmp != NULL) {
		kvp = (struct kvpair_struct *) tmp->payload;
//todo sanity check
		if( memcmp(key, kvp->key, sizeof(kvp->key)) == 0 )
			break;
		tmp = tmp->next;
	}

	if(tmp == NULL)
		return 1;

	if(ctx == NULL)
		memcpy(value, kvp->value, size==0 ? sizeof(kvp->value) : size);
	else
		ctr_unpack(value, kvp->value, sizeof(kvp->value), ctx);

	return 0;
}

int put(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value) {
	if(ctx != NULL)
		return putb(st, ctx, key, value, 0); //string based put has no sence with an encryption since it repeats behaviour of encrypted putb

	node *tmp = pop(&st->pool);
	if(tmp == NULL)
		return 1;

	struct kvpair_struct *kvp = (struct kvpair_struct *) tmp->payload;

//todo sanity check
	memset(kvp->key, 0, sizeof(kvp->key));
	memset(kvp->value, 0, sizeof(kvp->value));
	memcpy(kvp->key, key, strlen(key));
	memcpy(kvp->value, value, strlen(value));

	push(&st->baskets[st_hash(key)], tmp);
// we need to find a previous key and add it into a grace list
	while(tmp->next != NULL) {
		kvp = (struct kvpair_struct *) tmp->next->payload;
//todo sanity check
		if( strcmp(key, kvp->key) == 0 )
			break;
		tmp = tmp->next;
	}

	if(tmp->next == NULL)
		return 0;
// now tmp points to a top of prevous object
#ifdef DEBUG
	printa("top of old node = %x, next %x\t%s\t%s\n", tmp, tmp->next, kvp->key, kvp->value);
#endif
	add_to_grace(st, tmp);

	return 0;
}

int get(struct storage_struct *st, struct ectx_s *ctx, char *key, char *value) {
	if(ctx != NULL)
		return getb(st, ctx, key, value, 0); //the string based 'put' has no sence with encryption since it repeats the behaviour of the encrypted 'putb'

	struct kvpair_struct *kvp;

	node *tmp = st->baskets[st_hash(key)].top;
	while(tmp != NULL) {
		kvp = (struct kvpair_struct *) tmp->payload;
//todo sanity check
		if( strcmp(key, kvp->key) == 0 )
			break;
		tmp = tmp->next;
	}

	if(tmp == NULL)
		return 1;

	memcpy(value, kvp->value, strlen(kvp->value));

	return 0;
}
