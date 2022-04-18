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

#ifndef _h_ngs_c_fixture_
#define _h_ngs_c_fixture_

/**
* Unit tests for NGS C interface, common definitions
*/

#include <sysalloc.h>

#include <string>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <map>

#include <ktst/unit_test.hpp>

#include <kfc/xcdefs.h>
#include <kfc/except.h>

#include <kfc/ctx.h>
#include <kfc/rsrc.h>

#include <klib/time.h>

#include <vdb/database.h>
#include <vdb/table.h>

#include "../ncbi/ngs/NGS_ReadCollection.h"
#include "../ncbi/ngs/NGS_Read.h"
#include "../ncbi/ngs/NGS_ReadGroup.h"
#include "../ncbi/ngs/NGS_Reference.h"
#include "../ncbi/ngs/NGS_Alignment.h"
#include "../ncbi/ngs/NGS_Statistics.h"

#include <../ncbi/ngs/NGS_String.h>

#define SHOW_UNIMPLEMENTED 0

#define ENTRY \
    HYBRID_FUNC_ENTRY ( rcSRA, rcRow, rcAccessing ); \
    m_ctx = ctx;

#define ENTRY_ACC(acc) \
    ENTRY; \
    m_acc = acc; \
    m_coll = open ( ctx, acc );

#define ENTRY_GET_READ(acc, readNo) \
    ENTRY_ACC(acc); \
    GetRead(readNo);

#define ENTRY_GET_REF(acc,ref) \
    ENTRY_ACC(acc); \
    GetReference ( ref );

#define EXIT \
    REQUIRE ( ! FAILED () ); \
    Release()

////// additional REQUIRE macros

#define REQUIRE_FAILED() ( REQUIRE ( FAILED () ), CLEAR() )

// use for NGS calls returning NGS_String*
#define REQUIRE_STRING(exp, call) \
{\
    string str = toString ( call, ctx, true );\
    REQUIRE ( ! FAILED () );\
    REQUIRE_EQ ( string(exp), str);\
}

//////

std :: string
toString ( const NGS_String* str, ctx_t ctx, bool release_source = false )
{
    if ( str == 0 )
    {
        throw std :: logic_error ( "toString ( NULL ) called" );
    }
    std :: string ret = std::string ( NGS_StringData ( str, ctx ), NGS_StringSize ( str, ctx ) );
    if ( release_source )
    {
        NGS_StringRelease ( str, ctx );
    }
    return ret;
}

class NGS_C_Fixture
{
public:
    typedef std :: map< std :: string, NGS_ReadCollection * > Collections;
    typedef std :: map< std :: string, const VDatabase* > Databases;
    typedef std :: map< std :: string, const VTable* > Tables;

public:
    NGS_C_Fixture()
    : m_ctx(0), m_coll(0), m_read(0), m_readGroup (0), m_ref (0)
    {
        if ( colls == nullptr )
        {
            colls = new Collections();
        }
        if ( dbs == nullptr )
        {
            dbs = new Databases();
        }
        if ( tbls == nullptr )
        {
            tbls = new Tables();
        }
    }
    ~NGS_C_Fixture()
    {
    }

    // call once per process
    static void ReleaseCache()
    {
        HYBRID_FUNC_ENTRY ( rcSRA, rcRow, rcAccessing );
        for (auto c : *colls )
        {
            NGS_RefcountRelease ( ( NGS_Refcount* ) c.second, ctx );
        }
        delete colls;
        colls = nullptr;

        for (auto d : *dbs )
        {
            VDatabaseRelease ( d.second );
        }
        delete dbs;
        dbs = nullptr;

        for (auto t : *tbls )
        {
            VTableRelease ( t.second );
        }
        delete tbls;
        tbls = nullptr;
    }

    virtual void Release()
    {
        if (m_ctx != 0)
        {
            if (m_coll != 0)
            {
                NGS_RefcountRelease ( ( NGS_Refcount* ) m_coll, m_ctx );
            }
            if (m_read != 0)
            {
                NGS_ReadRelease ( m_read, m_ctx );
            }
            if (m_readGroup != 0)
            {
                NGS_ReadGroupRelease ( m_readGroup, m_ctx );
            }
            if (m_ref != 0)
            {
                NGS_ReferenceRelease ( m_ref, m_ctx );
            }
            m_ctx = 0; // a pointer into the caller's local memory
        }
    }

    NGS_ReadCollection * open ( ctx_t ctx, const char* acc )
    {
        FUNC_ENTRY ( ctx, rcSRA, rcRow, rcAccessing );

        std :: string ac(acc);
        auto c = colls->find(ac);
        if ( c != colls->end() )
        {
            NGS_ReadCollectionDuplicate( c->second, ctx );
            return c -> second;
        }

        KSleepMs(500); // trying to reduce the incidence of network timeouts on access to SDL in the following call
        ON_FAIL ( NGS_ReadCollection * ret = NGS_ReadCollectionMake ( ctx, acc ) )
        {
            throw std :: logic_error ( std::string ( "NGS_ReadCollectionMake(" ) + ac + ") failed" );
        }
        colls->insert( Collections::value_type(ac, ret) );
        NGS_ReadCollectionDuplicate( ret, ctx ); // wil be released by ReleaseCache()
        return ret;
    }

    const VDatabase * openDB( const char* p_acc )
    {
        const VDatabase *db;

        std :: string ac( p_acc );
        auto d = dbs->find( ac );
        if ( d != dbs->end() )
        {
            db = d->second;
            THROW_ON_RC ( VDatabaseAddRef( db ) );
        }
        else
        {
            KSleepMs(500); // trying to reduce the incidence of network timeouts on access to SDL in the following call
            THROW_ON_RC ( VDBManagerOpenDBRead ( m_ctx -> rsrc -> vdb, & db, NULL, p_acc ) );
            dbs->insert( Databases::value_type( ac, db) );
            THROW_ON_RC ( VDatabaseAddRef( db ) );
        }
        return db;
    }

    const VTable * openTable( const char* p_acc )
    {
        const VTable *tbl;

        std :: string ac( p_acc );
        auto t =tbls->find( ac );
        if ( t != tbls->end() )
        {
            tbl = t->second;
            THROW_ON_RC ( VTableAddRef( tbl ) );
        }
        else
        {
            KSleepMs(500); // trying to reduce the incidence of network timeouts on access to SDL in the following call
            THROW_ON_RC ( VDBManagerOpenTableRead ( m_ctx -> rsrc -> vdb, & tbl, NULL, p_acc ) );
            tbls->insert( Tables::value_type( ac, tbl) );
            THROW_ON_RC ( VTableAddRef( tbl ) );
        }
        return tbl;
    }

    std::string ReadId(int64_t id) const
    {
        std::ostringstream s;
        s << m_acc << ".R." << id;
        return s . str ();
    }
    void GetRead(const std::string & id)
    {
        m_read = NGS_ReadCollectionGetRead ( m_coll, m_ctx, id.c_str() );
        if (m_read != 0)
        {   // initialize the fragment iterator
            NGS_FragmentIteratorNext ( (NGS_Fragment*)m_read, m_ctx );
        }
    }
    void GetRead(int64_t id)
    {
        GetRead ( ReadId ( id ) );
    }

    void GetReference(const char* name)
    {
        m_ref = NGS_ReadCollectionGetReference ( m_coll, m_ctx, name );
        if ( m_ctx -> rc != 0 || m_ref == 0 )
            throw std :: logic_error ( "GetReference() failed" );
    }

    const KCtx*         m_ctx;  // points into the test case's local memory
    std::string         m_acc;
    NGS_ReadCollection* m_coll;
    NGS_Read*           m_read;
    NGS_ReadGroup*      m_readGroup;
    NGS_Reference*      m_ref;

    static Collections *    colls;
    static Databases *      dbs;
    static Tables *         tbls;
};

NGS_C_Fixture::Collections *    NGS_C_Fixture::colls    = nullptr;
NGS_C_Fixture::Databases *      NGS_C_Fixture::dbs      = nullptr;
NGS_C_Fixture::Tables *         NGS_C_Fixture::tbls      = nullptr;

#endif

