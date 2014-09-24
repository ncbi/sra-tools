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

#include "vdb-copy-includes.h"
#include <sysalloc.h>
#include <stdlib.h>

typedef struct progressbar
{
    bool initialized;
    uint16_t percent;
} progressbar;


rc_t make_progressbar( progressbar ** pb )
{
    if ( pb == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    (*pb) = calloc( 1, sizeof( progressbar ) );
    if ( *pb == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    return 0;
}


rc_t destroy_progressbar( progressbar * pb )
{
    if ( pb == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    free( pb );
    return 0;
}

static void progess_0a( const uint16_t percent )
{
    KOutMsg( "| %2u%%", percent );
}

static void progess_0( const uint16_t percent )
{
    if ( percent & 1 )
        KOutMsg( "\b\b\b\b- %2u%%", percent );
    else
        KOutMsg( "\b\b\b%2u%%", percent );
}

static void progess_1a( const uint16_t percent )
{
    uint16_t p1 = percent / 10;
    uint16_t p0 = percent - ( p1 * 10 );
    KOutMsg( "| %2u.%01u%%", p1, p0 );
}

static void progess_1( const uint16_t percent )
{
    uint16_t p1 = percent / 10;
    uint16_t p0 = percent - ( p1 * 10 );
    if ( ( p1 & 1 )&&( p0 == 0 ) )
        KOutMsg( "\b\b\b\b\b\b- %2u.%01u%%", p1, p0 );
    else
        KOutMsg( "\b\b\b\b\b%2u.%01u%%", p1, p0 );
}

static void progess_2a( const uint16_t percent )
{
    uint16_t p1 = percent / 100;
    uint16_t p0 = percent - ( p1 * 100 );
    KOutMsg( "| %2u.%02u%%", p1, p0 );
}

static void progess_2( const uint16_t percent )
{
    uint16_t p1 = percent / 100;
    uint16_t p0 = percent - ( p1 * 100 );
    if ( ( p1 & 1 )&&( p0 == 0 ) )
        KOutMsg( "\b\b\b\b\b\b\b- %2u.%02u%%", p1, p0 );
    else
        KOutMsg( "\b\b\b\b\b\b%2u.%02u%%", p1, p0 );
}

rc_t update_progressbar( progressbar * pb, const uint8_t fract_digits,
                         const uint16_t percent )
{
    uint8_t digits = fract_digits;
    if ( pb == NULL )
        return RC( rcVDB, rcNoTarg, rcParsing, rcSelf, rcNull );
    if ( digits > 2 )
        digits = 2;
    if ( pb->initialized )
    {
        if ( pb->percent != percent )
        {
            pb->percent = percent;
            switch( digits )
            {
            case 0 : progess_0( percent ); break;
            case 1 : progess_1( percent ); break;
            case 2 : progess_2( percent ); break;
            }
        }
    }
    else
    {
        pb->percent = percent;
        switch( digits )
        {
        case 0 : progess_0a( percent ); break;
        case 1 : progess_1a( percent ); break;
        case 2 : progess_2a( percent ); break;
        }
        pb->initialized = true;
    }
    return 0;
}
