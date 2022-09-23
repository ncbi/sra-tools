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

#ifndef _sra_tools_hpp_ngs_pileup_
#define _sra_tools_hpp_ngs_pileup_

#include <klib/defs.h>

#include <string>
#include <vector>

namespace ngs
{
    class Reference;
}

class NGS_Pileup
{
public:
    struct Settings
    {
        struct ReferenceSlice
        {
            ReferenceSlice( const std::string& p_name ) /* entire reference */
            :   m_name ( p_name ), 
                m_firstPos ( 0 ),
                m_lastPos ( 0 ),
                m_full ( true )
            {
            }
            ReferenceSlice( const std::string& p_name, 
                            int64_t p_firstPos, 
                            int64_t p_lastPos )
            :   m_name ( p_name ), 
                m_firstPos ( p_firstPos ),
                m_lastPos ( p_lastPos ),
                m_full ( false )
            {
            }
            
            std::string m_name;
            int64_t     m_firstPos; 
            int64_t     m_lastPos;
            bool        m_full;
        };
        
        void AddInput ( const std::string& accession ) { inputs . push_back ( accession ); }
        void AddReference ( const std::string& commonOrCanonicalName );
        void AddReferenceSlice ( const std::string& commonOrCanonicalName, 
                                 int64_t firstPos, 
                                 int64_t lastPos );
                                 
                                 
        typedef std::vector < std::string > Inputs;
        typedef std::vector < ReferenceSlice > References;
        
        Inputs inputs;
        std::ostream* output;
        References references;
    };
    
public:
    NGS_Pileup ( const Settings& p_settings );
    
    void Run () const;
    
private:
    struct TargetReference;
    class TargetReferences;
    
    Settings            m_settings;
};

#endif
