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

#pragma once

#include <string>
#include <map>
#include "opt_string.hpp"
#include "tool-path.hpp"

struct KConfig;

namespace sratools {

class Config {
    struct KConfig *obj;

    static constexpr char const *InstallKey() {
        return "/LIBS/GUID";
    }
public:
    Config(ToolPath const &runpath);
    ~Config();
    opt_string get(char const *const keypath) const;
    opt_string operator [](std::string const &keypath) const {;
        return get(keypath.c_str());
    }
    bool noInstallID() const;
    bool canSendCEToken() const {
        auto const x = get("/libs/cloud/report_instance_identity");
        return x ? x.value() == "true" : false;
    }
};

}
