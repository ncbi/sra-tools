/*==============================================================================
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
#include <string_view>

using namespace std;

Formatter::Format
Formatter::StringToFormat( const string & value )
{
    string lowercase = value;
    std::transform(
        lowercase.begin(),
        lowercase.end(),
        lowercase.begin(),
        [](unsigned char c){ return (char)tolower(c); }
    );
    if ( lowercase == "csv" ) return CSV;
    if ( lowercase == "xml" ) return XML;
    if ( lowercase == "json" ) return Json;
    if ( lowercase == "tab" ) return Tab;
    throw VDB::Error( string("Invalid value for the --format option: ") + value );
}

Formatter::Formatter( Format f, uint32_t l )
: m_fmt( f ), m_limit( l ), m_first ( true ), m_count( 0 )
{
}

Formatter::~Formatter()
{
}

string
Formatter::formatJsonSeparator( void ) const
{
    if ( m_first ) {
        Formatter * ncThis = const_cast<Formatter *>(this);
        ncThis->m_first = false;
        return "";
    }
    else
        return ",";
}

void
Formatter::expectSingleQuery( const string & error ) const
{
    Formatter * ncThis = const_cast<Formatter *>(this);
    if ( ++ncThis->m_count > 1 )
        throw VDB::Error( error );
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
    switch ( m_fmt )
    {
    case Default:
        // default format, 1 value per line
        return JoinPlatforms( platforms, "\n", "PLATFORM: " );
    case CSV:
        // CSV, all values on 1 line
        expectSingleQuery( "CSV format does not support multiple queries" );
        return JoinPlatforms( platforms, "," );
    case XML:
        // XML, each value in a tag, one per line
        return " <PLATFORMS>\n" +
            JoinPlatforms( platforms, "\n", "  <platform>", "</platform>" )
            + "\n </PLATFORMS>";
    case Json:
    {
        // Json, array of strings
        string out;
        const string separator(formatJsonSeparator());
        if (!separator.empty())
            out = " " + separator + "\n";
        out += string(" \"PLATFORMS\": [\n")
            + JoinPlatforms( platforms, ",\n", "  \"", "\"" ) + "\n ]";
        return out;
    }
    case Tab:
        // Tabbed, all values on 1 line
        expectSingleQuery("TAB format does not support multiple queries");
        return JoinPlatforms( platforms, "\t" );
    default:
        throw VDB::Error( "unsupported formatting option");
    }
}

string
Formatter::start( void ) const
{
    switch ( m_fmt )
    {
    case Default:
    case CSV:
    case Tab:
        return "";
    case XML:
        return "<SRA_INFO>";
    case Json:
        return "{";
    default:
        throw VDB::Error( "unsupported formatting option");
    }
}

string
Formatter::end( void ) const
{
    switch ( m_fmt )
    {
    case Default:
    case CSV:
    case Tab:
        return "";
    case XML:
        return "</SRA_INFO>";
    case Json:
        return "}";
    default:
        throw VDB::Error( "unsupported formatting option");
    }
}

string
Formatter::format( const string & value, const string & name ) const
{
    const string space(" ");

    switch ( m_fmt )
    {
    case CSV:
        expectSingleQuery("CSV format does not support multiple queries");
        return value;
    case Tab:
        expectSingleQuery( "TAB format does not support multiple queries" );
        return value;
    case Default:
        return name + ": " + value;
    case XML:
        return space + "<" + name + ">"
            + value + "</" + name + ">";
    case Json:
    {
        string out;
        const string separator(formatJsonSeparator());
        if (!separator.empty())
            out = space + separator + "\n";
        out += space + "\"" + name + string("\": \"") + value + "\"";
        return out;
    }
    default:
        throw VDB::Error( "unsupported formatting option");
    }
}

class SimpleSchemaDataFormatter : public VDB::SchemaDataFormatter {
    const std::string _space;
    int _indent;
public:
    SimpleSchemaDataFormatter(const std::string &space = "  ", int indent = 1) :
        _space(space), _indent(indent)
    {}
    void format(
        const struct VDB::SchemaData &d, int indent = -1, bool /*first*/ = true)
    {
        if (indent < 0)
            indent = _indent;
        for (int i = 0; i < indent; ++i)
            out += _space;
        out += d.name + "\n";
        for (auto it = d.parent.begin(); it < d.parent.end(); ++it)
            format(*it, indent + 1);
    }
};

class FullSchemaDataFormatter : public VDB::SchemaDataFormatter {
    const std::string _space;
    int _indent;
    const std::string _open;
    const std::string _openNext;
    const std::string _closeName;
    const std::string _close;
    const std::string _openParent1;
    const std::string _openParent2;
    const std::string _closeParent1;
    const std::string _closeParent2;
    const std::string _noParent;

public:
    FullSchemaDataFormatter(int indent, const std::string &space,
        const std::string &open, const std::string &openNext,
        const std::string &closeName, const std::string &close,
        const std::string &openParent1 = "",
        const std::string &openParent2 = "",
        const std::string &closeParent1 = "",
        const std::string &closeParent2 = "",
        const std::string &noParent = "")
        :
        _space(space), _indent(indent), _open(open), _openNext(openNext),
        _closeName(closeName), _close(close), _openParent1(openParent1),
        _openParent2(openParent2), _closeParent1(closeParent1),
        _closeParent2(closeParent2), _noParent(noParent)
    {}

    void format(const struct VDB::SchemaData &d,
        int indent = -1, bool first = true)
    {
        if (indent < 0)
            indent = _indent;
        if (!first) out += _openNext;

        for (int i = 0; i < indent; ++i) out += _space;
        out += _open + d.name + _closeName;

        if (!d.parent.empty()) {
            out += _openParent1;
            if (!_openParent2.empty()) {
                for (int i = 0; i < indent; ++i) out += _space;
                out += _openParent2;
                ++indent;
            }

            bool frst = true;
            for (auto it = d.parent.begin(); it < d.parent.end(); it++) {
                format(*it, indent + 1, frst);
                frst = false;
            }

            if (!_closeParent2.empty()) {
                out += _closeParent1;
                for (int i = 0; i < indent; ++i) out += _space;
                out += _closeParent2;
            }
            if (!_openParent2.empty())
                --indent;
        }
        else
            out += _noParent;

        for (int i = 0; i < indent; ++i) out += _space;
        out += _close;
    }
};

class JsonSchemaDataFormatter : public FullSchemaDataFormatter {
public:
    JsonSchemaDataFormatter(
        const std::string &open, int indent, const std::string &space = " "
    )
        : FullSchemaDataFormatter(indent, space, open, ",\n", "\"", "}", ",\n",
            " \"Parents\": [\n", "\n", "]\n", "\n")
    {}
};

class XmlSchemaDataFormatter : public FullSchemaDataFormatter {
public:
    XmlSchemaDataFormatter(int indent, const std::string &open,
        const std::string &close, const std::string &space = " "
    )
        : FullSchemaDataFormatter(indent, space, open, "", "\n", close)
    {}
};

string
Formatter::format( const SraInfo::SpotLayouts & layouts, SraInfo::Detail detail ) const
{
    ostringstream ret;

    size_t cnt = layouts.size();
    if ( m_limit != 0 && m_limit < cnt)
    {
        cnt = m_limit;
    }

    switch ( m_fmt )
    {
    case Default:
        {
            bool first_group = true;
            for( size_t i = 0; i < cnt; ++i )
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
                ret << "SPOT: " << l.count
                    << ( l.count == 1 ? " spot: " : " spots: " );
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
            const string separator(formatJsonSeparator());
            if (!separator.empty())
                ret << " " << separator << endl;
            ret << " \"SPOTS\": [" << endl;
            bool  first_layout = true;
            for( size_t i = 0; i < cnt; ++i )
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
                ret << "  { \"count\": " << l.count << ", \"reads\": ";

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
            ret << endl << " ]" << endl;
        }
        break;

    case CSV:
        expectSingleQuery( "CSV format does not support multiple queries" );
        for( size_t i = 0; i < cnt; ++i )
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
        expectSingleQuery("TAB format does not support multiple queries");
        for( size_t i = 0; i < cnt; ++i )
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
        ret << " <SPOTS>" << endl;
        for( size_t i = 0; i < cnt; ++i )
        {
            const SraInfo::SpotLayout & l = layouts[i];
            ret << "  <layout><count>" << l.count << "</count>";
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
        ret << " </SPOTS>";
        break;

    default:
        throw VDB::Error( "unsupported formatting option");
    }

    return ret.str();
}

string Formatter::format(const VDB::SchemaInfo & info) const
{
    string space("  ");
    int indent(3);
    bool first(true);
    string out;

    switch ( m_fmt )
    {
    case Default:
    {
        indent = 2;
        out = "SCHEMA:\n"

            + space + "DBS:\n";
        SimpleSchemaDataFormatter db(space, indent);
        for (auto it = info.db.begin(); it < info.db.end(); it++)
            db.format(*it);
        out += db.out;

        out += space + "TABLES:\n";
        SimpleSchemaDataFormatter table(space, indent);
        for (auto it = info.table.begin(); it < info.table.end(); it++)
            table.format(*it);
        out += table.out;

        out += space + "VIEWS:\n";
        SimpleSchemaDataFormatter view(space, indent);
        for (auto it = info.view.begin(); it < info.view.end(); it++)
            view.format(*it);
        out += view.out;

        break;
    }

    case Json:
    {
        const string separator(formatJsonSeparator());
        if (!separator.empty())
            out = space + separator + "\n";
        out += space + "\"SCHEMA\": {\n"

            + space + space + "\"DBS\": [\n";
        JsonSchemaDataFormatter db("{ \"Db\": \"", indent);
        first = true;
        for (auto it = info.db.begin(); it < info.db.end(); it++) {
            db.format(*it, indent, first);
            first = false;
        }
        out += db.out;
        out += space + space + "],\n"

            + space + space + "\"TABLES\": [\n";
        JsonSchemaDataFormatter table("{ \"Tbl\": \"", indent);
        first = true;
        for (auto it = info.table.begin(); it < info.table.end();
            ++it)
        {
            table.format(*it, indent, first);
            first = false;
        }
        out += table.out;
        out += "\n"
            + space + space + "],\n"

            + space + space + "\"VIEWS\": [\n";
        first = true;
        JsonSchemaDataFormatter view("{ \"View\": \"", indent);
        for (auto it = info.view.begin(); it < info.view.end(); it++) {
            view.format(*it, indent, first);
            first = false;
        }
        out += view.out;
        out += space + space + "]\n"

            + space + "}";
        break;
    }

    case XML:
    {
        out = space + "<SCHEMA>\n"

            + space + space + "<DBS>\n";
        XmlSchemaDataFormatter db(indent, "<Db>", "</Db>\n");
        for (auto it = info.db.begin(); it < info.db.end(); it++)
            db.format(*it);
        out += db.out;
        out += space + space + "</DBS>\n"

            + space + space + "<TABLES>\n";
        XmlSchemaDataFormatter table(indent, "<Tbl>", "</Tbl>\n");
        for (auto it = info.table.begin(); it < info.table.end(); it++)
            table.format(*it);
        out += table.out;
        out += space + space + "</TABLES>\n"

            + space + space + "<VIEWS>\n";
        XmlSchemaDataFormatter view(indent, "<View>", "</View>\n");
        for (auto it = info.view.begin(); it < info.view.end(); it++)
            view.format(*it);
        out += view.out;
        out += space + space + "</VIEWS>\n"

            + space + "</SCHEMA>";

        break;
    }

    default:
        throw VDB::Error( "unsupported formatting option for schema" );
    }

    return out;
}

static const string IndentUnit = string( 4, ' ' );

static
string
formatJsonNamed( const string & name, const string & value )
{
    return "\"" + name + "\":\"" + value + "\"";
}

static
string
formatJsonBool( const string & name, bool value )
{
    return "\"" + name + "\":" + (value? "true":"false") ;
}

static
string
formatContentFlagJson( const string & indent, const string & name, bool value )
{
    return ",\n" + indent + formatJsonBool( name, value );
}

static
string
formatContentNamedJson( const string & indent, const string & name, const string & value )
{
    return ",\n" + indent + formatJsonNamed( name, value );
}

void
Formatter::CountContents( const KDBContents & cont, unsigned int& tables, unsigned int& withChecksums, unsigned int& withoutChecksums, unsigned int& indices ) const
{
    switch ( cont.dbtype )
    {
    case kptTable:
        tables ++;
        break;
    case kptDatabase:
        break;
    case kptColumn:
        if ( ( cont.attributes & cca_HasChecksum_CRC ) ||
             ( cont.attributes & cca_HasChecksum_MD5 ) )
        {
            withChecksums ++;
        }
        else
        {
            withoutChecksums ++;
        }
        break;
    case kptIndex:
        indices ++;
        break;
    default:
        break;
    }
    if ( cont.firstChild != nullptr )
    {
        CountContents( * cont.firstChild, tables, withChecksums, withoutChecksums, indices );
    }
    if ( cont.nextSibling != nullptr )
    {
        CountContents( * cont.nextSibling, tables, withChecksums, withoutChecksums, indices );
    }
}

string
Formatter::FormatContentRootJson( const string & p_indent, const KDBContents & cont ) const
{
    string out;

    unsigned int tables = 0;
    unsigned int withChecksums = 0;
    unsigned int withoutChecksums = 0;
    unsigned int indices = 0;
    CountContents( cont, tables, withChecksums, withoutChecksums, indices );
    if ( cont.dbtype == kptDatabase )
    {
        out += formatContentNamedJson( p_indent, "tables", to_string(tables) );
    }
    out += formatContentNamedJson( p_indent, "columnsWithChecksums", to_string(withChecksums) );
    out += formatContentNamedJson( p_indent, "columnsWithoutChecksums", to_string(withoutChecksums) );
    out += formatContentNamedJson( p_indent, "indices", to_string(indices) );

    return out;
}

string
Formatter::FormatContentNodeJson( const string & p_indent, const KDBContents & cont, SraInfo::Detail detail, bool root ) const
{
    string out = p_indent + R"("name":")"+string(cont.name) + "\"";
    string type;
    switch ( cont.dbtype )
    {
    case kptTable:      type = "table"; break;
    case kptDatabase:   type = "database"; break;
    case kptColumn:     type = "column"; break;
    case kptIndex:      type = "index"; break;
    default: type = to_string( cont.dbtype ); break;
    }
    out += formatContentNamedJson( p_indent, "dbtype", type );

    switch ( detail )
    {
    case SraInfo::Short:
        out += FormatContentRootJson( p_indent, cont );
        return out; // not displaying children/siblings

    case SraInfo::Abbreviated:
        if ( root )
        {
            out += FormatContentRootJson( p_indent, cont );
        }

        break;

    case SraInfo::Full:
    case SraInfo::Verbose:
    default:
        type.clear();
        switch ( cont.fstype )
        {
        case kptFile:   type = "file"; break;
        case kptDir:    type = "directory"; break;
        default: break;
        }
        if ( ! type.empty() )
        {
            out += formatContentNamedJson( p_indent, "fstype", type );
        }

        if ( root )
        {
            out += FormatContentRootJson( p_indent, cont );
        }

        out += formatContentFlagJson( p_indent, "hasMetadata", cont.attributes & cca_HasMetadata );
        out += formatContentFlagJson( p_indent, "hasMD5_File", cont.attributes & cca_HasMD5_File );
        out += formatContentFlagJson( p_indent, "isLocked", cont.attributes & cca_HasLock );
        out += formatContentFlagJson( p_indent, "isSealed", cont.attributes & cca_HasSealed );
        out += formatContentFlagJson( p_indent, "hasErrors", cont.attributes & cca_HasErrors );
        switch ( cont.dbtype )
        {
        case kptTable:
            out += formatContentFlagJson( p_indent, "hasColumns", cont.attributes & cta_HasColumns );
            out += formatContentFlagJson( p_indent, "hasIndices", cont.attributes & cta_HasIndices );
            break;
        case kptDatabase:
            out += formatContentFlagJson( p_indent, "hasTables", cont.attributes & cda_HasTables );
            out += formatContentFlagJson( p_indent, "hasDatabases", cont.attributes & cda_HasDatabases );
            break;
        case kptColumn:
            out += formatContentFlagJson( p_indent, "hasChecksum_CRC", cont.attributes & cca_HasChecksum_CRC );
            out += formatContentFlagJson( p_indent, "hasChecksum_MD5", cont.attributes & cca_HasChecksum_MD5 );
            out += formatContentFlagJson( p_indent, "reversedByteOrder", cont.attributes & cca_ReversedByteOrder );
            out += formatContentFlagJson( p_indent, "isStatic", cont.attributes & cca_IsStatic );
            break;
        case kptIndex:
            out += formatContentFlagJson( p_indent, "isTextIndex", cont.attributes & cia_IsTextIndex );
            out += formatContentFlagJson( p_indent, "isIdIndex", cont.attributes & cia_IsIdIndex );
            out += formatContentFlagJson( p_indent, "reverseByteOrder", cont.attributes & cia_ReversedByteOrder );
            break;
        }
    }

    if ( cont.firstChild != nullptr )
    {
        const string in = p_indent + IndentUnit;
        out += ",\n" + p_indent + R"("children":[{)" + "\n" + FormatContentNodeJson( in, * cont.firstChild, detail, false ) + "\n" + in + "}\n" + p_indent + "]";
    }
    if ( cont.nextSibling != nullptr )
    {
        out += "\n" + p_indent + "},\n" + p_indent + "{\n" + FormatContentNodeJson( p_indent, * cont.nextSibling, detail, false );
    }

    return out;
}

string
Formatter::FormatContentRootDefault( const string & indent, const KDBContents & cont ) const
{
    string out;

    unsigned int tables = 0;
    unsigned int withChecksums = 0;
    unsigned int withoutChecksums = 0;
    unsigned int indices = 0;
    CountContents( cont, tables, withChecksums, withoutChecksums, indices );
    if ( cont.dbtype == kptDatabase )
    {
        out += indent + to_string(tables) + (tables == 1 ? " table" : " tables") + "\n";
    }
    out += indent + to_string(withChecksums) + (withChecksums == 1 ? " column":" columns") + " with checksums\n";
    out += indent + to_string(withoutChecksums) + (withoutChecksums == 1 ? " column":" columns") + " without checksums\n";
    out += indent + to_string(indices) + (indices == 1 ? " index" : " indices") + "\n";

    return out;
}

string
Formatter::FormatContentNodeDefault( const string & p_indent, const KDBContents & cont, SraInfo::Detail detail, bool root ) const
{
    string out = p_indent + string(cont.name) + ":";
    string indent = p_indent + IndentUnit;
    string type;
    switch ( cont.dbtype )
    {
    case kptTable:      type = "table"; break;
    case kptDatabase:   type = "database"; break;
    case kptColumn:     type = "column"; break;
    case kptIndex:      type = "index"; break;
    default: type = to_string( cont.dbtype ); break;
    }

    switch ( detail )
    {
    case SraInfo::Short:
        {
            out += "\n" + indent + type + "\n";
            out += FormatContentRootDefault( indent, cont );
        }
        return out; // not visiting children/siblings

    case SraInfo::Abbreviated:
        {
            out += type + "\n";
            if ( root )
            {
                out += FormatContentRootDefault( indent, cont );
            }
        }
        break;

    case SraInfo::Full:
    case SraInfo::Verbose:
    default:
        out += type;
        type.clear();
        switch ( cont.fstype )
        {
        case kptFile:   type = ", file"; break;
        case kptDir:    type = ", directory"; break;
        default: break;
        }
        out += type + "\n";

        if ( root )
        {
            out += FormatContentRootDefault( indent, cont );
        }

        if ( cont.attributes & cca_HasMetadata )    out += indent + "has metadata\n";
        if ( cont.attributes & cca_HasMD5_File )    out += indent + "has MD5 file\n";
        if ( cont.attributes & cca_HasLock )        out += indent + "is locked\n";
        if ( cont.attributes & cca_HasSealed )      out += indent + "is sealed\n";
        if ( cont.attributes & cca_HasErrors )      out += indent + "has errors\n";
        switch ( cont.dbtype )
        {
        case kptTable:
            if ( cont.attributes & cta_HasColumns ) out += indent + "has columns\n";
            if ( cont.attributes & cta_HasIndices ) out += indent + "has indices\n";
            break;
        case kptDatabase:
            if ( cont.attributes & cda_HasTables )      out += indent + "has tables\n";
            if ( cont.attributes & cda_HasDatabases )   out += indent + "has databases\n";
            break;
        case kptColumn:
            if ( cont.attributes & cca_HasChecksum_CRC )    out += indent + "has CRC checksum\n";
            if ( cont.attributes & cca_HasChecksum_MD5 )    out += indent + "has MD5 checksum\n";
            if ( cont.attributes & cca_ReversedByteOrder )  out += indent + "has reverse byte order\n";
            if ( cont.attributes & cca_IsStatic )           out += indent + "is static\n";
            break;
        case kptIndex:
            if ( cont.attributes & cia_IsTextIndex )        out += indent + "is on text\n";
            if ( cont.attributes & cia_IsIdIndex )          out += indent + "is on ID\n";
            if ( cont.attributes & cia_IsIdIndex )          out += indent + "has reverse byte order\n";
            break;
        }

        break;
    }

    if ( cont.firstChild != nullptr )
    {
        out += FormatContentNodeDefault( indent, * cont.firstChild, detail, false );
    }
    if ( cont.nextSibling != nullptr )
    {
        out += FormatContentNodeDefault( p_indent, * cont.nextSibling, detail, false );
    }

    return out;
}

string
Formatter::format( const KDBContents & cont, SraInfo::Detail detail ) const
{
    string out;

    switch ( m_fmt )
    {
    case Default:
    {
        out = FormatContentNodeDefault( string(), cont, detail, true );
        break;
    }
    case Json:
    {
        out = FormatContentNodeJson( IndentUnit, cont, detail, true );
        break;
    }
    case XML:
    default:
        throw VDB::Error( "unsupported formatting option for contents" );
    }

    return out;
}

static
string
formatTreeNodeDefault( size_t p_indent, const SraInfo::TreeNode & node )
{
    string ret = string ( p_indent, ' ' );
    ret += node.key;
    if ( node.type == SraInfo::TreeNode::Value )
    {
        ret += ": ";
        ret += node.value;
    }
    else
    {
        for ( auto n : node.subnodes )
        {
            if ( n.key != "update" && n.key != "file" )
            {
                ret += "\n";
                ret += formatTreeNodeDefault( p_indent + 1, n );
            }
        }
    }

    return ret;
}

static
string
escapeCsv( const string & input )
{
    string ret;
    bool hasComma = input.find( ',', 0 ) != string::npos;
    if ( hasComma )
    {
        ret += "\"";
    }

    for ( auto c : input )
    {
        switch ( c )
        {
        case '"':
            ret += "\"\"";
            break;
        default:
            ret += c;
        }
    }

    if ( hasComma )
    {
        ret += "\"";
    }
    return ret;
}

static
string
formatTreeNodeCsv( size_t p_indent, const SraInfo::TreeNode & node )
{
    string ret = string ( p_indent, ',' );
    ret += escapeCsv(node.key);
    if ( node.type == SraInfo::TreeNode::Value )
    {
        ret += ","; // important: no space before the next value
        ret += escapeCsv(node.value);
    }
    else
    {
        for ( auto n : node.subnodes )
        {
            if ( n.key != "update" && n.key != "file" )
            {
                ret += "\n";
                ret += formatTreeNodeCsv( p_indent + 1, n );
            }
        }
    }

    return ret;
}

static
string
formatTreeNodeJson( const SraInfo::TreeNode & node, bool hasKey = true )
{
    string ret;
    if ( hasKey ) // not an array element
    {
        ret += "\"";
        ret += node.key;
        ret += "\": ";
    }
    switch (node.type)
    {
    case SraInfo::TreeNode::Value:
        {
            bool needQuotes = node.value[0] != '{';
            if ( needQuotes )
            {
                ret += "\"";
            }
            ret += node.value;
            if ( needQuotes )
            {
                ret += "\"";
            }
        }
        break;
    case SraInfo::TreeNode::Array:
        {
            ret += "[ ";
            bool first = true;
            for ( auto n : node.subnodes )
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    ret += ", ";
                }
                ret += formatTreeNodeJson( n, false );
            }
            ret += " ]";
        }
        break;
    case SraInfo::TreeNode::Element:
        {
            ret += "{ ";
            bool first = true;
            for ( auto n : node.subnodes )
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    ret += ", ";
                }
                ret += formatTreeNodeJson( n );
            }
            ret += " }";
        }
        break;
    default:
        abort();
    }

    return ret;
}

static
string
formatTreeNodeXml( const SraInfo::TreeNode & node )
{
    string ret;
    ret += "<";
    ret += node.key;
    ret += ">";

    if ( node.type == SraInfo::TreeNode::Value )
    {
        ret += node.value;
    }
    else
    {
        for ( auto n : node.subnodes )
        {
            if ( n.key != "update" && n.key != "file" )
            {
                ret += formatTreeNodeXml( n );
            }
        }
    }

    ret += "</";
    ret += node.key;
    ret += ">\n";

    return ret;
}


string
Formatter::format( const SraInfo::Fingerprints & fp, SraInfo::Detail /*detail*/) const
{
    string out;

    switch ( m_fmt )
    {
    case Default:
        if ( fp.empty() )
        {
            return "none provided";
        }
        for (auto p : fp)
        {
            out += formatTreeNodeDefault( 0, p );
            out += "\n";
        }
        break;

    case CSV:
        if ( fp.empty() )
        {
            return "none provided";
        }
        for (auto p : fp)
        {
            out += formatTreeNodeCsv( 0, p );
            out += "\n";
        }
        break;

    case Json:
        {
            bool first = true;
            for (auto p : fp)
            {
                if (first)
                {
                    first = false;
                }
                else
                {
                    out += ", ";
                }
                out += formatTreeNodeJson( p );
            }
        }
        break;

    case XML:
        out += " <FINGERPRINTS>";
        for (auto p : fp)
        {
            out += formatTreeNodeXml( p );
        }
        out += "\n </FINGERPRINTS>";
        break;

    default:
        throw VDB::Error( "unsupported formatting option for fingerprint" );
    }
    return out;
}
