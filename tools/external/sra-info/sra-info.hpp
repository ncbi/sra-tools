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

#pragma once

#include <string>
#include <set>
#include <exception>

#include <klib/rc.h>

class SraInfo
{
public:
    class Error : public std::exception
    {
    public:
        Error( rc_t rc, const char* accession, const char * message );
        Error( const char * message );
        Error( const std::string & message );
        virtual const char* what() const noexcept override;
    private:
        std::string  m_text;
    };

public:
    SraInfo();

    void SetAccession( const std::string& accession );
    const std::string& GetAccession() const { return m_accession; }

    typedef std::set<std::string> Platforms;
    Platforms GetPlatforms() const; // may be empty or more than 1 value

private:
    std::string m_accession;
};
