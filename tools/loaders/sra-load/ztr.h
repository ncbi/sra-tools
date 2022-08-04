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
#ifndef _sra_load_ztr_
#define _sra_load_ztr_

#define ZTR_USE_TEXT_CHUNK 0
#define ZTR_USE_EXTRA_META 0

#define ZTR_SIG "\xaeZTR\r\n\x1A\n"
/* does buffer contain a ZTR signature?
 */
#define ZTR_IsSignature(X, N) ((N >= 8) && (memcmp(X, ZTR_SIG, 8) == 0) ? 1 : 0)

#define METANAME_IS_CASE_SENSITIVE 1

enum ztr_data_type {
	i8,
	i16,
	i32,
	f32,
	f64,
	vector4 = 16,
	i8v4  = i8  | vector4,
	i16v4 = i16 | vector4,
	i32v4 = i32 | vector4,
	f32v4 = f32 | vector4,
	f64v4 = f64 | vector4
};

enum ztr_chunk_type {
	none,
	unknown,
	ignore,
	BASE,
	BPOS,
	CLIP,
	CNF1,
	CNF4,
	REGN,
	SAMP,
	SMP4,
	TEXT,
	COMM,
	CR32,
	HUFF,

	READ = BASE,
	POSITION = BPOS,
	QUALITY1 = CNF1,
	QUALITY4 = CNF4,
	READ_STRUCTURE = REGN,
	SIGNAL1 = SAMP,
	SIGNAL4 = SMP4
};

enum region_type {
	Biological = 'B',
	Duplicate = 'D',
	Explicit = 'E',
	Inverted = 'I',
	Normal = 'N',
	Paired = 'P',
	Technical = 'T'
};

typedef struct region_t region_t;
struct region_t {
	int start;
	enum region_type type;
	char *name;
};

struct ztr_sequence_t {
	enum ztr_data_type datatype;
	size_t datasize;
	uint8_t *data;
	char *charset;
};

struct ztr_position_t {
	enum ztr_data_type datatype;
	size_t datasize;
	uint8_t *data;
};

struct ztr_clip_t {
	uint32_t left, right;
};

struct ztr_quality1_t {
	enum ztr_data_type datatype;
	size_t datasize;
	uint8_t *data;
	char *scale;
};

struct ztr_quality4_t {
	enum ztr_data_type datatype;
	size_t datasize;
	uint8_t *data;
	char *scale;
};

struct ztr_comment_t {
	size_t size;
	char *text;
};

struct ztr_region_t {
	size_t count;
	region_t region[1];
};

struct ztr_signal_t {
	enum ztr_data_type datatype;
	size_t datasize;
	uint8_t *data;
	char *channel;
	long offset;
};

struct ztr_signal4_t {
	enum ztr_data_type datatype;
	size_t datasize;
	uint8_t *data;
	char *Type;
	long offset;
};

typedef struct nvp_t {
    const char *name,
    *value;
} nvp_t;

#if ZTR_USE_TEXT_CHUNK
struct ztr_xmetadata_t {
	size_t count;
	nvp_t data[1];
};
#endif

typedef union ztr_t {
	struct ztr_sequence_t   *sequence;  /* chunk type 'BASE' */
	struct ztr_position_t   *position;  /* chunk type 'BPOS' */
	struct ztr_clip_t       *clip;      /* chunk type 'CLIP' */
	struct ztr_quality1_t   *quality1;  /* chunk type 'CNF1' */
	struct ztr_quality4_t   *quality4;  /* chunk type 'CNF4' */
	struct ztr_comment_t    *comment;   /* chunk type 'COMM' */
	struct ztr_region_t     *region;    /* chunk type 'REGN' */
	struct ztr_signal_t     *signal;    /* chunk type 'SAMP' */
	struct ztr_signal4_t    *signal4;   /* chunk type 'SMP4' */
#if ZTR_USE_TEXT_CHUNK
	struct ztr_xmetadata_t  *xmeta;     /* chunk type 'TEXT' */
#endif
} ztr_t;

typedef struct ZTR_Context ZTR_Context;

#endif /* _sra_load_ztr_ */
