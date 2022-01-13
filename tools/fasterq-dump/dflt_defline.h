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

#ifndef _h_dflt_defline_
#define _h_dflt_defline_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

const char * dflt_seq_defline( bool use_name, bool use_read_id, bool fasta );
const char * dflt_qual_defline( bool use_name, bool use_read_id );

/* ------------------------------------------------------------------------------------------- */

bool spot_group_requested( const char * seq_defline, const char * qual_defline );

/* ------------------------------------------------------------------------------------------- */

typedef struct defline_estimator_input_t
{
    const char * defline;
    const char * acc;
    uint32_t avg_name_len;
    uint32_t avg_seq_len;
    size_t row_count;
} defline_estimator_input_t;

size_t estimate_defline_length( const defline_estimator_input_t * input );

#ifdef __cplusplus
}
#endif

#endif
