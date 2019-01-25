/**
\file
\brief EActors object store

*/

#include "eos.h"
#include <string.h>

//#define EOS_DEBUG

extern void printa(const char *fmt, ...);

void drop_old(struct eos_struct *st, int basket) {
	node *tmp = pop_front(&st->grace.grace);
	if(tmp == NULL) {
//		if(basket)
//			asleep(basket);
		return;
	}

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

	queue *key_top = (queue *) tmp->payload;
	node *tmp2 = pop_front(key_top);

	int k = 0;
	while(tmp2 != NULL) {
#ifdef EOS_DEBUG
		struct kvpair_struct *kvp = (struct kvpair_struct *) tmp2->payload;
		printa("[%d] DROPPING: '%s' -- '%s'\n", k++, kvp->key, kvp->value);
#endif
		push_front(&st->pool, tmp2);
		tmp2 = pop_front(key_top);
	}

	push_front(&st->pool, tmp);
}

void grace_register(struct eos_struct *st, int id) {
	st->grace.grace_mask |= (1 << id);
}


void grace_update(struct eos_struct *st, int id) {
	st->grace.grace_actors[id]++;
}

void eos_init(void *gsp, char force) {
	struct eos_struct *eos = (struct eos_struct *) gsp;
	if(eos->init && !force) {
		printa("open inited eos\n");
		return;
	}

	memset(gsp, 0x0, EOS_SIZE);
	printa("Purge and init the eos\n");

	int i;

	for(i = 0; i < MAX_BASKETS; i++) {
		queue_init(&eos->baskets[i]);
	}

	queue_init(&eos->pool);
	for (i = EOS_NODES - 1; i >= 0; i--) {
		node *tmp = &eos->array[i];
		memset(tmp->payload, 0, PL_SIZE);
		push_front(&eos->pool, tmp);
	}

	queue_init(&eos->grace.grace);

	eos->init = 1;
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

int add_to_grace(struct eos_struct *st, node *next) {
	node *tmp = pop_front(&st->pool);
	if(tmp == NULL) {
		printa("no space for a grace object. todo\n");
		return 1;
	}

	queue *single_stack = (queue *) tmp->payload;
	queue_init(single_stack);

	push_back(&st->grace.grace, tmp);
	return 0;
}


queue *find_key_top(queue *top, char *key, int size) {
	struct kvpair_struct *kvp = NULL;

	node *tmp = top->top;
	while(tmp != NULL) {
		kvp = (struct kvpair_struct *) tmp->payload;
#ifdef EOS_DEBUG
		printa("FIND: '%s', requested '%s'\n", kvp->key, key);
#endif
		if( memcmp(key, kvp->key, size) == 0 )
			break;
		tmp = tmp->header.prev;
	}

	if (tmp == NULL)
		return NULL;

	return (queue *) kvp->value;
}

int put(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value) {
	if(ctx != NULL)
		return putb(st, ctx, key, value, 0); //string based put has no sence with an encryption since it repeats behaviour of encrypted putb

	node *pair_node = pop_front(&st->pool);
	if(pair_node == NULL)
		return 1;

//todo: key+value size check
	struct kvpair_struct *kvp = (struct kvpair_struct *) pair_node->payload;
	if(strlen(key) > sizeof(kvp->key)) {
		printa("key size is wrong\n");while(1);
	}

	if(strlen(value) > sizeof(kvp->value)) {
		printa("value size is wrong\n");while(1);
	}

	memset(kvp->key, 0, sizeof(kvp->key));
	memset(kvp->value, 0, sizeof(kvp->value));
//
	memcpy(kvp->key, key, strlen(key));
	memcpy(kvp->value, value, strlen(value));
//
	queue *key_top = find_key_top(&st->baskets[st_hash(key)], key, strlen(key));
	if(key_top != NULL) {
#ifdef EOS_DEBUG
		printa("PUT: found existing queue of '%s' keys\n", key);
#endif
		node *tmp = pop_front(&st->pool);
		if(tmp == NULL) {
			push_front(&st->pool, pair_node);
			return 1;
		}
		memset(tmp->payload, 0, sizeof(tmp->payload));

		queue *single_stack = (queue *) tmp->payload;
		queue_init(single_stack);
		push_front(single_stack, pair_node);

		swap_nodes(key_top, single_stack);
		push_back(&st->grace.grace, tmp);

		return 0;
	} 
#ifdef EOS_DEBUG
	else
		printa("PUT: queue for '%s' keys does not exist\n", key);
#endif
//key does not exist, we need another key queue
//todo: convert pair nodes into key_queues
	node *fresh_key_top = pop_front(&st->pool);
	if(fresh_key_top == NULL) {
#ifdef EOS_DEBUG
		printa("PUT: no node for new keys\n");
#endif
		push_front(&st->pool, pair_node);
		return 1;
	}

	kvp = (struct kvpair_struct *) fresh_key_top->payload;
	queue_init((queue *)&kvp->value);
	memcpy(kvp->key, key, strlen(key));
	push_front(&st->baskets[st_hash(key)], fresh_key_top);
	push_front((queue *)&kvp->value, pair_node);

	return 0;
}

int get(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value) {
	if(ctx != NULL)
		return getb(st, ctx, key, value, 0); //the string-based 'put' has no sence with encryption since it repeats the behaviour of the encrypted 'putb'

	struct kvpair_struct *kvp;

	queue *key_top = find_key_top(&st->baskets[st_hash(key)], key, strlen(key));
	if(key_top == NULL) {
		printa("GET: queue of '%s' keys does not exist\n", key);
		return 1;
	}

	node *tmp = key_top->top;

	while(tmp != NULL) {
		kvp = (struct kvpair_struct *) tmp->payload;
//todo sanity check
		if( strcmp(key, kvp->key) == 0 )
			break;
		tmp = tmp->header.prev;
	}

	if(tmp == NULL)
		return 1;

	memcpy(value, kvp->value, strlen(kvp->value));

	return 0;
}

int getb(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value, int size) {
	struct kvpair_struct tmp_kvp, *kvp;

	if( ctx != NULL) {
		pack_ctr(tmp_kvp.key, key, sizeof(kvp->key), ctx, NULL);
		key = (char *) tmp_kvp.key;
	}

	queue *key_top = find_key_top(&st->baskets[st_hashb(key, sizeof(kvp->key))], key, sizeof(kvp->key));
	if(key_top == NULL) {
		printa("GETB: queue of '%s' keys does not exist\n", key);
		return 1;
	}

	node *tmp = key_top->top;

	while(tmp != NULL) {
		kvp = (struct kvpair_struct *) tmp->payload;
//todo sanity check
		if( memcmp(key, kvp->key, sizeof(kvp->key)) == 0 )
			break;
		tmp = tmp->header.prev;
	}

	if(tmp == NULL)
		return 1;

	if(ctx == NULL) {
		memcpy(value, kvp->value, size);
		return 0;
	}

	unpack_ctr((char *)&tmp_kvp.value, kvp->value, sizeof(kvp->value), ctx, NULL);

	memcpy(value, &tmp_kvp.value[sizeof(kvp->key)], size);

	return 0;
}

int putb(struct eos_struct *st, struct ctx_ctr_s *ctx, char *key, char *value, int size) {
	struct kvpair_struct tmp_kvp;

	node *pair_node = pop_front(&st->pool);
	if(pair_node == NULL)
		return 1;

//todo: key+value size check
	struct kvpair_struct *kvp = (struct kvpair_struct *) pair_node->payload;

	if(size > sizeof(kvp->value)) {
		printa("value size is wrong\n");while(1);
	}

	if( (size + sizeof(kvp->key) > sizeof(kvp->value)) && (ctx != NULL) ) {
		printa("value size is wrong (2) \n");while(1);
	}

	memset(kvp->key, 0, sizeof(kvp->key));
	memset(kvp->value, 0, sizeof(kvp->value));
//
	if( ctx != NULL) {
		memcpy(tmp_kvp.value, key, sizeof(kvp->key));
		memcpy(&tmp_kvp.value[sizeof(kvp->key)], value, size);

		pack_ctr(kvp->key, key, sizeof(kvp->key), ctx, NULL);
		key = (char *) kvp->key;

		pack_ctr(kvp->value, tmp_kvp.value, sizeof(kvp->value), ctx, NULL);
	} else {
		memcpy(kvp->key, key, sizeof(kvp->key));
		memcpy(kvp->value, value, size);
	}
//
	queue *key_top = find_key_top(&st->baskets[st_hashb(key, sizeof(kvp->key))], key, sizeof(kvp->key));
	if(key_top != NULL) {
#ifdef EOS_DEBUG
		printa("PUTB: found existing queue of '%s' keys\n", key);
#endif
		node *tmp = pop_front(&st->pool);
		if(tmp == NULL) {
			push_front(&st->pool, pair_node);
			return 1;
		}
		memset(tmp->payload, 0, sizeof(tmp->payload));

		queue *single_stack = (queue *) tmp->payload;
		queue_init(single_stack);
		push_front(single_stack, pair_node);

		swap_nodes(key_top, single_stack);
		push_back(&st->grace.grace, tmp);

		return 0;
	} 
#ifdef EOS_DEBUG
	else
		printa("PUT: queue for '%s' keys does not exist\n", key);
#endif
//key does not exist, we need another key queue
//todo: convert pair nodes into key_queues
	node *fresh_key_top = pop_front(&st->pool);
	if(fresh_key_top == NULL) {
#ifdef EOS_DEBUG
		printa("PUT: no node for new keys\n");
#endif
		push_front(&st->pool, pair_node);
		return 1;
	}

	kvp = (struct kvpair_struct *) fresh_key_top->payload;
	queue_init((queue *)&kvp->value);
	memcpy(kvp->key, key, sizeof(kvp->key));
	push_front(&st->baskets[st_hashb(key, sizeof(kvp->key))], fresh_key_top);
	push_front((queue *)&kvp->value, pair_node);

	return 0;
}
