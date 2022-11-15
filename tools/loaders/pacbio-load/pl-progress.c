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

#include <klib/rc.h>
#include <klib/out.h>
#include <sysalloc.h>
#include <stdlib.h>


typedef struct pl_progress
{
    bool initialized;
    uint8_t fract_digits;
    uint64_t count;
    uint64_t position;
    uint16_t percent;
} pl_progress;


static uint8_t pl_calc_fract_digits( const uint64_t count )
{
    uint8_t res = 0;
    if ( count > 10000 )
    {
        if ( count > 100000 )
            res = 2;
        else
            res = 1;
    }
    return res;
}


rc_t pl_progress_make( pl_progress ** pb, const uint64_t count )
{
    if ( pb == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    (*pb) = calloc( 1, sizeof( struct pl_progress ) );
    if ( *pb == NULL )
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    (*pb)->count = count;
    (*pb)->fract_digits = pl_calc_fract_digits( count );
    (*pb)->position = 0;
    return 0;
}


rc_t pl_progress_destroy( pl_progress * pb )
{
    if ( pb == NULL )
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    free( pb );
    return KOutMsg( "\n" );
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


uint32_t calc_percent( pl_progress * pb )
{
    uint32_t res = 0;
    uint32_t factor = 100;
    if ( pb->fract_digits > 0 )
    {
        if ( pb->fract_digits > 1 )
            factor = 10000;
        else
            factor = 1000;
    }
        
    if ( pb->count > 0 )
    {
        if ( pb->position >= pb->count )
            res = factor;
        else
        {
            uint64_t temp = pb->position;
            temp *= factor;
            temp /= pb->count;
            res = (uint32_t) temp;
        }
    }
    return res;
}


rc_t pl_progress_increment( pl_progress * pb, const uint64_t step )
{
    uint32_t percent;
    if ( pb == NULL )
        return RC( rcVDB, rcNoTarg, rcParsing, rcSelf, rcNull );

    pb->position += step;
    percent = calc_percent( pb );
    if ( pb->initialized )
    {
        if ( pb->percent != percent )
        {
            pb->percent = percent;
            switch( pb->fract_digits )
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
        switch( pb->fract_digits )
        {
        case 0 : progess_0a( percent ); break;
        case 1 : progess_1a( percent ); break;
        case 2 : progess_2a( percent ); break;
        }
        pb->initialized = true;
    }
    return 0;
}
