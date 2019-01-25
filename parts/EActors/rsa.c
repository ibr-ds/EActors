/**
\file
\brief RSA support 

This module supports:
* RSA key pair generation
* Data encryption and decryption

Based on and inspired by:

* Intel® Software Guard Extensions SDK for Linux* OS
* wolfssl: https://github.com/wolfSSL/wolfssl/blob/master/wolfcrypt/user-crypto/src/rsa.c
* accessl: https://github.com/gozdal/accessl/blob/master/src/accel/accel_ipp.c
* RSA Key Generation: Intel IPP vs OpenSSL: https://gist.github.com/astojanov/d09ef96b6ee92834e781

TODO: SDK 1.9, 2.3 and 'master' has different API and different header. ippGetStatusString exists only in 1.9, while 2.3 and master (2.3+) use ippcpGetStatusString.
However, 2.3 does not have some header files which exist inside master ahd 1.9. So, now 'ipp(cp)GetStatusString' is commened out, but someday this should be fixed.

*/

#include "rsa.h"
#include <stdlib.h>
#include <string.h>
#include "sgx_trts.h"
#include "inc/ippcore.h"

extern void printa(const char *fmt, ...);

//#define DEBUG_RSA


/**
\brief Displays a content of a BN in hexdec format
\param[in] *key a pointer to a key object, used to store a modulus. can be NULL
\param[in] *pMsg a string message before a printing
\param[in] *pBN a BigNumber pointer
*/
void Type_BN(struct rsa_kp_struct *key, const char *pMsg, const IppsBigNumState *pBN) {
	int size, n;

	ippsGetSize_BN(pBN, &size);
	Ipp8u* bnValue =  (unsigned char *) malloc(size*4);
	int ret = ippsGetOctString_BN(bnValue, size *4, pBN);
	if (ret != ippStsNoErr) {
//		printa("decrypt error of %s\n", ippcpGetStatusString(ret));
		printa("decrypt error of %d\n", ret);
		while(1);
	}

//#ifdef DEBUG_RSA
	if(pMsg)
		printa("%s\t", pMsg);

	for( n = 0; n < size * 4; n++)
		printa("%02x", bnValue[n]);
	printa("\n");
//#endif

	if(key)
		memcpy(key->pub, bnValue, 256);

	free(bnValue);
}

/**
\brief Allocated a BN object and fill it
\param[in] len a length of an object
\param[in] *pData a data to fill
\return pointer to an allocated object
*/
IppsBigNumState* createBigNumState(int len, const Ipp32u *pData)  {
	int size;
	ippsBigNumGetSize(len, &size);
	IppsBigNumState* pBN = (IppsBigNumState*) malloc(size);
	ippsBigNumInit(len, pBN);
	if (pData != NULL)
		ippsSet_BN(IppsBigNumPOS, len, pData, pBN);

	return pBN;
}

/**
\brief generate an rsa pair
\param[in] *key an object where generated data will be stored
\param[in] bitRSA a RSA length, should be 1024
*/
void gen_rsa(struct rsa_kp_struct *key, int bitsRSA) {
	IppStatus status;
	int ret;

	int nTrials = 1;
	int bitsExp = 24;

	int sizeOfPrimeGen      = -1;
	int sizeOfRandomGen     = -1;
	int sizeOfPublicKey     = -1;
	int sizeOfPrivateKey    = -1;
	int sizeOfScratchBuffer = -1;

	IppsPrimeState *primeGen           = NULL;
	IppsPRNGState *randomGen           = NULL;
	Ipp8u *scratchBuffer               = NULL;

	Ipp32u e = 65537;

	IppsBigNumState *pPublicExp    = createBigNumState (bitsRSA / 32, NULL);
	IppsBigNumState *pPrivateExp   = createBigNumState (bitsRSA / 32, NULL);

	key->bitsRSA = bitsRSA;
	key->bitsExp = 24;
	key->pPublicExp = createBigNumState(1, &e);
	key->pModulus = createBigNumState (bitsRSA / 32, NULL);

// Prime Number Generator
	ippsPrimeGetSize(bitsRSA, &sizeOfPrimeGen);
	primeGen = (IppsPrimeState *) malloc(sizeOfPrimeGen);
	ippsPrimeInit(bitsRSA, primeGen);

// Pseudo Random Generator (default settings)
	ippsPRNGGetSize(&sizeOfRandomGen);
	randomGen = (IppsPRNGState*) malloc (sizeOfRandomGen);
	ippsPRNGInit(160, randomGen);

	char rnd_seed[20];
	sgx_read_rand(rnd_seed, 20);
	IppsBigNumState *seed = createBigNumState(5, (unsigned int *)rnd_seed);
	ippsPRNGSetSeed(seed, randomGen);

// Initialize the Public Key State
	ippsRSA_GetSizePublicKey(bitsRSA, bitsExp, &sizeOfPublicKey);
	key->publicKey = (IppsRSAPublicKeyState *)malloc(sizeOfPublicKey);
	ippsRSA_InitPublicKey(bitsRSA, bitsExp, key->publicKey, sizeOfPublicKey);

// Initialize the Private Key State
	int bitsP = (bitsRSA + 1) / 2;
	int bitsQ = bitsRSA - bitsP;
	ippsRSA_GetSizePrivateKeyType2(bitsRSA, bitsRSA, &sizeOfPrivateKey);
	key->privateKey = (IppsRSAPrivateKeyState *) malloc(sizeOfPrivateKey);
	ippsRSA_InitPrivateKeyType2(bitsP, bitsQ, key->privateKey, sizeOfPrivateKey);

// Initialize scratch buffer
	ippsRSA_GetBufferSizePrivateKey(&sizeOfScratchBuffer, key->privateKey);
	scratchBuffer = (Ipp8u *) malloc (sizeOfScratchBuffer);

	status = ippStsInsufficientEntropy;
	while(status == ippStsInsufficientEntropy) {
		status = ippsRSA_GenerateKeys(
				key->pPublicExp,
				key->pModulus, pPublicExp, pPrivateExp,
				key->privateKey, scratchBuffer, nTrials,
				primeGen, ippsPRNGen, randomGen);
		if (status == ippStsNoErr)
			break;

		if (status != ippStsInsufficientEntropy) {
//			printa("ippsRSA_GeneratKeys error of %s\n", ippcpGetStatusString(status));
			printa("ippsRSA_GeneratKeys error %d\n", status);
			while(1);
		}
	}

	Type_BN(key, "modulus", key->pModulus);

	free(pPublicExp);
	free(pPrivateExp);
	free(primeGen);
	free(randomGen);
	free(scratchBuffer);
}

/**
\brief fill rsa_kp_struct with known values of key pair. 
\param[in] *key a pointer to a key object to fill
\param[in] *pModulus a pointer where modulus of a public key is stores
\param[in] bitsRSA a key length, 1024 tested only
\param[in] type a format of pModulus

This function has several drawbacks:
* As one can see, it does not support uploading of a private key
* Also, a public key itself is not a public key but his modulus
* Someone (Type_BN or createBigNumState) gives mirrored order of data

This module should be reworked to enable support of pem/key/pkcs12 files
*/
void fill_rsa(struct rsa_kp_struct *key, char *pModulus, int bitsRSA, enum key_type type) {
	key->bitsRSA = bitsRSA;
	key->bitsExp = 24;
	Ipp32u e = 65537;

	char m_hex[128];

	int i;
	switch (type) {
		case HEXDEC_KEY:
			for(i = 0; i < 128; i++) {
				if(pModulus[2*i] < 'a')
					m_hex[127 - i] = (pModulus[2 * i] - '0') * 16;
				else
					m_hex[127 - i] = (pModulus[2 * i] - 'a' + 0xa) * 16;
				if(pModulus[2 * i + 1] < 'a')
					m_hex[127 - i] += (pModulus[2 * i + 1] - '0');
				else
					m_hex[127 - i] += (pModulus[2 * i + 1] - 'a' + 0xa);
			}
			break;
		case BIN_KEY:
			for(i = 0; i < 128; i++) {
				m_hex[127 - i] = pModulus[i];
			}
			break;
		case PKCS_KEY:
			printa("not supported\n");while(1);
			break;
	}

	key->pModulus = createBigNumState(32, (Ipp32u *) m_hex);
	key->pPublicExp = createBigNumState(1, &e);

	ippsRSA_GetSizePublicKey(key->bitsRSA, key->bitsExp, &key->sizeOfPublicKey);
	key->publicKey = (IppsRSAPublicKeyState *) malloc(key->sizeOfPublicKey);
	ippsRSA_InitPublicKey(key->bitsRSA, key->bitsExp, key->publicKey, key->sizeOfPublicKey);

	int ret = ippsRSA_SetPublicKey(key->pModulus, key->pPublicExp, key->publicKey);
	if (ret != ippStsNoErr) {
//		printa("ippsRSA_SetPublicKey error %s\n", ippcpGetStatusString(ret));
		printa("ippsRSA_SetPublicKey error %d\n", ret);
		while(1);
	}
}

/**
\brief encrypt a buffer
\param[in] *dst where to put an encrypted data
\param[in] *src where to take a plain text data
\param[in] size a size of the src data
\param[in] *key a key pair context
*/
void rsa_encrypt(char *dst, char *src, int size, struct rsa_kp_struct *key) {
	Ipp8u* scratchBuffer;
	int    scratchSz, ret;

	ret = ippsRSA_GetBufferSizePublicKey(&scratchSz, key->publicKey);
	if (ret != ippStsNoErr) {
		printa("RCA_ENCRYPT FAIL: %d, %d\n",__LINE__, ret);while(1);
	}

	scratchBuffer = malloc(scratchSz*(sizeof(Ipp8u)));
	if (scratchBuffer == NULL) {
		printa("%d\n",__LINE__);while(1);
	}

	ret = ippsRSAEncrypt_PKCSv15((Ipp8u *) src, size, NULL, (Ipp8u *) dst, key->publicKey, scratchBuffer);
	if (ret != ippStsNoErr) {
		free(scratchBuffer);
//		printa("encrypt error of %s\n", ippcpGetStatusString(ret));
		printa("encrypt error of %d\n", ret);
		while(1);
	}

	free(scratchBuffer);
}

/**
\brief decrypt a buffer
\param[in] *dst where to put a decrypted data
\param[in] *src where to take an encrypted data
\param[in] *key a key pair context
\return decrypted data size
*/
int rsa_decrypt(char *dst, char *src, struct rsa_kp_struct *key) {
	Ipp8u* scratchBuffer;
	int    scratchSz, outSz;
	IppStatus	ret;

	ret = ippsRSA_GetBufferSizePrivateKey(&scratchSz, key->privateKey);
	if (ret != ippStsNoErr) {
		printa("%d, %d\n",__LINE__, ret);while(1);
	}

	scratchBuffer = malloc(scratchSz*(sizeof(Ipp8u)));
	if (scratchBuffer == NULL) {
		printa("%d\n",__LINE__);while(1);
	}

	ret = ippsRSADecrypt_PKCSv15((Ipp8u *) src, (Ipp8u *) dst, &outSz, key->privateKey, scratchBuffer);
	if (ret != ippStsNoErr) {
//		printa("decrypt error of %s\n", ippcpGetStatusString(ret));
		printa("decrypt error of %d\n", ret);
		while(1);
	}

	free(scratchBuffer);
	return outSz;
}
