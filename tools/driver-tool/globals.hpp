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
 *  Declare and define global runtime constants
 *
 */

#pragma once
#include <map>
#include <string>
#include <vector>

#include "config.hpp"

namespace sratools {

// MARK: extracted from argv[0] or SRATOOLS_IMPERSONATE
extern std::string const *argv0; ///< full value from argv[0] or overridden value
extern std::string const *selfpath; ///< parsed from command line, may be empty
extern std::string const *basename;
extern std::string const *version_string; ///< as parsed or same as version of this tool if not parsed

extern std::vector<std::string> const *args; ///< original command line arguments, with argv[0] removed
extern std::map<std::string, std::string> const *parameters;

extern std::string const *location; ///< may be null

extern Config const *config;

extern std::vector<std::string> const PATH; ///< in which.cpp

}
