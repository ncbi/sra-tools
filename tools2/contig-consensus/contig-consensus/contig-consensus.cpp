/* ===========================================================================
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

#include <iostream>
#include "utility.hpp"
#include "vdb.hpp"
#include "writer.hpp"

using namespace utility;

static int process(std::string const &inpath, FILE *const out)
{
    return 0;
}

namespace contig_consensus {
    static void usage(CommandLine const &commandLine, bool error) {
        (error ? std::cerr : std::cout) << "usage: " << commandLine.program[0] << " [-out=<path>] <database>" << std::endl;
        exit(error ? 3 : 0);
    }
    static int main(CommandLine const &commandLine) {
        for (auto && arg : commandLine.argument) {
            if (arg == "--help" || arg == "-help" || arg == "-h" || arg == "-?") {
                usage(commandLine, false);
            }
        }
        auto inpath = std::string();
        auto outfile = std::string();
        for (auto && arg : commandLine.argument) {
            if (arg.substr(0, 5) == "-out=") {
                outfile = arg.substr(5);
                continue;
            }
            if (inpath.empty()) {
                inpath = arg;
                continue;
            }
            usage(commandLine, true);
        }
        if (inpath.empty())
            usage(commandLine, true);
        if (outfile.empty())
            return process(inpath, stdout);
        
        auto strm = fopen(outfile.c_str(), "w");
        if (strm == nullptr) {
            perror(outfile.c_str());
            exit(3);
        }
        auto const rslt = process(inpath, strm);
        fclose(strm);
        return rslt;
    }
}

int main(int argc, const char * argv[]) {
    return contig_consensus::main(CommandLine(argc, argv));
}
