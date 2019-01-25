/**
\file
\brief worker -- an execution entity of for anctors 

Worker:
- Pthread-powered actor caller
- Managed in untrusted part
- Has CPU affinity, enclave affinity and per enclave actor affinity
*/

#ifndef WORKER_H
#define WORKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../config.h"

/**
\brief A worker structure
*/
struct s_worker {
	unsigned long int id;					///< Pthread id
	unsigned int my_id;					///< an identifier of a worker
	unsigned char 		cpus_mask;			///< //cores binding policy
	unsigned long int	enclaves_mask;			///< enclave binding policy
	unsigned long int	actors_mask[MAX_ENCLAVES];	///< per-enclave actor calling policy
}	workers[MAX_WORKERS],					///< an array of workers
	dyn_workers[MAX_DYN_WORKERS]; 				///< an array of dynamicly created workers

void assign_workers(void); 					///< automaticly generated function with assignments

#ifdef __cplusplus
}
#endif


#endif