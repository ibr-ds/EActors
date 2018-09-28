#ifndef KE_LARSA_H
#define KE_LARSA_H

#include "../config.h"

#include "socket.h"
#include "rsa.h"
#include "packs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MEASURE_SIZE	32

struct LARSA_struct {
	sgx_target_info_t target;
	sgx_report_t report;
	unsigned char buf[1 * SGX_AESGCM_IV_SIZE + 16];
} __attribute__ ((aligned (64)));


int build_LARSA_master_socket(struct socket_s *sockA);
int build_LARSA_slave_socket(struct socket_s *sockA);

#ifdef __cplusplus
}
#endif

#endif //KE_LARSA_H