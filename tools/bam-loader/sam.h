/* ===========================================================================
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

#pragma once

#include <stdint.h>
#include "bam-alignment.h"

typedef struct SAM2BAM_Parser SAM2BAM_Parser;

typedef struct RefNameLookupContext RefNameLookupContext; /**< caller defined **/
typedef bool (*RefNameLookupFunction)(RefNameLookupContext *ctx, int ch, int32_t *result, rc_t *rc);

/** implementation details **/
typedef bool (*SAM_parser_f)(SAM2BAM_Parser *, int, rc_t *, char const *, char const *);
typedef struct number_parser number_parser;
struct number_parser {
    uint64_t mantissa;
    int32_t exponent;
    signed sgn:2;
    unsigned shift:6; /**< [0 - 19] **/
    unsigned state:23;
    unsigned ok:1;
};

/** \brief parser state **/
struct SAM2BAM_Parser {
    void *static_buffer;
    size_t max_size;
    bam_alignment *rslt; /**< on successful parse this is the result **/
    size_t rslt_size;    /**< on successful parse this is the size of the result **/

    /* implementation details follow */
    SAM_parser_f parser;
    size_t field_out_size;  /**< parsed size; only makes sense for some fields **/
    size_t field_in_size;   /**< input byte count for the current field (excluding delimters) **/
    size_t record_chars;    /**< input byte count for the whole record (including delimters but not line-feed) **/
    int field;
    int state;  /**< each parser has its own idea of state; state is always 0 at start of field **/
    bool cr;

    size_t extraArrayCountPos;
    number_parser numeric;
    RefNameLookupFunction lookup;
    RefNameLookupContext *lookup_ctx;
};

SAM2BAM_Parser *SAM2BAM_Parser_initialize(SAM2BAM_Parser *const self, void *const static_buffer, size_t const buffer_size, RefNameLookupFunction lookup, RefNameLookupContext *lookup_ctx);

SAM2BAM_Parser *SAM2BAM_Parser_parse(  char const *const data
                                     , unsigned const fields
                                     , unsigned const *const offsets
                                     , void *const static_buffer
                                     , size_t const buffer_size
                                     , RefNameLookupFunction lookup
                                     , RefNameLookupContext *lookup_ctx
                                     , rc_t *rc
                                     );
