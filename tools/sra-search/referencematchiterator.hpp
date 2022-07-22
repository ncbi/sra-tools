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

#ifndef _hpp_reference_match_iterator_
#define _hpp_reference_match_iterator_

#include <set>

#include "searchblock.hpp"
#include "referencespec.hpp"
#include "threadablesearch.hpp"
#include "fragmentmatchiterator.hpp"

struct KLock;

// searches reference by reference
// each iterator returned by NextIterator() is bound to a single reference
class ReferenceSearch : public ThreadableSearch
{
public:
    typedef std :: set < std :: string > ReportedFragments;

public:
    ReferenceSearch ( SearchBlock :: Factory &   p_factory,
                      const std :: string &      p_accession,
                      const ReferenceSpecs &     p_references = ReferenceSpecs(),
                      bool                       p_blobSearch = false );

    virtual ~ ReferenceSearch ();

    virtual MatchIterator * NextIterator ();

private:
    SearchBlock :: Factory &            m_factory;
    ngs :: ReadCollection               m_run;
    ReferenceSpecs                      m_references;
    ReferenceSpecs :: const_iterator    m_refIt;

    struct KLock*           m_accessionLock;
    ReportedFragments       m_reported; // used to eliminate double reports
    bool                    m_blobSearch;
    bool                    m_unalignedDone;
};

#endif
