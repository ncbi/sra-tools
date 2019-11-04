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
*  Communicate with SDL via srapath
*
*/

#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <algorithm>

#include "globals.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "proc.hpp"
#include "which.hpp"
#include "util.hpp"
#include "run-source.hpp"
#include "ncbi/json.hpp"

using namespace constants;

namespace sratools {

static std::string config_or_default(char const *const config_node, char const *const default_value)
{
    auto const &from_config = config->get(config_node);
    return from_config ? from_config.value() : default_value;
}

static std::string run_srapath(std::vector<std::string> const &runs)
{
    auto const toolpath = tool_name::path(tool_name::SRAPATH);
    auto const toolpath_s = std::string(toolpath);
    auto const version_string = config_or_default("/repository/remote/version", resolver::version());
    auto const url_string = config_or_default("/repository/remote/main/SDL.2/resolver-cgi", resolver::url());
    enum {
        TOOL_NAME,
        LOG_LEVEL, LOG_LEVEL_VALUE,
        FUNCTION_PARAM, FUNCTION_VALUE,
        JSON_PARAM,
        VERSION_PARAM, VERSION_VALUE,
        URL_PARAM, URL_VALUE,
        FIRST_NULL
    };
    auto const argc = FIRST_NULL
                    + (location ? 2 : 0)
                    + (ngc ? 2 : 0)
                    + (perm ? 2 : 0)
                    + runs.size();
    auto argv = new char const * [argc + 1];
    auto const argend = argv + argc + 1;

    std::fill(argv, argv + argc + 1, nullptr);
    argv[TOOL_NAME] = tool_name::runas(tool_name::SRAPATH);
    argv[LOG_LEVEL] = "--log-level";
    argv[LOG_LEVEL_VALUE] = "fatal";
    argv[FUNCTION_PARAM] = "--function";
    argv[FUNCTION_VALUE] = "names";
    argv[JSON_PARAM] = "--json";
    argv[VERSION_PARAM] = "--vers";
    argv[VERSION_VALUE] = version_string.c_str();
    argv[URL_PARAM] = "--url";
    argv[URL_VALUE] = url_string.c_str();
    {
        auto i = argv + FIRST_NULL;

        assert(i != argend && *i == NULL);
        if (location) {
            *i++ = "--location";
            assert(i != argend && *i == NULL);
            *i++ = location->c_str();
        }
        
        assert(i != argend && *i == NULL);
        if (perm) {
            *i++ = "--perm";
            assert(i != argend && *i == NULL);
            *i++ = perm->c_str();
        }
        if (ngc) {
            *i++ = "--ngc";
            assert(i != argend && *i == NULL);
            *i++ = ngc->c_str();
        }
        
        for (auto && run : runs) {
            assert(i != argend && *i == NULL);
            *i++ = run.c_str();
        }
        assert(i != argend && *i == NULL);
    }
    auto fd = -1;
    auto const child = process::run_child_with_redirected_stdout(&fd, [&]() {
        if (logging_state::is_debug()) {
            std::cerr << toolpath_s << ": ";
            for (auto i = argv; i != argend && *i; ++i) {
                auto const &value = *i;
                std::cerr << value << ' ';
            }
            std::cerr << std::endl;
        }
        execve(toolpath, argv);
        throw_system_error("failed to exec " + toolpath_s);
    });
    auto response = std::string();
    read_fd(fd, [&](char const *buffer, size_t nread) {
        response.append(buffer, nread);
    });
    close(fd);
    DEBUG_OUT << response << std::endl;
    
    auto const result = child.wait();
    delete [] argv;
    if (result.normal())
        return response;
    
    if (result.signaled()) {
        std::cerr << "srapath (pid: " << child.get_pid() << ") was killed by signal " << result.termsig() << std::endl;
    }
    else if (result.exit_code() != 0) {
        std::cerr << "srapath exited with error code " << result.exit_code() << std::endl;
        exit(result.exit_code());
    }
    std::unexpected();
}

struct srapath_unexpected_error : public std::logic_error
{
    srapath_unexpected_error() : std::logic_error("unexpected response from srapath") {}
};

static std::string getString(ncbi::JSONObject const &obj, char const *const member)
{
    auto const &value = obj.getValue(member);
    if (value.isString())
        return value.toString().toSTLString();
    throw srapath_unexpected_error();
}

static bool getOptionalString(std::string *result, ncbi::JSONObject const &obj, char const *const member)
{
    try {
        auto const &value = obj.getValue(member);
        if (value.isNull())
            return false;

        if (!value.isArray() && !value.isObject()) {
            auto const &string = value.toString();
            result->assign(string.toSTLString());
            return true;
        }
    }
    catch (...) { return false; }
    throw srapath_unexpected_error();
}

static bool getOptionalBoolOrString(bool *result, ncbi::JSONObject const &obj, char const *const member)
{
    try {
        auto const &value = obj.getValue(member);
        if (value.isBoolean()) {
            *result = value.toBoolean();
            return true;
        }
        if (value.isString()) {
            auto const &string = value.toString();
            *result = (string == "true");
            return true;
        }
    }
    catch (...) {
        return false;
    }
    throw srapath_unexpected_error();
}

template <typename F>
static void forEach(ncbi::JSONObject const &obj, char const *member, F && f)
{
    try {
        auto const &array = obj.getValue(member).toArray();
        auto const n = array.count();
        for (auto i = decltype(n)(0); i < n; ++i) {
            auto const &value = array.getValue(i);
            f(value);
        }
    }
    catch (...) {}
}

struct raw_response {
    struct error {
        std::string accession;
        std::string code;
        std::string message;
        error(std::string const &accession, std::string const &error, std::string const &message)
        : accession(accession)
        , code(error)
        , message(message)
        {
        }
    };
    struct accessionType : public std::pair<std::string, std::string> {
        accessionType(std::string const &accession, std::string const &type) : std::pair<std::string, std::string>(std::make_pair(accession, type)) {}
        std::string const &accession() const { return first; }
        std::string const &type() const { return second; }
        
        static accessionType make(ncbi::JSONObject const &obj) {
            auto const acc = getString(obj, "accession");
            try {
                auto const typ = getString(obj, "itemType");
                
                return accessionType(acc, typ);
            }
            catch (...) {}
            auto const code = getString(obj, "error");
            auto const message = getString(obj, "message");
            throw error(acc, code, message);
        }
    };
    struct remote {
        std::string source;
        std::string path;
        bool ceRequired;
        bool payRequired;
        remote(ncbi::JSONObject const &obj)
        : ceRequired(false)
        , payRequired(false)
        {
            source = getString(obj, "service");
            path = getString(obj, "path");
            getOptionalBoolOrString(&ceRequired, obj, "CE-Required");
            getOptionalBoolOrString(&payRequired, obj, "Payment-Required");
        }
    };
    struct remotes {
        using value_type = std::vector<remote>::value_type;
        std::string size;
        std::string localPath;
        std::string itemClass;
        std::string cachePath;
        std::vector<remote> remotes;
        bool haveSize, haveLocalPath, haveItemClass, haveCachePath;
        
        std::map<std::string, decltype(remotes.size())> remotesBySource() const {
            auto result = std::map<std::string, decltype(remotes.size())>();
            auto const n = remotes.size();
            for (auto i = decltype(n)(0); i < n; ++i) {
                result.insert({remotes[i].source, i});
            }
            return result;
        }
        source make_source(remote const &remote, std::string const &accession) const {
            auto result = source();
            
            if ((result.haveLocalPath = haveLocalPath) != false)
                result.localPath = localPath;
            if ((result.haveCachePath = haveCachePath) != false)
                result.cachePath = cachePath;
            if ((result.haveSize = haveSize) != false)
                result.fileSize = size;
            
            result.accession = accession;
            result.service = remote.source;
            result.remoteUrl = remote.path;
            result.needCE = remote.ceRequired;
            result.needPmt = remote.payRequired;
            
            result.haveAccession = true;
            
            return result;
        }
    };
    std::map<accessionType, remotes> responses;
    std::string ceToken;
    bool hasCEToken;

    remotes const &get_remotes(std::string const &acc, std::string const &typ) const
    {
        static auto const empty = remotes();
        auto const key = accessionType(acc, typ);
        auto const iter = responses.find(key);
        return iter != responses.end() ? iter->second : empty;
    }
    std::set<std::string> accessions() const {
        auto result = std::set<std::string>();
        
        for (auto & i : responses) {
            result.insert(i.first.accession());
        }
        return result;
    }
    static remotes::value_type make_remote(ncbi::JSONObject const &obj) {
        auto result = remote(obj);

        return result;
    }
    static decltype(responses)::value_type make_accession(ncbi::JSONObject const &obj) {
        auto const &accTyp = accessionType::make(obj);
        auto sources = remotes();
        
        sources.haveLocalPath = getOptionalString(&sources.localPath, obj, "local");
        sources.haveCachePath = getOptionalString(&sources.cachePath, obj, "cache");
        sources.haveItemClass = getOptionalString(&sources.itemClass, obj, "itemClass");
        sources.haveSize = getOptionalString(&sources.size, obj, "size");
        
        forEach(obj, "remote", [&](ncbi::JSONValue const &value) {
            assert(value.isObject());
            auto const &obj = value.toObject();
            sources.remotes.push_back(make_remote(obj));
        });
        return {accTyp, sources};
    }
    static raw_response make(ncbi::JSONObject const &obj) {
        auto result = raw_response();
        
        result.hasCEToken = getOptionalString(&result.ceToken, obj, "CE-Token");

        forEach(obj, "responses", [&](ncbi::JSONValue const &value) {
            assert(value.isObject());
            try {
                auto const &acc = make_accession(value.toObject());
                result.responses.emplace(acc);
            }
            catch (error const &err) {
                std::cerr << "Accession " << err.accession << ": Error (" << err.code << ") " << err.message << std::endl;
            }
        });

        return result;
    }
    static raw_response make(std::string const &response) {
        try {
            auto const jvRef = ncbi::JSON::parse(ncbi::String(response));
            auto const &obj = jvRef->toObject();
            return make(obj);
        }
        catch (...) {
            throw srapath_unexpected_error();
        }
    }
};

data_sources::data_sources(std::string const &responseJSON)
{
    auto const raw = raw_response::make(responseJSON);
    
    have_ce_token = raw.hasCEToken;
    if (raw.hasCEToken)
        ce_token_ = raw.ceToken;
    
    for (auto & acc : raw.accessions()) {
        auto const &runs = raw.get_remotes(acc, "sra");
        auto const &vcaches = raw.get_remotes(acc, "vdbcache");
        auto const &vcachesBySource = vcaches.remotesBySource();
        auto default_vcache = -1;
        {
            // default vdbcache source is either NCBI or the first one
            auto i = vcachesBySource.find("sra-ncbi");
            if (i != vcachesBySource.end())
                default_vcache = int(i->second);
            else if (!vcaches.remotes.empty())
                default_vcache = 0;
        }
        for (auto && run : runs.remotes) {
            // find a vdbcache source that matches this source or use the default one
            auto iter = vcachesBySource.find(run.source);
            auto vcache = (iter != vcachesBySource.end()) ? int(iter->second) : default_vcache;
            
            auto const run_source = runs.make_source(run, acc);
            addSource((vcache < 0) ? data_source(run_source)
                      : data_source(run_source, runs.make_source(vcaches.remotes[vcache], acc)));
        }
    }
}

#define SETENV(VAR, VAL) env_var::set(env_var::VAR, VAL.c_str())
#define SETENV_IF(EXPR, VAR, VAL) env_var::set(env_var::VAR, (EXPR) ? VAL.c_str() : NULL)
#define SETENV_BOOL(VAR, VAL) env_var::set(env_var::VAR, (VAL) ? "1" : NULL)

/// @brief set/unset run environment variables
void data_source::set_environment() const
{
    SETENV(REMOTE_URL, run.remoteUrl);
    SETENV_IF(run.haveCachePath, CACHE_URL, run.cachePath);
    SETENV_IF(run.haveLocalPath, LOCAL_URL, run.localPath);
    SETENV_IF(run.haveSize, SIZE_URL, run.fileSize);
    SETENV_BOOL(REMOTE_NEED_CE, run.needCE);
    SETENV_BOOL(REMOTE_NEED_PMT, run.needPmt);
    
    SETENV_IF(haveVdbCache, REMOTE_VDBCACHE, vdbcache.remoteUrl);
    SETENV_IF(haveVdbCache && vdbcache.haveCachePath, CACHE_VDBCACHE, vdbcache.cachePath);
    SETENV_IF(haveVdbCache && vdbcache.haveLocalPath, LOCAL_VDBCACHE, vdbcache.localPath);
    SETENV_IF(haveVdbCache && vdbcache.haveSize, SIZE_VDBCACHE, vdbcache.fileSize);
    SETENV_BOOL(CACHE_NEED_CE, haveVdbCache && vdbcache.needCE);
    SETENV_BOOL(CACHE_NEED_PMT, haveVdbCache && vdbcache.needPmt);
}

    
/// @brief set/unset CE Token environment variable
void data_sources::set_ce_token_env_var() const {
    SETENV_IF(have_ce_token, CE_TOKEN, ce_token_);
}

data_sources data_sources::preload(const std::vector<std::string> &runs) {
    auto to_load = std::vector<std::string>();
    auto local = std::vector<std::string>();
    
    for (auto && qry : runs) {
        if (access(qry.c_str(), R_OK) == 0)
            local.push_back(qry);
        else
            to_load.push_back(qry);
    }
    auto result = to_load.empty() ? data_sources() : data_sources(run_srapath(to_load));

    for (auto && file : local)
        result.addSource(data_source::local_file(file));
    
    return result;
}

} // namespace sratools
