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
	struct storage_struct *gsp;	///< a virtual address where the POS is mapped
	queue *gboxes_v2;		///< deprecated
	queue *gpool_v2;		///< deprecated
};

#ifdef __cplusplus
}
#endif

#endif //AARGS_H