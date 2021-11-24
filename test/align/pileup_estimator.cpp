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

/**
* Unit tests for the pileup-estimator
*/

#include <limits>	/* for std::numeric_limits<uint64_t>::max() in test #14 */

#include <ktst/unit_test.hpp>

#include <stdexcept> 
#include <string>

#include <klib/rc.h>
#include <klib/text.h>

#include <vdb/manager.h>
#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>

#include <align/reference.h>
#include <align/manager.h>
#include <align/iterator.h>

#include <align/unsupported_pileup_estimator.h>

#include <ngs/ncbi/NGS.hpp> // openReadCollection
#include <ngs/ErrorMsg.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/Reference.hpp>
#include <ngs/Alignment.hpp>
#include <ngs/PileupIterator.hpp>

using namespace std;

static rc_t calc_coverage_sum_using_ref_iter( const char * acc, const char * refname,
                                               uint64_t slice_start, uint32_t slice_len, uint64_t * result )
{
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", acc );
        if ( rc == 0 )
        {
            const VTable * tbl;
            rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
            if ( rc == 0 )
            {
                const VCursor * prim_curs;
                rc = VTableCreateCursorRead( tbl, &prim_curs );
                if ( rc == 0 )
                {
                    const ReferenceList *ref_list;
                    uint32_t reflist_options =  ereferencelist_usePrimaryIds;
                    rc = ReferenceList_MakeDatabase( &ref_list, db, reflist_options, 0, NULL, 0 ); /* align/reference.h */
                    if ( rc == 0 )
                    {
                        const ReferenceObj * ref_obj;
                        rc = ReferenceList_Find( ref_list, &ref_obj, refname, string_size( refname ) ); /* align/reference.h */
                        {
                            const AlignMgr * a_mgr;
                            rc = AlignMgrMakeRead( &a_mgr );   /* align/manager.h */
                            if ( rc == 0 )
                            {
                                ReferenceIterator * ref_iter;
                                rc = AlignMgrMakeReferenceIterator( a_mgr, &ref_iter, NULL, 0 );
                                if ( rc == 0 )
                                {
                                    rc = ReferenceIteratorAddPlacements( ref_iter,       /* the outer ref-iter */
                                                                         ref_obj,        /* the ref-obj for this chromosome */
                                                                         slice_start,    /* start ( zero-based ) */
                                                                         slice_len,      /* length */
                                                                         NULL,          /* ref-cursor */
                                                                         prim_curs,     /* align-cursor */
                                                                         primary_align_ids,    /* which id's */
                                                                         NULL,         /* what read-group */
                                                                         NULL );       /* placement-context */
                                    while ( rc == 0 )
                                    {
                                        rc = ReferenceIteratorNextReference( ref_iter, NULL, NULL, NULL );
                                        rc_t rc1 = 0;
                                        while ( rc == 0 && rc1 == 0 )
                                        {
                                            INSDC_coord_zero first_pos;
                                            INSDC_coord_len len;
                                            rc1 = ReferenceIteratorNextWindow( ref_iter, &first_pos, &len );
                                            rc_t rc2 = 0;
                                            while ( rc1 == 0 && rc2 == 0 )
                                            {
                                                rc2 = ReferenceIteratorNextPos( ref_iter, true ); /* do skip empty positions */
                                                if ( rc2 == 0 )
                                                {
                                                    uint32_t depth;
                                                    rc2 = ReferenceIteratorPosition( ref_iter, NULL, &depth, NULL );
                                                    if ( rc2 == 0 )
                                                        *result += depth;
                                                }
                                            }
                                        }
                                    }
                                    if ( GetRCState( rc ) == rcDone ) rc = 0;
                                    ReferenceIteratorRelease( ref_iter );
                                }
                                AlignMgrRelease( a_mgr );
                            }
                            ReferenceObj_Release( ref_obj );
                        }
                        ReferenceList_Release( ref_list );
                    }
                    VCursorRelease( prim_curs );
                }
                VTableRelease ( tbl );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
    return rc;
}

static rc_t calc_coverage_using_ref_iter( const char * acc, const char * refname,
                                          uint64_t slice_start, uint32_t slice_len, uint32_t * coverage )
{
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", acc );
        if ( rc == 0 )
        {
            const VTable * tbl;
            rc = VDatabaseOpenTableRead( db, &tbl, "%s", "PRIMARY_ALIGNMENT" );
            if ( rc == 0 )
            {
                const VCursor * prim_curs;
                rc = VTableCreateCursorRead( tbl, &prim_curs );
                if ( rc == 0 )
                {
                    const ReferenceList *ref_list;
                    uint32_t reflist_options =  ereferencelist_usePrimaryIds;
                    rc = ReferenceList_MakeDatabase( &ref_list, db, reflist_options, 0, NULL, 0 ); /* align/reference.h */
                    if ( rc == 0 )
                    {
                        const ReferenceObj * ref_obj;
                        rc = ReferenceList_Find( ref_list, &ref_obj, refname, string_size( refname ) ); /* align/reference.h */
                        {
                            const AlignMgr * a_mgr;
                            rc = AlignMgrMakeRead( &a_mgr );   /* align/manager.h */
                            if ( rc == 0 )
                            {
                                ReferenceIterator * ref_iter;
                                rc = AlignMgrMakeReferenceIterator( a_mgr, &ref_iter, NULL, 0 );
                                if ( rc == 0 )
                                {
                                    memset( coverage, 0, slice_len * ( sizeof *coverage ) );
                                    rc = ReferenceIteratorAddPlacements( ref_iter,       /* the outer ref-iter */
                                                                         ref_obj,        /* the ref-obj for this chromosome */
                                                                         slice_start,    /* start ( zero-based ) */
                                                                         slice_len,      /* length */
                                                                         NULL,          /* ref-cursor */
                                                                         prim_curs,     /* align-cursor */
                                                                         primary_align_ids,    /* which id's */
                                                                         NULL,         /* what read-group */
                                                                         NULL );       /* placement-context */
                                    while ( rc == 0 )
                                    {
                                        rc = ReferenceIteratorNextReference( ref_iter, NULL, NULL, NULL );
                                        rc_t rc1 = 0;
                                        while ( rc == 0 && rc1 == 0 )
                                        {
                                            INSDC_coord_zero first_pos;
                                            INSDC_coord_len len;
                                            rc1 = ReferenceIteratorNextWindow( ref_iter, &first_pos, &len );
                                            rc_t rc2 = 0;
                                            while ( rc1 == 0 && rc2 == 0 )
                                            {
                                                rc2 = ReferenceIteratorNextPos( ref_iter, true ); /* do skip empty positions */
                                                if ( rc2 == 0 )
                                                {
                                                    uint32_t depth;
                                                    INSDC_coord_zero this_pos;
                                                    rc2 = ReferenceIteratorPosition( ref_iter, &this_pos, &depth, NULL );
                                                    if ( rc == 0 )
                                                    {
                                                        int64_t ofs = ( this_pos - slice_start );
                                                        coverage[ ofs ] = depth;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    if ( GetRCState( rc ) == rcDone ) rc = 0;
                                    ReferenceIteratorRelease( ref_iter );
                                }
                                AlignMgrRelease( a_mgr );
                            }
                            ReferenceObj_Release( ref_obj );
                        }
                        ReferenceList_Release( ref_list );
                    }
                    VCursorRelease( prim_curs );
                }
                VTableRelease ( tbl );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
    return rc;
}


static void calc_coverage_using_ngs( ngs::String acc, ngs::String refname,
                                     uint64_t slice_start, uint32_t slice_len, uint32_t * coverage )
{
    try
    {
        ngs::ReadCollection run( ncbi::NGS::openReadCollection( acc ) );
        ngs::Reference ref = run.getReference ( refname );
        ngs::PileupIterator it = ref.getPileupSlice( slice_start, slice_len );
        while ( it.nextPileup() )
        {
            uint32_t depth( it.getPileupDepth() );
            if ( depth > 0 )
            {
                uint64_t pos( it.getReferencePosition() );
                int64_t ofs = ( pos - slice_start );
                if ( ofs >= 0 && ofs < slice_len )
                    coverage[ ofs ] = depth;
            }
        }
    }
    
    catch ( ngs::ErrorMsg & e ) {
        cerr << "Error: " << e . toString () << endl;
    }
    catch ( std::exception & e ) {
        cerr << "Error: " << e . what () << endl;
    }
    catch ( ... ) {
        cerr << "Error: unknown exception" << endl;
    }
}

const char * ACC1 = "SRR341578";
const char * ACC1_REF = "NC_011748.1";
const uint64_t slice1_start = 3000002;
const uint32_t slice1_len   = 200;


TEST_SUITE( PileupEstimatorTestSuite );

TEST_CASE ( Estimator_1 )
{
    std::cout << "Estimator-Test #1 ( estimator vs. ref_iter ) " << std::endl;
    
    uint64_t res1 = 0;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        String rname;
        StringInitCString( &rname, ACC1_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len, &res1 );
        REQUIRE_RC( rc );
        
        //std::cout << "result ( using RunPileupEstimator ) : " << res1 << std::endl;
        
        rc = ReleasePileupEstimator( estim );
        REQUIRE_RC( rc );
    }
    
    uint64_t res2 = 0;
    
    rc = calc_coverage_sum_using_ref_iter( ACC1, ACC1_REF, slice1_start, slice1_len, &res2 );
    REQUIRE_RC( rc );

    //std::cout << "result ( using ReferenceIterator ) : " << res2 << std::endl;    
    
    REQUIRE_EQUAL( res1, res2 );
}

TEST_CASE ( Estimator_2 )
{
    std::cout << "Estimator-Test #2 ( no sources and no cursors )" << std::endl;
    
    // MakePileupEstimator has to fail when source and the cursors are NULL
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, NULL, 0, NULL, NULL, 0, false );
    REQUIRE_RC_FAIL( rc );
}

TEST_CASE ( Estimator_3 )
{
    std::cout << "Estimator-Test #3 ( no self-ptr )" << std::endl;
        
    // MakePileupEstimator has to fail when given a NULL-ptr as *self
    rc_t rc = MakePileupEstimator( NULL, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC_FAIL( rc );
}

static rc_t make_cursor( const VDatabase *db, const VCursor ** curs, const char * tbl_name, uint32_t count, ... )
{
    const VTable * tbl;
    rc_t rc = VDatabaseOpenTableRead( db, &tbl, "%s", tbl_name );
    if ( rc == 0 )
    {
        rc = VTableCreateCursorRead( tbl, curs );
        if ( rc == 0 )
        {
            uint32_t i;
            va_list args;
            va_start( args, count );
            for ( i = 0; rc == 0 && i < count; i++ )
            {
                const char * colname = va_arg( args, const char * );
                uint32_t idx;
                rc = VCursorAddColumn( *curs, &idx, colname );
            }
            va_end( args );
            if ( rc == 0 )
                rc = VCursorOpen( *curs );
        }
        VTableRelease( tbl );
    }
    return rc;
}


TEST_CASE ( Estimator_4 )
{
    std::cout << "Estimator-Test #4 ( with 2 cursors provided )" << std::endl;
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ACC1 );
        REQUIRE_RC( rc );
        if ( rc == 0 )
        {
            const VCursor * prim_curs;
            rc = make_cursor( db, &prim_curs, "PRIMARY_ALIGNMENT", 2, "REF_POS", "REF_LEN" );
            REQUIRE_RC( rc );    
            if ( rc == 0 )
            {
                const VCursor * ref_curs;
                rc = make_cursor( db, &ref_curs, "REFERENCE", 4, "SEQ_ID", "SEQ_LEN", "MAX_SEQ_LEN", "PRIMARY_ALIGNMENT_IDS" );
                REQUIRE_RC( rc );
                if ( rc == 0 )
                {
                    struct PileupEstimator * estim;
                    rc_t rc = MakePileupEstimator( &estim, NULL, 0, ref_curs, prim_curs, 0, false );
                    REQUIRE_RC( rc );
                    if ( rc == 0 )
                    {
                        uint64_t res = 0;
                        
                        String rname;
                        StringInitCString( &rname, ACC1_REF );
                        
                        rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len, &res );
                        REQUIRE_RC( rc );
                        
                        //std::cout << "result: " << res << std::endl;
                        
                        rc = ReleasePileupEstimator( estim );
                        REQUIRE_RC( rc );
                    }
                    VCursorRelease( ref_curs );    
                }
                VCursorRelease( prim_curs );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
}


TEST_CASE ( Estimator_5 )
{
    std::cout << "Estimator-Test #5 ( with 2 cursors provided, one column on alignments missing )" << std::endl;
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ACC1 );
        REQUIRE_RC( rc );
        if ( rc == 0 )
        {
            const VCursor * prim_curs;
            rc = make_cursor( db, &prim_curs, "PRIMARY_ALIGNMENT", 1, "REF_POS" );
            REQUIRE_RC( rc );    
            if ( rc == 0 )
            {
                const VCursor * ref_curs;
                rc = make_cursor( db, &ref_curs, "REFERENCE", 4, "SEQ_ID", "SEQ_LEN", "MAX_SEQ_LEN", "PRIMARY_ALIGNMENT_IDS" );
                REQUIRE_RC( rc );
                if ( rc == 0 )
                {
                    struct PileupEstimator * estim;
                    rc_t rc = MakePileupEstimator( &estim, NULL, 0, ref_curs, prim_curs, 0, false );
                    REQUIRE_RC_FAIL( rc );
                    if ( rc == 0 )
                        ReleasePileupEstimator( estim );
                    VCursorRelease( ref_curs );    
                }
                VCursorRelease( prim_curs );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
}

TEST_CASE ( Estimator_6 )
{
    std::cout << "Estimator-Test #6 ( with 2 cursors provided, one column on ref missing )" << std::endl;
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ACC1 );
        REQUIRE_RC( rc );
        if ( rc == 0 )
        {
            const VCursor * prim_curs;
            rc = make_cursor( db, &prim_curs, "PRIMARY_ALIGNMENT", 2, "REF_POS", "REF_LEN" );
            REQUIRE_RC( rc );    
            if ( rc == 0 )
            {
                const VCursor * ref_curs;
                rc = make_cursor( db, &ref_curs, "REFERENCE", 3, "SEQ_ID", "SEQ_LEN", "MAX_SEQ_LEN" );
                REQUIRE_RC( rc );
                if ( rc == 0 )
                {
                    struct PileupEstimator * estim;
                    rc_t rc = MakePileupEstimator( &estim, NULL, 0, ref_curs, prim_curs, 0, false );
                    REQUIRE_RC_FAIL( rc );
                    if ( rc == 0 )
                        ReleasePileupEstimator( estim );
                    VCursorRelease( ref_curs );    
                }
                VCursorRelease( prim_curs );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
}

TEST_CASE ( Estimator_7 )
{
    std::cout << "Estimator-Test #7 ( with ref_cursor, no prim_cursor provided )" << std::endl;
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ACC1 );
        REQUIRE_RC( rc );
        if ( rc == 0 )
        {
            const VCursor * ref_curs;
            rc = make_cursor( db, &ref_curs, "REFERENCE", 4, "SEQ_ID", "SEQ_LEN", "MAX_SEQ_LEN", "PRIMARY_ALIGNMENT_IDS" );
            REQUIRE_RC( rc );
            if ( rc == 0 )
            {
                struct PileupEstimator * estim;
                rc_t rc = MakePileupEstimator( &estim, ACC1, 0, ref_curs, NULL, 0, false );
                REQUIRE_RC( rc );
                if ( rc == 0 )
                {
                    uint64_t res = 0;
                    
                    String rname;
                    StringInitCString( &rname, ACC1_REF );
                    
                    rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len, &res );
                    REQUIRE_RC( rc );
                    
                    //std::cout << "result: " << res << std::endl;
                    
                    rc = ReleasePileupEstimator( estim );
                    REQUIRE_RC( rc );
                }
                VCursorRelease( ref_curs );    
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
}

TEST_CASE ( Estimator_8 )
{
    std::cout << "Estimator-Test #8 ( with prim_cursor, no ref_cursor provided )" << std::endl;
    const VDBManager *mgr;
    rc_t rc = VDBManagerMakeRead( &mgr, NULL );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        const VDatabase *db;
        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", ACC1 );
        REQUIRE_RC( rc );
        if ( rc == 0 )
        {
            const VCursor * prim_curs;
            rc = make_cursor( db, &prim_curs, "PRIMARY_ALIGNMENT", 2, "REF_POS", "REF_LEN" );
            REQUIRE_RC( rc );    
            if ( rc == 0 )
            {
                struct PileupEstimator * estim;
                rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, prim_curs, 0, false );
                REQUIRE_RC( rc );
                if ( rc == 0 )
                {
                    uint64_t res = 0;
                    
                    String rname;
                    StringInitCString( &rname, ACC1_REF );
                    
                    rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len, &res );
                    REQUIRE_RC( rc );
                    
                    //std::cout << "result: " << res << std::endl;
                    
                    rc = ReleasePileupEstimator( estim );
                    REQUIRE_RC( rc );
                }
                VCursorRelease( prim_curs );
            }
            VDatabaseRelease( db );
        }
        VDBManagerRelease( mgr );
    }
}

TEST_CASE ( Estimator_9 )
{
    std::cout << "Estimator-Test #9 ( reference-name missing )" << std::endl;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        uint64_t res = 0;
        
        rc = RunPileupEstimator( estim, NULL, slice1_start, slice1_len, &res );
        REQUIRE_RC_FAIL( rc );
        if ( rc == 0 )
            ReleasePileupEstimator( estim );
    }
}

TEST_CASE ( Estimator_10 )
{
    std::cout << "Estimator-Test #10 ( slice-length is zero )" << std::endl;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        uint64_t res = 0;

        String rname;
        StringInitCString( &rname, ACC1_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice1_start, 0, &res );
        REQUIRE_RC_FAIL( rc );
        if ( rc == 0 )
            ReleasePileupEstimator( estim );
    }
}

TEST_CASE ( Estimator_11 )
{
    std::cout << "Estimator-Test #11 ( slice-start beyond end of reference )" << std::endl;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        uint64_t res = 0;

        String rname;
        StringInitCString( &rname, ACC1_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice1_start * 2, slice1_len, &res );
        REQUIRE_RC_FAIL( rc );
        if ( rc == 0 )
            ReleasePileupEstimator( estim );
    }
}

TEST_CASE ( Estimator_12 )
{
    std::cout << "Estimator-Test #12 ( slice-length beyond end of reference )" << std::endl;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        uint64_t res = 0;

        String rname;
        StringInitCString( &rname, ACC1_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len * 100000, &res );
        REQUIRE_RC_FAIL( rc );
        if ( rc == 0 )
            ReleasePileupEstimator( estim );
    }
}

TEST_CASE ( Estimator_13 )
{
    std::cout << "Estimator-Test #13 ( result-ptr missing )" << std::endl;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        String rname;
        StringInitCString( &rname, ACC1_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len, NULL );
        REQUIRE_RC_FAIL( rc );
        if ( rc == 0 )
            ReleasePileupEstimator( estim );
    }
}

const char * ACC2 = "SRR5450996";
const char * ACC2_REF = "NC_000068.7";
const uint64_t slice2_start = 98662227;
const uint32_t slice2_len   = 1000;

TEST_CASE ( Estimator_14 )
{
    std::cout << "Estimator-Test #14 ( cutoff-value on expensive accession ) " << std::endl;
    
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC2, 0, NULL, NULL, 1000000, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
		uint64_t res = 0;
		
        String rname;
        StringInitCString( &rname, ACC2_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice2_start, slice2_len, &res );
        REQUIRE_RC( rc );
        
        //std::cout << "result: " << res << std::endl;
     
		REQUIRE_EQUAL( res, std::numeric_limits<uint64_t>::max() );
	 
        rc = ReleasePileupEstimator( estim );
        REQUIRE_RC( rc );
    } 
}

TEST_CASE ( Estimator_15 )
{
    std::cout << "Estimator-Test #15 ( multiple invocations with different slices )" << std::endl;

    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC1, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
		uint64_t res = 0;
		
        String rname;
        StringInitCString( &rname, ACC1_REF );
        
        rc = RunPileupEstimator( estim, &rname, slice1_start, slice1_len, &res );
        REQUIRE_RC( rc );
        //std::cout << "result: " << res << std::endl;

        rc = RunPileupEstimator( estim, &rname, slice1_start + 100, slice1_len * 10, &res );
        REQUIRE_RC( rc );
        //std::cout << "result: " << res << std::endl;

        rc = RunPileupEstimator( estim, &rname, slice1_start + 1000, slice1_len * 100, &res );
        REQUIRE_RC( rc );
        //std::cout << "result: " << res << std::endl;

        rc = RunPileupEstimator( estim, &rname, slice1_start + 6000, slice1_len, &res );
        REQUIRE_RC( rc );
        //std::cout << "result: " << res << std::endl;
 
        rc = ReleasePileupEstimator( estim );
        REQUIRE_RC( rc );
    } 
}

const char * ACC3 = "SRR543323";
const char * ACC3_REF = "NC_000077.5";
const uint64_t slice3_start = 0;
const uint64_t slice3_len = 121843856;
const uint32_t max_depth = 32;

TEST_CASE ( Estimator_16 )
{
    std::cout << "Estimator-Test #16 ( RunCoverage, whole reference at once )" << std::endl;

    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC3, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        uint32_t * coverage1 = ( uint32_t * )calloc( slice3_len, sizeof * coverage1 );
        uint32_t * coverage2 = ( uint32_t * )calloc( slice3_len, sizeof * coverage2 );
        uint32_t depths1[ max_depth ];
        uint32_t depths2[ max_depth ];
        
        memset( depths1, 0, sizeof depths1 );
        memset( depths2, 0, sizeof depths2 );
        
        String rname;
        StringInitCString( &rname, ACC3_REF );
        
        rc = RunCoverage( estim, &rname, slice3_start, slice3_len, coverage1 );
        REQUIRE_RC( rc );

        rc = calc_coverage_using_ref_iter( ACC3, ACC3_REF, slice3_start, slice3_len, coverage2 );
        REQUIRE_RC( rc );
        
        uint32_t differences1 = 0;
        for ( uint32_t pos = 0; pos < slice3_len; pos++ )
        {
            if ( coverage1[ pos ] != coverage2[ pos ] )
            {
                std::cout << ( slice3_start + pos ) << " : " << coverage1[ pos ] << " , " << coverage2[ pos ] << std::endl;
                differences1++;
            }
            
            if ( coverage1[ pos ] >= max_depth )
                depths1[ max_depth - 1 ] += 1;
            else
                depths1[ coverage1[ pos ] ] += 1;

            if ( coverage2[ pos ] >= max_depth )
                depths2[ max_depth - 1 ] += 1;
            else
                depths2[ coverage2[ pos ] ] += 1;
                
        }
        
        uint32_t differences2 = 0;        
        for ( uint32_t pos = 0; pos < max_depth; pos++ )
        {
            if ( depths1[ pos ] != depths2[ pos ] )
            {
                std::cout << pos << " : " << depths1[ pos ] << " , " << depths2[ pos ] << std::endl;
                differences2++;
            }
        }
        
        REQUIRE_EQUAL( differences1, (uint32_t)0 );
        REQUIRE_EQUAL( differences2, (uint32_t)0 );
        
        rc = ReleasePileupEstimator( estim );
        REQUIRE_RC( rc );
        
        free( ( void * ) coverage1 );
        free( ( void * ) coverage2 );
    } 
}


TEST_CASE ( Estimator_17 )
{
    std::cout << "Estimator-Test #17 ( RunCoverage vs ngs-pileup )" << std::endl;

    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC3, 0, NULL, NULL, 0, true );
    REQUIRE_RC( rc );
    if ( rc == 0 )
    {
        uint32_t * coverage1 = ( uint32_t * )calloc( slice3_len, sizeof * coverage1 );
        uint32_t * coverage2 = ( uint32_t * )calloc( slice3_len, sizeof * coverage2 );
        uint32_t depths1[ max_depth ];
        uint32_t depths2[ max_depth ];
        
        memset( depths1, 0, sizeof depths1 );
        memset( depths2, 0, sizeof depths2 );
        
        String rname;
        StringInitCString( &rname, ACC3_REF );
        
        rc = RunCoverage( estim, &rname, slice3_start, slice3_len, coverage1 );
        REQUIRE_RC( rc );

        calc_coverage_using_ngs( ACC3, ACC3_REF, slice3_start, slice3_len, coverage2 );
        
        uint32_t differences1 = 0;
        for ( uint32_t pos = 0; pos < slice3_len; pos++ )
        {
            if ( coverage1[ pos ] != coverage2[ pos ] )
            {
                std::cout << ( slice3_start + pos ) << " : " << coverage1[ pos ] << " , " << coverage2[ pos ] << std::endl;
                differences1++;
            }
            
            if ( coverage1[ pos ] >= max_depth )
                depths1[ max_depth - 1 ] += 1;
            else
                depths1[ coverage1[ pos ] ] += 1;

            if ( coverage2[ pos ] >= max_depth )
                depths2[ max_depth - 1 ] += 1;
            else
                depths2[ coverage2[ pos ] ] += 1;
                
        }
        
        uint32_t differences2 = 0;        
        for ( uint32_t pos = 0; pos < max_depth; pos++ )
        {
            if ( depths1[ pos ] != depths2[ pos ] )
            {
                std::cout << pos << " : " << depths1[ pos ] << " , " << depths2[ pos ] << std::endl;
                differences2++;
            }
        }
        
        REQUIRE_EQUAL( differences1, (uint32_t)0 );
        REQUIRE_EQUAL( differences2, (uint32_t)0 );
        
        rc = ReleasePileupEstimator( estim );
        REQUIRE_RC( rc );
        
        free( ( void * ) coverage1 );
        free( ( void * ) coverage2 );
    } 
   
}


TEST_CASE ( Estimator_18 )
{
    std::cout << "Estimator-Test #18 ( loop through the references )" << std::endl;
    struct PileupEstimator * estim;
    rc_t rc = MakePileupEstimator( &estim, ACC3, 0, NULL, NULL, 0, false );
    REQUIRE_RC( rc );

    const ReferenceList *reflist;
    const VDBManager *mgr;
    rc = VDBManagerMakeRead( &mgr, NULL );
    REQUIRE_RC( rc );
    
    rc = ReferenceList_MakePath( &reflist, mgr, ACC3, ereferencelist_usePrimaryIds, 0, NULL, 0 );
    REQUIRE_RC( rc );
    
    uint32_t count1, count2;
    rc = EstimatorRefCount( estim, &count1 );
    REQUIRE_RC( rc );
    
    rc = ReferenceList_Count( reflist, &count2 );
    REQUIRE_RC( rc );    
    
    REQUIRE_EQUAL( count1, count2 );    
    std::cout << "count1 : " << count1 << "  count2 : " << count2 << std::endl;
    
    for ( uint32_t idx = 0; rc == 0 && idx < count1; ++idx )
    {
        String refname;
        uint64_t reflen;
        
        rc = EstimatorRefInfo( estim, idx, &refname, &reflen );
        REQUIRE_RC( rc );
        
        const ReferenceObj *refobj;
        rc = ReferenceList_Get( reflist, &refobj, idx );        
        REQUIRE_RC( rc );
        
        const char * seqid;
        rc = ReferenceObj_SeqId( refobj, &seqid );
        REQUIRE_RC( rc );
        
        String SeqId;
        StringInitCString( &SeqId, seqid );
        
        int cmp = StringCompare( &refname, &SeqId );
        REQUIRE_EQUAL( cmp, (int)0 );
        
        INSDC_coord_len seqlen;
        rc = ReferenceObj_SeqLength( refobj, &seqlen );
        REQUIRE_RC( rc );
        REQUIRE_EQUAL( (uint64_t)seqlen, reflen );
        
        ReferenceObj_Release( refobj );
        //std::cout << " [" << idx << "] : " << refname.addr << " . " << reflen << std::endl;    
    }
    
    ReferenceList_Release( reflist ); // has no return-value!
    rc = VDBManagerRelease( mgr );
    REQUIRE_RC( rc );
    
    rc = ReleasePileupEstimator( estim );
    REQUIRE_RC( rc );
}


//////////////////////////////////////////// Main
#include <kapp/args.h>
#include <klib/out.h>
#include <kfg/config.h>

extern "C"
{

ver_t CC KAppVersion ( void )
{
    return 0x1000000;
}

const char UsageDefaultName[] = "test-estimator";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ( "Usage:\n" "\t%s [options]\n\n", progname );
}

rc_t CC Usage( const Args* args )
{
    return 0;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    return PileupEstimatorTestSuite( argc, argv );
}

}

