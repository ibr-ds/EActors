/**
\file
\brief A header for an encryption support
*/

#ifndef PACK_GCM_H
#define PACK_GCM_H

#include <string.h>
#include <stdlib.h>

#include "../config.h"
#include "node.h"
#include "rsa.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GCM_KEY_SIZE	16

/**
\struct encryption context for AES GCM

\note I encrypt these variables together, do not move
*/
struct ctx_gcm_s {
	unsigned char key[GCM_KEY_SIZE];	///< encryption key
	unsigned char IV[SGX_AESGCM_IV_SIZE];	///< should be incremeted at both sides
};

void pack_gcm(char *dst, char *src, int size, struct ctx_gcm_s *ctx, char *mac);
void unpack_gcm(char *dst, char *src, int size, struct ctx_gcm_s *ctx, char *mac);
void alloc_gcm(struct ctx_gcm_s **ctx);
void free_gcm(struct ctx_gcm_s **ctx);
int pack_get_size_gcm();


#ifdef __cplusplus
}
#endif

#endif //PACK_GCM_H