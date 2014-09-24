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

typedef struct BufferedPairColWriter BufferedPairColWriter;
#define COLWRITER_IMPL BufferedPairColWriter

#include "buff-writer.h"
#include "row-set.h"
#include "idx-mapping.h"
#include "map-file.h"
#include "csra-tbl.h"
#include "col-pair.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "sra-sort.h"

#include <klib/sort.h>
#include <klib/rc.h>
#include <kdb/btree.h>

#include <string.h>
#include <bitstr.h>

FILE_ENTRY ( buff-writer );


/*--------------------------------------------------------------------------
 * BufferedPairColWriter
 */
struct BufferedPairColWriter
{
    ColumnWriter dad;

    cSRATblPair *tbl;   /* borrowed reference to table        */

    ColumnWriter *cw;   /* where to write data                */

    MemBank *mbank;     /* paged memory bank                  */
#define MAX_VOCAB_SIZE 32*1024
    /*** introducing VOCABULARY value->id->vocab_value ***/
    KBTree	*vocab_key2id;
    const void	*vocab_id2val[MAX_VOCAB_SIZE];
    uint32_t	vocab_cnt;
    /******************************************************/
    MapFile *idx;       /* optional row-id mapping index      */

    union
    {
        /* for mapping writers */
        struct
        {
            union
            {
                /* "immediate" data - size <= 64 bits and row_len == 1 */
                uint64_t imm;

                /* "indirect" data - stored in "mbank" */
                void *ptr;

            } val;
            int64_t new_id;
        } *data;

        /* standard ( old_id, new_id ) mapping pair */
        IdxMapping *map;

        /* for non-mapping writers */
        int64_t *ids;

    } u;

    /* for non-mapping writers - new=>old ord */
    uint32_t *ord;

    size_t num_items;   /* total number of items              */
    size_t cur_item;    /* index of currently available item  */
    size_t num_immed;   /* number of immediate items written  */

    uint32_t elem_bits; /* number of bits of each element     */
    uint32_t min_row_len;
    uint32_t max_row_len;
};

static
void BufferedPairColWriterWhack ( BufferedPairColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    MapFileRelease ( self -> idx, ctx );
    MemBankRelease ( self -> mbank, ctx );
    if(self -> vocab_key2id) KBTreeRelease  ( self -> vocab_key2id );
    ColumnWriterRelease ( self -> cw, ctx );
    MemFree ( ctx, self, sizeof * self );
}

static
const char *BufferedPairColWriterFullSpec ( const BufferedPairColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    return ColumnWriterFullSpec ( self -> cw, ctx );
}

static
void BufferedPairColWriterPreCopy ( BufferedPairColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterPreCopy ( self -> cw, ctx );
}

static
void BufferedPairColWriterPostCopy ( BufferedPairColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    if ( self -> mbank != NULL )
    {
        MemBankRelease ( self -> mbank, ctx );
        self -> mbank = NULL;
    }
    if ( self -> vocab_key2id != NULL )
    {
	KBTreeRelease  ( self -> vocab_key2id );
	self -> vocab_key2id = 0;
	self -> vocab_cnt = 0;
    }

    ColumnWriterPostCopy ( self -> cw, ctx );
}

static
void BufferedPairColWriterMapValues ( BufferedPairColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    rc_t rc;
    size_t i;
    int64_t id;
    bool assign_ids = ( bool ) self -> dad . align [ 0 ];

    if ( self -> elem_bits != 64 )
    {
        rc = RC ( rcExe, rcColumn, rcWriting, rcType, rcIncorrect );
        INTERNAL_ERROR ( rc, "column '%s' does not have 64-bit elements and cannot be mapped",
            ColumnWriterFullSpec ( self -> cw, ctx ) );
        return;
    }

    if ( self -> num_immed != self -> num_items )
    {
#if DISALLOW_VARIABLE_ROW_LENGTH_MAPPING
        rc = RC ( rcExe, rcColumn, rcWriting, rcType, rcIncorrect );
        INTERNAL_ERROR ( rc, "column '%s' does not have fixed row-length of 1 and cannot be mapped",
            ColumnWriterFullSpec ( self -> cw, ctx ) );
#else
        const uint32_t *base;
        /* detect case when we have ids that were written
           sparsely ( as NULLs ) rather than densely ( zeros for NULL ). */
        if ( self -> min_row_len == 0 && self -> max_row_len == 1 )
        {
            for ( i = 0; i < self -> num_items; ++ i )
            {
                base = self -> u . data [ i ] . val . ptr;
                if ( base != NULL )
                    memcpy ( & self -> u . data [ i ] . val . imm, & base [ 1 ], 8 );
            }

            MemBankRelease ( self -> mbank, ctx );
            self -> mbank = NULL;
            self -> num_immed = self -> num_items;
            self -> min_row_len = 1;
	    
        }
        else
        {
            STATUS ( 3, "randomly mapping old=>new values" );
            for ( i = 0; i < self -> num_immed; ++ i )
            {
                id = self -> u . data [ i ] . val . imm;
                if ( id != 0 )
                {
                    uint32_t *base;
                    ON_FAIL ( base = MemBankAlloc ( self -> mbank, ctx, sizeof id + sizeof * base, false ) )
                        return;
                    ON_FAIL ( id = MapFileMapSingleOldToNew ( self -> idx, ctx, id, assign_ids ) )
                        return;
                    memcpy ( & base [ 1 ], & id, sizeof id );
                    base [ 0 ] = 1;
                    self -> u . data [ i ] . val . ptr = base;
                }
            }
            for ( self -> num_immed = 0; i < self -> num_items; ++ i )
            {
                base = self -> u . data [ i ] . val . ptr;
                if ( base != NULL )
                {
                    uint32_t j, row_len = base [ 0 ];
                    int64_t *ids = ( int64_t* ) & base [ 1 ];
                    for ( j = 0; j < row_len; ++ j )
                    {
                        id = ids [ j ];
                        if ( id != 0 )
                        {
                            ON_FAIL ( ids [ j ] = MapFileMapSingleOldToNew ( self -> idx, ctx, id, assign_ids ) )
                                return;
                        }
                    }
                }
            }
            return;
        }
#endif
    }

    STATUS ( 3, "sorting %,u ( data, new_id ) map entries on immediate value", self -> num_items );
#if USE_OLD_KSORT
    ksort ( self -> u . map, self -> num_items, sizeof self -> u . map [ 0 ], IdxMappingCmpOld, ( void* ) ctx );
#else
    IdxMappingSortOld ( self -> u . map, ctx, self -> num_items );
#endif

    STATUS ( 3, "mapping old=>new values" );
    for ( i = 0; i < self -> num_items; ++ i )
    {
        id = self -> u . map [ i ] . old_id;
        if ( id != 0 )
        {
            ON_FAIL ( self -> u . map [ i ] . old_id = MapFileMapSingleOldToNew ( self -> idx, ctx, id, assign_ids ) )
                break;
        }
    }
}

static
void BufferedPairColWriterWriteMapped ( BufferedPairColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len )
{
    FUNC_ENTRY ( ctx );

    uint32_t *base;
    size_t i, row_bytes;
    const size_t pg_size = 256 * 1024;

    /* detect first write */
    if ( self -> u . data == NULL )
    {
        /* get map and content length from table */
        assert ( self -> idx != NULL );
        ON_FAIL ( self -> u . map = RowSetIteratorGetIdxMapping ( self -> tbl -> rsi, ctx, & self -> num_items ) )
        {
            ANNOTATE ( "failed to get ( old_id, new_id ) map for column '%s'", ColumnWriterFullSpec ( self -> cw, ctx ) );
            return;
        }

        /* require elem_bits to be constant for column */
        self -> elem_bits = elem_bits;

        /* OPTIMIZATION:
           IFF elem_bits <= 64 and row_len == 1, store in union
           otherwise, use membank. This requires that if row_len
           ever changes, we have to recover. */
        if ( elem_bits > 64 || row_len != 1 )
        {
            /* create a memory bank */
            /* TBD - used configured page size rather than 256K */
            ON_FAIL ( self -> mbank = MemBankMakePaged ( ctx, -1L, pg_size ) )
                return;
        }

        /* initialize min/max */
        self -> min_row_len = self -> max_row_len = row_len;
    }

    /* elem_bits should be constant */
    assert ( self -> elem_bits == elem_bits );

    /* handle optimized case */
    if ( self -> mbank == NULL )
    {
        /* first, detect a change */
        if ( row_len == 1 )
        {
            /* continue on optimized case */
            row_bytes = ( ( size_t ) elem_bits + 7 ) >> 3;
            if ( boff != 0 )
            {
                bitcpy ( & self -> u . data [ self -> cur_item ] . val . imm,
                    0, data, boff, ( bitsz_t ) elem_bits * row_len );
            }
            else
            {
                memcpy ( & self -> u . data [ self -> cur_item ] . val . imm, data, row_bytes );
            }

            /* record another immediate row */
            ++ self -> num_immed;

            /* detect last row */
            if ( ++ self -> cur_item < self -> num_items )
                return;

            /* map values */
            TRY ( BufferedPairColWriterMapValues ( self, ctx ) )
            {
                /* need to resort map on new_id */
                STATUS ( 3, "sorting %,u ( data, new_id ) map entries on new_id", self -> num_items );
#if USE_OLD_KSORT
                ksort ( self -> u . map, self -> num_items, sizeof self -> u . map [ 0 ], IdxMappingCmpNew, ( void* ) ctx );
#else
                IdxMappingSortNew ( self -> u . map, ctx, self -> num_items );
#endif

                /* write all rows to column writer */
                STATUS ( 3, "writing cell data to '%s'", ColumnWriterFullSpec ( self -> cw, ctx ) );
                for ( i = 0; i < self -> num_items; ++ i )
                {
                    /* write out row */
                    ON_FAIL ( ColumnWriterWrite ( self -> cw, ctx, self -> elem_bits, & self -> u . data [ i ] . val . imm, 0, 1 ) )
                        break;
                }
            }

            /* forget about map */
            self -> u . map = NULL;
            self -> cur_item = self -> num_items = self -> num_immed = 0;
            self -> elem_bits = 0;

            return;
        }

        /* need to recover */
        ON_FAIL ( self -> mbank = MemBankMakePaged ( ctx, -1L, pg_size ) )
            return;
    }

    /* update min/max row-len */
    if ( row_len < self -> min_row_len )
        self -> min_row_len = row_len;
    else if ( row_len > self -> max_row_len )
        self -> max_row_len = row_len;

    /* handle zero-length rows specially */
    if ( row_len == 0 )
        self -> u . data [ self -> cur_item ] . val . ptr = NULL;
    else
    {
        /* allocate space for the row */
        row_bytes = ( ( size_t ) elem_bits * row_len + 7 ) >> 3;
        TRY ( base = MemBankAlloc ( self -> mbank, ctx, row_bytes + sizeof row_len, false ) )
        {
            /* copy it in */
            memcpy ( base, & row_len, sizeof row_len );
            if ( boff != 0 )
                bitcpy ( & base [ 1 ], 0, data, boff, ( bitsz_t ) elem_bits * row_len );
            else
                memcpy ( & base [ 1 ], data, row_bytes );

            /* remember its location in the map */
            self -> u . data [ self -> cur_item ] . val . ptr = base;

            /* remember the last input pointer */
        }
    }

    if ( ! FAILED () )
    {
        /* detect last row */
        if ( ++ self -> cur_item == self -> num_items )
        {
            /* map values */
            TRY ( BufferedPairColWriterMapValues ( self, ctx ) )
            {
                /* need to resort map on new_id */
                STATUS ( 3, "sorting %,u ( data, new_id ) map entries on new_id", self -> num_items );
#if USE_OLD_KSORT
                ksort ( self -> u . map, self -> num_items, sizeof self -> u . map [ 0 ], IdxMappingCmpNew, ( void* ) ctx );
#else
                IdxMappingSortNew ( self -> u . map, ctx, self -> num_items );
#endif

                /* write all rows to column writer */
                STATUS ( 3, "writing cell data to '%s'", ColumnWriterFullSpec ( self -> cw, ctx ) );
                for ( i = 0; i < self -> num_immed; ++ i )
                {
                    /* write out row */
                    ON_FAIL ( ColumnWriterWrite ( self -> cw, ctx, self -> elem_bits, & self -> u . data [ i ] . val . imm, 0, 1 ) )
                        break;
                }
                for ( ; ! FAILED () && i < self -> num_items; ++ i )
                {
                    /* write out row */
                    base = self -> u . data [ i ] . val . ptr;
                    if ( base == NULL )
                        ColumnWriterWrite ( self -> cw, ctx, self -> elem_bits, "", 0, 0 );
                    else
                        ColumnWriterWrite ( self -> cw, ctx, self -> elem_bits, & base [ 1 ], 0, base [ 0 ] );
                }
            }

            /* drop the mem-bank */
            MemBankRelease ( self -> mbank, ctx );
            self -> mbank = NULL;

            /* forget about map */
            self -> u . data = NULL;
            self -> cur_item = self -> num_items = self -> num_immed = 0;
            self -> elem_bits = 0;
        }
    }
}

static
void BufferedPairColWriterWriteUnmapped ( BufferedPairColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len )
{
    FUNC_ENTRY ( ctx );

    size_t i, j, row_bytes;
    const size_t pg_size = 256 * 1024;

    /* detect first write */
    if ( self -> u . data == NULL )
    {
        /* get ids, ord and content length from table */
        assert ( self -> idx == NULL );
        ON_FAIL ( self -> u . ids = RowSetIteratorGetSourceIds ( self -> tbl -> rsi, ctx, & self -> ord, & self -> num_items ) )
        {
            ANNOTATE ( "failed to get old_id, ord maps for column '%s'", ColumnWriterFullSpec ( self -> cw, ctx ) );
            return;
        }

        /* require elem_bits to be constant for column */
        self -> elem_bits = elem_bits;

        /* OPTIMIZATION:
           IFF elem_bits <= 64 and row_len == 1, store in union
           otherwise, use membank. This requires that if row_len
           ever changes, we have to recover. */
        if ( elem_bits > 64 || row_len != 1 )
        {
	    rc_t rc=KBTreeMakeUpdate(&self->vocab_key2id, NULL, 100*1024*1024,
                              false, kbtOpaqueKey,
                              1, 1024, sizeof ( uint32_t ),
                              NULL
                              );
	    if(rc != 0) self->vocab_key2id = NULL;
	    self->vocab_cnt=0;
            /* create a memory bank */
            /* TBD - used configured page size rather than 256K */
            ON_FAIL ( self -> mbank = MemBankMakePaged ( ctx, -1L, pg_size ) )
               return;
        }
        /* initialize min/max */
        self -> min_row_len = self -> max_row_len = row_len;
    }

    /* elem_bits should be constant */
    assert ( self -> elem_bits == elem_bits );

    /* handle optimized case */
    if ( self -> mbank == NULL )
    {
        /* first, detect a change */
        if ( row_len == 1 )
        {
            /* continue on optimized case */
            row_bytes = ( ( size_t ) elem_bits + 7 ) >> 3;
            if ( boff != 0 )
            {
                bitcpy ( & self -> u . ids [ self -> cur_item ],
                    0, data, boff, ( bitsz_t ) elem_bits * row_len );
            }
            else
            {
                memcpy ( & self -> u . ids [ self -> cur_item ], data, row_bytes );
            }

            /* record another immediate row */
            ++ self -> num_immed;

            /* detect last row */
            if ( ++ self -> cur_item < self -> num_items )
                return;

            /* write all rows to column writer */
            STATUS ( 3, "writing cell data to '%s'", ColumnWriterFullSpec ( self -> cw, ctx ) );
            for ( i = 0; i < self -> num_items; ++ i )
            {
                /* map to new order */
                j = self -> ord [ i ];

                /* write out row */
                ON_FAIL ( ColumnWriterWrite ( self -> cw, ctx, self -> elem_bits, & self -> u . ids [ j ], 0, 1 ) )
                    break;
            }

            /* forget about map */
            self -> u . ids = NULL;
            self -> ord = NULL;
            self -> cur_item = self -> num_items = self -> num_immed = 0;
            self -> elem_bits = 0;

            return;
        }

        /* need to recover */
	{
	    rc_t rc=KBTreeMakeUpdate(&self->vocab_key2id, NULL, 100*1024*1024,
                              false, kbtOpaqueKey,
                              1, 1024, sizeof ( uint32_t ),
                              NULL
                              );
	    if(rc != 0) self->vocab_key2id = NULL;
	    self->vocab_cnt=0;
            /* create a memory bank */
            /* create a memory bank */
            /* TBD - used configured page size rather than 256K */
            ON_FAIL ( self -> mbank = MemBankMakePaged ( ctx, -1L, pg_size ) )
               return;
	}
    }

    /* update min/max row-len */
    if ( row_len < self -> min_row_len )
        self -> min_row_len = row_len;
    else if ( row_len > self -> max_row_len )
        self -> max_row_len = row_len;

    /* handle zero-length rows specially */
    if ( row_len == 0 ){
        self -> u . ids [ self -> cur_item ] = 0;
    } else {
    	uint32_t *base = NULL; 
        row_bytes = ( ( size_t ) elem_bits * row_len + 7 ) >> 3;
	if(self -> vocab_key2id){
		rc_t rc;
		bool wasInserted;
		uint64_t tmp_id=self -> vocab_cnt;
		if( self -> vocab_cnt < sizeof(self -> vocab_id2val)/sizeof(self -> vocab_id2val[0])){
			rc = KBTreeEntry(self -> vocab_key2id, &tmp_id, &wasInserted, data, row_bytes);
		} else {
			rc = KBTreeFind (self -> vocab_key2id, &tmp_id, data, row_bytes);
			if(rc == 0) wasInserted=false;
		}
		if(rc == 0){ /*** either entered or found **/
			if(wasInserted){/** allocate and remember in vocabulary ***/
				/* allocate space for the row */
				TRY ( base = MemBankAlloc ( self -> mbank, ctx, row_bytes + sizeof row_len, false ) )
				{
				    /* copy it in */
				    base[0] = row_len;
				    if ( boff != 0 ) bitcpy ( & base [ 1 ], 0, data, boff, ( bitsz_t ) elem_bits * row_len );
				    else memcpy ( & base [ 1 ], data, row_bytes );
				    self -> vocab_id2val[self -> vocab_cnt++] = base; 
				}
			} else { /** get from vocabulary **/
				assert(tmp_id < self -> vocab_cnt);
				base = (uint32_t*)self -> vocab_id2val[tmp_id];
			}
		}
		rc = 0;
	}
	if(base == NULL){
		TRY ( base = MemBankAlloc ( self -> mbank, ctx, row_bytes + sizeof row_len, false ) )
		{
		    /* copy it in */
		    base[0] = row_len;
		    if ( boff != 0 )
			bitcpy ( & base [ 1 ], 0, data, boff, ( bitsz_t ) elem_bits * row_len );
		    else
			memcpy ( & base [ 1 ], data, row_bytes );
		}

	}
	/* remember base location in the map */
	self -> u . ids [ self -> cur_item ] = ( int64_t ) ( size_t ) base;
	/* remember the last input pointer */
    }

    if ( ! FAILED () )
    {
        /* detect last row */
        if ( ++ self -> cur_item == self -> num_items )
        {
            /* write all rows to column writer */
	    uint32_t *last_base=NULL;
	    uint32_t   cnt=0;
            STATUS ( 3, "writing cell data to '%s' num_items=%ld vocab_size=%d num_immed=%d", ColumnWriterFullSpec ( self -> cw, ctx ), self -> num_items, self ->vocab_cnt, self -> num_immed );
            for ( i = 0; ! FAILED () && i < self -> num_items; ++ i )
            {
                /* map to new order */
                j = self -> ord [ i ];
                if ( j < self -> num_immed ){
		    if(cnt > 0){ /*** flush accumulated count ***/
			if(last_base==NULL) ColumnWriterWriteStatic(self->cw,ctx,self->elem_bits,           "",0,           0,cnt);
			else		    ColumnWriterWriteStatic(self->cw,ctx,self->elem_bits,&last_base[1],0,last_base[0],cnt);
			last_base=NULL;
			cnt=0;
		    }
                    ColumnWriterWrite ( self -> cw, ctx, self -> elem_bits, & self -> u . ids [ j ], 0, 1 );
                } else {
                    /* accumulate repeated rows */
                    void* ptr = (void*)(size_t)self->u.ids [ j ];
		    if (cnt == 0){
			cnt=1;
			last_base = ptr;
		    } else if (ptr == last_base){
			cnt++;
		    } else {
			if(last_base==NULL) ColumnWriterWriteStatic(self->cw,ctx,self->elem_bits,           "",0,           0,cnt);
			else		    ColumnWriterWriteStatic(self->cw,ctx,self->elem_bits,&last_base[1],0,last_base[0],cnt);
			last_base = ptr;
			cnt=1;
		    }
                }
	   }
	   if(cnt > 0){ /*** flush accumulated count ***/
		if(last_base==NULL) ColumnWriterWriteStatic(self->cw,ctx,self->elem_bits,           "",0,           0,cnt);
		else		    ColumnWriterWriteStatic(self->cw,ctx,self->elem_bits,&last_base[1],0,last_base[0],cnt);
            }
            /* drop the mem-bank */
            MemBankRelease ( self -> mbank, ctx );
            self -> mbank = NULL;

            /* forget about map */
            self -> u . ids = NULL;
            self -> ord = NULL;
            self -> cur_item = self -> num_items = self -> num_immed = 0;
            self -> elem_bits = 0;

            /* cleanup vocabulary */
            if(self -> vocab_key2id ) {
                KBTreeRelease  ( self -> vocab_key2id );
                self -> vocab_key2id = 0;
            }
            self -> vocab_cnt = 0;

        }
    }
}

static
void BufferedPairColWriterWriteStatic ( BufferedPairColWriter *self, const ctx_t *ctx,
    uint32_t elem_bits, const void *data, uint32_t boff, uint32_t row_len, uint64_t count )
{
    FUNC_ENTRY ( ctx );
    rc_t rc = RC ( rcExe, rcColumn, rcWriting, rcType, rcIncorrect );
    INTERNAL_ERROR ( rc, "writing to a non-static column" );
}


static
void BufferedPairColWriterCommit ( BufferedPairColWriter *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );
    ColumnWriterCommit ( self -> cw, ctx );
}


struct ColumnWriter_vt UnmappedBufferedPairColWriter_vt =
{
    BufferedPairColWriterWhack,
    BufferedPairColWriterFullSpec,
    BufferedPairColWriterPreCopy,
    BufferedPairColWriterPostCopy,
    BufferedPairColWriterWriteUnmapped,
    BufferedPairColWriterWriteStatic,
    BufferedPairColWriterCommit
};


struct ColumnWriter_vt MappedBufferedPairColWriter_vt =
{
    BufferedPairColWriterWhack,
    BufferedPairColWriterFullSpec,
    BufferedPairColWriterPreCopy,
    BufferedPairColWriterPostCopy,
    BufferedPairColWriterWriteMapped,
    BufferedPairColWriterWriteStatic,
    BufferedPairColWriterCommit
};


/*--------------------------------------------------------------------------
 * cSRATblPair
 *  interface to pairing of source and destination tables
 */

/* MakeBufferedColumnWriter
 *  make a wrapper that buffers all rows written
 *  then sorts on flush
 */
ColumnWriter *cSRATblPairMakeBufferedColumnWriter ( cSRATblPair *self,
    const ctx_t *ctx, ColumnWriter *writer )
{
    FUNC_ENTRY ( ctx );

    BufferedPairColWriter *buff;

    TRY ( buff = MemAlloc ( ctx, sizeof * buff, true ) )
    {
        TRY ( ColumnWriterInit ( & buff -> dad, ctx, & UnmappedBufferedPairColWriter_vt, false ) )
        {
            /* duplicate our friend */
            TRY ( buff -> cw = ColumnWriterDuplicate ( writer, ctx ) )
            {
                /* borrowed ( weak ) reference
                   cannot duplicate without creating cycle */
                buff -> tbl = self;

                return & buff -> dad;
            }
        }

        MemFree ( ctx, buff, sizeof * buff );
    }

    return NULL;
}

/* MakeBufferedIdRemapColumnWriter
 *  make a wrapper that buffers all rows written
 *  maps all ids through index MapFile
 *  then sorts on flush
 */
ColumnWriter *cSRATblPairMakeBufferedIdRemapColumnWriter ( cSRATblPair *self,
    const ctx_t *ctx, ColumnWriter *writer, MapFile *idx, bool assign_ids )
{
    FUNC_ENTRY ( ctx );

    BufferedPairColWriter *buff;

    TRY ( buff = MemAlloc ( ctx, sizeof * buff, true ) )
    {
        TRY ( ColumnWriterInit ( & buff -> dad, ctx, & MappedBufferedPairColWriter_vt, idx != NULL ) )
        {
            /* duplicate our friend */
            TRY ( buff -> cw = ColumnWriterDuplicate ( writer, ctx ) )
            {
                TRY ( buff -> idx = MapFileDuplicate ( idx, ctx ) )
                {
                    /* borrowed ( weak ) reference
                       cannot duplicate without creating cycle */
                    buff -> tbl = self;

                    /* preserve boolean in one of Dad's align bytes */
                    buff -> dad . align [ 0 ] = assign_ids;

                    return & buff -> dad;
                }

                ColumnWriterRelease ( writer, ctx );
            }
        }

        MemFree ( ctx, buff, sizeof * buff );
    }

    return NULL;
}
