/**
\file
\brief Description of arguments
*/

#ifndef AARGS_H
#define AARGS_H

#include "../config.h"
#include "node.h"
//#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
\struct aargs_s
\brief A structure for arguments provided by the untrusted constructor
*/
struct aargs_s {
	queue *gpool;			///< the default node pool
	queue *gboxes;			///< an array of boxes used for communications
	struct eos_struct *gsp;		///< a virtual address where the EOS is mapped
#ifdef V2
	queue *eboxes;			///< an array of boxes used for communication of trusted system actors
	queue *my_box;			///< my personal box
#endif
	queue *gboxes_v2;		///< deprecated
	queue *gpool_v2;		///< deprecated
};

#ifdef __cplusplus
}
#endif

#endif //AARGS_H