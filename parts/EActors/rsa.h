/**
\file
\brief RSA encryption support
*/

#ifndef RSA_H
#define RSA_H

#include "../config.h"
#include "inc/ippcp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
*\brief an rsa context (encryption keys and more)
*/
struct rsa_kp_struct{
	IppsBigNumState *pModulus;		///< a modulus of a key
	IppsBigNumState *pPublicExp;		///< an exponent value of a key
	IppsRSAPublicKeyState *publicKey;	///< a public key object
	IppsRSAPrivateKeyState *privateKey;	///< a private key object
	int bitsRSA;				///< a key length (1024)
	int bitsExp;				///< a length of an exponent (24)
	int sizeOfPublicKey; 			//I do not think that this variable is necessary

	char pub[256];				///< a modulus in hexdec form
};

enum key_type {
	BIN_KEY,
	HEXDEC_KEY,
	PKCS_KEY
};

void Type_BN(struct rsa_kp_struct *key, const char* pMsg, const IppsBigNumState* pBN);
IppsBigNumState* createBigNumState(int len, const Ipp32u* pData);
void gen_rsa(struct rsa_kp_struct *key, int bitsRSA);
void fill_rsa(struct rsa_kp_struct *key, char *pModulus, int bitsRSA, enum key_type type);
void rsa_encrypt(char *dst, char *src, int size, struct rsa_kp_struct *key);
int rsa_decrypt(char *dst, char *src, struct rsa_kp_struct *key);

#ifdef __cplusplus
}
#endif

#endif //RSA_H