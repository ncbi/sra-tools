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

#ifndef _h_ref_walker_0_
#define _h_ref_walker_0_

#ifdef __cplusplus
extern "C" {
#endif

#include "pileup_options.h"

#include <insdc/sra.h>

#include <stdlib.h>
#include <sysalloc.h>
#include <string.h>
#include <os-native.h>

typedef struct tool_rec tool_rec;
struct tool_rec
{
    bool reverse;   /* orientation towards reference ( false...in ref-orientation / true...reverse) */
    int32_t tlen;   /* template-len, for statistical analysis */
    uint32_t quality_len;
    uint8_t * quality;  /* ptr to quality... ( for sam-output ) */
};


typedef struct walk_data walk_data;
struct walk_data
{
    void *data;                             /* opaque pointer to data passed to each function */
    ReferenceIterator *ref_iter;            /* the global reference-iter */
    pileup_options *options;                /* the tool-options */
    struct ReferenceObj const * ref_obj;    /* the current reference-object */
    const char * ref_name;                  /* the name of the current reference */
    INSDC_coord_zero ref_start;             /* start of the current reference */
    INSDC_coord_len ref_len;                /* length of the current reference */
    INSDC_coord_zero ref_window_start;      /* start of the current reference-window */
    INSDC_coord_len ref_window_len;         /* length of the current reference-window */
    INSDC_coord_zero ref_pos;               /* current position on the reference */
    uint32_t depth;                         /* coverage at the current position */
    INSDC_4na_bin ref_base;                 /* reference-base at the current position */
    const char * spotgroup;                 /* name of the current spotgroup ( can be NULL! ) */
    size_t spotgroup_len;                   /* length of the name of the current spotgroup ( can be 0 ) */
    const PlacementRecord *rec;             /* current placement-record */
    tool_rec * xrec;                        /* current extended placement-record (orientation, quality...) */
    int32_t state;                          /* state of the current placement at the current ref-pos ( bitmasked!) */
    INSDC_coord_zero seq_pos;               /* position inside the alignment at the current ref-pos */
};


typedef struct walk_funcs walk_funcs;
struct walk_funcs
{
    /* changing reference */
    rc_t ( CC * on_enter_ref ) ( walk_data * data );
    rc_t ( CC * on_exit_ref ) ( walk_data * data );

    /* changing reference-window */
    rc_t ( CC * on_enter_ref_window ) ( walk_data * data );
    rc_t ( CC * on_exit_ref_window ) ( walk_data * data );

    /* changing reference-position */
    rc_t ( CC * on_enter_ref_pos ) ( walk_data * data );
    rc_t ( CC * on_exit_ref_pos ) ( walk_data * data );

    /* changing spot-group */
    rc_t ( CC * on_enter_spotgroup ) ( walk_data * data );
    rc_t ( CC * on_exit_spotgroup ) ( walk_data * data );

    /* on each alignment */
    rc_t ( CC * on_placement ) ( walk_data * data );
};


rc_t walk_0( walk_data * data, walk_funcs * funcs );

#ifdef __cplusplus
}
#endif

#endif /*  _h_ref_walker_0_ */
