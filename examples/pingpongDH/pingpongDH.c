/**
\file
\brief ping-pong with the diffie hellman procedure

*/

#include <fw.h>
#include "payload.h"
#include <sgx_dh.h>
#include <string.h>

extern void printa(const char *fmt, ...);

/**
\brief this structure describes a message format.
*/
struct mbox_struct {
	sgx_dh_msg1_t dh_msg1;
	sgx_dh_msg2_t dh_msg2;
	sgx_dh_msg3_t dh_msg3;
	char h_enc[64];
	sgx_aes_gcm_128bit_tag_t p_out_mac;
} __attribute__ ((aligned (64)));

/**
\brief a structure describes a content of the per-actor private store
*/
struct ps_struct {
	int stage;

	sgx_dh_msg1_t dh_msg1;		//Diffie-Hellman Message 1
	sgx_key_128bit_t dh_aek;	// Session Key
	sgx_dh_msg2_t dh_msg2;		//Diffie-Hellman Message 2
	sgx_dh_msg3_t dh_msg3;		//Diffie-Hellman Message 3

	sgx_dh_session_t sgx_dh_session;
	sgx_dh_session_enclave_identity_t responder_identity;
	uint32_t session_id;

	sgx_dh_session_enclave_identity_t initiator_identity;

	queue *pool;
	queue *mbox;
};

/**
\brief A ping actor.
*/
void aping(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	node *tmp = NULL;
	struct mbox_struct *msg = NULL;
	char hello[64]="Hello from ping";
	char h_enc[64];
	char h_dec[64];
	char iv[12] = "12345678912";
	sgx_aes_gcm_128bit_tag_t p_out_mac;

	uint32_t retstatus;
	sgx_status_t status = SGX_SUCCESS;


	switch(ps->stage) {
		case 0:
			memset(&ps->dh_aek,0, sizeof(sgx_key_128bit_t));
			memset(&ps->dh_msg1, 0, sizeof(sgx_dh_msg1_t));
			memset(&ps->dh_msg2, 0, sizeof(sgx_dh_msg2_t));
			memset(&ps->dh_msg3, 0, sizeof(sgx_dh_msg3_t));

			status = sgx_dh_init_session(SGX_DH_SESSION_INITIATOR, &ps->sgx_dh_session);
			if(status != SGX_SUCCESS) {
				printa("sgx_ds_init_session error = %d\n", status);while(1);
			}

			tmp = nalloc(ps->pool);
			msg =  (struct mbox_struct *) tmp->payload;

			printa("[INITIATOR] Sending  DH request \n");
			push_back(&ps->mbox[1], tmp);
			ps->stage++;
			break;
		case 1:
			tmp=pop_front(&ps->mbox[0]);
			if(tmp==NULL)
				return;

			msg = (struct mbox_struct *) tmp->payload;
			printa("[INITIATOR] Got MSG1 \n");

			memcpy(&ps->dh_msg1, &msg->dh_msg1, sizeof(sgx_dh_msg1_t));

			status = sgx_dh_initiator_proc_msg1(&ps->dh_msg1, &ps->dh_msg2, &ps->sgx_dh_session);
			if(status != SGX_SUCCESS) {
				printa("initiator_proc_msg1 = %d\n", status);while(1);
			}

			memcpy(&msg->dh_msg2, &ps->dh_msg2, sizeof(sgx_dh_msg2_t));

			printa("[INITIATOR] Sending  MSG2\n");
			push_back(&ps->mbox[1], tmp);
			ps->stage++;
			break;
		case 2:
			tmp=pop_front(&ps->mbox[0]);
			if(tmp==NULL)
				return;

			msg = (struct mbox_struct *) tmp->payload;
			printa("[INITIATOR] Got  MSG3\n");

			memcpy(&ps->dh_msg3, &msg->dh_msg3, sizeof(sgx_dh_msg3_t));

			status = sgx_dh_initiator_proc_msg3(&ps->dh_msg3, &ps->sgx_dh_session, &ps->dh_aek, &ps->responder_identity);
			if(status != SGX_SUCCESS) {
			        printa("initiator_proc_msg3 = %d\n", status);while(1);
			}

			printa("[INITIATOR] dh_aek:\n");
			for(int i = 0; i < 16; i ++) {
				printa("[%d] 0x%x\n", i, ps->dh_aek[i]);
			}

			nfree(tmp);

			ps->stage++;
			break;
		case 3:
			tmp = nalloc(ps->pool);
			if(tmp == NULL)
				return;
			msg =  (struct mbox_struct *) tmp->payload;

			status = sgx_rijndael128GCM_encrypt(&ps->dh_aek, hello, 64,
					h_enc, iv, 12,  NULL, 0, &p_out_mac);

			if(SGX_SUCCESS != status) {
				printa("Cannot encrypt\n");while(1);
			}

			memcpy(msg->h_enc, h_enc,64);
			memcpy(&msg->p_out_mac, &p_out_mac,sizeof(sgx_aes_gcm_128bit_tag_t));

			printa("[1] [PING] Sending  ENC MSG '%s'\n", hello);
			push_back(&ps->mbox[1], tmp);

			ps->stage = 4;
			break;
		case 4:
			tmp=pop_front(&ps->mbox[0]);
			if(tmp==NULL)
				return;

			msg = (struct mbox_struct *) tmp->payload;

			printa("[5] [PING] Got encrypted MSG\n");

			memcpy(h_enc, msg->h_enc, 64);
			memcpy(&p_out_mac, &msg->p_out_mac, sizeof(sgx_aes_gcm_128bit_tag_t));

			status = sgx_rijndael128GCM_decrypt(&ps->dh_aek, h_enc, 64,
					h_dec, iv, 12,  NULL, 0,  &p_out_mac);

			if(status != SGX_SUCCESS ) {
				printa("cannot decrypt = %d\n", status);while(1);
			}

			printa("[6] [PING] Decrypt message = '%s'\n", h_dec);

			ps->stage = 3;
			nfree(tmp);
			break;
	}
}

/**
\brief A pong actor.
*/
void apong(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	int ret = -1;
	char h_dec[64];
	char h_enc[64];
	sgx_aes_gcm_128bit_tag_t p_out_mac;
	char iv[12] = "12345678912";
	char hello[64]="Hello from pong";

	node *tmp = pop_front(&ps->mbox[1]);
	if(tmp==NULL)
		return;

	struct mbox_struct *msg = (struct mbox_struct *) tmp->payload;

	uint32_t retstatus;
	sgx_status_t status = SGX_SUCCESS;

	switch(ps->stage) {
		case 0:
			printa("[RESPONDER] Got request\n");

			memset(&ps->dh_aek,0, sizeof(sgx_key_128bit_t));
			memset(&ps->dh_msg1, 0, sizeof(sgx_dh_msg1_t));
			memset(&ps->dh_msg2, 0, sizeof(sgx_dh_msg2_t));
			memset(&ps->dh_msg3, 0, sizeof(sgx_dh_msg3_t));

			status = sgx_dh_init_session(SGX_DH_SESSION_RESPONDER, &ps->sgx_dh_session);
			if(status != SGX_SUCCESS) {
				printa("sgx_ds_init_session error = %d\n", status);while(1);
			}

			status = sgx_dh_responder_gen_msg1((sgx_dh_msg1_t*)&ps->dh_msg1, &ps->sgx_dh_session);
			if(status != SGX_SUCCESS) {
				printa("responder_gen_msg1 = %d\n", status); while(1);
			}

			memcpy(&msg->dh_msg1, &ps->dh_msg1, sizeof(sgx_dh_msg1_t));

			printa("[RESPONDER] Sending MSG1\n");
			ps->stage++;
			break;
		case 1:

			printa("[RESPONDER] Got MSG2\n");
			memset(&ps->dh_aek,0, sizeof(sgx_key_128bit_t));

			memcpy(&ps->dh_msg2, &msg->dh_msg2, sizeof(sgx_dh_msg2_t));

			status = sgx_dh_responder_proc_msg2(&ps->dh_msg2, 
                                                       &ps->dh_msg3, 
                                                       &ps->sgx_dh_session, 
                                                       &ps->dh_aek, 
                                                       &ps->initiator_identity);
			if(status != SGX_SUCCESS) {
				printa("responder_proc_msg = %d\n", status); while(1);
			}

			memcpy(&msg->dh_msg3, &ps->dh_msg3, sizeof(sgx_dh_msg3_t));

			printa("[RESPONDER] dh_aek:\n");
			for(int i = 0; i < 16; i ++) {
				printa("[%d] 0x%x\n", i, ps->dh_aek[i]);
			}

			printa("[RESPONDER] Sending MSG3\n");

			ps->stage++;
			break;
		case 2:
			printa("[2] [PONG] Got encrypted MSG\n");

			memcpy(h_enc, msg->h_enc, 64);
			memcpy(&p_out_mac, &msg->p_out_mac, sizeof(sgx_aes_gcm_128bit_tag_t));


			//Prepare the request message with the encrypted payload
			status = sgx_rijndael128GCM_decrypt(&ps->dh_aek, h_enc, 64,
					h_dec, iv, 12,  NULL, 0, &p_out_mac);

			if(status != SGX_SUCCESS ) {
				printa("cannot decrypt = %d\n", status);while(1);
			}

			printa("[3] [PONG] Decrypt message = '%s'\n", h_dec);

			status = sgx_rijndael128GCM_encrypt(&ps->dh_aek, hello, 64,
					h_enc, iv, 12,  NULL, 0, &p_out_mac);

			if(SGX_SUCCESS != status) {
				printa("Cannot encrypt\n");while(1);
			}

			memcpy(msg->h_enc, h_enc,64);
			memcpy(&msg->p_out_mac, &p_out_mac,sizeof(sgx_aes_gcm_128bit_tag_t));

			printa("[4] [PONG] Sending  ENC MSG '%s'\n", hello);

			ps->stage = 2;
			break;
	}

	push_back(&ps->mbox[0], tmp);
}

/**
\brief A pong actor.

- aping constructor, initializes initial variables
*/

int aping_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	ps->pool = gpool;
	ps->mbox = gboxes;

	ps->stage = 0;

	return 0;
}

int apong_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	ps->pool = gpool;
	ps->mbox = gboxes;

	ps->stage = 0;

	return 0;
}
