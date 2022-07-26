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

#ifndef _hpp_searchbuffer_
#define _hpp_searchbuffer_

#include <string>
#include <ngs/Fragment.hpp>
#include "searchblock.hpp"

// A buffer that can be searched for one or more matches, bound to an accession and an engine-side algorithm represented by a SearchBlock
// Subclasses implement fragment-based (1 hit) or blob-based (multiple hits) search
class SearchBuffer
{
public:
    struct Match
    {
        Match( const std :: string & p_accession, const std :: string & p_fragmentId, const std :: string & p_bases )
        :   m_accession ( p_accession ),
            m_fragmentId ( p_fragmentId ),
            m_bases ( p_bases )
        {
        }

        std :: string   m_accession;
        std :: string   m_fragmentId;
        std :: string   m_bases;
    };

public:
    SearchBuffer ( SearchBlock * p_sb, const std :: string & p_accession )
    :   m_searchBlock ( p_sb ),
        m_accession ( p_accession )
    {
    }

    virtual ~SearchBuffer()
    {
        delete m_searchBlock;
    }

    // the result is owned by the caller (destroy with delete())
    // NULL if no more matches
    virtual Match * NextMatch () = 0;

    const std :: string & AccessionName () const { return m_accession; }

protected:
    SearchBlock *   m_searchBlock;  // owned here
    std::string     m_accession;
};

#endif
