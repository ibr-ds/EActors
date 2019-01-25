/**
\file
\brief AES_CTR-based encryption. partly deprecated since it does not update the counter and is used as deterministic encryption for the POS

*/

#include "pack_ctr.h"
#include <sgx_trts.h>

extern void printa(const char *fmt, ...);

void init_uploaded_key(struct ctx_ctr_s *ctx) {
	ippsAESGetSize(&ctx->ctxSize);
	ctx->pAES = (IppsAESSpec*) malloc(ctx->ctxSize);
}

void create_random_aes_key(struct ctx_ctr_s *ctx) {
	sgx_read_rand(ctx->key, CTR_KEY_SIZE);
	sgx_read_rand(ctx->ctr0, CTR_CTR_SIZE);
	ippsAESGetSize(&ctx->ctxSize);
	ctx->pAES = (IppsAESSpec*) malloc(ctx->ctxSize);
}

void create_aes_key(struct ctx_ctr_s *ctx, char *key, char *ctr0) {
	memcpy(ctx->key, key, CTR_KEY_SIZE);
	memcpy(ctx->ctr0, ctr0, CTR_CTR_SIZE);
	ippsAESGetSize(&ctx->ctxSize);
	ctx->pAES = (IppsAESSpec*) malloc(ctx->ctxSize);
}

void destroy_aes_key(struct ctx_ctr_s *ctx) {
	memset(ctx->key, 0, CTR_KEY_SIZE);
	memset(ctx->ctr0, 0, CTR_CTR_SIZE);
	free(ctx->pAES);
}


/**
\brief an AES_CTR-based pack function
\param[out] *dst where encrypted data will be stored
\param[in] *src data to encrypt
\param[in] size a size of a data to encrypt
\param[in] *ctx an encryption context
\param[in] *mac MAC

\note data should be 64 byte aligned
*/
void pack_ctr(char *dst, char *src, int size, struct ctx_ctr_s *ctx, char *mac) {
//todo: should be CTR+MAC, but not just CTR 

	ippsAESInit(ctx->key, CTR_KEY_SIZE, ctx->pAES, ctx->ctxSize);

	char ctr[CTR_CTR_SIZE];
	memcpy(ctr, ctx->ctr0, CTR_CTR_SIZE);

	int ret = ippsAESEncryptCTR((const unsigned char*) src, (unsigned char*)dst, size, ctx->pAES, ctr, 64);

	if(ret != ippStsNoErr) {
		printa("pack does not work, error = %d\n", ret);while(1);
	}

	//I should update the counter but I do not do this because I use CTR the deterministic encryption for the EOS

	ippsAESInit(0, CTR_KEY_SIZE, ctx->pAES, ctx->ctxSize);
}

/**
\brief an AES_CTR-based unpack function
\param[out] *dst where decrypted data will be stored
\param[in] *src data to decrypt
\param[in] size a size of a data to decrypt
\param[in] *ctx an decryption context
\param[in] *mac MAC

\note data should be 64 byte aligned
\note todo: should be CTR+MAC, but not just CTR 
*/
void unpack_ctr(char *dst, char *src, int size, struct ctx_ctr_s *ctx, char *mac) {
	ippsAESInit(ctx->key, CTR_KEY_SIZE, ctx->pAES, ctx->ctxSize);

	char ctr[CTR_CTR_SIZE];
	memcpy(ctr, ctx->ctr0, CTR_CTR_SIZE);

	int ret = ippsAESDecryptCTR((const unsigned char*) src, (unsigned char *) dst, size, ctx->pAES, ctr, 64);
	if(ret != ippStsNoErr) {
		printa("unpack does not work, error = %d\n", ret);while(1);
	}

	//I should update the counter but I do not do this because I use CTR the deterministic encryption for the EOS

	ippsAESInit(0, CTR_KEY_SIZE, ctx->pAES, ctx->ctxSize);
}

/**
\brief memory allocation for the context
\param[in] **ctx where to allocate
*/
void alloc_ctr(struct ctx_ctr_s **ctx) {
	*ctx = malloc(sizeof(struct ctx_ctr_s));
	if(*ctx == NULL) {
		printa("[%d] malloc failed, die\n",__LINE__);while(1);
	}

	memset(*ctx, 0, sizeof(struct ctx_ctr_s));

	sgx_read_rand((*ctx)->key, CTR_KEY_SIZE);
	sgx_read_rand((*ctx)->ctr0, CTR_CTR_SIZE);

	ippsAESGetSize(&(*ctx)->ctxSize);
	(*ctx)->pAES = (IppsAESSpec*) malloc((*ctx)->ctxSize);
}

/**
\brief get size of the encryption context
\return size of the unused key
*/
int pack_get_size_ctr() {
	return (CTR_KEY_SIZE + CTR_CTR_SIZE);
}

