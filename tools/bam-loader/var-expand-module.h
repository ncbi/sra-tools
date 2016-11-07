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
#ifndef VAR_EXPAND_MODULE_H_
#define VAR_EXPAND_MODULE_H_

#include <klib/rc.h>

struct BAM_Alignment;

typedef struct var_expand_data
{
    uint64_t something;
} var_expand_data;

rc_t var_expand_init( var_expand_data ** data );

rc_t var_expand_handle( var_expand_data * data, struct BAM_Alignment const *alignment, char const refSequence[] );

rc_t var_expand_finish( var_expand_data * data );

#endif