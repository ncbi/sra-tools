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
*  load config
*
*/

#include "config.hpp"
#include <kfg/config.h>
#include <klib/text.h>

#include <iostream>
#include <cctype>
#include <set>
#include <cstdlib>

#include "support2.hpp"
#include "proc.hpp"
#include "tool-path.hpp"
#include "util.hpp"
#include "debug.hpp"

/**
 @brief make an opt_string from a String
 @note free's the String
 @param str the string, may be null
 @returns a new opt_string
 */
static opt_string makeFromKString(String *str)
{
    if (str) {
        auto result = std::string(str->addr, str->size);
        free(str);
        return result;
    }
    return opt_string();
}

namespace sratools {

opt_string Config::get(const char *const key) const
{
    String *value = NULL;
    KConfigReadString((KConfig *)obj, key, &value);
    return makeFromKString(value);
}

bool Config::noInstallID() const
{
    return get("/LIBS/GUID").has_value() ? false : true;
}

Config::~Config()
{
    KConfigRelease((KConfig *)obj);
}

Config::Config(ToolPath const &runpath) {
    (void)runpath;
    obj = NULL;
    rc_t rc = KConfigMake((KConfig **)&obj, NULL);
    if (rc != 0) {
        assert(obj != NULL);
        throw std::runtime_error("failed to load configuration");
    }
}

#if DEBUG || _DEBUGGING
static bool forceInstallID(void)
{
    auto const &force = EnvironmentVariables::get("SRATOOLS_FORCE_INSTALL");
    return force ? !force.empty() && force != "0" : false;
}
static opt_string fakeID(void) {
    return forceInstallID() ? opt_string("8badf00d-1111-4444-8888-deaddeadbeef") : opt_string();
}
#else
static bool forceInstallID(void)
{
    return false;
}
static opt_string fakeID(void) {
    return opt_string();
}
#endif

opt_string Config::installID() const {
    auto const realID = get("/LIBS/GUID");
    return realID ? realID : fakeID();
}

}
