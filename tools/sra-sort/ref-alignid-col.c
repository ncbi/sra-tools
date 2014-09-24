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

struct AlignIdColReader;
#define COLREADER_IMPL struct AlignIdColReader

#include "ref-alignid-col.h"
#include "glob-poslen.h"
#include "csra-tbl.h"
#include "csra-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "idx-mapping.h"
#include "map-file.h"
#include "sra-sort.h"

#include <vdb/cursor.h>
#include <kapp/main.h>
#include <klib/sort.h>
#include <klib/rc.h>

#include <string.h>
#include <assert.h>

FILE_ENTRY ( ref-alignid-col );


/*--------------------------------------------------------------------------
 * IdPosLen
 */
typedef struct IdPosLen IdPosLen;
struct IdPosLen
{
    int64_t id;
    uint64_t poslen;
};

#if USE_OLD_KSORT
static
int CC IdPosLenCmpPos ( const void *a, const void *b, void *data )
{
    const IdPosLen *ap = a;
    const IdPosLen *bp = b;

    if ( ap -> poslen < bp -> poslen )
        return -1;
    if ( ap -> poslen > bp -> poslen )
        return 1;
    if ( ap -> id < bp -> id )
        return -1;
    return ap -> id > bp -> id;
}

static
int CC cmp_int64_t ( const void *a, const void *b, void *data )
{
    const int64_t *ap = a;
    const int64_t *bp = b;

    if ( * ap < * bp )
        return -1;
    return * ap > * bp;
}
#else

static
void ksort_IdPosLen_pos ( IdPosLen *pbase, size_t total_elems )
{
#define SWAP( a, b, off, size )                               \
    do                                                        \
    {                                                         \
        IdPosLen tmp = * ( const IdPosLen* ) ( a );           \
        * ( IdPosLen* ) ( a ) = * ( const IdPosLen* ) ( b );  \
        * ( IdPosLen* ) ( b ) = tmp;                          \
    } while ( 0 )


#define CMP( a, b )                                                                                   \
     ( ( ( ( const IdPosLen* ) ( a ) ) -> poslen == ( ( const IdPosLen* ) ( b ) ) -> poslen ) ?       \
       ( ( ( const IdPosLen* ) ( a ) ) -> id < ( ( const IdPosLen* ) ( b ) ) -> id ) ? -1 :           \
       ( ( ( const IdPosLen* ) ( a ) ) -> id > ( ( const IdPosLen* ) ( b ) ) -> id )                  \
       : ( ( ( const IdPosLen* ) ( a ) ) -> poslen < ( ( const IdPosLen* ) ( b ) ) -> poslen ) ? -1 : \
       ( ( ( const IdPosLen* ) ( a ) ) -> poslen > ( ( const IdPosLen* ) ( b ) ) -> poslen ) )

    KSORT ( pbase, total_elems, sizeof * pbase, 0, sizeof * pbase );

#undef SWAP
#undef CMP

}
#endif


/*--------------------------------------------------------------------------
 * AlignIdColReader
 */
typedef struct AlignIdColReader AlignIdColReader;
struct AlignIdColReader
{
    ColumnReader dad;

    /* state for id range:
       first = slot [ 0 ] of map
       row_id = needs to match request - we only accept serial scans
       next_id = next id to fetch from "ids"
       last_excl = end of "ids" */
    int64_t first, row_id, next_id, last_excl;

    /* sequence for handing out new ids */
    int64_t new_id;

    /* reader onto our join index column */
    ColumnReader *ids;

    /* reader onto alignment table
       to retrieve GLOBAL_REF_START + REF_LEN */
    ColumnReader *poslen;

    /* bi-directional index for writing
       new=>old and old=>new mappings */
    MapFile *idx;

    /* buffer for sorted data */
    union
    {
        int64_t *ids;
        IdPosLen *id_poslen;
        IdxMapping *id_map;
    } u;
    size_t max_elems;
    size_t num_elems;
    size_t cur_elem;

    /* buffer for id row */
    int64_t *row;
    size_t max_row_len;

    /* chunk-size */
    size_t chunk_size;

    /* whether we have all ids or not */
    bool entire_table;
};

static
void AlignIdColReaderDestroy ( AlignIdColReader *self, const ctx_t *ctx )
{
    if ( self -> ids != NULL )
    {
        FUNC_ENTRY ( ctx );

        STATUS ( 4, "destroying align-column reader '%s'", ColumnReaderFullSpec ( self -> ids, ctx ) );

        /* tear everything down */
        MapFileRelease ( self -> idx, ctx );
        self -> idx = NULL;

        ColumnReaderRelease ( self -> poslen, ctx );
        self -> poslen = NULL;

        ColumnReaderRelease ( self -> ids, ctx );
        self -> ids = NULL;

        if ( self -> u . id_poslen != NULL )
        {
            MemFree ( ctx, self -> u . id_poslen, sizeof * self -> u . id_poslen * self -> max_elems );
            self -> u . id_poslen = NULL;
        }
        if ( self -> row != NULL )
        {
            MemFree ( ctx, self -> row, sizeof self -> row [ 0 ] * self -> max_row_len );
            self -> row = NULL;
        }
    }
}

static
void AlignIdColReaderWhack ( AlignIdColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* tear down self */
    AlignIdColReaderDestroy ( self, ctx );

    /* tear down super-class */
    ColumnReaderDestroy ( & self -> dad, ctx );

    /* gone */
    MemFree ( ctx, self, sizeof * self );
}

static
const char *AlignIdColReaderFullSpec ( const AlignIdColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    return ColumnReaderFullSpec ( self -> ids, ctx );
}

static
uint64_t AlignIdColReaderIdRange ( const AlignIdColReader *self, const ctx_t *ctx, int64_t *opt_first )
{
    FUNC_ENTRY ( ctx );
    return ColumnReaderIdRange ( self -> ids, ctx, opt_first );
}

static
void AlignIdColReaderFlushToIdx ( AlignIdColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> idx != NULL )
    {
        const Tool *tp = ctx -> caps -> tool;

        /* write new=>old ids  */
        STATUS ( 3, "%s new=>old id map", tp -> write_new_to_old ? "writing" : "setting" );
        ON_FAIL ( MapFileSetNewToOld ( self -> idx, ctx, self -> u . id_map, self -> num_elems ) )
        {
            ANNOTATE ( "failed to record new to old id mappings for '%s'", ColumnReaderFullSpec ( self -> ids, ctx ) );
            return;
        }
        if ( tp -> sort_before_old2new )
        {
            /* sort by old ids */
            STATUS ( 3, "sorting id-map by old-id" );
#if USE_OLD_KSORT
            ksort ( self -> u . id_map, self -> num_elems, sizeof self -> u . id_map [ 0 ], IdxMappingCmpOld, ( void* ) ctx );
#else
            IdxMappingSortOld ( self -> u . id_map, ctx, self -> num_elems );
#endif
        }

        /* write old=>new ids  */
        STATUS ( 3, "writing old=>new id map" );
        ON_FAIL ( MapFileSetOldToNew ( self -> idx, ctx, self -> u . id_map, self -> num_elems ) )
        {
            ANNOTATE ( "failed to record old to new id mappings for '%s'", ColumnReaderFullSpec ( self -> ids, ctx ) );
            return;
        }

        STATUS ( 3, "done writing id maps" );
    }

    /* empty buffer */
    self -> first = self -> next_id;
}

static
void AlignIdColReaderPreCopy ( AlignIdColReader *self, const ctx_t *ctx )
{
    /* TBD - a good place to allocate memory */
}

static
void AlignIdColReaderPostCopy ( AlignIdColReader *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* called after a single copy
       assumes that entire row-range of table has been copied */

    /* tear down self */
    AlignIdColReaderDestroy ( self, ctx );
}

static
const void *AlignIdColReaderRead ( AlignIdColReader *self, const ctx_t *ctx,
    int64_t row_id, uint32_t *elem_bits, uint32_t *boff, uint32_t *row_len )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    const void *base;
    uint32_t num_read32;
    size_t i, num_read;

    /* handle bad parameters */
    uint32_t dummy [ 3 ];
    if ( elem_bits == NULL )
        elem_bits = & dummy [ 0 ];
    if ( boff == NULL )
        boff = & dummy [ 1 ];
    if ( row_len == NULL )
        row_len = & dummy [ 2 ];

    /* THERE ARE three stages to this method:
       1a. detect need to allocate id map
       1b. detect need to flush to index
       2.  detect need to refresh id map
       3.  respond to read request
     */

    if ( row_id != self -> row_id )
    {
        /* error condition */
        rc = RC ( rcExe, rcCursor, rcReading, rcParam, rcIncorrect );
        INTERNAL_ERROR ( rc, "AlignIdColReader only supports serial access" );
        return NULL;
    }

    /* stage 1 - allocate or flush */
    if ( self -> u . ids == NULL && ! self -> entire_table )
    {
        /* limit ids to actual count */
        uint64_t num_ids = MapFileCount ( self -> idx, ctx );
        self -> max_elems = ctx -> caps -> tool -> max_ref_idx_ids;
        if ( ( uint64_t ) self -> max_elems > num_ids )
            self -> max_elems = ( size_t ) num_ids;

        /* allocate the buffer */
        do
        {
            CLEAR ();

            TRY ( self -> u . id_poslen = MemAlloc ( ctx, sizeof self -> u . id_poslen [ 0 ] * self -> max_elems, false ) )
            {
                STATUS ( 4, "allocated map buffer with %,zu elements", self -> max_elems );
                break;
            }

            self -> max_elems >>= 1;
        }
        while ( self -> max_elems >= 1024 * 1024 );

        if ( FAILED () )
        {
            ANNOTATE ( "failed to allocate id buffer" );
            return NULL;
        }

        /* notice if we have the entire table */
        if ( ( uint64_t ) self -> max_elems == num_ids )
            self -> entire_table = true;

        /* allocate a row buffer */
        self -> max_row_len = 8 * 1024;
        ON_FAIL ( self -> row = MemAlloc ( ctx, sizeof self -> row [ 8 ] * self -> max_row_len, false ) )
        {
            ANNOTATE ( "failed to allocate row buffer" );
            return NULL;
        }

        assert ( self -> first == self -> next_id );
        assert ( self -> first == row_id );
    }
    else if ( row_id == self -> next_id && self -> first < row_id )
    {
        ON_FAIL ( AlignIdColReaderFlushToIdx ( self, ctx ) )
            return NULL;
    }

    /* stage 2 - refresh */
    if ( self -> first == self -> next_id )
    {
        /* if there are no more rows, return NULL  */
        if ( self -> next_id == self -> last_excl )
        {
            STATUS ( 3, "done reading '%s'", ColumnReaderFullSpec ( self -> ids, ctx ) );
            return NULL;
        }

        self -> cur_elem = 0;

        /* if the entire table will be read into memory,
           there is no point in reading/sorting the ids */
        if ( self -> entire_table )
        {
            STATUS ( 3, "auto-generating %,zu ids for '%s'", self -> max_elems, ColumnReaderFullSpec ( self -> ids, ctx ) );
            for ( i = 0; i < self -> max_elems; ++ i )
            {
                self -> u . id_poslen [ i ] . id = self -> first + i;
#if _DEBUGGING
                self -> u . id_poslen [ i ] . poslen = 0;
#endif
            }

            self -> num_elems = self -> max_elems;
            self -> next_id = self -> last_excl;
        }
        else
        {
            /* read up to max_elems into id array */
            STATUS ( 3, "reading ids from '%s'", ColumnReaderFullSpec ( self -> ids, ctx ) );
            for ( self -> num_elems = 0; self -> next_id < self -> last_excl; self -> num_elems += num_read, ++ self -> next_id )
            {
                /* read a row of ids */
                ON_FAIL ( base = ColumnReaderRead ( self -> ids, ctx, self -> next_id, elem_bits, boff, & num_read32 ) )
                {
                    ANNOTATE ( "failed to read id column" );
                    return NULL;
                }

                /* we expect empty rows */
                num_read = num_read32;
                if ( num_read == 0 )
                    continue;

                assert ( * elem_bits == sizeof self -> u . ids [ 0 ] * 8 );
                assert ( * boff == 0 );

                /* detect when buffer is full */
                if ( self -> num_elems + num_read > self -> max_elems )
                {
                    /* if we've read at least one row, then we're okay */
                    if ( self -> first < self -> next_id )
                        break;

                    /* error condition - buffer was too small to read a single row */
                    rc = RC ( rcExe, rcCursor, rcReading, rcBuffer, rcInsufficient );
                    ERROR ( rc, "allocated buffer was too small ( %zu elems ) to read a single row ( id %ld, row-len %zu )",
                            self -> max_elems, self -> next_id, num_read );
                    return NULL;
                }

                /* bring them in */
                memcpy ( & self -> u . ids [ self -> num_elems ], base, num_read * sizeof self -> u . ids [ 0 ] );
            }

            /* sort to produce sparse but ordered list */
            STATUS ( 3, "sorting %,zu 64-bit ids", self -> num_elems );
#if USE_OLD_KSORT
            ksort ( self -> u . ids, self -> num_elems, sizeof self -> u . ids [ 0 ], cmp_int64_t, ( void* ) ctx );
#else
            ksort_int64_t ( self -> u . ids, self -> num_elems );
#endif

            /* transform from ids to id_poslen */
            STATUS ( 3, "expanding ids to ( id, position, len ) tuples" );
            for ( i = self -> num_elems; i > 1 ; )
            {
                -- i;
                self -> u . id_poslen [ i ] . id = self -> u . ids [ i ];
            }
        }

        /* read num_elems from poslen */
        STATUS ( 3, "reading ( position, len ) pairs from '%s'", ColumnReaderFullSpec ( self -> poslen, ctx ) );
        for ( i = 0; i < self -> num_elems; ++ i )
        {
            ON_FAIL ( base = ColumnReaderRead ( self -> poslen, ctx, self -> u . id_poslen [ i ] . id, elem_bits, boff, & num_read32 ) )
            {
                ANNOTATE ( "failed to read global ref-start from alignment table" );
                return NULL;
            }

            num_read = num_read32;
            assert ( * elem_bits == sizeof self -> u . id_poslen [ 0 ] . poslen * 8 );
            assert ( * boff == 0 );
            assert ( num_read == 1 );

            self -> u . id_poslen [ i ] . poslen = * ( uint64_t* ) base;
        }

        /* sort by poslen */
        STATUS ( 3, "sorting ( id, position, len ) tuples on position, len DESC" );
#if USE_OLD_KSORT
        ksort ( self -> u . id_poslen, self -> num_elems, sizeof self -> u . id_poslen [ 0 ], IdPosLenCmpPos, ( void* ) ctx );
#else
        ksort_IdPosLen_pos ( self -> u . id_poslen, self -> num_elems );
#endif

        /* write poslen to temp column */
        STATUS ( 3, "writing ( position, len ) to temp column" );
        ON_FAIL ( MapFileSetPoslen ( self -> idx, ctx, self -> u . id_map, self -> num_elems ) )
        {
            ANNOTATE ( "failed to write temporary poslen column for '%s'", ColumnReaderFullSpec ( self -> ids, ctx ) );
            return NULL;
        }

        STATUS ( 3, "done writing ( position, len )" );
    }

    /* stage 3 - build a row */
    for ( num_read = ( size_t ) ( self -> num_elems - self -> cur_elem ), i = 0; i < num_read; ++ i )
    {
        /* whatever the current element is, it must belong to this row */
        uint64_t pos = decode_pos_len ( self -> u . id_poslen [ self -> cur_elem + i ] . poslen );
        int64_t cur_row_id = global_to_row_id ( pos, self -> chunk_size );
        if ( cur_row_id != row_id )
        {
            num_read = i;
            break;
        }
    }

    /* "num_read" now has the number of elements in this row */
    if ( num_read > self -> max_row_len )
    {
        STATUS ( 4, "reallocating row buffer for row-length %,zu", num_read );

        /* realloc the row buffer */
        MemFree ( ctx, self -> row, self -> max_row_len * sizeof self -> row [ 0 ] );

        self -> max_row_len = ( num_read + 4095 ) & ~ ( size_t ) 4095;
        ON_FAIL ( self -> row = MemAlloc ( ctx, sizeof self -> row [ 0 ] * self -> max_row_len, false ) )
        {
            ANNOTATE ( "failed to reallocate row buffer" );
            return NULL;
        }
    }

    /* build the row, assigning new ids */
    for ( i = 0; i < num_read; ++ i )
    {
        self -> row [ i ] = ++ self -> new_id;
        self -> u . id_map [ self -> cur_elem + i ] . new_id = self -> new_id;
    }

    /* advance row_id */
    self -> cur_elem += num_read;
    if ( ++ self -> row_id == self -> last_excl )
    {
        /* if this was the last row, flush index  */
        ON_FAIL ( AlignIdColReaderFlushToIdx ( self, ctx ) )
            return NULL;

        if ( ctx -> caps -> tool -> idx_consistency_check && self -> idx != NULL )
        {
            ON_FAIL ( MapFileConsistencyCheck ( self -> idx, ctx ) )
                return NULL;
        }
    }

    /* return row */
    * elem_bits = sizeof self -> row [ 0 ] * 8;
    * boff = 0;
    * row_len = num_read;
    return self -> row;
}

static ColumnReader_vt AlignIdColReader_vt =
{
    AlignIdColReaderWhack,
    AlignIdColReaderFullSpec,
    AlignIdColReaderIdRange,
    AlignIdColReaderPreCopy,
    AlignIdColReaderPostCopy,
    AlignIdColReaderRead
};


/*--------------------------------------------------------------------------
 * RefTblPair
 */
static
size_t RefTblPairGetChunkSize ( TablePair *self, const ctx_t *ctx, const VCursor **cursp )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;

    rc = VTableCreateCursorRead ( self -> stbl, cursp );
    if ( rc != 0 )
        ERROR ( rc, "failed to create cursor on '%s'", self -> full_spec );
    else
    {
        uint32_t idx;
        const VCursor *curs = * cursp;
        rc = VCursorAddColumn ( curs, & idx, "MAX_SEQ_LEN" );
        if ( rc != 0 )
            rc = VCursorAddColumn ( curs, & idx, "CHUNK_SIZE" );
        if ( rc != 0 )
            ERROR ( rc, "VCursorAddColumn - failed to determine chunk size of '%s'", self -> full_spec );
        else
        {
            rc = VCursorOpen ( curs );
            if ( rc != 0 )
                ERROR ( rc, "VCursorOpen - failed to determine chunk size of '%s'", self -> full_spec );
            else
            {
                uint32_t row_len, chunk_size;
                rc = VCursorReadDirect ( curs, 1, idx,
                    sizeof chunk_size * 8, & chunk_size, 1, & row_len );
                if ( rc != 0 )
                    ERROR ( rc, "VCursorReadDirect - failed to determine chunk size of '%s'", self -> full_spec );
                else if ( row_len != 1 )
                {
                    rc = RC ( rcExe, rcCursor, rcReading, rcData, rcInsufficient );
                    ERROR ( rc, "VCursorReadDirect - failed to determine chunk size of '%s'", self -> full_spec );
                }
                else
                {
                    STATUS ( 3, "'%s' chunk size determined to be %u", self -> full_spec, chunk_size );
                    return chunk_size;
                }
            }
        }

        VCursorRelease ( curs );
        * cursp = NULL;
    }

    return 0;
}


/*--------------------------------------------------------------------------
 * ColumnReader
 *  interface to read column in random order
 */


/* MakeAlignIdReader
 */
ColumnReader *TablePairMakeAlignIdReader ( TablePair *self,
    const ctx_t *ctx, TablePair *align_tbl, MapFile *idx, const char *colspec )
{
    FUNC_ENTRY ( ctx );

    size_t chunk_size;
    const VCursor *curs;

    /* determine chunk size and keep cursor for id reader */
    TRY ( chunk_size = RefTblPairGetChunkSize ( self, ctx, & curs ) )
    {
        ColumnReader *ids;

        /* need to create a traditional reader on self */
        TRY ( ids = TablePairMakeColumnReader ( self, ctx, curs, colspec, true ) )
        {
            ColumnReader *poslen;

            /* need to create a poslen reader on align_tbl */
            TRY ( poslen = TablePairMakeGlobalPosLenReader ( align_tbl, ctx, ( uint32_t ) chunk_size ) )
            {
                int64_t first;
                uint64_t count;

                /* need to create a mapfile based on range of poslen */
                TRY ( count = ColumnReaderIdRange ( poslen, ctx, & first ) )
                {
                    TRY ( MapFileSetIdRange ( idx, ctx, first, count ) )
                    {
                        AlignIdColReader *col;
                        TRY ( col = MemAlloc ( ctx, sizeof * col, true ) )
                        {
                            TRY ( ColumnReaderInit ( & col -> dad, ctx, & AlignIdColReader_vt ) )
                            {
                                /* one more point of failure when obtaining the id range */
                                TRY ( count = ColumnReaderIdRange ( ids, ctx, & first ) )
                                {
                                    TRY ( col -> idx = MapFileDuplicate ( idx, ctx ) )
                                    {
                                        col -> first = col -> row_id = col -> next_id = first;
                                        col -> last_excl = first + count;
                                        col -> ids = ids;
                                        col -> poslen = poslen;
                                        col -> chunk_size = chunk_size;
                                        col -> entire_table = false;
                                    
                                        /* the actual working buffer is allocated upon demand
                                           to avoid occupying space while waiting to execute */

                                        /* douse the cursor here */
                                        VCursorRelease ( curs );

                                        return & col -> dad;
                                    }
                                }
                            }
                            
                            MemFree ( ctx, col, sizeof * col );
                        }
                        
                        MapFileRelease ( idx, ctx );
                    }
                }
                
                ColumnReaderRelease ( poslen, ctx );
            }
            
            ColumnReaderRelease ( ids, ctx );
        }

        VCursorRelease ( curs );
    }

    return NULL;
}
