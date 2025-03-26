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

#include "sra-info.hpp"

class Formatter
{
public:
    typedef enum {
        Default,
        CSV,
        XML,
        Json,
        Tab
    } Format;
    static Format StringToFormat( const std::string & value );

public:
    Formatter( Format = Default, uint32_t limit = 0 );
    virtual ~Formatter();

    Format getFormat( void ) const { return m_fmt; }

    std::string start( void ) const;
    std::string end( void ) const;

    std::string format( const SraInfo::Platforms & ) const;
    std::string format( const std::string &, const std::string & = "") const;
    std::string format( const SraInfo::SpotLayouts &, SraInfo::Detail ) const;
    std::string format( const VDB::SchemaInfo & ) const;
    std::string format( const KDBContents &, SraInfo::Detail ) const;
    std::string format( const SraInfo::Fingerprints &, SraInfo::Detail ) const;

private:
    std::string formatJsonSeparator( void ) const;
    void expectSingleQuery( const std::string & error ) const;

    std::string FormatContentNodeDefault( const std::string & ident, const KDBContents & cont, SraInfo::Detail, bool root ) const;
    std::string FormatContentNodeJson( const std::string & ident, const KDBContents & cont, SraInfo::Detail, bool root ) const;
    std::string FormatContentRootDefault( const std::string & p_indent, const KDBContents & cont ) const;
    std::string FormatContentRootJson( const std::string & indent, const KDBContents & cont ) const;

    void CountContents( const KDBContents & cont, unsigned int& tables, unsigned int& withChecksums, unsigned int& withoutChecksums, unsigned int& indices ) const;

    Format m_fmt;
    uint32_t m_limit;
    bool m_first;
    int m_count;
};
