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

#include "fragmentmatchiterator.hpp"

#include <ncbi/NGS.hpp>

#include "searchbuffer.hpp"

using namespace std;
using namespace ngs;

///////////////////// FragmentMatchIterator

class FragmentMatchIterator : public MatchIterator
{
public:
    FragmentMatchIterator ( SearchBlock :: Factory & p_factory, ngs::ReadCollection p_run, bool p_unalignedOnly = false )
    :   MatchIterator ( p_factory, p_run . getName () ),
        m_readIt ( p_run . getReads ( Read :: all ) ),
        m_sb ( p_factory . MakeSearchBlock () )
    {
        m_readIt . nextRead ();
    }

    ~ FragmentMatchIterator()
    {
        delete m_sb;
    }

    virtual SearchBuffer :: Match * NextMatch ()
    {
        do
        {
            while ( m_readIt . nextFragment () )
            {
                // report one match per fragment
                StringRef bases = m_readIt . getFragmentBases ();
                if ( m_sb -> FirstMatch ( bases . data (), bases . size () ) )
                {
                    return new SearchBuffer :: Match ( m_accession, m_readIt . getFragmentId () . toString (), bases . toString () );
                }
            }
        }
        while ( m_readIt . nextRead () );
        return 0;
    }

private:
    ngs::ReadIterator   m_readIt;
    SearchBlock *       m_sb;
};

///////////////////// UnalignedFragmentMatchIterator

UnalignedFragmentMatchIterator :: UnalignedFragmentMatchIterator ( SearchBlock :: Factory & p_factory, const ngs::ReadCollection & m_run )
:   MatchIterator ( p_factory, m_run . getName () ),
    m_readIt ( m_run . getReads ( ( ngs :: Read :: ReadCategory ) ( ngs :: Read :: unaligned | ngs :: Read :: partiallyAligned ) ) ),
    m_sb ( p_factory . MakeSearchBlock () )
{
    m_readIt . nextRead ();
}

UnalignedFragmentMatchIterator :: ~ UnalignedFragmentMatchIterator()
{
    delete m_sb;
}

SearchBuffer :: Match *
UnalignedFragmentMatchIterator :: NextMatch ()
{
    do
    {
        while ( m_readIt . nextFragment () )
        {
            if ( ! m_readIt . isAligned () )
            {
                // report one match per fragment
                StringRef bases = m_readIt . getFragmentBases ();
                if ( m_sb -> FirstMatch ( bases . data (), bases . size () ) )
                {
                    return new SearchBuffer :: Match ( m_accession, m_readIt . getFragmentId () . toString (), bases . toString () );
                }
            }
        }
    }
    while ( m_readIt . nextRead () );
    return 0;
}

///////////////////// FragmentSearch

FragmentSearch :: FragmentSearch ( SearchBlock :: Factory & p_factory, const std::string & p_accession, bool p_unalignedOnly )
: m_iter ( 0 )
{
    ngs::ReadCollection coll ( ncbi :: NGS :: openReadCollection ( p_accession ) );

    if ( p_unalignedOnly )
    {
        m_iter = new UnalignedFragmentMatchIterator ( p_factory, coll );
    }
    else
    {
        m_iter = new FragmentMatchIterator ( p_factory, coll );
    }
}

FragmentSearch :: ~ FragmentSearch ()
{
    delete m_iter;
}

MatchIterator *
FragmentSearch :: NextIterator ()
{   // fragments can only be processed sequentially, so there is just 1 iterator to return
    if ( m_iter != 0  )
    {
        MatchIterator * ret = m_iter;
        m_iter = 0;
        return ret;
    }
    return 0;
}
