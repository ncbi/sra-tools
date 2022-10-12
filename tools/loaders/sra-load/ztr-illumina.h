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
#ifndef _sra_load_ztr_illumina_
#define _sra_load_ztr_illumina_

#include <klib/defs.h>

#include "ztr.h"

typedef struct ztr_raw_t {
    uint8_t type[4];
    size_t metalength, datalength;
    uint8_t *meta, *data;
} ztr_raw_t;

/* allocate and initialize a ZTR parser context
   caller is responsible to free resulting pointer

   return 1 if context can't be allocated,
   0 for no error.
 */
rc_t ILL_ZTR_CreateContext(ZTR_Context **ctx);

/* add some bytes to context's input buffer
   the caller is responsible for ensuring that
   the pointer remains valid until the bytes
   are consumed.  the caller is responsible for
   free'ing it.  the context will simply forget
   the bytes when it has consumed them all.
 
   returns 1 if data can't be added to the buffer chain,
   0 for no error.
 */
rc_t ILL_ZTR_AddToBuffer(ZTR_Context *ctx, const uint8_t *data, uint32_t datasize);

/* get a copy of any unconsumed input bytes from the context.
   the bytes are consumed in the process.  the caller is
   responsible for free'ing the resulting pointer.
   the resulting pointer is should be sent in to AddToBuffer
   when more bytes are available.
 
   returns 1 if the *data can't be allocated, 0 for no error.
 */
rc_t ILL_ZTR_BufferGetRemainder(ZTR_Context *ctx, const uint8_t **data, uint32_t *datasize);

bool ILL_ZTR_BufferIsEmpty(const ZTR_Context *ctx);

/* parse a ZTR header from the input buffer
   returns 0 if no error, 1 otherwise
 */
rc_t ILL_ZTR_ParseHeader(ZTR_Context *ctx);

/* parse a ZTR block from the input buffer
   returns
        0: no error
        1: not valid ZTR
        2: out of memory

   the caller is responsible for free'ing ztr_raw->meta and
   ztr_raw->data when it is no longer needed
 */
rc_t ILL_ZTR_ParseBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw);

/* cook a ztr_raw_t to produce a ztr_t
   sets defaults, processes meta data, etc.

   returns
        0: no error
        1: not valid ZTR
        2: out of memory

   Parameters:
    ctx [in]: the context returned from ZTR_CreateContext
    ztr_raw [in]: the structure filled in by ZTR_ParseBlock
    ztr [out]: the resulting data object
    chunkType [out]: how to interpret the data object
        if chunkType is none or unknown, it is probably an error
        if chunkType is ignore, then the chunk was valid and known
        but of no interest to you, e.g. a huffman dictionary chunk
    extraMeta [out optional]: any unprocessed name-value pairs
    extraMetaCount [out optional]: the number of unprocessed name-value pairs
 
   NB. ztr and its pointers refer to ztr_raw, so don't free
   ztr_raw before you are done with ztr
 
   ztr can be free'ed in one-fell-swoop
   *extraMeta can be free'ed in one-fell-swoop
 */
#if ZTR_USE_EXTRA_META
    rc_t ILL_ZTR_ProcessBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw, ztr_t *ztr,
                              enum ztr_chunk_type *chunkType, nvp_t **extraMeta, int *extraMetaCount);
#else
    rc_t ILL_ZTR_ProcessBlock(ZTR_Context *ctx, ztr_raw_t *ztr_raw, ztr_t *ztr,
                              enum ztr_chunk_type *chunkType);
#endif

/* free a context created by ZTR_CreateContext
   do this when done processing the ZTR stream.
 */
rc_t ILL_ZTR_ContextRelease(ZTR_Context *ctx);

rc_t ILL_ZTR_Decompress(ZTR_Context *ctx, enum ztr_chunk_type type, ztr_t ztr, const ztr_t base);

#define ZTR_CreateContext		ILL_ZTR_CreateContext
#define ZTR_ContextRelease		ILL_ZTR_ContextRelease
#define ZTR_AddToBuffer			ILL_ZTR_AddToBuffer
#define ZTR_BufferGetRemainder	ILL_ZTR_BufferGetRemainder
#define ZTR_BufferIsEmpty		ILL_ZTR_BufferIsEmpty
#define ZTR_ParseHeader			ILL_ZTR_ParseHeader
#define ZTR_ParseBlock			ILL_ZTR_ParseBlock
#define ZTR_ProcessBlock		ILL_ZTR_ProcessBlock

#endif /* _sra_load_ztr_illumina_ */
