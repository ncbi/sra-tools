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
 *  argv manipulations
 *
 */

#pragma once
#include <tuple>
#include <string>
#include <vector>
#include <map>
#include <iterator>

#include "opt_string.hpp"

namespace sratools {

using Dictionary = std::map<std::string, std::string>;
using ParamList = std::vector<std::pair<std::string, opt_string>>;
using ArgsList = std::vector<std::string>;

/// @brief checks if the current item matches and extracts the value if it does
///
/// @param param the wanted parameter, e.g. '--option-file'
/// @param current the current item
/// @param end the end, e.g. args.end()
///
/// @returns if found, tuple<true, this is the value, the next item to resume at>. if not found, tuple<false, "", current>
extern
std::tuple<bool, std::string, ArgsList::iterator> matched(std::string const &param, ArgsList::iterator current, ArgsList::iterator end);


/// @brief load argv, handle any option files
///
/// @param argc argc - 1
/// @param argv argv + 1
///
/// @returns the args. May throw on I/O error or parse error.
extern
ArgsList loadArgv(int argc, char *argv[]);


/// @brief splits argv into parameters and arguments
///
/// @param parameters [out] recieves the parameters, always translated to long form
/// @param arguments [out] recieves the arguments
/// @param longNames maps parameter short form to long form
/// @param hasArg what parameters (long form) take arguments
///
/// @return false if not parsed okay or help, else true
extern
bool parseArgs(  ParamList *parameters
               , ArgsList *arguments
               , Dictionary const &longNames
               , Dictionary const &hasArg
               );

} // namespace sratools
