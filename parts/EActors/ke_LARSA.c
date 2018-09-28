/**
\file
\brief Local Attestation with RSA. 
*/

#include "ke_LARSA.h"
#include <sgx_utils.h>

extern void printa(const char *fmt, ...);

// I cannot make a general dispatcher for all types of sockets. In this case the compiler puts all methods even some of them are not actyally needed.
// Additionly, the last enclave needs somehow resolve the csum issue: it will have the master code while in fact it cannot be master (in the current design).
// Separation between GCM/MCMCPY/CTR also should be implemented independently. However, since the framework in some extend relies on both these types of encryption technologies, 
// I decided to use 'if' despatchers here. 

/**
\brief LARSA-based key exchange, master side
*/
int build_LARSA_master_socket(struct socket_s *sockA) {
	node *tmp = NULL;
	struct LARSA_struct *msg = NULL;
	sgx_report_t temp_report;
	sgx_status_t sgx_ret;
	sgx_report_t report;
	char payload[64];

	if(!is_enc(sockA))
		return 0;

	switch(sockA->state) {
		case 0:
			tmp = nalloc(sockA->pool);
			msg = (struct LARSA_struct *) tmp->payload;

			memset(&temp_report, 0, sizeof(temp_report));
			memset(&msg->target, 0, sizeof(sgx_target_info_t));

			gen_rsa(&sockA->rsa_master, 1024);
			memcpy(msg->buf, sockA->rsa_master.pub, 128);
			ippsSHA224MessageDigest(msg->buf, sizeof(msg->buf), payload);

			sgx_ret = sgx_create_report(NULL, (sgx_report_data_t *)payload, &temp_report);
			if (sgx_ret != SGX_SUCCESS) {
	    			printa("Enclave: Error while creating report = %d\n", sgx_ret);
				while(1);
			}

			memcpy(&msg->target.mr_enclave, &temp_report.body.mr_enclave, sizeof(sgx_measurement_t));
			memcpy(&msg->target.attributes, &temp_report.body.attributes, sizeof(sgx_attributes_t));
			memcpy(&msg->target.misc_select, &temp_report.body.misc_select, sizeof(sgx_misc_select_t));

			printa("[MASTER] Sending  LA request %d %d \n", sizeof(msg->target), sizeof(msg->report));
			push_back(sockA->out, tmp);

			sockA->state = 1;
			break;
		case 1:
			tmp=pop_front(sockA->in);
			if(tmp==NULL)
				break;

			msg = (struct LARSA_struct *) tmp->payload;
			printa("[MASTER] Got LA responce \n");

			memcpy(&report, &msg->report, sizeof(report));
			sgx_ret = sgx_verify_report(&report);
			if (sgx_ret != SGX_SUCCESS) {
	    			printa("Enclave: Error while verifing report = %d\n", sgx_ret);
				while(1);
			}

			printa("[MASTER] Verify OK\n");

			fill_rsa(&sockA->rsa_slave, msg->buf, 1024, BIN_KEY);

			extern unsigned char csum[MEASURE_SIZE * (MAX_ENCLAVES-1)];
			if(memcmp(&csum[sockA->mid * MEASURE_SIZE], &report.body.mr_enclave, sizeof(sgx_measurement_t)) != 0) {
	    			printa("Wrong responce\n Expected:\tReceived:\n");
				for(int i = 0; i < MEASURE_SIZE; i++)
					printa("%x\t%x\n", csum[sockA->mid*MEASURE_SIZE+i], report.body.mr_enclave.m[i]);
				while(1);
			}

			printa("[MASTER] LA: Match\n");

			nfree(tmp);

			sockA->state = 2;
			break;
		case 2:
			tmp = nalloc(sockA->pool);
			msg = (struct LARSA_struct *) tmp->payload;

			rsa_encrypt(msg->buf, sockA->ctx, pack_get_size(sockA), &sockA->rsa_slave); /// calling convention. sock->ctx begins with data which needs to be encrypted

			push_back(sockA->out, tmp);
			sockA->state = 3;
			break;
		case 3:
			return 0;
	}

	return 1;
}

/**
\brief LARSA-based key exchange, slave side
*/
int build_LARSA_slave_socket(struct socket_s *sockA) {
	node *tmp = NULL;
	struct LARSA_struct *msg;

	char payload[64];
	sgx_status_t sgx_ret;
	sgx_target_info_t target;
	sgx_report_t s_report;

	if(!is_enc(sockA))
		return 0;

	switch(sockA->state) {
		case 0:
			printa("[SLAVE] state 0\n");

			gen_rsa(&sockA->rsa_slave, 1024);
			sockA->state = 1;
			break;
		case 1:
			tmp = pop_front(sockA->in);
			if(tmp==NULL)
				break;

			msg = (struct LARSA_struct *) tmp->payload;

			printa("[SLAVE] Got LA request\n");

			memcpy(&target, &msg->target, sizeof(target));

			fill_rsa(&sockA->rsa_master, msg->buf, 1024, BIN_KEY);
			memcpy(msg->buf, sockA->rsa_slave.pub, 128);

			ippsSHA224MessageDigest(msg->buf, sizeof(msg->buf), payload);

			sgx_ret = sgx_create_report(&target, (sgx_report_data_t *)payload, &s_report);
			if (sgx_ret != 0) {
				printa("sgx_create_report FAILED ret=%x\n", sgx_ret);
				while(1);
			}
			memcpy(&msg->report, &s_report, sizeof(s_report));

			push_back(sockA->out, tmp);
			sockA->state = 2;
			break;
		case 2:
			tmp = pop_front(sockA->in);
			if(tmp == NULL)
				break;

			printa("[SLAVE] got RSA-encrypted session key\n");
			msg = (struct LARSA_struct *) tmp->payload;

			rsa_decrypt(sockA->ctx, msg->buf, &sockA->rsa_slave);

			if(sockA->type == CTR) {
				ippsAESGetSize(&((struct ctx_ctr_s *)sockA->ctx)->ctxSize);
				((struct ctx_ctr_s *)sockA->ctx)->pAES = (IppsAESSpec*) malloc(((struct ctx_ctr_s *)sockA->ctx)->ctxSize);
			}

			nfree(tmp);

			sockA->state = 3;
			break;
		case 3:
			return 0;
	}

	return 1;
}
