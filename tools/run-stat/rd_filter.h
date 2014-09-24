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

#ifndef _h_rd_filter_
#define _h_rd_filter_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/namelist.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/text.h>
#include <vdb/cursor.h>
#include "definitions.h"
#include "helper.h"

typedef struct rd_filter
{
    /* the index of the columns needed */
    uint32_t idx_READ;
    uint32_t idx_QUALITY;

    uint32_t idx_TRIM_START;
    uint32_t idx_TRIM_LEN;

    uint32_t idx_READ_START;
    uint32_t idx_READ_LEN;
    uint32_t idx_READ_FILTER;
    uint32_t idx_READ_TYPE;

    /* the data to be read */
    char_buffer bases;
    char_buffer filtered_bases;
    char_buffer quality;
    char_buffer filtered_quality;
    uint32_t trim_start;
    uint32_t trim_end;

    int_buffer read_start;
    int_buffer read_len;
    int_buffer read_filter;
    int_buffer read_type;

    /* flags, how the read is to be processed */
    bool bio;
    bool trim;
    bool cut;

} rd_filter;
typedef rd_filter* p_rd_filter;


rc_t rd_filter_init( p_rd_filter self );

void rd_filter_destroy( p_rd_filter self );

rc_t rd_filter_pre_open( p_rd_filter self, const VCursor * cur );

rc_t rd_filter_read( p_rd_filter self, const VCursor * cur );

rc_t rd_filter_set_flags( p_rd_filter self, bool bio, bool trim, bool cut );

uint32_t filter_get_read_len( p_rd_filter self );

char filter_get_base( p_rd_filter self, const uint32_t idx );

uint8_t filter_get_quality( p_rd_filter self, const uint32_t idx );

#ifdef __cplusplus
}
#endif

#endif
