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

Formatter::Format
Formatter::StringToFormat( const string & value )
{
    string lowercase = value;
    std::transform(
        lowercase.begin(),
        lowercase.end(),
        lowercase.begin(),
        [](unsigned char c){ return tolower(c); }
    );
    if ( lowercase == "csv" ) return CSV;
    if ( lowercase == "xml" ) return XML;
    if ( lowercase == "json" ) return Json;
    if ( lowercase == "tab" ) return Tab;
    throw VDB::Error( string("Invalid value for the --format option: ") + value );
}

Formatter::Formatter( Format f, uint32_t l )
: fmt( f ), limit( l )
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
Formatter::format( const SraInfo::SpotLayouts & layouts, SraInfo::Detail ) const
{
    size_t count = layouts.size();
    if ( limit != 0 && limit < count )
    {
        count = limit;
    }

    switch ( fmt )
    {
    case Default:
        {
            ostringstream ret;
            for( size_t i = 0; i < count; ++i )
            {
                const SraInfo::SpotLayout & l = layouts[i];
                bool  first = true;
                ret << l.count << ( l.count == 1 ? " spot: " : " spots: " );
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
                    ret << r.type << "(length=" << r.length << ")";
                }
                ret << endl;
            }
            return ret.str();
        }

    case Json:
        {
            ostringstream ret;
            ret << "[" << endl;
            bool  first_layout = true;
            for( size_t i = 0; i < count; ++i )
            {
                const SraInfo::SpotLayout & l = layouts[i];
                if ( first_layout )
                {
                    first_layout = false;
                }
                else
                {
                    ret << "," << endl;
                }

                bool  first_read = true;
                ret << "{ \"count\": " << l.count << ", \"reads\": [";
                for ( auto r : l.reads )
                {
                    if ( first_read )
                    {
                        first_read = false;
                    }
                    else
                    {
                        ret << ", ";
                    }
                    ret << "{ \"type\": \"" << r.type << "\", \"length\": " << r.length << " }";
                }
                ret << "] }";
            }
            ret << endl << "]" << endl;
            return ret.str();
        }

    case CSV:
        {
            ostringstream ret;
            for( size_t i = 0; i < count; ++i )
            {
                const SraInfo::SpotLayout & l = layouts[i];
                ret << l.count << ", ";
                for ( auto r : l.reads )
                {
                    ret << r.type << ", " << r.length << ", ";
                }
                ret << endl;
            }
            return ret.str();
        }

    case Tab:
        {
            ostringstream ret;
            for( size_t i = 0; i < count; ++i )
            {
                const SraInfo::SpotLayout & l = layouts[i];
                ret << l.count << "\t";
                for ( auto r : l.reads )
                {
                    ret << r.type << "\t" << r.length << "\t";
                }
                ret << endl;
            }
            return ret.str();
        }

    case XML:
        {
            ostringstream ret;
            for( size_t i = 0; i < count; ++i )
            {
                const SraInfo::SpotLayout & l = layouts[i];
                ret << "<layout><count>" << l.count << "</count>";
                for ( auto r : l.reads )
                {
                    ret << "<read><type>" << r.type << "</type><length>" << r.length << "</length></read>";
                }
                ret << "</layout>" << endl;
            }
            return ret.str();
        }

    default:
        throw VDB::Error( "unsupported formatting option");
    }

}
