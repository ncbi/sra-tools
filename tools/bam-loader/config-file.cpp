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

/*
 * Config file:
 *  The config file consists of lines containing whitespace (ASCII 9 or 32)
 *  seperated fields.  The fields are:
 *      NAME (unique)
 *      SEQID
 *      extra (optional)
 */

static bool ConfigFile_Line_read(std::istream &is, ConfigFile::Line &line) {
    static char const whitespace[] = " \t";
    std::string fline;

    std::getline(is, fline); if (fline.size() == 0) return false;

    size_t const nameend = fline.find_first_of(whitespace);
    if (nameend == std::string::npos) return false;

    size_t const seqidstart = fline.find_first_not_of(whitespace, nameend);
    if (seqidstart == std::string::npos) return false;

    line.NAME = std::string(fline, 0, nameend);

    size_t const seqidend = fline.find_first_of(whitespace, seqidstart);
    line.SEQID = std::string(fline, seqidstart, seqidend);

    if (seqidend != std::string::npos) {
        size_t const extrastart = fline.find_first_not_of(whitespace, seqidend);
        if (extrastart != std::string::npos) {
            line.EXTRA = std::string(fline, extrastart);
        }
    }
    return true;
}

ConfigFile::ConfigFile(std::istream &is) {
    for ( ; ; ) {
        Line line;

        if (ConfigFile_Line_read(is, line))
            lines.push_back(line);
        else
            break;
    }
}

ConfigFile const ConfigFile::load(std::string const &filename) {
    std::ifstream ifs(filename);

    return ifs.is_open() ? ConfigFile::load(ifs) : ConfigFile();
}

#ifdef TESTING
int main(int argc, char *argv[]) {
    if (argc > 1) {
        auto config = ConfigFile::load(argv[1]);

        std::cout << "Loaded " << config.lines.size() << " lines" << std::endl;
        for (auto i = config.lines.begin(); i != config.lines.end(); ++i) {
            std::cout << i->NAME << '\t' << i->SEQID << '\t' << i->EXTRA << std::endl;
        }
    }
}
#endif
