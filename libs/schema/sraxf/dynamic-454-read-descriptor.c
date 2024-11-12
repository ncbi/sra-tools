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
#include <vdb/extern.h>

#include <sra/sradb.h>
#include <vdb/xform.h>
#include <vdb/table.h>
#include <kdb/index.h>
#include <klib/data-buffer.h>
#include <klib/text.h>
#include <klib/rc.h>
#include <search/grep.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* dynamic_read_desc
 *  uses inputs to determine read type and segmentation
 *
 *  "spot" [ DATA ] - bases for entire spot
 *
 *  "key" [ DATA, CONTROL ] - bases for key sequence. for version 1,
 *  the first base following key is taken as biological start
 *
 *  "linker" [ DATA, CONTROL, OPTIONAL ] - if present, is used to separate
 *  all bases following "key" into mate pair biological reads
 *
 *  returns a trio for each identified read, with read type, start and length
 */
enum { dyn_read_type, dyn_read_start, dyn_read_len };

typedef uint32_t dynamic_read_desc [ 3 ];

static
rc_t CC dynamic_read_desc_static ( void *self, const VXformInfo *info, int64_t row_id,
    VRowResult *rslt, uint32_t argc, const VRowData argv [] )
{
    rc_t rc;
    dynamic_read_desc *p;
    KDataBuffer *dst = rslt -> data;

    /* severe error if adapter is longer than spot */
    if ( argv [ 0 ] . u . data . elem_count < argv [ 1 ] . u . data . elem_count )
        return RC ( rcSRA, rcFunction, rcExecuting, rcData, rcCorrupt );

    /* the buffer should have already been given the correct element size */
    if ( dst -> elem_bits != 32 * 3 )
    {
        rc = KDataBufferCast ( dst, dst, 32 * 3, true );
        if ( rc != 0 )
            return rc;
    }

    /* we always produce 2 reads */
    if ( dst -> elem_count != 2 )
    {
        rc = KDataBufferResize ( dst, 2 );
        if ( rc != 0 )
            return rc;
    }

    p = dst -> base;

    /* adapter */
    p [ 0 ] [ dyn_read_type ] = SRA_READ_TYPE_TECHNICAL;
    p [ 0 ] [ dyn_read_start ] = 0;
    assert(argv [ 1 ] . u . data . elem_count >> 32 == 0);
    p [ 0 ] [ dyn_read_len ] = (uint32_t)argv [ 1 ] . u . data . elem_count;

    /* fragment */
    p [ 1 ] [ dyn_read_type ] = SRA_READ_TYPE_BIOLOGICAL;
    p [ 1 ] [ dyn_read_start ] = p [ 0 ] [ dyn_read_len ];
    assert(argv [ 0 ] . u . data . elem_count >> 32 == 0);
    p [ 1 ] [ dyn_read_len ] = (uint32_t)(argv [ 0 ] . u . data . elem_count) - p [ 0 ] [ dyn_read_len ];

    rslt -> elem_count = 2;
    return 0;
}


/* dynamic_read_desc_with_linker
 */
typedef struct linker_agrep linker_agrep;
struct linker_agrep
{
    /* I suspect this should be unsigned... */
    int32_t edit_distance;
};

int32_t debug_callback(void *cbinfo, AgrepMatch *match, AgrepContinueFlag *cont)
{
    printf("Match: pos %d len %d score %d\n", match->position, match->length,
           match->score);
    return 0;
}


static
rc_t CC dynamic_read_desc_with_linker ( void *xself, const VXformInfo *info, int64_t row_id,
    VRowResult *rslt, uint32_t argc, const VRowData argv [] )
{
    rc_t rc;
    dynamic_read_desc *p;
    KDataBuffer *dst = rslt -> data;
    const linker_agrep *self = ( const void* ) xself;

    AgrepMatch match;
    int32_t found;
    AgrepFlags agrepflags;
    Agrep *agrep;
    /* AgrepCallArgs args; */

    const char *agreppattern;
    char buf[4096];
    const char *text;
    uint32_t textlen;

    /* severe error if adapter is longer than spot */
    if ( argv [ 0 ] . u . data . elem_count < argv [ 1 ] . u . data . elem_count )
        return RC ( rcSRA, rcFunction, rcExecuting, rcData, rcCorrupt );

    /* the buffer should have already been given the correct element size */
    if ( dst -> elem_bits != 32 * 3 )
    {
        rc = KDataBufferCast ( dst, dst, 32 * 3, true );
        if ( rc != 0 )
            return rc;
    }

    /* we always produce 4 reads for when the linker is present */
    if ( dst -> elem_count != 4 )
    {
        rc = KDataBufferResize ( dst, 4 );
        if ( rc != 0 )
            return rc;
    }

    /* TBD - a mechanism for detecting when this has not changed
       since typically it will be identical for every row in a table
       but not necessarily so */
    agreppattern = argv[2].u.data.base;
    agreppattern += argv[2].u.data.first_elem;
    textlen = (uint32_t)string_copy(buf, sizeof buf, agreppattern, argv[2].u.data.elem_count);
    if ( textlen >= sizeof buf )
        return RC ( rcSRA, rcFunction, rcExecuting, rcData, rcExcessive );

    text = argv[0].u.data.base;
    text += argv[0].u.data.first_elem;

    assert(argv[0].u.data.elem_count >> 32 == 0);
    textlen = (uint32_t)argv[0].u.data.elem_count;

    text += argv[1].u.data.elem_count;
    textlen -= argv[1].u.data.elem_count;

    agrepflags = AGREP_TEXT_EXPANDED_2NA
        | AGREP_PATTERN_4NA
        | AGREP_EXTEND_BETTER
        | AGREP_LEFT_MAINTAIN_SCORE
        | AGREP_ANYTHING_ELSE_IS_N;

    /* This might fail due to size restrictions. */
    rc = AgrepMake(&agrep, agrepflags | AGREP_ALG_MYERS, buf);
    if (rc == 0) {
        /* fprintf(stderr, "Using myers.\n"); */
    } else {
        rc = AgrepMake(&agrep, agrepflags | AGREP_ALG_MYERS_UNLTD, buf);
        /* Try one more time. */
        if (rc) {
            rc = AgrepMake(&agrep, agrepflags | AGREP_ALG_DP, buf);
        }
        if (rc)
            return rc;
    }

    found = AgrepFindBest(agrep, self->edit_distance, text, textlen, &match);

    if (found) {

        p = dst -> base;

        /* adapter */
        p [ 0 ] [ dyn_read_type ] = SRA_READ_TYPE_TECHNICAL;
        p [ 0 ] [ dyn_read_start ] = 0;
        assert(argv [ 1 ] . u . data . elem_count >> 32 == 0);
        p [ 0 ] [ dyn_read_len ] = (uint32_t)argv [ 1 ] . u . data . elem_count;

        /* fragment */
        p [ 1 ] [ dyn_read_type ] = SRA_READ_TYPE_BIOLOGICAL;
        p [ 1 ] [ dyn_read_start ] = (uint32_t)argv [ 1 ] . u . data . elem_count;
        p [ 1 ] [ dyn_read_len ] = match.position;

        /* linker */
        p [ 2 ] [ dyn_read_type ] = SRA_READ_TYPE_TECHNICAL;
        p [ 2 ] [ dyn_read_start ] = match.position + (uint32_t)argv[1].u.data.elem_count;
        p [ 2 ] [ dyn_read_len ] = match.length;

        /* fragment */
        p [ 3 ] [ dyn_read_type ] = SRA_READ_TYPE_BIOLOGICAL;
        p [ 3 ] [ dyn_read_start ] = match.position + match.length + (uint32_t)argv[1].u.data.elem_count;
        p [ 3 ] [ dyn_read_len ] = (uint32_t)argv [ 0 ] . u . data . elem_count - match.position - match.length - (uint32_t)argv[1].u.data.elem_count;

        rslt -> elem_count = 4;

    } else {


        p = dst -> base;

        /* adapter */
        p [ 0 ] [ dyn_read_type ] = SRA_READ_TYPE_TECHNICAL;
        p [ 0 ] [ dyn_read_start ] = 0;
        assert(argv [ 1 ] . u . data . elem_count >> 32 == 0);
        p [ 0 ] [ dyn_read_len ] = (uint32_t)argv [ 1 ] . u . data . elem_count;

        /* fragment */
        p [ 1 ] [ dyn_read_type ] = SRA_READ_TYPE_BIOLOGICAL;
        p [ 1 ] [ dyn_read_start ] = (uint32_t)argv [ 1 ] . u . data . elem_count;
        p [ 1 ] [ dyn_read_len ] = (uint32_t)argv [ 0 ] . u . data . elem_count - (uint32_t)argv [ 1 ] . u . data . elem_count;

        /* linker */
        p [ 2 ] [ dyn_read_type ] = SRA_READ_TYPE_TECHNICAL;
        p [ 2 ] [ dyn_read_start ] = (uint32_t)argv [ 0 ] . u . data . elem_count;
        p [ 2 ] [ dyn_read_len ] = 0;

        /* fragment */
        p [ 3 ] [ dyn_read_type ] = SRA_READ_TYPE_BIOLOGICAL;
        p [ 3 ] [ dyn_read_start ] = (uint32_t)argv [ 0 ] . u . data . elem_count;
        p [ 3 ] [ dyn_read_len ] = 0;

        rslt -> elem_count = 4;

    }

    AgrepWhack(agrep);

    return 0;
}

static
void CC dyn_454_read_desc_free_wrapper( void *ptr )
{
	free( ptr );
}

/*
 extern function U32 [ 3 ] NCBI:SRA:_454_:dynamic_read_desc #1 < * U32 edit_distance >
    ( NCBI:SRA:_454_:drdparam_set spot, NCBI:SRA:_454_:drdparam_set key * NCBI:SRA:_454_:drdparam_set linker );
 */
VTRANSFACT_IMPL ( NCBI_SRA__454__dynamic_read_desc, 1, 0, 0 ) ( const void *self,
    const VXfactInfo *info, VFuncDesc *rslt, const VFactoryParams *cp, const VFunctionParams *dp )
{
    linker_agrep *xself;

    if ( dp -> argc == 2 )
    {
        rslt -> u . ndf = dynamic_read_desc_static;
        rslt -> variant = vftNonDetRow;
        return 0;
    }

    xself = malloc ( sizeof * xself );
    if ( xself == NULL )
        return RC ( rcSRA, rcFunction, rcConstructing, rcMemory, rcExhausted );

    /* NB - this should be changed to unsigned */
    xself -> edit_distance = 5;
    if ( cp -> argc == 1 )
        xself -> edit_distance = cp -> argv [ 0 ] . data . i32 [ 0 ];

    rslt -> self = xself;
    rslt -> whack = dyn_454_read_desc_free_wrapper;
    rslt -> u . rf = dynamic_read_desc_with_linker;
    rslt -> variant = vftRow;
    return 0;
}
