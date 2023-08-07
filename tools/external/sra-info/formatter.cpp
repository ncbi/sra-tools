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
Formatter::start( void ) const
{
    switch ( fmt )
    {
    case Default:
    case CSV:
    case Tab:
        return "";
    case XML:
        return "<SRA_INFO>\n";
    case Json:
        return "{";
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
Formatter::format( const SraInfo::SpotLayouts & layouts, SraInfo::Detail detail ) const
{
    ostringstream ret;

    size_t count = layouts.size();
    if ( limit != 0 && limit < count )
    {
        count = limit;
    }

    switch ( fmt )
    {
    case Default:
        {
            bool first_group = true;
            for( size_t i = 0; i < count; ++i )
            {
                if ( first_group )
                {
                    first_group = false;
                }
                else
                {
                    ret << endl;
                }

                const SraInfo::SpotLayout & l = layouts[i];
                bool  first = true;
                ret << l.count << ( l.count == 1 ? " spot: " : " spots: " );
                switch( detail )
                {
                case SraInfo::Short: ret << l.reads.size() << " reads"; break;
                case SraInfo::Abbreviated:
                    for ( auto r : l.reads )
                    {
                        ret << r.Encode( detail );
                    }
                    break;
                default:
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
                        ret << r.Encode( detail );
                    }
                }
            }
        }
        break;

    case Json:
        {
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
                ret << "{ \"count\": " << l.count << ", \"reads\": ";

                switch( detail )
                {
                case SraInfo::Short: ret << l.reads.size(); break;
                case SraInfo::Abbreviated:
                    ret << "\"";
                    for ( auto r : l.reads )
                    {
                        ret << r.Encode( detail );
                    }
                    ret << "\"";
                    break;
                default:
                    ret << "[";
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
                        ret << "{ \"type\": \"" << r.TypeAsString(detail) << "\", \"length\": " << r.length << " }";
                    }
                    ret << "]";
                }

                ret << " }";
            }
            ret << endl << "]" << endl;
        }
        break;

    case CSV:
        for( size_t i = 0; i < count; ++i )
        {
            const SraInfo::SpotLayout & l = layouts[i];
            switch( detail )
            {
            case SraInfo::Short:
                ret << l.count << ", " << l.reads.size() << ", ";
                break;
            case SraInfo::Abbreviated:
                ret << l.count << ", ";
                for ( auto r : l.reads )
                {
                    ret << r.Encode(detail);
                }
                break;
            default:
                ret << l.count << ", ";
                for ( auto r : l.reads )
                {
                    ret << r.TypeAsString(detail) << ", " << r.length << ", ";
                }
            }
            ret << endl;
        }
        break;

    case Tab:
        for( size_t i = 0; i < count; ++i )
        {
            const SraInfo::SpotLayout & l = layouts[i];
            switch( detail )
            {
            case SraInfo::Short:
                ret << l.count << "\t" << l.reads.size() << "\t";
                break;
            case SraInfo::Abbreviated:
                ret << l.count << "\t";
                for ( auto r : l.reads )
                {
                    ret << r.Encode(detail);
                }
                break;
            default:
                ret << l.count << "\t";
                for ( auto r : l.reads )
                {
                    ret << r.TypeAsString(detail)  << "\t" << r.length << "\t";
                }
            }
            ret << endl;
        }
        break;

    case XML:
        for( size_t i = 0; i < count; ++i )
        {
            const SraInfo::SpotLayout & l = layouts[i];
            ret << "<layout><count>" << l.count << "</count>";
            switch( detail )
            {
            case SraInfo::Short:
                ret << "<reads>" << l.reads.size() << "</reads>";
                break;
            case SraInfo::Abbreviated:
                ret << "<reads>";
                for ( auto r : l.reads )
                {
                    ret << r.Encode(detail);
                }
                ret << "</reads>";
                break;
            default:
                for ( auto r : l.reads )
                {
                    ret << "<read><type>" << r.TypeAsString(detail) << "</type><length>" << r.length << "</length></read>";
                }
            }

            ret << "</layout>" << endl;
        }
        break;

    default:
        throw VDB::Error( "unsupported formatting option");
    }

    return ret.str();
}

string Formatter::format(const VDB::SchemaInfo & info) const
{
    string space(" ");
    int indent(3);
    bool first(true);
    string out;

    switch ( fmt )
    {
    case Default:
        indent = 2;
        out = "SCHEMA\n"

            + space + "DBS\n";
        for (auto it = info.db.begin(); it < info.db.end(); it++)
            (*it).print(space, indent, out);

        out += space + "TABLES\n";
        for (auto it = info.table.begin(); it < info.table.end(); it++)
            (*it).print(space, indent, out);

        out += space + "VIEWS\n";
        for (auto it = info.view.begin(); it < info.view.end(); it++)
            (*it).print(space, indent, out);

        break;

    case Json:
        out = "{\n"
            + space + "\"SCHEMA\": {\n"

            + space + space + "\"DBS\": [\n";
        first = true;
        for (auto it = info.db.begin(); it < info.db.end(); it++) {
            (*it).print(indent, space, out, first,
                "{ \"Db\": \"", ",\n", "\"", "}",
                ",\n", " \"Parents\": [\n", "\n", "]\n", "\n");
            first = false;
        }
        out += space + space + "],\n"

            + space + space + "\"TABLES\": [\n";
        first = true;
        for (auto it = info.table.begin(); it < info.table.end(); it++) {
            (*it).print(indent, space, out, first,
                "{ \"Tbl\": \"", ",\n", "\"", "}",
                ",\n", " \"Parents\": [\n", "\n", "]\n", "\n");
            first = false;
        }
        out += "\n"
            + space + space + "],\n"

            + space + space + "\"VIEWS\": [\n";
        first = true;
        for (auto it = info.view.begin(); it < info.view.end(); it++) {
            (*it).print(indent, space, out, first,
                "{ \"View\": \"", ",\n", "\"", "}",
                ",\n", " \"Parents\": [\n", "\n", "]\n", "\n");
            first = false;
        }
        out += space + space + "]\n"

            + space + "}\n"
            + "}";
        break;

    case XML:
      //  out = "<SRA_INFO>\n"
        //    + space +
        out = "<SCHEMA>\n"

            + space + space + "<DBS>\n";
        for (auto it = info.db.begin(); it < info.db.end(); it++)
            (*it).print(indent, space, out, true,
                "<Db>", "", "\n", "</Db>\n");
        out += space + space + "</DBS>\n"

            + space + space + "<TABLES>\n";
        for (auto it = info.table.begin(); it < info.table.end(); it++)
            (*it).print(indent, space, out, true,
                "<Tbl>", "", "\n", "</Tbl>\n");
        out += space + space + "</TABLES>\n" 

            + space + space + "<VIEWS>\n";
        for (auto it = info.view.begin(); it < info.view.end(); it++)
            (*it).print(indent, space, out, true,
                "<View>", "", "\n", "</View>\n");
        out += space + space + "</VIEWS>\n"

        //   + space
            + "</SCHEMA>"//\n"
            //+ "</SRA_INFO>"
            ;
        break;

    default:
        throw VDB::Error( "unsupported formatting option for schema" );
    }

    return out;
}
