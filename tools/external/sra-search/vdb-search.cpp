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

#include <iostream>

#include "vdb-search.hpp"

#include <queue>
#include <atomic>

#include <atomic32.h>

#include <klib/time.h>

#include <kproc/thread.h>
#include <kproc/lock.h>

#include <ngs/ErrorMsg.hpp>

#include "blobmatchiterator.hpp"
#include "fragmentmatchiterator.hpp"
#include "referencematchiterator.hpp"

using namespace std;
using namespace ngs;

bool VdbSearch :: logResults = false;

//////////////////// VdbSearch :: OutputQueue

class VdbSearch :: OutputQueue
{   // thread safe output queue; one consumer, multiple producers
    // counts producers
    // if there are active producers, Pop will wait for new items to appear or the last producer to go away
public:
    OutputQueue ( unsigned int p_producers )
    {
        atomic32_set ( & m_producers, p_producers );
        rc_t rc = KLockMake ( & m_outputQueueLock );
        if ( rc != 0 )
        {
            throw ( ErrorMsg ( "KLockMake failed" ) );
        }
    }
    ~OutputQueue()
    {
        KLockAcquire ( m_outputQueueLock );
        while ( m_queue . size () > 0 )
        {
            delete m_queue . front ();
            m_queue . pop ();
        }
        KLockUnlock ( m_outputQueueLock );

        KLockRelease ( m_outputQueueLock );
    }

    void ProducerDone () // called by the producers
    {
        assert ( atomic32_read ( & m_producers ) > 0 );
        atomic32_dec ( & m_producers );
    }

    void Push ( SearchBuffer :: Match * p_match ) // called by the producers
    {
        KLockAcquire ( m_outputQueueLock );
        m_queue . push ( p_match );
        KLockUnlock ( m_outputQueueLock );
    }

    // called by the consumer; will block until items become available or the last producer goes away
    SearchBuffer :: Match * Pop ()
    {
        while ( true )
        {
            KLockAcquire ( m_outputQueueLock );
            if ( m_queue . size () > 0 )
            {
               SearchBuffer :: Match * ret = m_queue . front ();
               m_queue.pop();
               KLockUnlock ( m_outputQueueLock );
               return ret;
            }
            KLockUnlock ( m_outputQueueLock );
            if ( atomic32_read ( & m_producers ) == 0 )
            {
               break;
            }
            KSleepMs(1);
        }

        assert ( atomic32_read ( & m_producers ) == 0 );
        return 0;
    }

private:
    queue < SearchBuffer :: Match * > m_queue;

    KLock* m_outputQueueLock;

    atomic32_t m_producers;
};

////////////////////  VdbSearch :: SearchThreadBlock

struct VdbSearch :: SearchThreadBlock
{
    VdbSearch :: OutputQueue& m_output;

    VdbSearch :: SearchQueue &                  m_search;
    KLock *                                     m_searchQueueLock;
    VdbSearch :: SearchQueue :: const_iterator  m_nextSearch;

    atomic_bool m_quitting;

    SearchThreadBlock ( SearchQueue& p_search, OutputQueue& p_output )
    :   m_output ( p_output ),
        m_search ( p_search ),
        m_searchQueueLock ( 0 ),
        m_nextSearch ( m_search . begin () ),
        m_quitting ( false )
    {
        rc_t rc = KLockMake ( & m_searchQueueLock );
        if ( rc != 0 )
        {
            throw ( ErrorMsg ( "KLockMake failed" ) );
        }
    }
    ~SearchThreadBlock ()
    {
        KLockRelease ( m_searchQueueLock );
    }
};

//////////////////// VdbSearch :: Settings

static
const
struct {
    const char* name;
    VdbSearch :: Algorithm value;
} Algorithms[] = {
#define ALG(n) { #n, VdbSearch :: n }
    { "FgrepStandard", VdbSearch :: FgrepDumb },
    ALG ( FgrepBoyerMoore ),
    ALG ( FgrepAho ),
    ALG ( AgrepDP ),
    ALG ( AgrepWuManber ),
    ALG ( AgrepMyers ),
    ALG ( AgrepMyersUnltd ),
    ALG ( NucStrstr ),
    ALG ( SmithWaterman ),
#undef ALG
};

VdbSearch :: Settings :: Settings ()
:   m_algorithm ( VdbSearch :: FgrepDumb ),
    m_isExpression ( false ),
    m_minScorePct ( 100 ),
    m_threads ( 2 ),
    m_threadPerAcc ( false ),
    m_useBlobSearch ( true ),
    m_referenceDriven ( false ),
    m_maxMatches ( 0 ),
    m_unaligned ( false ),
    m_fasta ( false ),
    m_fastaLineLength ( 70 )
{
}

bool
VdbSearch :: Settings :: SetAlgorithm ( const std :: string& p_algStr )
{
    for ( size_t i = 0 ; i < sizeof ( Algorithms ) / sizeof ( Algorithms [ 0 ] ); ++i )
    {
        if ( string ( Algorithms [ i ] . name ) == p_algStr )
        {
            m_algorithm = Algorithms [ i ] . value;
            return true;
        }
    }
    return false;
}

//////////////////// VdbSearch

static
void
CheckArguments ( const VdbSearch :: Settings& p_settings )
{
    if ( p_settings . m_isExpression && p_settings . m_algorithm != VdbSearch :: NucStrstr )
    {
        throw invalid_argument ( "query expressions are only supported for NucStrstr" );
    }
    if ( p_settings . m_minScorePct != 100 )
    {
        switch ( p_settings . m_algorithm )
        {
            case VdbSearch :: FgrepDumb:
            case VdbSearch :: FgrepBoyerMoore:
            case VdbSearch :: FgrepAho:
            case VdbSearch :: NucStrstr:
                throw invalid_argument ( "this algorithm only supports 100% match" );
            default:
                break;
        }
    }
}

VdbSearch :: VdbSearch ( const Settings& p_settings )
:   m_settings ( p_settings ),
    m_sbFactory ( m_settings ),
    m_buf ( 0 ),
    m_output ( 0 ),
    m_searchBlock ( 0 ),
    m_matchCount ( 0 )
{
    if ( m_settings . m_useBlobSearch && m_settings . m_algorithm == VdbSearch :: SmithWaterman )
    {
        m_settings . m_useBlobSearch = false; // SW takes too long on big buffers
    }
    if ( m_settings . m_unaligned )
    {   // unaligned goes by fragments, single threaded
        m_settings . m_useBlobSearch = false;
        m_settings . m_threads = 1;
    }

    CheckArguments ( m_settings );

    for ( vector<string>::const_iterator i = m_settings . m_accessions . begin(); i != m_settings . m_accessions . end(); ++i )
    {
        if ( m_settings . m_referenceDriven )
        {
            m_searches . push_back ( new ReferenceSearch ( m_sbFactory, *i, m_settings . m_references, m_settings . m_useBlobSearch ) );
        }
        else if ( m_settings . m_useBlobSearch )
        {
            m_searches . push_back ( new BlobSearch ( m_sbFactory, *i ) );
        }
        else
        {
            m_searches . push_back ( new FragmentSearch ( m_sbFactory, *i, m_settings . m_unaligned ) );
        }
    }
}

VdbSearch :: ~VdbSearch ()
{
    // make sure all threads are gone before we release shared objects
    if ( m_searchBlock != 0 )
    {
        m_searchBlock -> m_quitting . store ( true );
        for ( ThreadPool :: iterator i = m_threadPool . begin (); i != m_threadPool. end (); ++i )
        {
            //KThreadCancel ( *i ); does not work too well, instead using m_searchBlock -> m_quitting to command threads to exit orderly
            KThreadWait ( *i, 0 );
            KThreadRelease ( *i );
        }
        delete m_searchBlock;
    }

    for ( SearchQueue::iterator i = m_searches . begin(); i != m_searches . end(); ++i )
    {
        delete * i;
    }
    m_searches . clear ();

    delete m_buf;
    delete m_output;
}

VdbSearch :: SupportedAlgorithms
VdbSearch :: GetSupportedAlgorithms ()
{
    vector < string > ret;
    for ( size_t i = 0 ; i < sizeof ( Algorithms ) / sizeof ( Algorithms [ 0 ] ); ++i )
    {
        ret . push_back ( Algorithms [ i ] . name );
    }
    return ret;
}

rc_t CC VdbSearch :: ThreadPerIterator ( const KThread *, void *data )
{
    assert ( data );
    SearchThreadBlock& sb = * reinterpret_cast < SearchThreadBlock* > ( data );
    assert ( sb . m_searchQueueLock );
    // cout << "Thread " << (void*)self << " started " << endl;
    while ( ! sb . m_quitting . load() )
    {
        MatchIterator* it = 0;
        KLockAcquire ( sb . m_searchQueueLock );
        while ( ! sb . m_quitting . load() && sb . m_nextSearch != sb . m_search . end () )
        {
            it = ( * sb . m_nextSearch ) -> NextIterator ();
            if ( it == 0 )
            {
                ++ sb . m_nextSearch;
            }
            else
            {
                break;
            }
        }
        KLockUnlock ( sb . m_searchQueueLock );

        if ( it == 0 )
        {
            break;
        }

        // cout << "Thread " << (void*)self << " next iterator " << endl;
        while ( ! sb . m_quitting . load() )
        {
            KLockAcquire ( sb . m_searchQueueLock );
            SearchBuffer :: Match * m = it -> NextMatch ();
            KLockUnlock ( sb . m_searchQueueLock );
            if ( m == 0 )
            {
                break;
            }
            // cout << "Thread " << (void*)self << " next match " << endl;
            sb . m_output . Push ( m );
        }

        delete it;
    }
    // cout << "Thread " << (void*)self << " finished " << endl;

    sb . m_output . ProducerDone();
    return 0;
}

void
VdbSearch :: FormatMatch ( const SearchBuffer :: Match & p_source, Match & p_result )
{
    p_result . m_fragmentId = p_source . m_fragmentId;
    if ( m_settings . m_fasta )
    {
        p_result . m_formatted = string ( ">" ) + p_result . m_fragmentId + "\n";

        size_t start = 0;
        const size_t totalBases = p_source . m_bases . length ();
        while ( start < totalBases )
        {
            p_result . m_formatted += p_source . m_bases . substr ( start, m_settings . m_fastaLineLength ) + "\n";
            start += m_settings . m_fastaLineLength;
        }
    }
    else
    {   // by default, simply the Id of the fragment
        p_result . m_formatted = p_source . m_fragmentId;
    }
}

bool
VdbSearch :: NextMatch ( Match & p_match )
{
    if ( m_settings . m_maxMatches != 0 && m_matchCount >= m_settings . m_maxMatches )
    {
        return false;
    }

    if ( m_output == 0 ) // first call to NextMatch() - set up worker threads
    {
        size_t threadNum = m_settings . m_threads;

        if ( ! m_settings . m_useBlobSearch && threadNum > m_searches . size () )
        {   // in thread-per-accession mode, no need for more threads than there are accessions
            threadNum = m_searches . size ();
        }

        m_output = new OutputQueue ( threadNum );
        m_searchBlock = new SearchThreadBlock ( m_searches, *m_output );
        for ( unsigned  int i = 0 ; i != threadNum; ++i )
        {
            KThread* t;
            rc_t rc = KThreadMakeStackSize ( & t, ThreadPerIterator, m_searchBlock, 16*1024*1024 );
            if ( rc != 0 )
            {
                throw ( ErrorMsg ( "KThreadMake failed" ) );
            }
            m_threadPool . push_back ( t );
        }
    }

    // block until a result appears on the output queue or all searches are marked as completed
    SearchBuffer :: Match * m = m_output -> Pop ();
    if ( m != 0 )
    {
        ++m_matchCount;
        FormatMatch ( * m, p_match );
        delete m;
        return true;
    }
    return false;
}

//////////////////// VdbSearch :: SearchBlockFactory

VdbSearch :: SearchBlockFactory :: SearchBlockFactory ( const VdbSearch :: Settings& p_settings )
:   m_settings ( p_settings )
{
}

SearchBlock*
VdbSearch :: SearchBlockFactory :: MakeSearchBlock () const
{
    switch ( m_settings . m_algorithm )
    {
        case VdbSearch :: FgrepDumb:
            return new FgrepSearch ( m_settings . m_query, FgrepSearch :: FgrepDumb );
        case VdbSearch :: FgrepBoyerMoore:
            return new FgrepSearch ( m_settings . m_query, FgrepSearch :: FgrepBoyerMoore );
        case VdbSearch :: FgrepAho:
            return new FgrepSearch ( m_settings . m_query, FgrepSearch :: FgrepAho );

        case VdbSearch :: AgrepDP:
            return new AgrepSearch ( m_settings . m_query, AgrepSearch :: AgrepDP, m_settings . m_minScorePct );
        case VdbSearch :: AgrepWuManber:
            return new AgrepSearch ( m_settings . m_query, AgrepSearch :: AgrepWuManber, m_settings . m_minScorePct );
        case VdbSearch :: AgrepMyers:
            return new AgrepSearch ( m_settings . m_query, AgrepSearch :: AgrepMyers, m_settings . m_minScorePct );
        case VdbSearch :: AgrepMyersUnltd:
            return new AgrepSearch ( m_settings . m_query, AgrepSearch :: AgrepMyersUnltd, m_settings . m_minScorePct );

        case VdbSearch :: NucStrstr:
            return new NucStrstrSearch ( m_settings . m_query, m_settings . m_isExpression, m_settings . m_useBlobSearch );

        case VdbSearch :: SmithWaterman:
            return new SmithWatermanSearch ( m_settings . m_query, m_settings . m_minScorePct );

        default:
            throw ( ErrorMsg ( "SearchBlockFactory: unsupported algorithm" ) );
    }
}
