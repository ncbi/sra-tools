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

struct GlobalPosLenReader;
#define COLREADER_IMPL struct GlobalPosLenReader

#include "glob-poslen.h"
#include "tbl-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "map-file.h"
#include "sra-sort.h"

#include <insdc/insdc.h>
#include <vdb/cursor.h>
#include <vdb/vdb-priv.h>
#include <klib/printf.h>
#include <klib/sort.h>
#include <klib/rc.h>

#include <string.h>
#include <assert.h>

FILE_ENTRY ( glob-poslen );


/*--------------------------------------------------------------------------
 * GlobalPosLenReader
 */
struct GlobalPosLenReader
{
    ColumnReader dad;

    uint64_t poslen;

    const VCursor *curs;
    uint32_t idx [ 3 ];

    uint32_t chunk_size;

    size_t full_spec_size;
    char full_spec [ 1 ];
};

static
void GlobalPosLenReaderWhack ( GlobalPosLenReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    VCursorRelease ( self -> curs );
    MemFree ( ctx, self, sizeof * self + self -> full_spec_size );
}

static
const char *GlobalPosLenReaderFullSpec ( const GlobalPosLenReader *self, const ctx_t *ctx )
{
    return self -> full_spec;
}

static
uint64_t GlobalPosLenReaderIdRange ( const GlobalPosLenReader *self, const ctx_t *ctx, int64_t *opt_first )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    uint64_t count;

    rc = VCursorIdRange ( self -> curs, 0, opt_first, & count );
    if ( rc != 0 )
        ERROR ( rc, "VCursorIdRange failed on '%s'", self -> full_spec );

    return count;
}

static
void GlobalPosLenReaderPreCopy ( GlobalPosLenReader *self, const ctx_t *ctx )
{
}

static
void GlobalPosLenReaderPostCopy ( GlobalPosLenReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    /* called after a single copy
       assumes that entire row-range of table has been copied */

    rc = VCursorRelease ( self -> curs );
    if ( rc != 0 )
        ERROR ( rc, "VCursorRelease failed on '%s'", self -> full_spec );
    else
        self -> curs = NULL;
}

static
const void *GlobalPosLenReaderReadGlobal ( GlobalPosLenReader *self, const ctx_t *ctx,
    int64_t row_id, uint32_t *elem_bits, uint32_t *boff, uint32_t *row_len )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const void *base;

    assert ( elem_bits != NULL );
    assert ( boff != NULL );
    assert ( row_len != NULL );

    rc = VCursorCellDataDirect ( self -> curs, row_id, self -> idx [ 0 ],
        elem_bits, & base, boff, row_len );
    if ( rc != 0 )
        ERROR ( rc, "failed to read REF_LEN for row %ld", row_id );
    else
    {
        INSDC_coord_len ref_len;
        assert ( * boff == 0 );
        assert ( * row_len == 1 );
        assert ( * elem_bits == sizeof ref_len * 8 );
        ref_len = ( ( const INSDC_coord_len* ) base ) [ 0 ];

        rc = VCursorCellDataDirect ( self -> curs, row_id, self -> idx [ 1 ],
            elem_bits, & base, boff, row_len );
        if ( rc != 0 )
            ERROR ( rc, "failed to read GLOBAL_REF_START for row %ld", row_id );
        else
        {
            uint64_t pos;
            assert ( * boff == 0 );
            assert ( * row_len == 1 );
            assert ( * elem_bits == sizeof ( uint64_t ) * 8 );
            pos = ( ( const uint64_t* ) base ) [ 0 ];

            self -> poslen = encode_pos_len ( pos, ref_len );
#if 1
            assert ( decode_pos_len ( self -> poslen ) == pos );
            assert ( poslen_to_len ( self -> poslen ) == ref_len );
#endif
            return & self -> poslen;
        }
    }

    return NULL;

}

static
const void *GlobalPosLenReaderReadLocal ( GlobalPosLenReader *self, const ctx_t *ctx,
    int64_t row_id, uint32_t *elem_bits, uint32_t *boff, uint32_t *row_len )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const void *base;

    assert ( elem_bits != NULL );
    assert ( boff != NULL );
    assert ( row_len != NULL );

    rc = VCursorCellDataDirect ( self -> curs, row_id, self -> idx [ 0 ],
        elem_bits, & base, boff, row_len );
    if ( rc != 0 )
        ERROR ( rc, "failed to read REF_LEN for row %ld", row_id );
    else
    {
        INSDC_coord_len ref_len;
        assert ( * boff == 0 );
        assert ( * row_len == 1 );
        assert ( * elem_bits == sizeof ref_len * 8 );
        ref_len = ( ( const INSDC_coord_len* ) base ) [ 0 ];

        rc = VCursorCellDataDirect ( self -> curs, row_id, self -> idx [ 1 ],
            elem_bits, & base, boff, row_len );
        if ( rc != 0 )
            ERROR ( rc, "failed to read REF_START for row %ld", row_id );
        else
        {
            INSDC_coord_zero ref_start;
            assert ( * boff == 0 );
            assert ( * row_len == 1 );
            assert ( * elem_bits == sizeof ref_start * 8 );
            ref_start = ( ( const INSDC_coord_zero* ) base ) [ 0 ];

            rc = VCursorCellDataDirect ( self -> curs, row_id, self -> idx [ 2 ],
                elem_bits, & base, boff, row_len );
            if ( rc != 0 )
                ERROR ( rc, "failed to read REF_ID for row %ld", row_id );
            else
            {
                int64_t ref_id;
                assert ( * boff == 0 );
                assert ( * row_len == 1 );
                assert ( * elem_bits == sizeof ref_id * 8 );
                ref_id = ( ( const int64_t* ) base ) [ 0 ];

                if ( ref_id <= 0 )
                    self -> poslen = 0;
                else
                {
                    uint64_t pos = local_to_global ( ref_id, self -> chunk_size, ref_start );
                    self -> poslen = encode_pos_len ( pos, ref_len );
#if 1
                    assert ( global_to_row_id ( pos, self -> chunk_size ) == ref_id );
                    assert ( decode_pos_len ( self -> poslen ) == pos );
                    assert ( poslen_to_len ( self -> poslen ) == ref_len );
#endif
                }

                return & self -> poslen;
            }
        }
    }

    return NULL;
}

static  ColumnReader_vt GlobalPosLenReader_vt =
{
    GlobalPosLenReaderWhack,
    GlobalPosLenReaderFullSpec,
    GlobalPosLenReaderIdRange,
    GlobalPosLenReaderPreCopy,
    GlobalPosLenReaderPostCopy,
    GlobalPosLenReaderReadGlobal
};

static  ColumnReader_vt LocalPosLenReader_vt =
{
    GlobalPosLenReaderWhack,
    GlobalPosLenReaderFullSpec,
    GlobalPosLenReaderIdRange,
    GlobalPosLenReaderPreCopy,
    GlobalPosLenReaderPostCopy,
    GlobalPosLenReaderReadLocal
};



/* MakeGlobalPosLenReader
 */
ColumnReader *TablePairMakeGlobalPosLenReader ( TablePair *self, const ctx_t *ctx, uint32_t chunk_size )
{
    FUNC_ENTRY ( ctx );

    GlobalPosLenReader *reader;
    size_t full_spec_size = self -> full_spec_size + sizeof "src..GLOBAL_POSLEN" - 1;

    TRY ( reader = MemAlloc ( ctx, sizeof * reader + full_spec_size, false ) )
    {
        /* we don't yet know whether the reader will have GLOBAL_REF_START */
        TRY ( ColumnReaderInit ( & reader -> dad, ctx, & GlobalPosLenReader_vt ) )
        {
            rc_t rc = string_printf ( reader -> full_spec, full_spec_size + 1, NULL, "src.%s.GLOBAL_POSLEN", self -> full_spec );
            if ( rc != 0 )
                ABORT ( rc, "cannot seem to calculate string sizes" );
            else
            {
                rc = VTableCreateCursorRead ( self -> stbl, & reader -> curs );
                if ( rc != 0 )
                    ERROR ( rc, "failed to create cursor on 'src.%s'", self -> full_spec );
                else
                {
                    /* we need length as a 2nd sorting key */
                    rc = VCursorAddColumn ( reader -> curs, & reader -> idx [ 0 ], "REF_LEN" );
                    if ( rc != 0 )
                        ERROR ( rc, "VCursorAddColumn failed on '%s' cursor", self -> full_spec );
                    else
                    {
                        /* add columns AFTER open */
                        rc = VCursorPermitPostOpenAdd ( reader -> curs );
                        if ( rc != 0 )
                            ERROR ( rc, "VCursorPermitPostOpenAdd failed on '%s' cursor", self -> full_spec );
                        else
                        {
                            /* open single-column cursor - errors adding further columns
                               help determine what type of positioning we have */
                            rc = VCursorOpen ( reader -> curs );
                            if ( rc != 0 )
                                ERROR ( rc, "VCursorOpen failed on '%s' cursor", self -> full_spec );
                            else
                            {
                                /* finish initialization */
                                reader -> chunk_size = chunk_size;
                                reader -> full_spec_size = ( uint32_t ) full_spec_size;

                                /* now try to get GLOBAL_REF_START */
                                rc = VCursorAddColumn ( reader -> curs, & reader -> idx [ 1 ], "GLOBAL_REF_START" );
                                if ( rc == 0 )
                                {
                                    STATUS ( 3, "'%s' table has GLOBAL_REF_START", self -> full_spec );
                                    
                                    /* retrieval function returns this position directly */
                                    return & reader -> dad;
                                }

                                /* try for local refpos */
                                rc = VCursorAddColumn ( reader -> curs, & reader -> idx [ 1 ], "REF_START" );
                                if ( rc == 0 )
                                    rc = VCursorAddColumn ( reader -> curs, & reader -> idx [ 2 ], "REF_ID" );
                                if ( rc == 0 )
                                {
                                    STATUS ( 3, "'%s' table has local REF_START", self -> full_spec );

                                    /* retrieval function synthesizes local to global */
                                    reader -> dad . vt = & LocalPosLenReader_vt;
                                    return & reader -> dad;
                                }

                                ERROR ( rc, "failed to populate '%s' cursor", reader -> full_spec );
                            }
                        }
                    }

                    VCursorRelease ( reader -> curs );
                }
            }
        }

        MemFree ( ctx, reader, sizeof * reader + full_spec_size );
    }

    return NULL;
}
