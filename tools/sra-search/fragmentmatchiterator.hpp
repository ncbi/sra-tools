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

#ifndef _hpp_fragment_match_iterator_
#define _hpp_fragment_match_iterator_

#include <ngs/ReadCollection.hpp>
#include "threadablesearch.hpp"
#include "matchiterator.hpp"

class SearchBuffer;

// Searches fragment by fragment
// returns 1 iterator for the entire SEQUENCE table
class FragmentSearch : public ThreadableSearch
{
public:
    FragmentSearch ( SearchBlock :: Factory & p_factory, const std::string & p_accession, bool p_unalignedOnly = false );

    virtual ~ FragmentSearch ();

    virtual MatchIterator * NextIterator ();

private:
    MatchIterator * m_iter;
};

class UnalignedFragmentMatchIterator : public MatchIterator
{
public:
    UnalignedFragmentMatchIterator ( SearchBlock :: Factory & p_factory, const ngs::ReadCollection & p_run );
    virtual ~UnalignedFragmentMatchIterator ();

    virtual SearchBuffer :: Match * NextMatch ();

private:
    ngs::ReadIterator   m_readIt;
    SearchBlock *       m_sb;
};



#endif
