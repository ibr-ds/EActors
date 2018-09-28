#include "ke_SEAL.h"
#include "errors.h"

extern void printa(const char *fmt, ...);
/** 
\brief Key-exchange protected with sealing. Master side
\note Should be used because of security. Use LARSA
\note Because we can send only 16 bytes at once we send several requests. 
\note TODO: these requests should be protected: firstly we need to send a hash, then keys
*/
int build_SEAL_master_socket(struct socket_s *sockA) {
	char sealed[SEALED_KEY_SIZE];
	int sgx_ret = 0;
	node *tmp = NULL;

	if(!is_enc(sockA))
		return 0;


	switch(sockA->state) {
		case 0:
			if(sockA->type == GCM) {
				sgx_read_rand(((struct ctx_gcm_s *)sockA->ctx)->key, GCM_KEY_SIZE);
				sgx_read_rand(((struct ctx_gcm_s *)sockA->ctx)->IV, SGX_AESGCM_IV_SIZE);
			}

			if(sockA->type == CTR) {
				sgx_read_rand(((struct ctx_ctr_s *)sockA->ctx)->key, CTR_KEY_SIZE);
				sgx_read_rand(((struct ctx_ctr_s *)sockA->ctx)->ctr0, CTR_CTR_SIZE);
		
				ippsAESGetSize(&((struct ctx_ctr_s *)sockA->ctx)->ctxSize);
				((struct ctx_ctr_s *)sockA->ctx)->pAES = (IppsAESSpec*) malloc(((struct ctx_ctr_s *)sockA->ctx)->ctxSize);
			}
		default:
			if( sockA->state * 16 >= pack_get_size(sockA))
				return 0;

			tmp =  nalloc(sockA->pool);
				if (tmp == NULL) {
					printa("no node for a constructor (%d)?\n", __LINE__);while(1);
			}


			sgx_ret = sgx_seal_data(0, NULL, 16, sockA->ctx + 16 * sockA->state, SEALED_KEY_SIZE, (sgx_sealed_data_t*) sealed);
			if(sgx_ret != SGX_SUCCESS)
				parse_sgx_ret(sgx_ret);

			memcpy(tmp->payload, sealed, SEALED_KEY_SIZE);

			push_back(sockA->out, tmp);
			sockA->state++;
	}

	return 1;
}

/** 
\brief Key-exchange protected with sealing. Slave side
\note Should be used because of security. Use LARSA. 
*/
int build_SEAL_slave_socket(struct socket_s *sockA) {
	char sealed[SEALED_KEY_SIZE];
	int sgx_ret = 0;
	int key_len = 16;

	if(!is_enc(sockA))
		return 0;

	if( sockA->state * 16 >= pack_get_size(sockA))
		return 0;

	node *tmp = pop_front(sockA->in);
	if(tmp == NULL)
		return 1;

	memcpy(sealed, tmp->payload, SEALED_KEY_SIZE);

	nfree(tmp);

	sgx_ret = sgx_unseal_data((sgx_sealed_data_t*)sealed, NULL, 0, sockA->ctx + 16 * sockA->state, &key_len);
	if(sgx_ret != SGX_SUCCESS)
		parse_sgx_ret(sgx_ret);

	sockA->state++;

	if( sockA->state * 16 == pack_get_size(sockA)) {
		if(sockA->type == CTR) {
			ippsAESGetSize(&((struct ctx_ctr_s *)sockA->ctx)->ctxSize);
			((struct ctx_ctr_s *)sockA->ctx)->pAES = (IppsAESSpec*) malloc(((struct ctx_ctr_s *)sockA->ctx)->ctxSize);
		}
		return 0;
	}

	return 1;
}
