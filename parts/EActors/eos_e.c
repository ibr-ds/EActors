/**
\file
\brief Trusted functions for a persisyent encrypted eos

\deprecated
*/

#include "eos.h"

extern void printa(const char *fmt, ...);

void retrive_eos_keys(struct eos_struct *st, struct ctx_ctr_s *ctx) {
	int i, key_len = CTR_KEY_SIZE, ret;
	char sealed[SEALED_KEY_SIZE];

	if(ctx == NULL) {
		printa("cant retrive keys into NULL contex\n");while(1);
	}

//need something more clever
	for(i = 0; i < SEALED_KEY_SIZE; i++) {
		if(st->sealed_key[i]!=0)
			break;
	}

	if(i == SEALED_KEY_SIZE) {
		create_random_aes_key(ctx);
		return;
	}

	memcpy(sealed, st->sealed_key, SEALED_KEY_SIZE);

	ret = sgx_unseal_data((sgx_sealed_data_t*)sealed, NULL, 0, &ctx->key[0], &key_len);
	if(ret != SGX_SUCCESS)
		parse_sgx_ret(ret);

	memcpy(sealed, st->sealed_ctr0, SEALED_KEY_SIZE);

	ret = sgx_unseal_data((sgx_sealed_data_t*)sealed, NULL, 0, ctx->ctr0, &key_len);
	if(ret != SGX_SUCCESS)
		parse_sgx_ret(ret);
	if(key_len != CTR_CTR_SIZE) {
		printa("wrong ctr size (%d), probably hacked\n", key_len);while(1);
	}

	init_uploaded_key(ctx);
}

void seal_eos_keys(struct eos_struct *st, struct ctx_ctr_s *ctx) {
	int i, ret;
	char tmp[SEALED_KEY_SIZE];

	if(ctx == NULL) {
		printa("cant seal an empty context\n");while(1);
	}

	ret = sgx_seal_data(0, NULL, CTR_KEY_SIZE, (unsigned char*)ctx->key, SEALED_KEY_SIZE, (sgx_sealed_data_t*) tmp);
	if(ret != SGX_SUCCESS)
		parse_sgx_ret(ret);

	memcpy(st->sealed_key, tmp, SEALED_KEY_SIZE);

	ret = sgx_seal_data(0, NULL, CTR_CTR_SIZE, (unsigned char*)ctx->ctr0, SEALED_KEY_SIZE, (sgx_sealed_data_t*) tmp);
	if(ret != SGX_SUCCESS)
		parse_sgx_ret(ret);

	memcpy(st->sealed_ctr0, tmp, SEALED_KEY_SIZE);

	eos_sync(0);
}
