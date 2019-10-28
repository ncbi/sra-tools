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
 *  provide tool command info
 *
 */

#include <string>
#include <map>
#include <set>
#include <vector>
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

namespace sratools {

using namespace constants;

ArgsInfo infoFor(int toolID)
{
    switch (toolID) {
        case tool_name::SRAPATH:
            return {
                {
                    { "-f", "--function" },
                    { "-t", "--timeout" },
                    { "-a", "--protocol" },
                    { "-e", "--vers" },
                    { "-u", "--url" },
                    { "-p", "--param" },
                    { "-r", "--raw" },
                    { "-j", "--json" },
                    { "-d", "--project" },
                    { "-c", "--cache" },
                    { "-P", "--path" },
                    { "-V", "--version" },
                    { "-L", "--log-level" },
                    { "-v", "--verbose" },
                    { "-+", "--debug" },
                    { "-h", "--help" },
                    { "-?", "--help" },
                },
                {
                    { "--function", "TRUE" },
                    { "--location", "TRUE" },
                    { "--timeout", "TRUE" },
                    { "--protocol", "TRUE" },
                    { "--vers", "TRUE" },
                    { "--url", "TRUE" },
                    { "--param", "TRUE" },
                    { "--log-level", "TRUE" },
                    { "--debug", "TRUE" },
                }
            };
        case tool_name::FASTERQ_DUMP:
            return {
                {
                    { "-+", "--debug" },
                    { "-3", "--split-3" },
                    { "-?", "--help" },
                    { "-B", "--bases" },
                    { "-L", "--log-level" },
                    { "-M", "--min-read-len" },
                    { "-N", "--rowid-as-name" },
                    { "-O", "--outdir" },
                    { "-P", "--print-read-nr" },
                    { "-S", "--split-files" },
                    { "-V", "--version" },
                    { "-Z", "--stdout" },
                    { "-b", "--bufsize" },
                    { "-c", "--curcache" },
                    { "-e", "--threads" },
                    { "-f", "--force" },
                    { "-h", "--help" },
                    { "-m", "--mem" },
                    { "-o", "--outfile" },
                    { "-p", "--progress" },
                    { "-s", "--split-spot" },
                    { "-t", "--temp" },
                    { "-v", "--verbose" },
                    { "-x", "--details" },
                },
                {
                    { "--bufsize", "TRUE" },
                    { "--curcache", "TRUE" },
                    { "--debug", "TRUE" },
                    { "--log-level", "TRUE" },
                    { "--mem", "TRUE" },
                    { "--min-read-len", "TRUE" },
                    { "--outdir", "TRUE" },
                    { "--outfile", "TRUE" },
                    { "--stdout", "TRUE" },
                    { "--temp", "TRUE" },
                    { "--threads", "TRUE" },
                }
            };
        case tool_name::SAM_DUMP:
            return {
                {
                    { "-+", "--debug" },
                    { "-1", "--primary" },
                    { "-=", "--hide-identical" },
                    { "-?", "--help" },
                    { "-L", "--log-level" },
                    { "-Q", "--qual-quant" },
                    { "-V", "--version" },
                    { "-c", "--cigar-long" },
                    { "-g", "--spot-group" },
                    { "-h", "--help" },
                    { "-n", "--no-header" },
                    { "-p", "--prefix" },
                    { "-r", "--header" },
                    { "-s", "--seqid" },
                    { "-u", "--unaligned" },
                    { "-v", "--verbose" },
                },
                {
                    { "--aligned-region", "TRUE" },
                    { "--cursor-cache", "TRUE" },
                    { "--debug", "TRUE" },
                    { "--header-comment", "TRUE" },
                    { "--header-file", "TRUE" },
                    { "--log-level", "TRUE" },
                    { "--matepair-distance", "TRUE" },
                    { "--min-mapq", "TRUE" },
                    { "--output-buffer-size", "TRUE" },
                    { "--output-file", "TRUE" },
                    { "--prefix", "TRUE" },
                    { "--qual-quant", "TRUE" },
                    { "--rna-splice-level", "TRUE" },
                    { "--rna-splice-log", "TRUE" },
                }
            };
        case tool_name::PREFETCH:
            return {
                {
                    { "-+", "--debug" },
                    { "-?", "--help" },
                    { "-L", "--log-level" },
                    { "-N", "--min-size" },
                    { "-O", "--output-directory" },
                    { "-R", "--rows" },
                    { "-T", "--type" },
                    { "-V", "--version" },
                    { "-X", "--max-size" },
                    { "-a", "--ascp-path" },
                    { "-c", "--check-all" },
                    { "-f", "--force" },
                    { "-h", "--help" },
                    { "-l", "--list" },
                    { "-n", "--numbered-list" },
                    { "-o", "--output-file" },
                    { "-p", "--progress" },
                    { "-s", "--list-sizes" },
                    { "-t", "--transport" },
                    { "-v", "--verbose" },
                },
                {
                    { "--ascp-options", "TRUE" },
                    { "--ascp-path", "TRUE" },
                    { "--debug", "TRUE" },
                    { "--force", "TRUE" },
                    { "--location", "TRUE" },
                    { "--log-level", "TRUE" },
                    { "--max-size", "TRUE" },
                    { "--min-size", "TRUE" },
                    { "--order", "TRUE" },
                    { "--output-directory", "TRUE" },
                    { "--output-file", "TRUE" },
                    { "--progress", "TRUE" },
                    { "--rows", "TRUE" },
                    { "--transport", "TRUE" },
                    { "--type", "TRUE" },
                }
            };
        case tool_name::SRA_PILEUP:
            return {
                {
                    { "-+", "--debug" },
                    { "-?", "--help" },
                    { "-L", "--log-level" },
                    { "-V", "--version" },
                    { "-d", "--duplicates" },
                    { "-e", "--seqname" },
                    { "-h", "--help" },
                    { "-n", "--noqual" },
                    { "-o", "--outfile" },
                    { "-p", "--spotgroups" },
                    { "-q", "--minmapq" },
                    { "-r", "--aligned-region" },
                    { "-t", "--table" },
                    { "-v", "--verbose" },
                },
                {
                    { "--aligned-region", "TRUE" },
                    { "--debug", "TRUE" },
                    { "--duplicates", "TRUE" },
                    { "--function", "TRUE" },
                    { "--log-level", "TRUE" },
                    { "--merge-dist", "TRUE" },
                    { "--minmapq", "TRUE" },
                    { "--minmismatch", "TRUE" },
                    { "--outfile", "TRUE" },
                    { "--table", "TRUE" },
                }
            };
        default:
            break;
    }
    assert(!"reachable");
}

} // namespace sratools
