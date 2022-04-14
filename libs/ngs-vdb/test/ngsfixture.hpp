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

#ifndef _h_ngsfixture_
#define _h_ngsfixture_

/**
* Unit tests for NGS C++ interface, common definitions
*/

#include <kfg/config.h> /* KConfigDisableUserSettings */

#include <ngs/ncbi/NGS.hpp>

#include <ktst/unit_test.hpp>
#include <klib/time.h>

#include <sysalloc.h>
#include <assert.h>

#include <map>

#define SHOW_UNIMPLEMENTED 0

class NgsFixture
{
public:
    typedef std :: map< std :: string, ncbi::ReadCollection > Collections;

public:
    NgsFixture() :
        suppress_errors(false)
    {
        if ( colls == nullptr )
        {
            colls = new NgsFixture::Collections();
        }
    }

    ~NgsFixture()
    {
    }

    static void ReleaseCache()
    {
        delete colls;
        colls = nullptr;
    }

    ncbi::ReadCollection open ( const char* acc )
    {
        std :: string ac(acc);
        auto c = colls->find(ac);
        if ( c != colls->end() )
        {
            return c -> second;
        }
        KSleepMs(500); // trying to reduce the incidence of network timeouts on access to SDL in the following call
        ncbi::ReadCollection ret = ncbi :: NGS :: openReadCollection ( acc );
        colls->insert( Collections::value_type(ac, ret) );
        return ret;
    }

    ngs :: ReadIterator getReads ( const char* acc, ngs :: Read :: ReadCategory cat )
    {
        return open ( acc ) . getReads ( cat );
    }
    ngs :: Read getRead (const char* acc, const ngs :: String& p_id)
    {
        return open ( acc ). getRead ( p_id );
    }
    ngs :: Read getFragment (const char* acc, const ngs :: String& p_readId, uint32_t p_fragIdx)
    {
        ngs :: Read read = getRead ( acc, p_readId );
        assert ( p_fragIdx != 0 );
        while ( p_fragIdx > 0 )
        {
            read . nextFragment ();
            -- p_fragIdx;
        }

        return read;
    }

    ngs :: Reference getReference ( const char* acc, const char* spec )
    {
        return open ( acc ) . getReference ( spec );
    }
    bool hasReference ( const char* acc, const char* spec )
    {
        return open ( acc ) . hasReference ( spec );
    }
    ngs :: ReferenceIterator getReferences ( const char* acc )
    {
        return open ( acc ) . getReferences ();
    }

    bool suppress_errors;

    static Collections * colls;
};

NgsFixture::Collections * NgsFixture::colls = nullptr;

#endif

