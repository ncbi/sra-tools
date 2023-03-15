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
*  Communicate with SDL
*
*/

#include "util.hpp"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <algorithm>
#include <functional>
#include <cstdint>

#include "globals.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "proc.hpp"
#include "util.hpp"
#include "file-path.hpp"
#include "opt_string.hpp"
#include "run-source.hpp"
#include "SDL-response.hpp"
#include "sratools.hpp"

#include "service.hpp"
using Service = vdb::Service;

using namespace constants;
using namespace sratools;

#include <vdb/manager.h> // VDBManagerPreferZeroQuality
#include <vdb/vdb-priv.h> // VDBManagerGetQualityString

static std::string config_or_default(char const *const config_node, char const *const default_value)
{
    auto const &from_config = config->get(config_node);
    return from_config ? from_config.value() : default_value;
}

static Service::Response get_SDL_response(Service const &query, std::vector<std::string> const &runs, bool const haveCE)
{
    if (runs.empty())
        throw std::domain_error("No query");
    
    auto const &version_string = config_or_default("/repository/remote/version", resolver::version());
    auto const &url_string = config_or_default("/repository/remote/main/SDL.2/resolver-cgi", resolver::url());

    assert(!runs.empty());
    query.add(runs);

    if (location)
        query.setLocation(*location);

    if (perm && haveCE)
        query.setPermissionsFile(*perm);

    if (ngc)
        query.setNGCFile(*ngc);

    return query.response(url_string, version_string);
}

struct RemoteKey {
    std::string prefix;
    std::string filePath;
    std::string fileSize;
    std::string service;
    std::string region;
    std::string project;
    std::string CER;
    std::string payR;
    std::string qualityType;
    std::string cachePath;
    std::string cacheSize;
    std::string cacheCER;
    std::string cachePayR;

    RemoteKey(unsigned index)
    : prefix(std::string("remote/") + std::to_string(index))
    , filePath(prefix + "/filePath")
    , fileSize(prefix + "/fileSize")
    , service(prefix + "/service")
    , region(prefix + "/region")
    , project(prefix + "/project")
    , CER(prefix + "/needCE")
    , payR(prefix + "/needPmt")
    , qualityType(prefix + "/qualityType")
    , cachePath(prefix + "/cachePath")
    , cacheSize(prefix + "/cacheSize")
    , cacheCER(prefix + "/cacheNeedCE")
    , cachePayR(prefix + "/cacheNeedPmt")
    {}
};

struct LocalKey {
    static char const filePath[];
    static char const qualityType[];
    static char const cachePath[];
};
char const LocalKey::filePath[] = "local/filePath";
char const LocalKey::qualityType[] = "local/qualityType";
char const LocalKey::cachePath[] = "local/cachePath";

data_sources::accession::info::info(Dictionary const *pinfo, unsigned index)
{
    auto const &info = *pinfo;
    auto const &names = env_var::names();

    if (index == 0) {
        auto const filePath = info.find(LocalKey::filePath);
        auto const qualityType = info.find(LocalKey::qualityType);
        auto const cachePath = info.find(LocalKey::cachePath);
        
        assert(filePath != info.end());
        environment[names[env_var::LOCAL_URL]] = filePath->second;
        if (cachePath != info.end())
            environment[names[env_var::LOCAL_VDBCACHE]] = cachePath->second;
        if (qualityType != info.end())
            this->qualityType = qualityType->second;
        service = filePath->second;
    }
    else {
        auto const key = RemoteKey(index);
        auto const filePath = info.find(key.filePath);
        auto const fileSize = info.find(key.fileSize);
        auto const CER = info.find(key.CER);
        auto const payR = info.find(key.payR);
        auto const qualityType = info.find(key.qualityType);
        auto const service = info.find(key.service);
        auto const region = info.find(key.region);
        auto const project = info.find(key.project);
        
        auto const cachePath = info.find(key.cachePath);

        assert(filePath != info.end());
        environment[names[env_var::REMOTE_URL]] = filePath->second;
        if (fileSize != info.end())
            environment[names[env_var::SIZE_URL]] = filePath->second;
        if (CER != info.end())
            environment[names[env_var::REMOTE_NEED_CE]] = "1";
        if (payR != info.end())
            environment[names[env_var::REMOTE_NEED_PMT]] = "1";

        if (cachePath != info.end()) {
            auto const cacheSize = info.find(key.cacheSize);
            auto const cacheCER = info.find(key.cacheCER);
            auto const cachePayR = info.find(key.cachePayR);
            
            environment[names[env_var::REMOTE_VDBCACHE]] = cachePath->second;
            if (cacheSize != info.end())
                environment[names[env_var::SIZE_VDBCACHE]] = cacheSize->second;
            if (cacheCER != info.end())
                environment[names[env_var::CACHE_NEED_CE]] = "1";
            if (cachePayR != info.end())
                environment[names[env_var::CACHE_NEED_PMT]] = "1";
        }
        
        assert(service != info.end());
        assert(region != info.end());
        this->service = service->second + "." + region->second;

        if (qualityType != info.end())
            this->qualityType = qualityType->second;

        if (project != info.end())
            this->project = project->second;
    }
}

data_sources::accession::accession(Dictionary const &p_queryInfo)
: queryInfo(p_queryInfo)
{
    auto const remote = queryInfo.find("remote");
    if (remote == queryInfo.end()) {
        first = 0;
        count = 1;
    }
    else {
        auto const n = std::stoi(remote->second);
        first = 1;
        count = n + 1;
    }
}

#define SETENV(VAR, VAL) env_var::set(env_var::VAR, VAL.c_str())
#define SETENV_IF(EXPR, VAR, VAL) env_var::set(env_var::VAR, (EXPR) ? VAL.c_str() : NULL)
#define SETENV_BOOL(VAR, VAL) env_var::set(env_var::VAR, (VAL) ? "1" : NULL)

/// @brief set/unset CE Token environment variable
void data_sources::set_ce_token_env_var() const {
    SETENV_IF(have_ce_token, CE_TOKEN, ce_token_);
}

void data_sources::set_param_bits_env_var(uint64_t bits) const {
    char buffer[32];
    auto i = sizeof(buffer);
    
    buffer[--i] = '\0';
    while (bits) {
        auto const nibble = bits & 0x0F;
        buffer[--i] = (char)(nibble < 10 ? (nibble + '0') : (nibble - 10 + 'A'));
        bits >>= 4;
    }
    
    env_var::set(env_var::PARAMETER_BITS, buffer + i);
}


void data_sources::preferNoQual() {
    VDBManager * v = NULL;
    const char * quality(NULL);
    rc_t rc = VDBManagerPreferZeroQuality(v);
    if (rc == 0)
        rc = VDBManagerGetQualityString(v, &quality);
    if (rc == 0) {
        assert(quality);
        env_var::preferNoQual(quality);
    }
    VDBManagerRelease(v);
}

data_sources::data_sources(CommandLine const &cmdline, Arguments const &parsed)
{
    have_ce_token = false;

    parsed.eachArgument([&](Argument const &arg) {
        std::string const i = arg.argument;
        auto &x = queryInfo[i];
        
        x["name"] = i;
        x["local"] = i;
        x[LocalKey::filePath].assign(cmdline.pathForArgument(arg));
    });
}

data_sources::QualityPreference data_sources::qualityPreference() {
    QualityPreference result = {};

#if MS_Visual_C
#pragma warning(disable: 4061) // garbage!!! doesn't know what default is!!!!!
#endif
    switch (Service::preferredQualityType()) {
    case vdb::Service::full:
        result.isFullQuality = true;
    case vdb::Service::none:
        result.isSet = true;
    default:
        break;
    }
    return result;
}

static bool setIfExists(Dictionary &result, std::string const &key, std::string const &name)
{
    if (FilePath(name.c_str()).exists()) {
        result[key] = name;
        return true;
    }
    return false;
}

static bool cwdSetIfExists(Dictionary &result, std::string const &key, FilePath const &cwd, std::string const &name)
{
    if (FilePath::exists(name)) {
        result[key].assign(cwd.append(name));
        return true;
    }
    return false;
}

/// \brief Adds extensions to name and checks for existence.
/// \note The invariant is `result["path"]` is set to the name of the file system object if it exists.
/// \returns true if it is a file system object.
static bool tryWithSraExtensions(Dictionary &result, std::string const &name, bool const wantFullQuality)
{
    for (auto ext : Accession::extensionsFor(wantFullQuality)) {
        if (setIfExists(result, "path", name + ext))
            return true;
    }
    return false;
}

static void lookForCacheFileIn(FilePath const &directory, Dictionary &result, bool const wantFullQuality, Accession const &accession)
{
    auto const &key = LocalKey::cachePath;
    auto const suffix = ".vdbcache";
    auto const &path = result[LocalKey::filePath];
    std::string const base = FilePath(path.c_str()).baseName();
    auto temp = base;

    // try acc.ext.vdbcache
    if (cwdSetIfExists(result, key, directory, base + suffix))
        return;

    for (auto sep = temp.find_last_of("."); sep != temp.npos; sep = temp.find_last_of(".")) {
        temp = temp.substr(0, sep);
        
        // try acc.vdbcache.ext
        if (cwdSetIfExists(result, key, directory, temp + suffix + base.substr(sep)))
            return;

        // try acc.vdbcache
        if (cwdSetIfExists(result, key, directory, temp + suffix))
            return;
    }
    // try accession.vdbcache
    cwdSetIfExists(result, key, directory, accession.accession() + suffix);

    (void)(wantFullQuality);
}

/// \note The invariant is `result["path"]` is set to the name of the file system object if it exists.
/// \returns true if name is a file system object.
static bool getLocalFilePath(Dictionary &result, FilePath const &cwd, std::string const &name, bool const wantFullQuality)
{
    if (!setIfExists(result, "path", name) && !tryWithSraExtensions(result, name, wantFullQuality))
        return false;
    
    auto const &filename = result["path"]; // might have extension added
    auto const accession = Accession(filename.c_str());
    auto const exts = accession.sraExtensions();
    
    LOG(9) << name << " is " << filename << " in directory" << (std::string)cwd << "." << std::endl;
    if (exts.size() == 1) {
        result[LocalKey::filePath] = result["path"].assign(cwd.append(filename));
        result[LocalKey::qualityType] = exts.front().first > 0 ? Accession::qualityTypeForLite : Accession::qualityTypeForFull;
        lookForCacheFileIn(cwd, result, wantFullQuality, accession);
        return true;
    }
    LOG(3) << name << " does not look like an SRA file." << std::endl;
    return false;
}

static void getADInfo(Dictionary &result, FilePath const &cwd, Accession const &accession, bool const wantFullQuality)
{
    auto const name = accession.accession();
    for (auto ext : Accession::extensionsFor(wantFullQuality)) {
        auto const withExt = name + ext;
        if (FilePath::exists(withExt)) {
            auto const qualityType = Accession::qualityTypeFor(ext);

            result[LocalKey::filePath].assign(cwd.append(withExt));
            result[LocalKey::qualityType] = qualityType;

            LOG(9) << result["name"] << " looks like an SRA Accession Directory, found file " << withExt << " with " << qualityType << " quality scores." << std::endl;
            lookForCacheFileIn(cwd, result, wantFullQuality, accession);
            return;
        }
    }
    LOG(2) << result["name"] << " looks like an SRA Accession Directory?" << std::endl;
}

static
std::map<std::string, Dictionary> getLocalFileInfo(CommandLine const &cmdline, Arguments const &parsed, FilePath const &cwd, bool const wantFullQuality)
{
    std::map<std::string, Dictionary> result;
    
    parsed.eachArgument([&](Argument const &arg) {
#if MS_Visual_C
#pragma warning(disable: 4456) // garbage!!!
#endif
        auto const name = std::string(arg.argument);
        auto const path = cmdline.pathForArgument(arg);
        auto const dir_base = path.split();
        auto const filename = std::string(dir_base.second);
        auto const hasDirName = !dir_base.first.empty();

        result[name]["name"] = name;
        
        auto &i = *result.find(name);
        auto const accession = Accession(filename);
        auto const isaRun = accession.type() == run;

        try {
            path.makeCurrentDirectory();

            i.second["local"] = name;
            LOG(9) << name << " is a directory." << std::endl;

            auto const path = FilePath::cwd(); // canonical path
            i.second["path"].assign(path);

            if (isaRun && accession.extension().empty()) // accession directories don't have extensions
                getADInfo(i.second, path, accession, wantFullQuality);
            else
                LOG(3) << name << " is a directory, but doesn't look like an SRA Accession Directory." << std::endl;

            cwd.makeCurrentDirectory();
            return;
        }
        catch (std::system_error const &e) {
            auto const what = std::string(e.what());
            if (what.find("chdir") == std::string::npos || what.find(name) == std::string::npos)
                throw;
            LOG(9) << "can't chdir to " << (std::string)path << " but that's okay."<< std::endl;
        }
        
        if (!isaRun) {
            LOG(3) << name << " does not look like an SRA file or directory." << std::endl;
            return;
        }
        if (hasDirName) {
            LOG(9) << name << " has a directory '" << std::string(dir_base.first) << "'." << std::endl;
            try {
                auto const dir = dir_base.first.copy();
                
                dir.makeCurrentDirectory();
                {
                    auto const path = FilePath::cwd(); // canonical path
                    if (getLocalFilePath(i.second, path, filename, wantFullQuality))
                        i.second["local"] = name;
                }
                cwd.makeCurrentDirectory();
            }
            catch (std::system_error const &e) {
                LOG(1) << std::string(dir_base.first) << " is a directory, but can't chdir to it: " << e.what() << std::endl;
            }
            return;
        }
        if (getLocalFilePath(i.second, cwd, filename, wantFullQuality))
            i.second["local"] = name;
    });
    return result;
}

data_sources::data_sources(CommandLine const &cmdline, Arguments const &args, bool withSDL)
{
    {
        auto const cwd = FilePath::cwd();
        queryInfo = getLocalFileInfo(cmdline, args, cwd, qualityPreference().isFullQuality);
        cwd.makeCurrentDirectory();
    }
    auto const havePerm = perm != nullptr;
    auto const canSendCE = config->canSendCEToken();
    if (logging_state::is_dry_run()) ; else assert(!(havePerm && !canSendCE));

    auto const &ceToken = Service::CE_Token();
    if (logging_state::is_dry_run()) ; else assert(!(havePerm && ceToken.empty()));

    have_ce_token = canSendCE && !ceToken.empty();
    if (have_ce_token) ce_token_ = ceToken;

    if (withSDL) {
        auto const service = Service::make();
        std::vector<std::string> terms;

        for (auto const &i : queryInfo) {
            auto const f = i.second.find("local");
            if (f == i.second.end())
                terms.emplace_back(i.first);
        }
        if (terms.empty())
            return;
        try {
            auto const response = get_SDL_response(service, terms, have_ce_token);
            LOG(8) << "SDL response:\n" << response << std::endl;
            
            auto const parsed = Response2::makeFrom(response.responseText());
            LOG(7) << "Parsed SDL Response" << std::endl;

            for (auto const &sdl_result : parsed.results) {
                auto const &query = sdl_result.query;
                auto &info = queryInfo[query];

                LOG(6) << "Query " << query << " " << sdl_result.status << " " << sdl_result.message << std::endl;
                info["accession"] = query;
                info["SDL/status"] = sdl_result.status;
                info["SDL/message"] = sdl_result.message;

                if (sdl_result.status == "200") {
                    // The first pass rejects files that do not have the desired quality type.
                    // SDL should have applied similar logic, so this is probably unneccessary.
                    unsigned added = 0, pass = qualityPreference().isFullQuality ? 0 : 1;
                    auto encrypted = false;

                    do {
                        ++pass;
                        for (auto const &fl : sdl_result.getByType("sra")) {
                            auto const &file = fl.first;
#if MS_Visual_C
#pragma warning(disable: 4459)
#endif
                            auto const &location = fl.second;
                            
                            if (file.hasExtension(".pileup")) continue;
                            if (!file.object) continue;

                            if (pass == 1 && qualityPreference().isFullQuality && file.noqual)
                                continue;
                            
                            auto const &projectId = location.projectId;
                            auto const &service = location.service;
                            auto const &region = location.region;

                            if (ngc == nullptr && projectId) {
                                std::cerr <<
                                    "The data for " << query << " from " << service << "." << region << " is encrypted.\n"
                                    "To use this data, please get an ngc file from dbGaP for " << projectId << ", and rerun with --ngc <file>." << std::endl;
                                encrypted = true;
                                continue;
                            }

                            auto const key = RemoteKey(added + 1);

                            info[key.filePath] = location.link;
                            info[key.service] = service;
                            info[key.region] = region;
                            info[key.qualityType] = file.noqual ? Accession::qualityTypeForLite : Accession::qualityTypeForFull;
                            if (projectId.has_value())
                                info[key.project] = projectId.value();
                            if (file.size.has_value())
                                info[key.fileSize] = file.size.value();
                            if (location.ceRequired)
                                info[key.CER] = "1";
                            if (location.payRequired)
                                info[key.payR] = "1";

                            auto const cache = sdl_result.getCacheFor(fl);
                            if (cache >= 0) {
                                auto const cacheFile = sdl_result.flat(cache);
                                info[key.cachePath] = cacheFile.second.link;
                                if (cacheFile.first.size.has_value())
                                    info[key.cacheSize] = cacheFile.first.size.value();
                                if (cacheFile.second.ceRequired)
                                    info[key.cacheCER] = "1";
                                if (cacheFile.second.payRequired)
                                    info[key.cachePayR] = "1";
                            }
                            added += 1;
                        }
                    } while (added == 0 && pass == 1);
                    
                    if (added == 0) {
                        std::cerr << "No usable source for " << query << " was found.\n" <<
                            query << " might be available in a different cloud provider or region." << std::endl;
                        if (encrypted)
                            std::cerr << "Or you can get an ngc file from dbGaP, and rerun with --ngc <file>." << std::endl;
                    }
                    else {
                        queryInfo[query]["remote"] = std::to_string(added);
                    }
                }
                else if (sdl_result.status == "404") {
                    // use the local data (see below)
                    LOG(5) << "Query '" << query << "' 404 " << sdl_result.message << std::endl;
                }
                else {
                    std::cerr << "Query " << query << ": Error " << sdl_result.status << " " << sdl_result.message << std::endl;
                }
            }
        }
        catch (Response2::DecodingError const &err) {
            LOG(1) << err << std::endl;
        }
        catch (std::domain_error const &de) {
            if (de.what() && strcmp(de.what(), "No query") == 0)
                ;
            else
                throw de;
        }
        catch (vdb::exception const &e) {
            LOG(1) << "Failed to talk to SDL" << std::endl;
            LOG(2) << e.failedCall() << " returned " << e.resultCode() << std::endl;

            std::cerr << e.msg << "." << std::endl;
            exit(EX_USAGE);
        }
        catch (...) {
            throw std::logic_error("Error communicating with NCBI");
        }
    }
}

data_sources data_sources::preload(CommandLine const &cmdline, Arguments const &parsed)
{
    return logging_state::testing_level() != 2 ? data_sources(cmdline, parsed, config->canUseSDL()) : data_sources(cmdline, parsed);
}
