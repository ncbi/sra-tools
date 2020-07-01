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

namespace sratools {

static bool forceInstallID(void)
{
#if DEBUG || _DEBUGGING
    auto const &force = EnvironmentVariables::get("SRATOOLS_FORCE_INSTALL");
    return force ? !force.empty() && force != "0" : false;
#else
    return false;
#endif
}

opt_string Config::get(const char *const keypath) const
{
    String *kresult = NULL;
    rc_t rc = KConfigReadString(obj, keypath, &kresult);
    if (rc == 0) {
        assert(kresult != NULL);
        std::string result(kresult->addr, kresult->size);
        free(kresult);
        return result;
    }
    return opt_string();
}

bool Config::noInstallID() const
{
    return get(InstallKey()).has_value() || forceInstallID() ? false : true;
}


Config::~Config()
{
    KConfigRelease(obj);
}

Config::Config(ToolPath const &runpath) {
    obj = NULL;
    rc_t rc = KConfigMake(&obj, NULL);
    if (rc != 0) {
        assert(obj != NULL);
        throw std::runtime_error("failed to load configuration");
    }
}

}
