/*===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 */

#include <klib/sort.h>
#include <klib/rc.h>


#include <stdlib.h>
#include <stdint.h>
#include <memory.h>
#include <byteswap.h>
#include <endian.h>

#include <assert.h>
#include <stdio.h>

#include "ztr-huffman.h"

typedef uint8_t *sym_t;

struct decode_table_t {
	int sig_bits;
	bitsz_t sym_len;
	sym_t symbol;
	decode_table_t *next;
};

static void assign(uint8_t *dst, const uint16_t code[32], int len) {
	len = (len + 7) >> 3;
	
	while (len--)
		*dst++ = *code++;
}

static void inc(uint16_t code[32], int len) {
	int c;
	
	--len;
	code[len >> 3] += 0x80 >> (len & 7);
	len >>= 3;
	c = code[len] >> 8;
	while (len && c) {
		code[len] &= 0xFF;
		--len;
		++code[len];
		c = code[len] >> 8;
	}
}

struct index_t {
	uint16_t i, len;
};

static int idx_cmp(const void *A, const void *B, void * ignored) {
	const struct index_t *a = A;
	const struct index_t *b = B;
	
	if (a->len < b->len)
		return -1;
	if (a->len > b->len)
		return 1;
	if (a->i < b->i)
		return -1;
	if (a->i > b->i)
		return 1;
	return 0;
}

#if 0
static void huffman_codes(uint8_t **codes, const uint8_t length[256]) {
	struct index_t idx[256];
	size_t total_len;
	int i;
	uint16_t code[32];
	uint8_t *cp = (uint8_t *)&codes[256];
	
	for (total_len = 0, i = 0; i != 256; ++i) {
		total_len += ((idx[i].len = length[i]) + 7) >> 3;
		idx[i].i = i;
	}
	ksort(idx, 256, sizeof(*idx), idx_cmp, NULL);
	for (i = 0; i != 256 && idx[i].len == 0; ++i)
		;
	memset(code, 0, 32);
	for ( ; i != 256; ++i) {
		uint16_t j = idx[i].i, n = idx[i].len;
		codes[j] = cp;
		assign(cp, code, n);
		inc(code, n);
		cp += (n + 7) >> 3;
	}
}
#endif

static void huffman_codes257(uint8_t **codes, const uint8_t length[257]) {
	struct index_t idx[257];
	size_t total_len;
	int i;
	uint16_t code[32];
	uint8_t *cp = (uint8_t *)&codes[257];
	
	for (total_len = 0, i = 0; i != 257; ++i) {
		total_len += ((idx[i].len = length[i]) + 7) >> 3;
		idx[i].i = i;
	}
	ksort(idx, 257, sizeof(*idx), idx_cmp, NULL);
	for (i = 0; i != 257 && idx[i].len == 0; ++i)
		;
	memset(code, 0, 32);
	for ( ; i != 257; ++i) {
		uint16_t j = idx[i].i, n = idx[i].len;
		codes[j] = cp;
		assign(cp, code, n);
		inc(code, n);
		cp += (n + 7) >> 3;
	}
}

static void huffman_codes19(uint8_t **codes, const uint8_t length[19]) {
	struct index_t idx[19];
	size_t total_len;
	int i;
	uint16_t code[32];
	uint8_t *cp = (uint8_t *)&codes[19];
	
	for (total_len = 0, i = 0; i != 19; ++i) {
		total_len += ((idx[i].len = length[i]) + 7) >> 3;
		idx[i].i = i;
	}
	ksort(idx, 19, sizeof(*idx), idx_cmp, NULL);
	for (i = 0; i != 19 && idx[i].len == 0; ++i)
		;
	memset(code, 0, 32);
	for ( ; i != 19; ++i) {
		uint8_t j = idx[i].i, n = idx[i].len;
		codes[j] = cp;
		assign(cp, code, n);
		inc(code, n);
		cp += (n + 7) >> 3;
	}
}

static uint16_t get_bits1(const uint8_t *data, int *bp, int bc) {
	uint16_t val = *(uint16_t *)(data + (*bp >> 3));
	
#if __BYTE_ORDER == __BIG_ENDIAN
	val = bswap_16(val);
#endif
	val >>= *bp & 0x07;
	val &= (1 << bc) - 1;
	*bp += bc;
	return val;
}

static uint32_t get_bits4(const uint8_t *data, int *bp, int bc) {
	uint32_t val = *(uint32_t *)(data + (*bp >> 3));
	
#if __BYTE_ORDER == __BIG_ENDIAN
	val = bswap_32(val);
#endif
	val >>= *bp & 0x07;
	val &= (1L << bc) - 1;
	*bp += bc;
	return val;
}

#if 0
static int get_bits(const uint8_t *data, int *bp, int bc) {
	uint32_t val = get_bits4(data, bp, bc);

	fprintf(stderr, "%02X/%u\n", val, bc);
	return val;
}
#else
#define get_bits get_bits4
#endif

static rc_t build_table(
						decode_table_t *tbl,
						uint8_t *pcode,
						bitsz_t len,
						const sym_t sym,
						bitsz_t sym_len
) {
	int i, code = *pcode, mask;
	static const uint8_t rev[] = 
		"\x00\x80\x40\xc0\x20\xa0\x60\xe0\x10\x90\x50\xd0\x30\xb0\x70\xf0"
		"\x08\x88\x48\xc8\x28\xa8\x68\xe8\x18\x98\x58\xd8\x38\xb8\x78\xf8"
		"\x04\x84\x44\xc4\x24\xa4\x64\xe4\x14\x94\x54\xd4\x34\xb4\x74\xf4"
		"\x0c\x8c\x4c\xcc\x2c\xac\x6c\xec\x1c\x9c\x5c\xdc\x3c\xbc\x7c\xfc"
		"\x02\x82\x42\xc2\x22\xa2\x62\xe2\x12\x92\x52\xd2\x32\xb2\x72\xf2"
		"\x0a\x8a\x4a\xca\x2a\xaa\x6a\xea\x1a\x9a\x5a\xda\x3a\xba\x7a\xfa"
		"\x06\x86\x46\xc6\x26\xa6\x66\xe6\x16\x96\x56\xd6\x36\xb6\x76\xf6"
		"\x0e\x8e\x4e\xce\x2e\xae\x6e\xee\x1e\x9e\x5e\xde\x3e\xbe\x7e\xfe"
		"\x01\x81\x41\xc1\x21\xa1\x61\xe1\x11\x91\x51\xd1\x31\xb1\x71\xf1"
		"\x09\x89\x49\xc9\x29\xa9\x69\xe9\x19\x99\x59\xd9\x39\xb9\x79\xf9"
		"\x05\x85\x45\xc5\x25\xa5\x65\xe5\x15\x95\x55\xd5\x35\xb5\x75\xf5"
		"\x0d\x8d\x4d\xcd\x2d\xad\x6d\xed\x1d\x9d\x5d\xdd\x3d\xbd\x7d\xfd"
		"\x03\x83\x43\xc3\x23\xa3\x63\xe3\x13\x93\x53\xd3\x33\xb3\x73\xf3"
		"\x0b\x8b\x4b\xcb\x2b\xab\x6b\xeb\x1b\x9b\x5b\xdb\x3b\xbb\x7b\xfb"
		"\x07\x87\x47\xc7\x27\xa7\x67\xe7\x17\x97\x57\xd7\x37\xb7\x77\xf7"
		"\x0f\x8f\x4f\xcf\x2f\xaf\x6f\xef\x1f\x9f\x5f\xdf\x3f\xbf\x7f\xff";
	
	if (len > 8) {
		int j = rev[code];
		
		if (tbl[j].next == NULL)
			tbl[j].next = calloc(256, sizeof(decode_table_t));
		if (tbl[j].next == NULL)
			return RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
		
		tbl[j].sig_bits = 8;
		return build_table(tbl[j].next, pcode + 1, len - 8, sym, sym_len);
	}
	code &= (mask = (0xFF00 >> len) & 0xFF);
	for (i = code; i != 256; ++i) {
		int j = rev[i];
		
		if ((i & mask) != code)
			break;
		assert(tbl[j].sig_bits == 0);
		tbl[j].sym_len = sym ? sym_len : 1;
		tbl[j].symbol = sym;
		tbl[j].sig_bits = len;
	}
	return 0;
}

static void	fixup_table(decode_table_t *dt, decode_table_t *root) {
	int i;
	
	for (i = 0; i != 256; ++i) {
		if (dt[i].next)
			fixup_table(dt[i].next, root);
		else
			dt[i].next = root;
	}
}

static void free_table(decode_table_t *dt, decode_table_t *root) {
	int i;
	
	for (i = 0; i != 256; ++i) {
		if (dt[i].next && dt[i].next != root)
			free_table(dt[i].next, root);
	}
	free(dt);
}


static const uint8_t symbols[290] = 
	"\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	"\x20\x21\x22\x23\x24\x25\x26\x27\x28\x29\x2a\x2b\x2c\x2d\x2e\x2f"
	"\x30\x31\x32\x33\x34\x35\x36\x37\x38\x39\x3a\x3b\x3c\x3d\x3e\x3f"
	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x4a\x4b\x4c\x4d\x4e\x4f"
	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f"
	"\x60\x61\x62\x63\x64\x65\x66\x67\x68\x69\x6a\x6b\x6c\x6d\x6e\x6f"
	"\x70\x71\x72\x73\x74\x75\x76\x77\x78\x79\x7a\x7b\x7c\x7d\x7e\x7f"
	"\x80\x81\x82\x83\x84\x85\x86\x87\x88\x89\x8a\x8b\x8c\x8d\x8e\x8f"
	"\x90\x91\x92\x93\x94\x95\x96\x97\x98\x99\x9a\x9b\x9c\x9d\x9e\x9f"
	"\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"
	"\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7\xb8\xb9\xba\xbb\xbc\xbd\xbe\xbf"
	"\xc0\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9\xca\xcb\xcc\xcd\xce\xcf"
	"\xd0\xd1\xd2\xd3\xd4\xd5\xd6\xd7\xd8\xd9\xda\xdb\xdc\xdd\xde\xdf"
	"\xe0\xe1\xe2\xe3\xe4\xe5\xe6\xe7\xe8\xe9\xea\xeb\xec\xed\xee\xef"
	"\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9\xfa\xfb\xfc\xfd\xfe\xff";

static void decode_zlib_length_codes(uint8_t *length, const uint8_t *data, int *bp, size_t datasize) {
	uint8_t length19[19];
	int llc, dc, clc;
	uint8_t storage19[19 * sizeof(uint8_t *) + 19 * 32];
	uint8_t **codes19;
	decode_table_t dt1[256];
	static const uint8_t ndx[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
	int i, n;
	int bits = datasize << 3;
	
	codes19 = (uint8_t **)storage19;
	memset(length19, 0, sizeof(length19));
	memset(storage19, 0, sizeof(storage19));
	
	llc = get_bits1(data, bp, 5) + 257;
	dc  = get_bits1(data, bp, 5) + 1;
	clc = get_bits1(data, bp, 4) + 4;

	for (i = 0; i != clc; ++i)
		length19[ndx[i]] = get_bits(data, bp, 3);

	huffman_codes19(codes19, length19);
	memset(dt1, 0, sizeof(dt1));
	for (i = 0; i != 19; ++i) {
		if (codes19[i] != NULL)
			build_table(dt1, codes19[i], length19[i], (sym_t)(symbols + i), 8);
	}
	fixup_table(dt1, dt1);
	i = 0;
	while (bits > *bp) {
		uint8_t code;
		const decode_table_t *cp;
		int bc = 8;
		
		if (bits - *bp < 8)
			bc = bits - *bp;
		code = get_bits1(data, bp, bc);
		cp = dt1 + code;
		/* fprintf(stderr, "%02X/%u\n", code & ((1L << cp->sig_bits) - 1), cp->sig_bits); */
		*bp -= bc - cp->sig_bits;

		switch (cp->symbol[0]) {
			case 16:
				n = get_bits1(data, bp, 2) + 3;
				memset(length + i, length[i - 1], n);
				i += n;
				break;
			case 17:
				n = get_bits1(data, bp, 3) + 3;
				memset(length + i, 0, n);
				i += n;
				break;
			case 18:
				n = get_bits1(data, bp, 7) + 11;
				memset(length + i, 0, n);
				i += n;
				break;
			default:
				length[i++] = cp->symbol[0];
				break;
		}
		if (i >= llc)
			break;
	}
	assert(i == llc);
	i = 0;
	while (bits > *bp) {
		uint8_t code;
		const decode_table_t *cp;
		int bc = 8;
		
		if (bits - *bp < 8)
			bc = bits - *bp;
		code = get_bits1(data, bp, bc);
		cp = dt1 + code;
		/* fprintf(stderr, "%02X/%u\n", code & ((1L << cp->sig_bits) - 1), cp->sig_bits); */
		*bp -= bc - cp->sig_bits;
		
		switch (cp->symbol[0]) {
			case 16:
				n = get_bits1(data, bp, 2) + 3;
				i += n;
				break;
			case 17:
				n = get_bits1(data, bp, 3) + 3;
				i += n;
				break;
			case 18:
				n = get_bits1(data, bp, 7) + 11;
				i += n;
				break;
			default:
				++i;
				break;
		}
		if (i >= dc)
			break;
	}
	assert(i == dc);
}

rc_t handle_huffman_codes(ztr_huffman_table *y, const uint8_t *data, size_t datasize) {
	int bc = 0;
	int i, j;
	uint8_t storage[257 * sizeof(uint8_t *) + 257 * 32];
	uint8_t **codes;

	codes = (uint8_t **)storage;
	
	get_bits1(data, &bc, 1);
	
	switch (get_bits1(data, &bc, 2)) {
		default:
			return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
		case 2:
			y->tblcnt = 1;
			break;
		case 3:
			y->tblcnt = get_bits(data, &bc, get_bits1(data, &bc, 4) + 1) + 1;
			break;
	}
	y->tbl = calloc(y->tblcnt, sizeof(*y->tbl));
	for (i = 0; i != y->tblcnt; ++i) {
		uint8_t length[289];
		
		memset(length, 0, sizeof(length));
		memset(storage, 0, sizeof(storage));
		decode_zlib_length_codes(length, data, &bc, datasize);
		huffman_codes257(codes, length);
		y->tbl[i] = calloc(257, sizeof(decode_table_t));
		for (j = 0; j != 256; ++j) {
			if (codes[j] != NULL)
				build_table(y->tbl[i], codes[j], length[j], (sym_t)(symbols + j), 8);
		}
		if (codes[256] != NULL)
			build_table(y->tbl[i], codes[256], length[256], (sym_t)0, 0);
	}
	y->bits_left = (datasize << 3) - bc;
	
	return 0;
}

rc_t handle_special_huffman_codes(ztr_huffman_table *y, int which_one) {
	typedef uint8_t length_array[257];
	
	static const length_array special0 = 
        {
            5, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 2, 14, 2, 14, 14, 14, 3, 14, 14, 14, 14, 14, 14, 4, 14, 
            14, 14, 14, 14, 2, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 
            6
        };
	static const length_array special1 = 
        {
            7, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 7, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 2, 8, 2, 8, 15, 15, 3, 8, 15, 15, 8, 15, 8, 4, 15, 
            15, 15, 8, 8, 2, 15, 8, 8, 15, 8, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            11
        };
	static const length_array special2 = 
        {
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 6, 15, 15, 6, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            3, 11, 8, 15, 15, 15, 15, 10, 15, 15, 15, 15, 6, 9, 7, 15, 
            11, 11, 15, 15, 15, 15, 15, 15, 15, 15, 15, 10, 15, 15, 15, 10, 
            15, 9, 10, 10, 15, 10, 11, 11, 10, 8, 15, 15, 15, 10, 9, 15, 
            15, 15, 15, 10, 9, 15, 15, 10, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 4, 7, 6, 5, 3, 6, 6, 5, 4, 15, 8, 5, 6, 4, 4, 
            6, 15, 5, 4, 4, 5, 7, 6, 10, 6, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
            15
        };
	int i, j;
	uint8_t storage[257 * sizeof(uint8_t *) + 257 * 32];
	uint8_t **codes;
	const uint8_t *special;
    
	codes = (uint8_t **)storage;
	
	switch (which_one) {
	case 0:
		special = special0;
		break;
	case 1:
		special = special1;
		break;
	case 2:
		special = special2;
		break;
	default:
		assert (which_one >= 0 && which_one < 3);
        return RC(rcSRA, rcFormatter, rcParsing, rcData, rcInvalid);
	}

	if (y->tbl)
        return 0;
	y->tbl = calloc(y->tblcnt, sizeof(*y->tbl));
    y->tblcnt = 1;
    
	for (i = 0; i != y->tblcnt; ++i) {
		memset(storage, 0, sizeof(storage));
		huffman_codes257(codes, special);
		y->tbl[i] = calloc(257, sizeof(decode_table_t));
		for (j = 0; j != 256; ++j) {
			if (codes[j] != NULL)
				build_table(y->tbl[i], codes[j], special[j], (sym_t)(symbols + j), 8);
		}
		if (codes[256] != NULL)
			build_table(y->tbl[i], codes[256], special[256], (sym_t)0, 0);
	}
	y->bits_left = 0;
	
	return 0;
}

rc_t decompress_huffman(const ztr_huffman_table *tbl, uint8_t **Data, size_t *datasize) {
#if 0
	const uint8_t *src = *Data;
	size_t srclen = *datasize, dstlen = 0;
	size_t sbits, dalloc;
	uint8_t *dst, *Dst;
	int bp, tblNo = 0, n = tbl->tblcnt;
	const decode_table_t *cp = tbl->tbl[0];
    
    bp =  8 - tbl->bits_left;
	src += 2; srclen -= 2;
    
	Dst = dst = malloc(dalloc = srclen << 1);
	if (Dst != NULL) {
		sbits = srclen << 3;
		while (sbits) {
			uint8_t code;
			int bc = 8;
			
			if (sbits < 8)
				bc = sbits;
			code = get_bits1(src, &bp, bc);
			cp += code;
            
			bp -= bc - cp->sig_bits;
			sbits -= cp->sig_bits;
			src += bp >> 3;
			bp &= 7;
			
			if (cp->symbol == NULL) {
				if (cp->next == NULL) {
					break;
                }
			}
			else {
				if (++dstlen == dalloc) {
					void *temp;
					
					temp = realloc(Dst, dalloc <<= 1);
					if (temp == NULL)
						goto NoMem;
					
					Dst = temp;
					dst = Dst + dstlen - 1;
				}
				tblNo = (tblNo + 1) % n;
				*dst++ = *cp->symbol;
			}
			cp = cp->next;
			if (cp == NULL)
				cp = tbl->tbl[tblNo];
		}
		free((void *)*Data);
		*Data = Dst;
		*datasize = dstlen;
		return 0;
	}
#else
	const uint8_t *src = *Data;
	size_t srclen = *datasize, dstlen = 0;
	const uint8_t *endp = src + srclen;
	size_t dalloc;
	uint8_t *dst, *Dst;
	int tblNo = 0, n = tbl->tblcnt;
	const decode_table_t *cp = tbl->tbl[0];
    uint16_t code;
    int bits = tbl->bits_left;
    
	src += 2; srclen -= 2;
    code = (*src++) >> (8 - bits);
    
	Dst = dst = malloc(dalloc = srclen << 2);
	if (Dst != NULL) {
		while (src != endp) {
            if (bits < 8) {
                code |= ((uint16_t)(*src++)) << bits;
                bits += 8;
            }
        PROCESS_BITS:
			cp += code & 0xFF;
            
            bits -= cp->sig_bits;
            code >>= cp->sig_bits;
            
			if (cp->symbol == NULL) {
                cp = cp->next;
                if (cp)
                    continue;
                assert(src == endp && bits < 8);
                bits = 0;
                break;
			}
            if (++dstlen == dalloc) {
                void *temp;
                
                temp = realloc(Dst, dalloc <<= 1);
                if (temp == NULL)
                    goto NoMem;
                
                Dst = temp;
                dst = Dst + dstlen - 1;
            }
            *dst++ = *cp->symbol;
            tblNo = (tblNo + 1) % n;
			cp = tbl->tbl[tblNo];
		}
        if (bits)
            goto PROCESS_BITS;
		free((void *)*Data);
		*Data = Dst;
		*datasize = dstlen;
		return 0;
	}
#endif
NoMem:
	free((void *)*Data);
	*Data = NULL;
	*datasize = 0;
	return RC(rcSRA, rcFormatter, rcParsing, rcMemory, rcExhausted);
}

rc_t free_huffman_table(ztr_huffman_table *tbl) {
    int i;
    
    for (i = 0; i != tbl->tblcnt; ++i) {
        free_table(tbl->tbl[i], tbl->tbl[i]);
    }
    free(tbl->tbl);
    return 0;
}
