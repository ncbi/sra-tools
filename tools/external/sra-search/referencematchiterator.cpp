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

#include "referencematchiterator.hpp"

#include <iostream>
#include <stdexcept>

#include <klib/log.h>

#include <kproc/lock.h>

#include <insdc/insdc.h>

#include <ncbi/NGS.hpp>
#include <ngs/Reference.hpp>

#include <ngs-vdb/VdbReference.hpp>

#include "searchbuffer.hpp"
#include "fragmentmatchiterator.hpp"

using namespace std;
using namespace ngs;
using namespace ncbi::ngs::vdb;

static
string
ReverseComplementDNA ( const string& p_source)
{
    // the conversion table has been copied from ncbi-vdb/libs/loader/common-writer.c
    static const INSDC_dna_text complement [] = {
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 , '.',  0 ,
        '0', '1', '2', '3',  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C',
        'D',  0 ,  0 , 'M',  0 , 'K', 'N',  0 ,
         0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W',
         0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,
         0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0 ,  0
    };

    string ret;
    ret.reserve ( p_source . size () );
    for ( string :: const_reverse_iterator i = p_source . rbegin (); i != p_source . rend (); ++i )
    {
        char ch = complement [ (unsigned int)*i ];
        if ( ch == 0 )
        {
            throw invalid_argument ( string ( "Unexpected character in query:'" ) + *i + "'" );
        }
        ret += ch;
    }
    return ret;
}

class ReferenceSearchBase : public SearchBuffer
{
public:
    ReferenceSearchBase( SearchBlock *                          p_sb,
                         ReadCollection                         p_run,
                         Reference                              p_reference,
                         ReferenceSearch :: ReportedFragments & p_reported,
                         KLock *                                p_lock )
    :   SearchBuffer ( p_sb, p_run . getName() ),
        m_run ( p_run ),
        m_reference ( p_reference ),
        m_dbLock ( p_lock ),
        m_refSearch ( 0 ),
        m_refSearchReverse ( 0 ),
        m_alIt ( 0 ),
        m_fragIt ( 0 ),
        m_reported ( p_reported ),
        BlobBoundaryOverlap ( m_searchBlock -> GetQuery () . size() * 2 ),
        m_reverse ( false )
    {
        KLockAddRef ( m_dbLock );

        const unsigned int ReferenceMatchTolerancePct = 90; // search on reference has to be looser than in reads
        const unsigned int ThresholdPct = ( m_searchBlock -> GetScoreThreshold() * ReferenceMatchTolerancePct ) / 100;
        m_refSearch         = new AgrepSearch ( m_searchBlock -> GetQuery (),                           AgrepSearch :: AgrepMyersUnltd, ThresholdPct );
        m_refSearchReverse  = new AgrepSearch ( ReverseComplementDNA ( m_searchBlock -> GetQuery () ),  AgrepSearch :: AgrepMyersUnltd, ThresholdPct );
    }

    virtual ~ReferenceSearchBase()
    {
        KLockRelease ( m_dbLock );

        delete m_refSearch;
        delete m_refSearchReverse;
        delete m_alIt;
        delete m_fragIt;
    }

    virtual string BufferId () const
    {
        return m_reference . getCanonicalName ();
    }

    StringRef CurrentFragmentId ()
    {
        assert ( m_fragIt != 0 );
        return m_fragIt -> getFragmentId ();
    }
    StringRef CurrentFragmentBases ()
    {
        assert ( m_fragIt != 0 );
        KLockAcquire ( m_dbLock );
        StringRef ret = m_fragIt -> getFragmentBases ();
        KLockUnlock ( m_dbLock );
        return ret;
    }

protected:
    bool NextFragment ()
    {
        while ( true ) // for each alignment on the slice
        {
            if ( m_fragIt == 0 )
            {   // start searching alignment on the matched slice
                KLockAcquire ( m_dbLock );
                bool hasNextAl = m_alIt -> nextAlignment();
                if ( ! hasNextAl )
                {
                    delete m_alIt;
                    m_alIt = 0;
                    KLockUnlock ( m_dbLock );
                    break;
                }

                m_fragIt = new FragmentIterator ( m_run . getRead ( m_alIt -> getReadId () . toString() ) );   //TODO: there may be a shortcut to get to the fragment's bases
                KLockUnlock ( m_dbLock );
            }

            if ( m_fragIt != 0 )
            {
                while ( m_fragIt -> nextFragment () ) // foreach fragment
                {
                    StringRef fragBases = CurrentFragmentBases ();

                    // cout << "Searching " << CurrentFragmentId . toString ( << "'" << fragBases . toString () << "'" << endl;
                    if ( m_searchBlock -> FirstMatch ( fragBases . data (), fragBases . size () ) ) // this search is with the original score threshold
                    {
                        KLockAcquire ( m_dbLock );
                        string id = CurrentFragmentId () . toString ();
                        if ( m_reported . find ( id ) == m_reported.end () )
                        {
                            // cout << "Found " << id << endl;
                            m_reported . insert ( id );
                            KLockUnlock ( m_dbLock );
                            return true;
                        }
                        KLockUnlock ( m_dbLock );
                    }
                }
                // no (more) matches on this read
                delete m_fragIt;
                m_fragIt = 0;
            }
        }
        return false;
    }

protected:
    ReadCollection  m_run;
    Reference       m_reference;

    KLock*          m_dbLock;

    // loose search on the reference
    SearchBlock *   m_refSearch;
    SearchBlock *   m_refSearchReverse;

    AlignmentIterator * m_alIt;
    FragmentIterator *  m_fragIt;

    ReferenceSearch :: ReportedFragments & m_reported; // all fragments reported for the parent ReferenceMatchIterator, to eliminate double reports

    const size_t    BlobBoundaryOverlap;

    bool m_reverse;
};

//////////////////// ReferenceSearchBuffer

class ReferenceSearchBuffer : public ReferenceSearchBase
{
public:
    ReferenceSearchBuffer ( SearchBlock *                           p_sb,
                            ReadCollection                          p_run,
                            Reference                               p_reference,
                            ReferenceSearch :: ReportedFragments &  p_reported,
                            KLock *                                 p_lock )
    :   ReferenceSearchBase ( p_sb, p_run, p_reference, p_reported, p_lock ),
        m_start ( 0 ),
        m_bases ( m_reference.getReferenceBases ( 0 ) ),
        m_offset ( 0 )
    {
        if ( m_reference . getIsCircular () )
        {   // append the start of the reference to be used in search for wraparound matches
            m_bases += m_bases . substr ( 0, BlobBoundaryOverlap );
        }
    }

    virtual SearchBuffer :: Match * NextMatch ()
    {
        while ( true )  // for each match on the reference
        {
            if ( m_alIt == 0 ) // start searching at m_offset
            {
                //cout << "searching at " << m_offset << endl;
                uint64_t hitStart;
                uint64_t hitEnd;
                if ( ! m_reverse )
                {
                    if ( ! m_refSearch -> FirstMatch ( m_bases . data () + m_offset, m_bases . size () - m_offset, & hitStart, & hitEnd ) )
                    {   // no more matches on this reference; switch to reverse search
                        m_reverse = true;
                        m_offset = 0;
                        continue;
                    }
                }
                else
                {
                    if ( ! m_refSearchReverse -> FirstMatch ( m_bases . data () + m_offset, m_bases . size () - m_offset, & hitStart, & hitEnd ) )
                    {   // no more reverse matches on this reference; the end.
                        return 0;
                    }
                }
                // cout << "Match on " << BufferId () << " at " << ( m_start + m_offset + hitStart ) << "-" << ( m_start + m_offset + hitEnd ) << ( m_refSearch == 0 ? " (reverse)" : "" ) << endl;
                m_alIt = new AlignmentIterator ( m_reference . getAlignmentSlice ( ( int64_t ) ( m_start + m_offset + hitStart ), hitEnd - hitStart ) );
                m_offset += hitEnd; //TODO: this may be too far
            }

            if ( NextFragment () )
            {
                return new Match ( m_accession, CurrentFragmentId () . toString (), CurrentFragmentBases () . toString () );
            }
        }
    }

private:
    uint64_t        m_start;
    String          m_bases;
    uint64_t        m_offset;
};

//////////////////// ReferenceBlobSearchBuffer

class ReferenceBlobSearchBuffer : public ReferenceSearchBase
{
public:
    ReferenceBlobSearchBuffer ( SearchBlock *                           p_sb,
                                ReadCollection                          p_run,
                                Reference                               p_reference,
                                ReferenceSearch :: ReportedFragments &  p_reported,
                                KLock *                                 p_lock )
    :   ReferenceSearchBase ( p_sb, p_run, p_reference, p_reported, p_lock ),
        m_startInBlob ( 0 ),
        m_blobIter ( VdbReference ( p_reference ) . getBlobs() ),
        m_curBlob ( 0 ),
        m_nextBlob ( 0 ),
        m_offsetInReference ( 0 ),
        m_offsetInBlob ( 0 ),
        m_lastBlob ( false )
    {
        SetupFirstBlob();
    }

    ~ReferenceBlobSearchBuffer ()
    {
    }

    virtual SearchBuffer :: Match * NextMatch ()
    {
        if ( m_bases . size () == 0 )
        {
            return 0;
        }

        while ( true ) // for each blob
        {
            KLockAcquire ( m_dbLock );
            m_unpackedBlobSize = m_curBlob . UnpackedSize ();
            KLockUnlock ( m_dbLock );

            m_reverse = false;
            m_offsetInBlob = 0;

            while ( true )  // for each match in the blob
            {
                if ( m_alIt == 0 ) // start searching at m_offsetInBlob
                {
                    uint64_t hitStart;
                    uint64_t hitEnd;
                    if ( ! m_reverse )
                    {
                        // int64_t first;
                        // uint64_t count;
                        // m_curBlob . GetRowRange ( & first, & count );
                        // cout << (void*)this << " searching at " << m_offsetInReference + m_offsetInBlob
                        //     << " blob size=" << m_bases.size() << " unpacked=" << m_unpackedBlobSize
                        //     << " rows=" <<  first << "-" << ( first + count - 1) << endl;

                        if ( ! m_refSearch -> FirstMatch ( m_bases . data () + m_offsetInBlob, m_bases . size () - m_offsetInBlob, & hitStart, & hitEnd ) )
                        {   // no more matches in this blob; switch to reverse search
                            m_reverse = true;
                            m_offsetInBlob = m_startInBlob;
                            continue;
                        }
                    }
                    else if ( ! m_refSearchReverse -> FirstMatch ( m_bases . data () + m_offsetInBlob, m_bases . size () - m_offsetInBlob, & hitStart, & hitEnd ) )
                    {   // no more reverse matches on this reference; the end for this blob
                        break;
                    }

                    if ( m_offsetInBlob + hitStart >= m_curBlob.Size() ) // the hit is entirely in the next blob, we'll get it on the next iteration
                    {
                        if ( m_reverse )
                        {
                            break;
                        }
                        else
                        {
                            m_reverse = true;
                            m_offsetInBlob = m_startInBlob;
                            continue;
                        }
                    }

                    // cout << "Match on " << BufferId () << " '" << string ( m_bases . data () + m_offsetInBlob + hitStart, hitEnd - hitStart ) << "'" <<
                    //         " at " << ( m_offsetInReference + m_offsetInBlob + hitStart ) << "-" << ( m_offsetInReference + m_offsetInBlob + hitEnd ) << ( m_reverse ? " (reverse)" : "" ) <<
                    //         " in blob " << ( m_offsetInBlob + hitStart ) <<
                                    // endl;
                    uint64_t inReference;
                    uint32_t repeatCount;
                    uint64_t increment;
                    // cout << (void*)this << " Resolving " << m_offsetInBlob + hitStart << endl;
                    m_curBlob . ResolveOffset ( m_offsetInBlob + hitStart, & inReference, & repeatCount, & increment );
                    // cout << (void*)this << " Resolved to  " << inReference << " repeat=" << repeatCount << " inc=" << increment << endl;
                    if ( repeatCount > 1 )
                    {
                        PLOGMSG (klogWarn, (klogWarn, "Match against a repeated reference row detected; some matching fragments may be missed. Reference=$(r) pos=$(p) repeat=$(t)",
                                                      "r=%s,p=%d,t=%d",
                                                      m_reference . getCanonicalName () . c_str(), inReference, repeatCount));
                    }
                    m_offsetInBlob += hitEnd; //TODO: this may be too far
                    m_alIt = new AlignmentIterator ( m_reference . getAlignmentSlice ( ( int64_t ) inReference, hitEnd - hitStart ) );
                }

                if ( NextFragment () )
                {
                    return new Match ( m_accession, CurrentFragmentId () . toString (), CurrentFragmentBases () . toString () );
                }
            }

            m_offsetInReference += m_unpackedBlobSize;

            if ( m_lastBlob )
            {
                return 0;
            }

            m_curBlob = m_nextBlob;

            AdvanceBases ();
        }
    }

private:
    bool SetupFirstBlob()
    {
        if ( ! m_blobIter . hasMore () )
        {
            return false;
        }

        m_curBlob = m_blobIter. nextBlob();

        if ( m_reference . getIsCircular () )
        {   // save the start of the reference to be used in search for wraparound matches
            m_circularStart = m_reference . getReferenceBases ( 0, BlobBoundaryOverlap );
        }

        AdvanceBases ();
        return true;
    }

    void AdvanceBases ()
    {
        m_bases . reserve ( m_curBlob . Size() + BlobBoundaryOverlap ); // try to minimize re-allocation
        m_bases . assign ( m_curBlob . Data(), m_curBlob . Size() );
        if ( m_blobIter . hasMore () )
        {   // append querySize bases from the beginning of the next blob, to catch matches across the two blobs' boundary
            m_nextBlob = m_blobIter. nextBlob ();
            m_bases . append ( m_nextBlob . Data(), BlobBoundaryOverlap );
        }
        else
        {
            m_lastBlob = true;
            if ( m_reference . getIsCircular () )
            {
                m_bases . append ( m_circularStart );
            }
        }
    }

private:
    uint64_t                m_startInBlob;

    ReferenceBlobIterator   m_blobIter;
    ReferenceBlob           m_curBlob;
    ReferenceBlob           m_nextBlob;

    // we search a blob plus BlobBoundaryOverlap bases from the next blob, all copied into this buffer
    String                  m_bases;

    String                  m_circularStart; // this represent next-to-last blob's starting bases for circular references

    uint64_t                m_offsetInReference;
    uint64_t                m_offsetInBlob;
    uint64_t                m_unpackedBlobSize;
    bool                    m_lastBlob;
};

//////////////////// ReferenceMatchIterator
class ReferenceMatchIterator : public MatchIterator
{
public:
    ReferenceMatchIterator ( SearchBlock :: Factory &               p_factory,
                             ReadCollection                         p_run,
                             Reference                              p_reference,
                             bool                                   p_blobSearch,
                             ReferenceSearch :: ReportedFragments & p_reported,
                             KLock *                                  p_lock )
    :   MatchIterator ( p_factory, p_run . getName () ),
        m_buffer ( 0 )
    {
        if ( p_blobSearch )
        {
            m_buffer = new ReferenceBlobSearchBuffer ( m_factory . MakeSearchBlock(),
                                                    p_run,
                                                    p_reference,
                                                    p_reported,
                                                    p_lock );
        }
        else
        {
            m_buffer = new ReferenceSearchBuffer ( m_factory . MakeSearchBlock(),
                                                p_run,
                                                p_reference,
                                                p_reported,
                                                p_lock );
        }
    }

    virtual ~ReferenceMatchIterator ()
    {
        delete m_buffer;
    }

    virtual SearchBuffer :: Match * NextMatch ()
    {
        SearchBuffer :: Match * ret = 0;
        if ( m_buffer != 0 )
        {
            ret = m_buffer -> NextMatch ();
            if ( ret == 0 )
            {
                delete m_buffer;
                m_buffer = 0;
            }
        }
        return ret;
    }

private:
    SearchBuffer * m_buffer;
};

//////////////////// ReferenceSearch

ReferenceSearch :: ReferenceSearch ( SearchBlock :: Factory &   p_factory,
                                     const std :: string &      p_accession,
                                     const ReferenceSpecs &     p_references,
                                     bool                       p_blobSearch )
:   m_factory ( p_factory ),
    m_run ( ncbi :: NGS :: openReadCollection ( p_accession ) ),
    m_references ( p_references ),
    m_blobSearch ( p_blobSearch ),
    m_unalignedDone ( false )
{
    if ( m_references . empty () )
    {   // search all references if none specified
        ReferenceIterator refIter = m_run . getReferences ();
        while ( refIter . nextReference () )
        {
            m_references . push_back ( refIter . getCanonicalName () );
        }
    }
    m_refIt = m_references . begin ();

    rc_t rc = KLockMake ( & m_accessionLock );
    if ( rc != 0 )
    {
        throw ( ErrorMsg ( "KLockMake failed" ) );
    }
}

ReferenceSearch :: ~ ReferenceSearch ()
{
    KLockRelease ( m_accessionLock );
}

MatchIterator *
ReferenceSearch :: NextIterator ()
{   // split by reference + unaligned
    while ( m_refIt != m_references . end () )
    {
        // cout << "Searching on " << m_refIt -> m_name << endl;
        try
        {
            MatchIterator * ret = new ReferenceMatchIterator ( m_factory, m_run, m_run . getReference ( m_refIt -> m_name ), m_blobSearch, m_reported, m_accessionLock );
            ++ m_refIt;
            return ret;
        }
        catch ( ngs :: ErrorMsg & ex )
        {
            const string NotFoundMsg = "Reference not found";
            if ( string ( ex . what (), NotFoundMsg . size () ) == NotFoundMsg )
            {
                ++ m_refIt;
                continue;
            }
            throw;
        }
    }

    if ( m_references . empty () && ! m_unalignedDone ) // unaligned fragments are not searched if reference spec is not empty
    {
        m_unalignedDone = true; // only return unaligned iteartor once
        //TODO: make sure this iterator uses CMP_READ
        return new UnalignedFragmentMatchIterator ( m_factory, m_run );
    }

    return 0;
}
