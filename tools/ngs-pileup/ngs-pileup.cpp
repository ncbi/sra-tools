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

#include "ngs-pileup.hpp"

#include <iostream>

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/PileupIterator.hpp>

struct NGS_Pileup::TargetReference
{
    typedef std :: vector < ngs :: PileupIterator > Iterators;
    
    std :: string   canonicalName;
    uint64_t        length;
    Iterators       iterators;
    
    void Process ( std::ostream& out )
    {
        for ( uint64_t pos = 0; pos < length; ++ pos )
        {
            uint32_t total_depth = 0;
            for ( Iterators :: iterator i = iterators . begin (); i != iterators. end (); ++i )
            {
                if ( ! i -> nextPileup () )
                {
                    /*TODO: make sure that this is the first pileup in the container and all other pileups are finished too */
std::cout << "Stopped at pos=" << pos << std::endl;        
                    break;
                }
                total_depth += i -> getPileupDepth ();
            }
        
            if ( total_depth > 0 )
            {
                out << canonicalName
                    << '\t' << ( pos + 1 ) // convert to 1-based position to emulate samtools
                    << '\t' << total_depth
                    << std :: endl;
            }
        }
    }
};

class NGS_Pileup::TargetReferences : public std :: vector < TargetReference >
{
public :
    void AddReference ( ngs :: Reference ref )
    {
        ngs :: PileupIterator pileups = ref . getPileups ( ngs :: Alignment :: all );
        
        // find the container of iterators for this reference 
        std :: string name = ref . getCanonicalName ();
        for ( iterator i = begin(); i != end (); ++ i )
        {
            if ( i -> canonicalName == name )
            {
                i -> iterators . push_back ( pileups );
                return;
            }
        }
        // not found - add new reference
        TargetReference newRef;
        newRef . canonicalName = name;
        newRef . length = ref . getLength ();
        newRef . iterators . push_back ( pileups );
        push_back ( newRef );
    }
};
 
NGS_Pileup::NGS_Pileup ( const Settings& p_settings )
: m_settings( p_settings )
{
}

static
bool FindReference ( const NGS_Pileup :: Settings :: References & requested, const ngs :: Reference & ref )
{
    for ( NGS_Pileup :: Settings :: References :: const_iterator i = requested . begin(); 
          i != requested . end (); 
          ++i )
    {   
        if ( *i == ref . getCanonicalName () || *i == ref . getCommonName () )
        {
            return true;
        }
    }
    return false;
}
    
void 
NGS_Pileup::Run () const
{
    TargetReferences references;
    
    // build the set of target references
    for ( Settings :: Inputs :: const_iterator i = m_settings . inputs . begin(); i != m_settings . inputs . end (); ++i )
    {   
        ngs :: ReadCollection col = ncbi :: NGS :: openReadCollection ( *i );
        ngs :: ReferenceIterator refIt = col . getReferences ();
        while ( refIt . nextReference () )
        {
            // all references are requested, or the one we are looking at is requested
            if ( m_settings . references . empty () || FindReference ( m_settings . references, refIt ) )
            {
                /* need to create a Reference object that is not attached to the iterator, so as
                    it is not invalidated on the next call to refIt.NextReference() */
                references . AddReference ( col . getReference ( refIt. getCommonName () ) );
            }
        }
    }
    
    std :: ostream & out ( m_settings . output != (std::ostream*)0 ? * m_settings . output : std :: cout );
    
    // walk the references and output pileups
    for ( TargetReferences :: iterator i = references . begin(); i != references . end (); ++i )
    {   
        i -> Process ( out );
    }
}
