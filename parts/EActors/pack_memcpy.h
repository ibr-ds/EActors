/**
\file
\brief memcpy-based encryption, for testing purposes only
*/

#ifndef PACK_MEMCPY_H
#define PACK_MEMCPY_H

#include <string.h>
#include <stdlib.h>

#include "../config.h"
#include "node.h"

#ifdef __cplusplus
extern "C" {
#endif


struct ctx_memcpy_s {
	unsigned char key[16]; /// <for compatibility
};


void pack_memcpy(char *, char *, int);
void unpack_memcpy(char *, char *, int);
void alloc_memcpy(struct ctx_memcpy_s **ctx);
int pack_get_size_memcpy();

#ifdef __cplusplus
}
#endif

#endif //PACK_MEMCPY_H