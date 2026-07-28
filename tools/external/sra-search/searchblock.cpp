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

#include "searchblock.hpp"

#include <cstring>

#include <klib/rc.h>
#include <klib/printf.h>

#include <search/grep.h>
#include <search/nucstrstr.h>
#include <search/smith-waterman.h>

#include <ngs/ErrorMsg.hpp>

using namespace std;
using namespace ngs;

static
void
ThrowRC(const char* p_message, rc_t p_rc)
{
    char buf[1024];
    rc_t rc = string_printf(buf, sizeof(buf), NULL, "%s, rc = %R", p_message, p_rc);
    if ( rc != 0 )
    {
        throw ErrorMsg ( p_message );
    }
    throw ErrorMsg ( buf );
}

//////////////////// SearchBlock subclasses

FgrepSearch :: FgrepSearch ( const string& p_query, Algorithm p_algorithm )
: SearchBlock ( p_query )
{
    m_queries[0] = m_query . c_str();

    rc_t rc = 0;
    switch ( p_algorithm )
    {
    case FgrepDumb:
        rc = FgrepMake ( & m_fgrep, FGREP_MODE_ACGT | FGREP_ALG_DUMB, m_queries, 1 );
        break;
    case FgrepBoyerMoore:
        rc = FgrepMake ( & m_fgrep, FGREP_MODE_ACGT | FGREP_ALG_BOYERMOORE, m_queries, 1 );
        break;
    case FgrepAho:
        rc = FgrepMake ( & m_fgrep, FGREP_MODE_ACGT | FGREP_ALG_AHOCORASICK, m_queries, 1 );
        break;
    default:
        throw ( ErrorMsg ( "FgrepSearch: unsupported algorithm" ) );
    }
    if ( rc != 0 )
    {
        ThrowRC ( "FgrepMake() failed", rc );
    }
}

FgrepSearch :: ~FgrepSearch ()
{
    FgrepFree ( m_fgrep );
}

bool
FgrepSearch :: FirstMatch ( const char* p_bases, size_t p_size, uint64_t * p_hitStart, uint64_t * p_hitEnd )
{
    FgrepMatch matchinfo;
    bool ret = FgrepFindFirst ( m_fgrep, p_bases, p_size, & matchinfo ) != 0;
    if ( ret )
    {
        if ( p_hitStart != 0 )
        {
            * p_hitStart = matchinfo . position;
        }
        if ( p_hitEnd != 0 )
        {
            * p_hitEnd = matchinfo . position + matchinfo . length;
        }
    }
    return ret;
}

AgrepSearch :: AgrepSearch ( const string& p_query, Algorithm p_algorithm, uint8_t p_minScorePct )
:   SearchBlock ( p_query ),
    m_minScorePct ( p_minScorePct )
{
    rc_t rc = 0;
    switch ( p_algorithm )
    {
    case AgrepDP:
        rc = AgrepMake ( & m_agrep, AGREP_MODE_ASCII | AGREP_ALG_DP, m_query . c_str() );
        break;
    case AgrepWuManber:
        rc = AgrepMake ( & m_agrep, AGREP_MODE_ASCII | AGREP_ALG_WUMANBER, m_query . c_str() );
        break;
    case AgrepMyers:
        rc = AgrepMake ( & m_agrep, AGREP_MODE_ASCII | AGREP_ALG_MYERS, m_query . c_str() );
        break;
    case AgrepMyersUnltd:
        rc = AgrepMake ( & m_agrep, AGREP_MODE_ASCII | AGREP_ALG_MYERS_UNLTD, m_query . c_str() );
        break;
    default:
        throw ( ErrorMsg ( "AgrepSearch: unsupported algorithm" ) );
    }
    if ( rc != 0 )
    {
        ThrowRC ( "AgrepMake failed", rc );
    }
}

AgrepSearch ::  ~AgrepSearch ()
{
    AgrepWhack ( m_agrep );
}

bool
AgrepSearch :: FirstMatch ( const char* p_bases, size_t p_size, uint64_t *  p_hitStart, uint64_t * p_hitEnd )
{
    AgrepMatch matchinfo;
    bool ret = AgrepFindFirst ( m_agrep,
                                m_query . size () * ( 100 - m_minScorePct ) / 100, // 0 = perfect match
                                p_bases,
                                p_size,
                                & matchinfo ) != 0;
    if ( ret )
    {
        if ( p_hitStart != 0 )
        {
            * p_hitStart = matchinfo . position;
        }
        if ( p_hitEnd != 0 )
        {
            * p_hitEnd = matchinfo . position + matchinfo . length;
        }
    }
    return ret;
}

NucStrstrSearch :: NucStrstrSearch ( const string& p_query, bool p_positional, bool p_useBlobSearch )
:   SearchBlock ( p_query ),
    m_positional ( p_positional || p_useBlobSearch ) // when searching blob-by-blob, have to use positional mode since it reports position of the match, required in blob mode
{
    rc_t rc = NucStrstrMake ( &m_nss, m_positional ? 1 : 0, m_query . c_str (), m_query . size () );
    if ( rc != 0 )
    {
        throw ( ErrorMsg ( "NucStrstrMake() failed" ) );
    }
}

NucStrstrSearch ::  ~NucStrstrSearch ()
{
    NucStrstrWhack ( m_nss );
}

bool
NucStrstrSearch :: FirstMatch ( const char* p_bases, size_t p_size, uint64_t * p_hitStart, uint64_t * p_hitEnd )
{
    if ( ! m_positional && ( p_hitStart != 0 || p_hitEnd != 0 ) )
    {
        throw ( ErrorMsg ( "NucStrstrSearch: non-positional search in a blob is not supported" ) );
    }

    // convert p_bases to 2na packed since nucstrstr works with that format only
    const size_t bufSize = p_size / 4 + 1 + 16; // NucStrstrSearch expects the buffer to be at least 16 bytes longer than the sequence
    unsigned char* buf2na = new unsigned char [ bufSize ];
    ConvertAsciiTo2NAPacked ( p_bases, p_size, buf2na, bufSize );

    unsigned int selflen;
    int pos = ::NucStrstrSearch ( m_nss, reinterpret_cast < const void * > ( buf2na ), 0, p_size, & selflen );
    bool ret = pos > 0;
    if ( ret )
    {
        if ( p_hitStart != 0 )
        {
            * p_hitStart = pos - 1;
        }
        if ( p_hitEnd != 0 )
        {
            * p_hitEnd = pos - 1 + selflen;
        }
    }
    delete [] buf2na;
    return ret;
}

unsigned char map [ 1 << ( sizeof ( char ) * 8 ) ];
static class InitMap {
public:
    InitMap()
    {
        memset( map, 0, sizeof map );
        map[(unsigned)'A'] = map[(unsigned)'a'] = 0;
        map[(unsigned)'C'] = map[(unsigned)'c'] = 1;
        map[(unsigned)'G'] = map[(unsigned)'g'] = 2;
        map[(unsigned)'T'] = map[(unsigned)'t'] = 3;
    }
} initMap;

void
NucStrstrSearch :: ConvertAsciiTo2NAPacked ( const char* pszRead, size_t nReadLen, unsigned char* pBuf2NA, size_t nBuf2NASize )
{
    static size_t shiftLeft [ 4 ] = { 6, 4, 2, 0 };

    fill ( pBuf2NA, pBuf2NA + nBuf2NASize, (unsigned char)0 );

    for ( size_t iChar = 0; iChar < nReadLen; ++iChar )
    {
        size_t iByte = iChar / 4;
        if ( iByte > nBuf2NASize )
        {
            assert ( false );
            break;
        }

        pBuf2NA[iByte] |= map [ size_t ( pszRead [ iChar ] ) ] << shiftLeft [ iChar % 4 ];
    }
}

SmithWatermanSearch :: SmithWatermanSearch ( const string& p_query, uint8_t p_minScorePct )
:   SearchBlock ( p_query ),
    m_minScorePct ( p_minScorePct )
{
    rc_t rc = SmithWatermanMake ( &m_sw, m_query . c_str() );
    if ( rc != 0 )
    {
        ThrowRC ( "SmithWatermanMake() failed", rc );
    }
}

SmithWatermanSearch :: ~SmithWatermanSearch ()
{
    SmithWatermanWhack ( m_sw );
}

bool
SmithWatermanSearch :: FirstMatch ( const char* p_bases, size_t p_size, uint64_t * p_hitStart, uint64_t * p_hitEnd )
{
    SmithWatermanMatch matchinfo;
    unsigned int scoreThreshold = ( m_query . size () * 2 ) * m_minScorePct / 100; // m_querySize * 2 == exact match
    rc_t rc = SmithWatermanFindFirst ( m_sw, scoreThreshold, p_bases, p_size, & matchinfo );
    if ( rc == 0 )
    {
        if ( p_hitStart != 0 )
        {
            * p_hitStart = matchinfo . position;
        }
        if ( p_hitEnd != 0 )
        {
            * p_hitEnd = matchinfo . position + matchinfo . length;
        }
        return true;
    }
    else if ( GetRCObject ( rc ) == (RCObject)rcQuery  && GetRCState ( rc ) == rcNotFound )
    {
        return false;
    }
    ThrowRC ( "SmithWatermanFindFirst() failed", rc );
    return false;
}
