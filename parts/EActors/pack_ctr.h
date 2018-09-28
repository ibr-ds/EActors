/**
\file
\brief A header for an encryption support
*/

#ifndef PACK_CTR_H
#define PACK_CTR_H

#include <string.h>
#include <stdlib.h>

#include "../config.h"
#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "inc/ippcp.h"

//NOTE: only this key_size was tested
#define CTR_KEY_SIZE	16
#define CTR_CTR_SIZE	16

/**
\brief A per socket encryption context. 

Do not move key/ctr0
*/
struct ctx_ctr_s {
	Ipp8u key[CTR_KEY_SIZE];	///< an encryption key
	Ipp8u ctr0[CTR_CTR_SIZE];	///< a counter
	IppsAESSpec* pAES;		///< an aes context
	int ctxSize;			///< a size
};

void init_uploaded_key(struct ctx_ctr_s *ctx);
void create_random_aes_key(struct ctx_ctr_s *ctx);
void create_aes_key(struct ctx_ctr_s *ctx, char *key, char *ctr0);
void destroy_aes_key(struct ctx_ctr_s *ctx);

void pack_ctr(char *, char *, int, struct ctx_ctr_s *, char *);
void unpack_ctr(char *, char *, int, struct ctx_ctr_s *, char *);
void alloc_ctr(struct ctx_ctr_s **ctx);
void free_ctr(struct ctx_ctr_s **ctx);
int pack_get_size_ctr();


#ifdef __cplusplus
}
#endif

#endif //PACK_CTR_H