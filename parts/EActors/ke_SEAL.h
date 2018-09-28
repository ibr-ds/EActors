#ifndef KE_SEALING_H
#define KE_SEALING_H

#include "../config.h"

#include "socket.h"
#include "packs.h"
#include "sealed_data_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

int build_SEAL_master_socket(struct socket_s *sockA);
int build_SEAL_slave_socket(struct socket_s *sockA);

#ifdef __cplusplus
}
#endif

#endif //KE_SEALING_H