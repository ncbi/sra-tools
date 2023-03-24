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

#include "formatter.hpp"

#include <algorithm>

using namespace std;

    typedef enum {
        Default,
        CSV,
        XML,
        Json,
        Piped,
        Tab
    } Format;
Formatter::Format
Formatter::StringToFormat( const string & value )
{
    string lowercase = value;
    std::transform(
        lowercase.begin(),
        lowercase.end(),
        lowercase.begin(),
        [](unsigned char c){ return std::tolower(c); }
    );
    if ( lowercase == "csv" ) return CSV;
    if ( lowercase == "xml" ) return XML;
    if ( lowercase == "json" ) return Json;
    if ( lowercase == "piped" ) return Piped;
    if ( lowercase == "tab" ) return Tab;
    throw VDB::Error( string("Invalid value for the --format option: ") + value );
}

Formatter::Formatter( Format f )
: fmt( f )
{
}

Formatter::~Formatter()
{
}

string
JoinPlatforms( const SraInfo::Platforms & platforms,
               const string & separator,
               const string & prefix = string(),
               const string & suffix = string()
)
{
    string ret;
    bool first = true;
    for( auto p : platforms )
    {
        if ( ! first )
        {
            ret += separator;
        }
        ret+= prefix + p + suffix;
        first = false;
    }
    return ret;
}

string
Formatter::format( const SraInfo::Platforms & platforms ) const
{
    switch ( fmt )
    {
    case Default:
    case Piped:
        // default format, 1 value per line
        return JoinPlatforms( platforms, "\n" );
    case CSV:
        // CSV, all values on 1 line
        return JoinPlatforms( platforms, "," );
    case XML:
        // XML, each value in a tag, one per line
        return JoinPlatforms( platforms, "\n", "<platform>", "</platform>" );
    case Json:
        // Json, array of strings
        return string("[\n") + JoinPlatforms( platforms, ",\n", "\"", "\"" ) + "\n]";
    case Tab:
        // Tabbed, all values on 1 line
        return JoinPlatforms( platforms, "\t" );
    default:
        throw VDB::Error( "unsupported formatting option");
    }
}

string
Formatter::format( const string & value ) const
{
    switch ( fmt )
    {
    case Default:
    case Piped:
    case CSV:
    case XML:
    case Tab:
        return value;
    case Json:
        return string("\"") + value + "\"";
    default:
        throw VDB::Error( "unsupported formatting option");
    }
}

string
Formatter::format( const SraInfo::SpotLayouts & layouts ) const
{
    ostringstream ret;
    for( auto l : layouts )
    {
        bool  first = true;
        for ( auto r : l.reads )
        {
            if ( first )
            {
                first = false;
            }
            else
            {
                ret << ", ";
            }
            ret << r.type << "(" << r.length << ")";
        }
        ret << ":" << l.count << endl;
    }
    return ret.str();
}
