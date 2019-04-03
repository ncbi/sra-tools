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

#include "id2name.h"

#include <klib/rc.h>

#include <kproc/lock.h>

const uint64_t ChunkSize = 1024 * 1024;

rc_t Id2Name_Init ( Id2name * self )
{
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC ( rcCont, rcIndex, rcConstructing, rcSelf, rcNull );
    }
    else
    {
        rc = KVectorMake ( & self -> ids );
        if ( rc == 0 )
        {
            rc = KDataBufferMakeBytes ( & self -> names, ChunkSize );
            if ( rc == 0 )
            {
                rc = KLockMake ( & self -> lock );
                if ( rc == 0 )
                {
                    self->first_free = 0;
                }
                else
                {
                    KDataBufferWhack ( & self -> names );
                }
            }
            else
            {
                KVectorRelease ( self -> ids );
            }
        }
    }
    return rc;
}

rc_t Id2Name_Whack ( Id2name * self )
{
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC ( rcCont, rcIndex, rcDestroying, rcSelf, rcNull );
    }
    else
    {
        rc_t rc2;
        rc = KDataBufferWhack ( & self -> names );
        rc2 = KLockRelease ( self -> lock );
        if ( rc == 0 )
        {
            rc = rc2;
        }

        KVectorRelease ( self -> ids );
    }
    return rc;
}

#include <stdio.h>

rc_t Id2Name_Add ( Id2name * self, uint64_t id, const char * name )
{   /* expects 0-terminated name */
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC ( rcCont, rcIndex, rcInserting, rcSelf, rcNull );
    }
    else if ( name == NULL )
    {
        rc = RC ( rcCont, rcIndex, rcInserting, rcParam, rcNull );
    }
    else
    {
        rc = KLockAcquire ( self -> lock );
        if ( rc == 0 )
        {
            rc_t rc2;
            size_t size = strlen(name) + 1;
            while ( self -> first_free + size >= KDataBufferBytes ( & self -> names ) )
            {
/*printf("Id2Name_Add: resizing to %lu\n", KDataBufferBytes ( & self -> names ) + ChunkSize);*/
                rc =  KDataBufferResize( & self -> names, KDataBufferBytes ( & self -> names ) + ChunkSize );
                if ( rc != 0 )
                {
                    KLockUnlock ( self -> lock );
                    return rc;
                }
            }
/*printf("Id2Name_Add: writing %s\n", name );*/
            strncpy( ( char * ) ( self -> names . base ) + self -> first_free, name, size ); /* size includes 0-terminator */
            rc = KVectorSetU64 ( self -> ids, id, self -> first_free );
            self -> first_free += size;

            rc2 = KLockUnlock ( self -> lock );
            if ( rc == 0 )
            {
                rc = rc2;
            }
        }
    }
    return rc;
}

rc_t Id2Name_Get ( Id2name * self, uint64_t id, const char ** res )
{   /* the result is 0-terminated */
    rc_t rc;
    if ( self == NULL )
    {
        rc = RC ( rcCont, rcIndex, rcAccessing, rcSelf, rcNull );
    }
    else if ( res == NULL )
    {
        rc = RC ( rcCont, rcIndex, rcAccessing, rcParam, rcNull );
    }
    else
    {
        uint64_t name_offset;
        rc = KVectorGetU64 ( self -> ids, id, & name_offset );
        if ( rc == 0 )
        {
            *res = (const char*) ( self -> names . base ) + name_offset;
        }
    }
    return rc;
}


