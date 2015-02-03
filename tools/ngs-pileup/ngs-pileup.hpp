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
        typedef std::vector < std::string > Inputs;
        typedef std::vector < std::string > References;
        
        Inputs inputs;
        std::ostream* output;
        References references;
        
        void AddInput ( const std::string& accession ) { inputs . push_back ( accession ); }
        void AddReference ( const std::string& commonOrCanonicalName ) { references . push_back ( commonOrCanonicalName ); }
        void AddReferenceSlice ( const std::string& commonOrCanonicalName, int64_t start, uint64_t length ) 
        { 
            references . push_back ( commonOrCanonicalName ); 
        }
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
