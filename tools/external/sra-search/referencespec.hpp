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

#ifndef _hpp_reference_spec_
#define _hpp_reference_spec_

#include <string>
#include <vector>
#include <stdint.h>
#include <cassert>

struct ReferenceSpec
{
    ReferenceSpec ( const std :: string& p_name ) // full
    :   m_name ( p_name ),
        m_start ( 0 ),
        m_end ( 0 ),
        m_full ( true )
    {
    }

    ReferenceSpec ( const std :: string& p_name, uint64_t p_start, uint64_t p_end ) // slice [ p_start .. p_end )
    :   m_name ( p_name ),
        m_start ( p_start ),
        m_end ( p_end ),
        m_full ( false )
    {
        assert ( false ); // not suported yet
    }

    std::string m_name;
    uint64_t m_start;
    uint64_t  m_end;
    bool m_full;
};

typedef std :: vector < ReferenceSpec > ReferenceSpecs;

#endif

