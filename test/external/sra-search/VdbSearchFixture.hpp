/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author'm_s official duties as a United States Government employee and
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

#pragma once

#include <string>
#include <stdexcept>

#include "vdb-search.hpp"

class VdbSearchFixture
{
public:
    VdbSearchFixture ()
    : m_s ( 0 )
    {
        VdbSearch :: logResults = false;
    }
    ~VdbSearchFixture ()
    {
        delete m_s;
    }

    void LogResults()
    {
        VdbSearch :: logResults = true;
    }

    void Setup ( const std::string& p_query, VdbSearch :: Algorithm p_algorithm, const std::string& p_accession = std::string() )
    {
        m_settings . m_algorithm = p_algorithm;
        m_settings . m_query = p_query;
        if ( ! p_accession . empty () )
        {
            m_settings . m_accessions . push_back ( p_accession );
        }
        delete m_s;
        m_s = new VdbSearch ( m_settings );
    }

    void SetupSingleThread ( const std::string& p_query, VdbSearch :: Algorithm p_algorithm, const std::string& p_accession = std::string() )
    {
        m_settings . m_threads = 1;
        Setup ( p_query, p_algorithm, p_accession );
    }

    void SetupMultiThread ( const std::string& p_query, VdbSearch :: Algorithm p_algorithm, unsigned int p_threads, const std::string& p_accession = std::string() )
    {
        m_settings . m_threads = p_threads;
        m_settings . m_useBlobSearch = true;
        Setup ( p_query, p_algorithm, p_accession );
    }

    const std::string & NextFragmentId ()
    {
        if ( ! NextMatch () )
        {
            throw std::logic_error ( "VdbSearchFixture::NextFragmentId : NextMatch() failed" );
        }
        return m_result . m_fragmentId;
    }

    bool NextMatch ()
    {
        return m_s -> NextMatch ( m_result );
    }

    VdbSearch :: Settings   m_settings;
    VdbSearch*              m_s;
    VdbSearch :: Match      m_result;
};
