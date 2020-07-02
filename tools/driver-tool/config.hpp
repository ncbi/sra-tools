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

namespace sratools {

/// @brief class to manage KConfig object
class Config {
    /// @brief the KConfig object
    void *obj;

    /// @brief the installation ID
    opt_string installID() const;
public:
    /// @brief create a new KConfig object
    /// @param runpath ignored
    Config(ToolPath const &runpath);

    /// @brief release the KConfig object
    ~Config();

    /// @brief get value for keypath (if any)
    /// @param keypath the path of the value to get
    opt_string get(char const *const keypath) const;

    /// @brief get value for keypath (if any)
    /// @param keypath the path of the value to get
    opt_string operator [](std::string const &keypath) const {;
        return get(keypath.c_str());
    }

    /// @brief true if there is no installation ID
    bool noInstallID() const;

    /// @brief true if user has allowed sending the cloud instance identity
    bool canSendCEToken() const {
        auto const x = get("/libs/cloud/report_instance_identity");
        return x ? x.value() == "true" : false;
    }
};

}
