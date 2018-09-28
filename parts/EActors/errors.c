/**
\file
\brief Parser of return codes
*/

#include "errors.h"
#include "sgx_error.h"

extern void printa(const char *fmt, ...);

/**
\brief sgx return code parser
*/
void parse_sgx_ret(int ret) {
	switch(ret) {
		case SGX_ERROR_MAC_MISMATCH:
			/* MAC of the sealed data is incorrect.
			The sealed data has been tampered.*/
			printa("SGX RET ERROR at line %d\n",__LINE__);
			break;
		case SGX_ERROR_INVALID_ATTRIBUTE:
			/*Indicates attribute field of the sealed data is incorrect.*/
			printa("SGX RET ERROR at line %d\n",__LINE__);
			break;
		case SGX_ERROR_INVALID_ISVSVN:
			/* Indicates isv_svn field of the sealed data is greater than
			the enclave’s ISVSVN. This is a downgraded enclave.*/
			printa("SGX RET ERROR at line %d\n",__LINE__);
			break;
		case SGX_ERROR_INVALID_CPUSVN:
			/* Indicates cpu_svn field of the sealed data is greater than
			the platform’s cpu_svn. enclave is  on a downgraded platform.*/
			printa("SGX RET ERROR at line %d\n",__LINE__);
			break;
		case SGX_ERROR_INVALID_KEYNAME:
			/*Indicates key_name field of the sealed data is incorrect.*/
			printa("SGX RET ERROR at line %d\n",__LINE__);
			break;
		default:
			printa("SGX RET ERROR at line %d = %x\n",__LINE__, ret);
			break;
        }
	while(1);
}
