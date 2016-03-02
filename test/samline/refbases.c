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

#include <klib/rc.h>

#include <kfs/directory.h>

#include <vdb/manager.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <os-native.h>
#include <sysalloc.h>

#include <string.h>

#include "refbases.h"

static uint32_t read_uint32( const VCursor * cur, uint32_t col_idx )
{
    uint32_t elem_bits, boff, row_len;
    const uint32_t * value;
    rc_t rc = VCursorCellDataDirect ( cur, 1, col_idx, &elem_bits, (const void**)&value, &boff, &row_len );
    if ( rc == 0 )
        return *value;
    return 0;
}

static uint32_t read_buffer( const VCursor * cur, char * buffer, int64_t row_id,
                             uint32_t offset, size_t buflen, uint32_t col_idx )
{
    uint32_t elem_bits, boff, row_len, res = 0;
    const char * value;
    rc_t rc = VCursorCellDataDirect ( cur, row_id, col_idx, &elem_bits, (const void**)&value, &boff, &row_len );
    if ( rc == 0 && row_len > offset )
    {
        res = ( row_len - offset );
        if ( res > buflen ) res = buflen;
        memcpy ( buffer, &value[ offset ], res );
    }
    return res;
}


static uint32_t read_bases( const VCursor * cur, char * buffer, uint32_t ref_pos_1_based, 
                            uint32_t ref_len, uint32_t col_idx, uint32_t max_seq_len )
{
    uint32_t res = 0, n_read = 1;
    uint32_t row_id = ( ( ref_pos_1_based - 1 ) / max_seq_len ) + 1;
    uint32_t offset = ( ref_pos_1_based - 1 ) - ( ( row_id - 1 ) * max_seq_len );
    size_t buflen = ref_len;
    while ( res < ref_len && n_read > 0 )
    {
        n_read = read_buffer( cur, &buffer[ res ], row_id++, offset, buflen, col_idx );
        res += n_read;
        buflen -= n_read;
        offset = 0;
    }
    return res;
}


char * read_refbases( const char * refname, uint32_t ref_pos_1_based, uint32_t ref_len, uint32_t * bases_in_ref )
{
    char * res = NULL;
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc == 0 )
    {
        const VDBManager * mgr;
        rc = VDBManagerMakeRead ( &mgr, dir );
        if ( rc == 0 )
        {
            const VTable * tab;
            rc = VDBManagerOpenTableRead( mgr, &tab, NULL, "%s", refname );
            if ( rc == 0 )
            {
                const VCursor * cur;
                rc = VTableCreateCursorRead( tab, &cur );
                if ( rc == 0 )
                {
                    uint32_t base_count_idx, read_idx, max_seq_len_idx;
                    rc = VCursorAddColumn( cur, &base_count_idx, "BASE_COUNT" );
                    if ( rc == 0 )
                    {
                        rc = VCursorAddColumn( cur, &read_idx, "READ" );
                        if ( rc == 0 )
                        {
                            rc = VCursorAddColumn( cur, &max_seq_len_idx, "MAX_SEQ_LEN" );
                            if ( rc == 0 )
                            {
                                rc = VCursorOpen ( cur );
                                if ( rc == 0 )
                                {
                                    uint32_t base_count = read_uint32( cur, base_count_idx );
                                    if ( bases_in_ref != NULL )
                                        *bases_in_ref = base_count;
                                    uint32_t max_seq_len = read_uint32( cur, max_seq_len_idx );
                                    if ( base_count > ( ref_pos_1_based + ref_len ) && max_seq_len > 0 )
                                    {
                                        res = malloc( ref_len + 1 );
                                        if ( res != NULL )
                                        {
                                            uint32_t n_read = read_bases( cur, res, ref_pos_1_based,
                                                                          ref_len, read_idx, max_seq_len );
                                            res[ n_read ] = 0;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    VCursorRelease( cur );
                }
                VTableRelease( tab );
            }
            VDBManagerRelease( mgr );
        }
        KDirectoryRelease( dir );
    }
    return res;
}


uint32_t ref_len( const char * refname )
{
    uint32_t res = 0;
    KDirectory * dir;
    rc_t rc = KDirectoryNativeDir( &dir );
    if ( rc == 0 )
    {
        const VDBManager * mgr;
        rc = VDBManagerMakeRead ( &mgr, dir );
        if ( rc == 0 )
        {
            const VTable * tab;
            rc = VDBManagerOpenTableRead( mgr, &tab, NULL, "%s", refname );
            if ( rc == 0 )
            {
                const VCursor * cur;
                rc = VTableCreateCursorRead( tab, &cur );
                if ( rc == 0 )
                {
                    uint32_t base_count_idx;
                    rc = VCursorAddColumn( cur, &base_count_idx, "BASE_COUNT" );
                    if ( rc == 0 )
                    {
                        rc = VCursorOpen ( cur );
                        if ( rc == 0 )
                            res = read_uint32( cur, base_count_idx );
                    }
                    VCursorRelease( cur );
                }
                VTableRelease( tab );
            }
            VDBManagerRelease( mgr );
        }
        KDirectoryRelease( dir );
    }
    return res;
}
