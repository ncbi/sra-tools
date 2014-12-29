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
*/

#include "config-file.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <cctype>
#include <sstream>

/*
 * Config file:
 *  The config file consists of lines containing whitespace (ASCII 9 or 32)
 *  seperated fields.  The fields are:
 *      NAME (unique)
 *      SEQID
 *      extra (optional)
 */

/// reads a line with leading and trailing whitespace trimmed;
/// ignores lines consisting entirely of whitespace;
static std::string const getline(std::istream &is)
{
    std::string line("");
    auto ws = true;
    size_t len = 0;
    
    for ( ; ; ) {
        auto const ch = is.get();
        if (ch < 0)
            break;
        if (ws && isspace(ch))
            continue;
        
        if (ch == '\n' || ch == '\r')
            break;
        
        ws = false;
        line.push_back(ch);
        if (!isspace(ch))
            len = line.size();
    }
    line.erase(len);
    
    return line;
}

template <typename T>
struct Range {
    T start, end;
    
    Range(T const init) : start(init), end(init) {}
    T const size() const {
        return start < end ? (end - start) : 0;
    }
};

struct Parse {
    Range<std::string::size_type> name;
    Range<std::string::size_type> seqid;
    Range<std::string::size_type> extra;
    
    Parse()
    : name(std::string::npos)
    , seqid(std::string::npos)
    , extra(std::string::npos)
    {}
    bool good() const {
        return name.size() > 0 && seqid.size() > 0;
    }
};

static Parse parseLine(std::string const &in) {
    static std::string const whitespace(" \t");
    Parse rslt;

    if (in[0] != '#') {
        rslt.name.start = 0;

        rslt.name.end = in.find_first_of(whitespace);
        if (rslt.name.end != std::string::npos) {
            rslt.seqid.start = in.find_first_not_of(whitespace, rslt.name.end);
            if (rslt.seqid.start != std::string::npos) {
                rslt.seqid.end = in.find_first_of(whitespace, rslt.seqid.start);
                if (rslt.seqid.end == std::string::npos)
                    rslt.seqid.end = in.size();
                else {
                    rslt.extra.start = in.find_first_not_of(whitespace, rslt.seqid.end);
                    rslt.extra.end = in.size();
                }
            }
        }
    }
    
    return rslt;
}

static ConfigFile::Line makeLine(std::string const &in, Parse const &parse) {
    ConfigFile::Line rslt;
    
    rslt.NAME = in.substr(parse.name.start, parse.name.size());
    rslt.SEQID = in.substr(parse.seqid.start, parse.seqid.size());
    if (parse.extra.size() > 0)
        rslt.EXTRA = in.substr(parse.extra.start, parse.extra.size());
    else
        rslt.EXTRA = "";
    
    return rslt;
}

static ConfigFile::Unparsed makeUnparsed(unsigned const lineno, std::string const &line) {
    ConfigFile::Unparsed rslt;
    
    rslt.lineno = lineno;
    rslt.line = line;
    
    return rslt;
}

ConfigFile::ConfigFile(std::istream &is) {
    unsigned lineno = 0;
    
    while (is.good()) {
        auto const in = getline(is);
        if (in.size() == 0) {
            break;
        }
        ++lineno;

        auto const parse = parseLine(in);
        if (parse.good())
            lines.push_back(makeLine(in, parse));
        else
            unparsed.push_back(makeUnparsed(lineno, in));
    }
    if (is.eof())
        msg = "no errors";
    else
        msg = "error reading input";
    
    std::ostringstream oss;
    oss << msg << "; lines read: " << lineno;
    msg = oss.str();
}

ConfigFile ConfigFile::load(std::string const &filename) {
    std::ifstream ifs(filename);

    return ifs.is_open() ? ConfigFile::load(ifs) : ConfigFile();
}

void ConfigFile::printDescription(std::ostream &os, bool const detail) const {
    os << "Loaded " << lines.size() << " records" << std::endl;
    if (detail) {
        for (auto i = lines.begin(); i != lines.end(); ++i)
            os << i->NAME << '\t' << i->SEQID << '\t' << i->EXTRA << std::endl;
    }
    os << "Unparsed lines: " << unparsed.size() << std::endl;
    if (detail) {
        for (auto i = unparsed.begin(); i != unparsed.end(); ++i)
            os << i->lineno << '\t' << i->line << std::endl;
    }
    os << msg << std::endl;
}

#ifdef TESTING
int main(int argc, char *argv[]) {
    auto const config = argc > 1 ? ConfigFile::load(argv[1]) : ConfigFile::load(std::cin);
    
    config.printDescription(std::cout, true);
}
#endif
