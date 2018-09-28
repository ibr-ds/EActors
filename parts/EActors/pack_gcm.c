/**
\file
\brief AES_GCM-based encryption.

*/

#include "pack_gcm.h"
#include <sgx_trts.h>

extern void printa(const char *fmt, ...);

/** 
\brief Increment big number
\param[in] *ctr a big number
\param[in] size the length in bytes
*/
void incr_ctr(char *ctr, int size) {
	int i;
	for(i = 0; i < size; i++) {
		if ( ++ctr[i] != 0)
			return;
	}

	if(ctr[i-1] == 0) {
		printa("[%d] counter overflow, please implement session restart or start with a smaller counter/IV\n", __LINE__);while(1);
	}
}

/**
\brief an AES_GCM-based pack function
\param[out] *dst where encrypted data will be stored
\param[in] *src data to encrypt
\param[in] size a size of a data to encrypt
\param[in] *ctx an decryption context
\param[in] *mac MAC
*/
void pack_gcm(char *dst, char *src, int size, struct ctx_gcm_s *ctx, char *mac) {
	sgx_status_t sgx_ret = sgx_rijndael128GCM_encrypt(
				&ctx->key,
				src, size, 
				dst,
				ctx->IV, SGX_AESGCM_IV_SIZE,
				NULL, 0,
				(sgx_aes_gcm_128bit_tag_t *) mac);

	if (sgx_ret != SGX_SUCCESS) {
		printa("Error while encrypting = %x\n", sgx_ret);
		while(1);
	}

	incr_ctr(ctx->IV, SGX_AESGCM_IV_SIZE);

}

/**
\brief an AES_GCM-based unpack function
\param[out] *dst where decrypted data will be stored
\param[in] *src data to decrypt
\param[in] size a size of a data to decrypt
\param[in] *ctx an decryption context
\param[in] *mac MAC

*/
void unpack_gcm(char *dst, char *src, int size, struct ctx_gcm_s *ctx, char *mac) {
	sgx_status_t sgx_ret = sgx_rijndael128GCM_decrypt(
		&ctx->key,
		src,
		size,
		dst,
		ctx->IV, SGX_AESGCM_IV_SIZE,
		NULL, 0,
		(sgx_aes_gcm_128bit_tag_t *) mac);

	if (sgx_ret != SGX_SUCCESS) {
		printa("Error while encrypting = %x\n", sgx_ret);
		while(1);
	}

	incr_ctr(ctx->IV, SGX_AESGCM_IV_SIZE);
}

/**
\brief memory allocation for the context
\param[in] **ctx where to allocate
*/
void alloc_gcm(struct ctx_gcm_s **ctx) {
	*ctx = malloc(sizeof(struct ctx_gcm_s));
	if(*ctx == NULL) {
		printa("[%d] malloc failed, die\n",__LINE__);while(1);
	}

	memset(*ctx, 0, sizeof(struct ctx_gcm_s));

	sgx_read_rand((*ctx)->key, GCM_KEY_SIZE);
	sgx_read_rand((*ctx)->IV, SGX_AESGCM_IV_SIZE);
}


/**
\brief get size of the encryption context
\return size of the unused key
*/
int pack_get_size_gcm() {
	return (1 * SGX_AESGCM_IV_SIZE + 16);
}
