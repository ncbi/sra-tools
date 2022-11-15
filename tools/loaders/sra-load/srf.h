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
#ifndef _sra_load_srf_
#define _sra_load_srf_

#include "writer-illumina.h"

#define SRF_READ_BAD (1U << 0)
#define SRF_READ_WITHDRAWN (1U << 1)

#define SRF_CONTAINER_HEADER_MIN_SIZE (8)
#define SRF_BLOCK_HEADER_MIN_SIZE (5)

enum SRF_ChunkTypes {
    SRF_ChunkTypeUnknown = '?',
    SRF_ChunkTypeContainer = 'S',
    SRF_ChunkTypeIndex = 'I',
    SRF_ChunkTypeHeader = 'H',
    SRF_ChunkTypeRead = 'R',
    SRF_ChunkTypeXML = 'X'
};

/* SRF_ParseChunk
   try to parse an SRF chunk

   returns:
       0: no error
       rcInsufficient: not enough src data to parse;
           provide more data or declare an error
       rcInvalid: the data is invalid

   Parameters:
       src [in, required]: the bytes to parse
       srclen: the number of bytes parsable
       size [out, required]: the total size of the chunk
           the next chunk starts at src + size
       type [out, required]: the type of chunk found
       data [out, required]: the start of any inner data
          in the chunk
*/
enum RCState SRF_ParseChunk(const uint8_t *src,
		                    size_t srclen,
		                    uint64_t *size,
		                    enum SRF_ChunkTypes *type,
		                    size_t *data);

/* SRF_ParseContainerHeader
 try to parse an SRF Container Header
 
 Returns:
	0: no error
        rcInsufficient: not enough data
	rcInvalid: data invalid

 Parameters:
	src [in, required]: the bytes to parse
	src_length: the length of src
	versMajor, versMinor [out, optional]: SRF version
	contType [out, optional]: type of the data contained in
		this SRF; *contType should be 'Z' per the spec.
	baseCaller [out, optional]: the base caller program that
		generated the contained data
	baseCallerVersion [out, optional]: the version of
		base caller program
 */
int SRF_ParseContainerHeader(const uint8_t *src,
                             uint64_t src_length,
                             unsigned *versMajor,
                             unsigned *versMinor,
                             char *contType,
                             pstring *baseCaller,
                             pstring *baseCallerVersion);

/* SRF_ParseDataChunk
 try to parse an SRF data chunk (i.e. block header type = 'H')
 
 Returns:
	0: no error
        rcInsufficient: not enough data
	rcInvalid: data invalid
 
 Parameters:
	src [in, required]: the bytes to parse
	src_length: the length of src
	parsed_length [out, required]: length of SRF data in
		this chunk; the bytes src[parsed_length..src_length],
		if any, are ZTR.
	prefix [out, optional]: (pstring) read name prefix, if any
	prefixType [out, optional]: read name prefix type 'E' or 'I'
	counter [out, optional]: initial value of counter, only
		relevant if prefixType == 'I'
 */
int SRF_ParseDataChunk(const uint8_t *src,
                       size_t src_length,
                       size_t *parsed_length,
                       pstring *prefix,
                       char *prefixType,
                       uint32_t *counter);

/* SRF_ParseReadChunk
 try to parse an SRF read chunk (i.e. block header type = 'R')
 
 Returns:
	0: no error
        rcInsufficient: not enough data
	rcInvalid: data invalid
 
 Parameters:
	src [in, required]: the bytes to parse
	src_length: the length of src
	parsed_length [out, required]: length of SRF data in
		this chunk; the bytes src[parsed_length..src_length],
		if any, are ZTR.
	flags [out, optional]: read flags
	id [out, optional]: read Id
 */
int SRF_ParseReadChunk(const uint8_t *src,
		               size_t src_length,
		               size_t *parsed_length,
		               uint8_t *flags,
		               pstring *id);

#endif /* _sra_load_srf_ */
