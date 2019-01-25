/**
\file
\brief ping-pong with local attestation

*/

#include <fw.h>
#include "payload.h"
#include <sgx_report.h>
#include <string.h>

extern void printa(const char *fmt, ...);

/**
\brief this structure describes a message format.
*/
struct mbox_struct {
	sgx_target_info_t target;
	sgx_report_t report;
	unsigned char buf[128];
} __attribute__ ((aligned (64)));

/**
\brief a structure describes a content of a per actor private store
*/
struct ps_struct {
	int state;

	struct rsa_kp_struct rsa_ping, rsa_pong;

	queue *pool;
	queue *mbox;
};

/**
\brief A ping actor.
*/
int aping(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	node *tmp = NULL;
	struct mbox_struct *msg = NULL;
	sgx_report_t temp_report;
	sgx_status_t sgx_ret;
	sgx_report_t report;

	char de_msg[128];
	char payload[64];


	switch(ps->state) {
		case 0:
			tmp = nalloc(ps->pool);

			msg = (struct mbox_struct *) tmp->payload;

			memset(&temp_report, 0, sizeof(temp_report));
			memset(&msg->target, 0, sizeof(sgx_target_info_t));

			memcpy(msg->buf, ps->rsa_ping.pub, 128);

			ippsSHA224MessageDigest(msg->buf, sizeof(msg->buf), payload);


			sgx_ret = sgx_create_report(NULL, payload, &temp_report);
			if (sgx_ret != SGX_SUCCESS) {
	    			printa("Enclave: Error while creating report = %d\n", sgx_ret);
				while(1);
			}

			memcpy(&msg->target.mr_enclave, &temp_report.body.mr_enclave, sizeof(sgx_measurement_t));
			memcpy(&msg->target.attributes, &temp_report.body.attributes, sizeof(sgx_attributes_t));
			memcpy(&msg->target.misc_select, &temp_report.body.misc_select, sizeof(sgx_misc_select_t));

			printa("[PING] Sending  LA request %d %d \n", sizeof(msg->target), sizeof(msg->report));
			push_back(&ps->mbox[1], tmp);

			ps->state = 1;
			break;
		case 1:
			tmp=pop_front(&ps->mbox[0]);
			if(tmp==NULL)
				return 1;

			msg = (struct mbox_struct *) tmp->payload;
			printa("[PING] Got LA responce \n");

			memcpy(&report, &msg->report, sizeof(report));
			sgx_ret = sgx_verify_report(&report);
			if (sgx_ret != SGX_SUCCESS) {
	    			printa("Enclave: Error while verifing report = %d\n", sgx_ret);
				while(1);
			}

			printa("[PING] Verify OK\n");

			fill_rsa(&ps->rsa_pong, msg->buf, 1024, BIN_KEY);

			extern unsigned char csum[1][32];
			if(memcmp(csum[0], &report.body.mr_enclave, sizeof(sgx_measurement_t)) != 0) {
	    			printa("Wrong responce\n Expected:\tReceived:\n");
				for(int i = 0; i < 32; i++)
					printa("%x\t%x\n", csum[0][i], report.body.mr_enclave.m[i]);
				while(1);
			}

			printa("[PING] LA: Match\n");

			nfree(tmp);
			ps->state = 2;
			break;
		case 2:
			tmp = nalloc(ps->pool);
			msg = (struct mbox_struct *) tmp->payload;

			rsa_encrypt(msg->buf, "ping", sizeof("ping"), &ps->rsa_pong);
			printa("[PING] Send ENC message '%c%c%c%c'\n", msg->buf[0], msg->buf[1],msg->buf[2], msg->buf[3]);
			push_back(&ps->mbox[1], tmp);

			ps->state = 3;
			break;
		case 3:
			tmp=pop_front(&ps->mbox[0]);
			if(tmp==NULL)
				return 1;

			msg = (struct mbox_struct *) tmp->payload;
			printa("[PING] Got ENC message '%c%c%c%c'\n", msg->buf[0], msg->buf[1],msg->buf[2], msg->buf[3]);

			rsa_decrypt(de_msg, msg->buf, &ps->rsa_ping);
			printa("[PING] Got DEC message '%s'\n", de_msg);

			nfree(tmp);
			ps->state = 2;
			break;
	}

	return 1;
}

/**
\brief A pong actor.
*/
int apong(struct actor_s *self) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;
	char de_msg[128];

	char payload[64];
	sgx_status_t sgx_ret;
	sgx_target_info_t target;
	sgx_report_t s_report;

	node *tmp = pop_front(&ps->mbox[1]);
	if(tmp==NULL)
		return 1;

	struct mbox_struct *msg = (struct mbox_struct *) tmp->payload;

	switch(ps->state) {
		case 0:

			printa("[PONG] Got LA request\n");

			memcpy(&target, &msg->target, sizeof(target));

			fill_rsa(&ps->rsa_ping, msg->buf, 1024, BIN_KEY);
			memcpy(msg->buf, ps->rsa_pong.pub, 128);

			ippsSHA224MessageDigest(msg->buf, sizeof(msg->buf), payload);

			sgx_ret = sgx_create_report(&target, payload, &s_report);
			if (sgx_ret != 0) {
				printa("sgx_create_report FAILED ret=%x\n", sgx_ret);
				while(1);
			}
			memcpy(&msg->report, &s_report, sizeof(s_report));

			push_back(&ps->mbox[0], tmp);
			ps->state = 1;
			break;
		case 1:
			printa("[PONG] Got ENC message '%c%c%c%c'\n", msg->buf[0], msg->buf[1],msg->buf[2], msg->buf[3]);
			rsa_decrypt(de_msg, msg->buf, &ps->rsa_pong);
			printa("[PONG] Got DEC message '%s'\n", de_msg);

			rsa_encrypt(msg->buf, "pong", sizeof("pong"), &ps->rsa_ping);
			printa("[PONG] Send ENC message '%c%c%c%c'\n", msg->buf[0], msg->buf[1],msg->buf[2], msg->buf[3]);

			push_back(&ps->mbox[0], tmp);
			break;
	}

	return 1;
}

/**
\brief A pong actor.

- aping constructor, initializes initial variables
*/

int aping_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	ps->pool = gpool;
	ps->mbox = gboxes;

	ps->state = 0;

	gen_rsa(&ps->rsa_ping, 1024);

	return 0; //we are done
}

int apong_ctr(struct actor_s *self, queue *gpool, queue *ppool, queue *gboxes, queue *pboxes) {
	struct ps_struct *ps = (struct ps_struct *) self->ps;

	ps->pool = gpool;
	ps->mbox = gboxes;

	ps->state = 0;

	gen_rsa(&ps->rsa_pong, 1024);

	return 0; //we are done
}
