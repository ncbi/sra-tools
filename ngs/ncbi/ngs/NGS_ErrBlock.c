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

#include "NGS_ErrBlock.h"
#include <ngs/itf/ErrBlock.h>

#define SRC_LOC_DEFINED 1

#include <kfc/ctx.h>
#include <kfc/except.h>
#include <klib/text.h>

#include <string.h>

/*--------------------------------------------------------------------------
 * NGS_ErrBlock
 */
void NGS_ErrBlockThrow ( struct NGS_ErrBlock_v1 * self, ctx_t ctx )
{
    if ( FAILED () )
    {
        size_t size;

        /* need to detect error type at some point... */
        self -> xtype = xt_error_msg;

        /* copy the message, up to max size */
        size = string_copy_measure ( self -> msg, sizeof self -> msg, WHAT () );
        if ( size >= sizeof self -> msg )
            strcpy ( & self -> msg [ sizeof self -> msg - 4 ], "..." );

        CLEAR ();
    }
}
