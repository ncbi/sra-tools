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

#include "caps.h"
#include "ctx.h"
#include "mem.h"
#include "except.h"

#include <vdb/manager.h>
#include <kdb/manager.h>
#include <kfg/config.h>

#include <string.h>

FILE_ENTRY ( caps );

/*--------------------------------------------------------------------------
 * Caps
 *  a very watered down version of vdb-3 capabilities
 */


/* Init
 *  initialize a local block from another
 */
void CapsInit ( Caps *caps, const ctx_t *ctx )
{
    if ( caps != NULL )
    {
        memset ( caps, 0, sizeof * caps );
        if ( ctx != NULL && ctx -> caps != NULL )
        {
            const Caps *orig = ctx -> caps;
            FUNC_ENTRY ( ctx );
            TRY ( caps -> mem = MemBankDuplicate ( orig -> mem, ctx ) )
            {
                rc_t rc = KConfigAddRef ( caps -> cfg = orig -> cfg );
                if ( rc != 0 )
                {
                    caps -> cfg = NULL;
                    ERROR ( rc, "failed to duplicate reference to KConfig" );
                }
                else
                {
                    rc = KDBManagerAddRef ( caps -> kdb = orig -> kdb );
                    if ( rc != 0 )
                    {
                        caps -> kdb = NULL;
                        ERROR ( rc, "failed to duplicate reference to KDBManager" );
                    }
                    else
                    {
                        rc = VDBManagerAddRef ( caps -> vdb = orig -> vdb );
                        if ( rc != 0 )
                        {
                            caps -> vdb = NULL;
                            ERROR ( rc, "failed to duplicate reference to VDBManager" );
                        }
                        else
                        {
                            caps -> tool = orig -> tool;
                        }
                    }
                }
            }
        }
    }
}


/* Whack
 *  release references
 */
void CapsWhack ( Caps *self, const ctx_t *ctx )
{
    if ( self != NULL )
    {
        rc_t rc;
        MemBank *mem;

        self -> tool = NULL;

        rc = VDBManagerRelease ( self -> vdb );
        if ( rc != 0 )
            ABORT ( rc, "failed to release reference to VDBManager" );
        self -> vdb = NULL;

        rc = KDBManagerRelease ( self -> kdb );
        if ( rc != 0 )
            ABORT ( rc, "failed to release reference to KDBManager" );
        self -> kdb = NULL;

        rc = KConfigRelease ( self -> cfg );
        if ( rc != 0 )
            ABORT ( rc, "failed to release reference to KConfig" );
        self -> cfg = NULL;

        mem = self -> mem;
        MemBankRelease ( mem, ctx );
        self -> mem = NULL;
    }
}
