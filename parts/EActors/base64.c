/**
\file
\brief base64 encode/decode. Can be used for processing of containers with keys
*/

#include "base64.h"
#include <ctype.h>

void b64_decode (char *dst, const char *src, size_t len) {
	b64_decode_ex(dst, src, len, NULL);
}

void b64_decode_ex (char *dst, const char *src, size_t len, size_t *decsize) {
	int i = 0;
	int j = 0;
	int l = 0;
	size_t size = 0;
	unsigned char *dec = dst;
	unsigned char buf[3];
	unsigned char tmp[4];

	while (len--) {
		if ('=' == src[j])
			break;
		if (!(isalnum(src[j]) || '+' == src[j] || '/' == src[j]))
			break;

		tmp[i++] = src[j++];

		// if 4 bytes read then decode into `buf'
		if (4 == i) {
		// translate values in `tmp' from table
			for (i = 0; i < 4; ++i) {
		// find translation char in `b64_table'
				for (l = 0; l < 64; ++l) {
					if (tmp[i] == b64_table[l]) {
						tmp[i] = l;
						break;
					}
				}
			}

			// decode
			buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
			buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
			buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

			if (dec != NULL) {
				for (i = 0; i < 3; ++i)
					dec[size++] = buf[i];
			} else {
				return;
			}

		i = 0;
		}
	}

	if (i > 0) {
	// fill `tmp' with `\0' at most 4 times
		for (j = i; j < 4; ++j)
			tmp[j] = '\0';

		for (j = 0; j < 4; ++j) {
			for (l = 0; l < 64; ++l) {
				if (tmp[j] == b64_table[l]) {
					tmp[j] = l;
					break;
				}
			}
		}

		// decode remainder
		buf[0] = (tmp[0] << 2) + ((tmp[1] & 0x30) >> 4);
		buf[1] = ((tmp[1] & 0xf) << 4) + ((tmp[2] & 0x3c) >> 2);
		buf[2] = ((tmp[2] & 0x3) << 6) + tmp[3];

		// write remainer decoded buffer to `dec'
		for (j = 0; (j < i - 1); ++j)
			dec[size++] = buf[j];
	}

	dec[size] = '\0';

	// Return back the size of decoded string if demanded.
	if (decsize != NULL)
		*decsize = size;
}

//needs to be tested again
void b64_encode (char *dst, const unsigned char *src, size_t len) {
	int i = 0;
	int j = 0;
	char *enc = NULL;
	size_t size = 0;
	unsigned char buf[4];
	unsigned char tmp[3];

	// alloc
	enc = dst;

	// parse until end of source
	while (len--) {
		// read up to 3 bytes at a time into `tmp'
		tmp[i++] = *(src++);

		// if 3 bytes read then encode into `buf'
		if (3 == i) {
			buf[0] = (tmp[0] & 0xfc) >> 2;
			buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
			buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
			buf[3] = tmp[2] & 0x3f;

			for (i = 0; i < 4; ++i)
				enc[size++] = b64_table[buf[i]];

			i = 0;
		}
	}

	if (i > 0) {
		// fill `tmp' with `\0' at most 3 times
		for (j = i; j < 3; ++j)
			tmp[j] = '\0';

		// perform same codec as above
		buf[0] = (tmp[0] & 0xfc) >> 2;
		buf[1] = ((tmp[0] & 0x03) << 4) + ((tmp[1] & 0xf0) >> 4);
		buf[2] = ((tmp[1] & 0x0f) << 2) + ((tmp[2] & 0xc0) >> 6);
		buf[3] = tmp[2] & 0x3f;

		// perform same write to `enc` with new allocation
		for (j = 0; (j < i + 1); ++j)
			enc[size++] = b64_table[buf[j]];

		while ((i++ < 3))
			enc[size++] = '=';
	}

	enc[size] = '\0';
}