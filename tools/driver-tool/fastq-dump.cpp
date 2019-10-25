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
 *
 * Project:
 *  sratools command line tool
 *
 * Purpose:
 *  drive fastq-dump
 *
 */

#include <string>
#include <map>
#include <set>
#include <vector>
#include <utility>
#include <iostream>

#include <cstdlib>

#include <unistd.h>
#include <sys/stat.h>
#include <sysexits.h>

#include "globals.hpp"
#include "which.hpp"
#include "debug.hpp"
#include "tool-args.hpp"
#include "constants.hpp"

#include "fastq-dump.hpp"

#if __cplusplus >= 201403L
#define exchange std::exchange
#else
template <class T, class U = T>
static inline T exchange(T &obj, U &&new_value)
{
    auto const old_value = std::move(obj);
    obj = std::move(new_value);
    return old_value;
}
#endif

namespace sratools {

using namespace constants;

static bool parse_fastq_dump_args(ParamList &params, ArgsList &accessions)
{
    static Dictionary const longArgs =
    {
        { "-+", "--debug" },
        { "-?", "--help" },
        { "-A", "--accession" },
        { "-B", "--dumpbase" },
        { "-C", "--dumpcs" },
        { "-E", "--qual-filter" },
        { "-F", "--origfmt" },
        { "-G", "--spot-group" },
        { "-I", "--readids" },
        { "-K", "--keep-empty-files" },
        { "-L", "--log-level" },
        { "-M", "--minReadLen" },
        { "-N", "--minSpotId" },
        { "-O", "--outdir" },
        { "-Q", "--offset" },
        { "-R", "--read-filter" },
        { "-T", "--group-in-dirs" },
        { "-V", "--version" },
        { "-W", "--clip" },
        { "-X", "--maxSpotId" },
        { "-Z", "--stdout" },
        { "-h", "--help" },
        { "-v", "--verbose" },
    };
    static Dictionary const hasArgs =
    {
        { "--accession", "TRUE" },
        { "--aligned-region", "TRUE" },
        { "--debug", "TRUE" },
        { "--defline-qual", "TRUE" },
        { "--defline-seq", "TRUE" },
        { "--dumpcs", "0" },
        { "--fasta", "0" },
        { "--log-level", "TRUE" },
        { "--matepair-distance", "TRUE" },
        { "--maxSpotId", "TRUE" },
        { "--minReadLen", "TRUE" },
        { "--minSpotId", "TRUE" },
        { "--offset", "TRUE" },
        { "--outdir", "TRUE" },
        { "--read-filter", "TRUE" },
        { "--spot-groups", "TRUE" },
        { "--table", "TRUE" },
    };
    auto nextIsParamArg = 0;
    
    for (decltype(args->size()) i = 0; i < args->size(); ++i) {
        auto &arg = args->at(i);
        auto const maybeParam = arg.size() > 0 && arg[0] == '-';
        auto const isArgOrAcc = arg.size() > 0 && arg[0] != '-';

        switch (exchange(nextIsParamArg, 0)) {
            case 2:
                if (maybeParam)
                    break;
            case 1:
                assert(params.size() > 0);
                params.back().second = arg;
                continue;
            default:
                break;
        }
        if (isArgOrAcc) {
            accessions.push_back(arg);
            continue;
        }
        if (arg.size() < 2) {
            return false;
        };
        
        auto param = arg;
        if (arg[1] == '-') {
            // long form
        }
        else {
            // short form
            auto const iter = longArgs.find(arg);
            if (iter == longArgs.end()) {
                return false;
            };
            param = iter->second;
        }
        if (param == "--help") {
            return false;
        }
        
        params.push_back({param, opt_string()});

        auto const iter = hasArgs.find(param);
        if (iter != hasArgs.end()) {
            nextIsParamArg = (iter->second == "0") ? 2 : 1;
        }
    }
    return true;
}

void running_as_fastq_dump [[noreturn]] () {
    auto const &toolname = tool_name::runas(tool_name::FASTQ_DUMP);
    auto const &toolpath = tool_name::path(tool_name::FASTQ_DUMP);
    ParamList params;
    ArgsList accessions;
    
    if (parse_fastq_dump_args(params, accessions)) {
        if (ngc) {
            params.push_back({"--ngc", *ngc});
        }
        processAccessions(toolname, toolpath
                          , nullptr, ".fastq"
                          , params, accessions);
    }
    else {
        toolHelp(toolpath);
    }
}


} // namespace sratools
