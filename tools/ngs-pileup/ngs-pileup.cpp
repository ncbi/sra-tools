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

using namespace std;

struct NGS_Pileup::TargetReference
{
    typedef pair < int64_t, int64_t >       Slice;
    typedef vector < Slice >                Slices;
    typedef vector < ngs :: Reference >     Targets;
    typedef vector < ngs :: PileupIterator> Pileups;
    
    string  m_canonicalName;
    Slices  m_slices;
    Targets m_targets;
    Pileups m_pileups;
    bool    m_complete;
    
    TargetReference ( ngs :: Reference p_ref )
    : m_canonicalName ( p_ref . getCanonicalName() ), m_complete ( true )
    {
        AddReference ( p_ref );
    }
    TargetReference ( ngs :: Reference p_ref, 
                      int64_t p_first, 
                      int64_t p_last )
    : m_canonicalName ( p_ref . getCanonicalName() ), m_complete ( false )
    {
        AddReference ( p_ref );
    }
    ~TargetReference ()
    {
    }
    
    void AddSlice ( int64_t p_first, int64_t p_last )
    {
    }
    void MakeComplete ()
    {
        m_complete = true;
        m_slices . clear();
    }
    
    void AddReference ( ngs :: Reference p_ref )
    {
        m_targets. push_back ( p_ref );
    }
    
    void Process ( ostream& out )
    {
        int64_t firstPos = 0;
        int64_t lastPos = m_targets . front () . getLength () - 1;

        // create pileup iterators 
        for ( Targets::iterator i = m_targets.begin(); i != m_targets.end(); ++i ) 
        {
            m_pileups . push_back ( i -> getPileups ( ngs::Alignment::all ) );
        }
        
        int64_t curPos = firstPos;
        while ( curPos <= lastPos ) 
        {
            uint32_t total_depth = 0;
            for ( Pileups :: iterator i = m_pileups . begin (); i != m_pileups. end (); ++i )
            {
                bool next = i -> nextPileup ();
                assert ( next );
                total_depth += i -> getPileupDepth ();
            }
        
            if ( total_depth > 0 )
            {
                out << m_canonicalName
                    << '\t' << ( curPos + 1 ) // convert to 1-based position to emulate samtools
                    << '\t' << total_depth
                    << endl;
            }
            
            ++ curPos;
        }
    }
};

class NGS_Pileup::TargetReferences : public vector < TargetReference >
{
public :
    void AddComplete ( ngs :: Reference ref )
    {
        string name = ref . getCanonicalName ();
        for ( iterator i = begin(); i != end (); ++ i )
        {   
            if ( i -> m_canonicalName == name )
            {
                i -> AddReference ( ref );
                return;
            }
        }
        // not found - add new reference
        push_back ( TargetReference ( ref ) );
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
        if ( i->m_name == ref . getCanonicalName () || i->m_name == ref . getCommonName () )
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
    for ( Settings :: Inputs :: const_iterator i = m_settings . inputs . begin(); 
          i != m_settings . inputs . end (); 
          ++i )
    {   
        ngs :: ReadCollection col = ncbi :: NGS :: openReadCollection ( *i );
        ngs :: ReferenceIterator refIt = col . getReferences ();
        while ( refIt . nextReference () )
        {
            if ( m_settings . references . empty () ) // all references requested
            {
                /* need to create a Reference object that is not attached to the iterator, so as
                    it is not invalidated on the next call to refIt.NextReference() */
                references . AddComplete ( col . getReference ( refIt. getCommonName () ) );
            }
            else if ( FindReference ( m_settings . references, refIt ) )
            {
                //TODO: handle slices
                references . AddComplete ( col . getReference ( refIt. getCommonName () ) );
            }
        }
    }
    
    ostream & out ( m_settings . output != (ostream*)0 ? * m_settings . output : cout );
    
    // walk the references and output pileups
    for ( TargetReferences :: iterator i = references . begin(); i != references . end (); ++i )
    {   
        i -> Process ( out );
    }
}

//// NGS_Pileup::Settings

void 
NGS_Pileup::Settings::AddReference ( const string& commonOrCanonicalName ) 
{
    references . push_back ( ReferenceSlice ( commonOrCanonicalName ) ); 
}

void 
NGS_Pileup::Settings::AddReferenceSlice ( const string& commonOrCanonicalName, 
                                        int64_t firstPos, 
                                        int64_t lastPos )
{ 
    references . push_back ( ReferenceSlice ( commonOrCanonicalName, firstPos, lastPos ) ); 
}

