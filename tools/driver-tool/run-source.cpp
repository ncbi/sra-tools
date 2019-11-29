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

#include "service.hpp"
using Service = vdb::Service;

using namespace constants;

namespace sratools {

static std::string config_or_default(char const *const config_node, char const *const default_value)
{
    auto const &from_config = config->get(config_node);
    return from_config ? from_config.value() : default_value;
}

struct SDL_unexpected_error : public std::logic_error
{
    SDL_unexpected_error(std::string const &what) : std::logic_error(std::string("unexpected response from srapath: ") + what) {}
};

static std::string getString(ncbi::JSONObject const &obj, char const *const member)
{
    auto const &value = obj.getValue(member);
    
    if (!value.isNull() && !value.isArray() && !value.isObject())
        return value.toString().toSTLString();
    throw SDL_unexpected_error(std::string("expected a string value labeled ") + member);
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
    throw SDL_unexpected_error(std::string("expected a scalar value labeled ") + member);
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
    throw SDL_unexpected_error(std::string("expected a boolean-like value labeled ") + member);
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

static std::string get_SDL_response(Service const &query, std::vector<std::string> const &runs, bool const haveCE)
{
    auto const &version_string = config_or_default("/repository/remote/version", resolver::version());
    auto const &url_string = config_or_default("/repository/remote/main/SDL.2/resolver-cgi", resolver::url());
    
    query.add(runs);

    if (location)
        query.setLocation(*location);

    if (perm && haveCE)
        query.setPermissionsFile(*perm);
    
    if (ngc)
        query.setNGCFile(*ngc);
    
    auto const &response = query.response(url_string, version_string);
    return response;
}

struct raw_response {
    struct ResponseEntry {
        struct FileEntry {
            struct LocationEntry {
                std::string link;
                std::string service;
                std::string region;
                std::string expirationDate;
                std::string encryptedForProjectId;
                bool ceRequired;
                bool payRequired;
                
                LocationEntry(ncbi::JSONObject const &obj)
                : ceRequired(false)
                , payRequired(false)
                {
                    link = getString(obj, "link");
                    service = getString(obj, "service");
                    region = getString(obj, "region");
                    getOptionalBoolOrString(&ceRequired, obj, "ceRequired");
                    getOptionalBoolOrString(&payRequired, obj, "payRequired");
                    getOptionalString(&expirationDate, obj, "expirationDate");
                    getOptionalString(&encryptedForProjectId, obj, "encryptedForProjectId");
                }
            };
            using Locations = std::vector<LocationEntry>;
            std::string object;
            std::string type;
            std::string name;
            std::string size;
            std::string md5;
            std::string format;
            std::string modificationDate;
            Locations locations;
            
            FileEntry(ncbi::JSONObject const &obj) {
                type = getString(obj, "type");
                name = getString(obj, "name");

                getOptionalString(&object, obj, "object");
                getOptionalString(&size, obj, "size");
                getOptionalString(&md5, obj, "md5");
                getOptionalString(&modificationDate, obj, "modificationDate");
                
                forEach(obj, "locations", [&](ncbi::JSONValue const &value) {
                    assert(value.isObject());
                    auto const &entry = LocationEntry(value.toObject());
                    locations.emplace_back(entry);
                });
            }
            Locations::const_iterator from_ncbi() const {
                return std::find_if(locations.begin(), locations.end(), [&](LocationEntry const &x) {
                    return x.service == "sra-ncbi" || x.service == "ncbi";
                });
            }
            Locations::const_iterator matching(LocationEntry const &other) const {
                return std::find_if(locations.begin(), locations.end(), [&](LocationEntry const &x) {
                    return x.region == other.region && x.service == other.service;
                });
            }
            LocationEntry const &best_matching(LocationEntry const &other) const {
                if (locations.size() > 1) {
                    auto const match = matching(other);
                    if (match != locations.end())
                        return *match;
                    
                    auto const ncbi = from_ncbi();
                    if (ncbi != locations.end())
                        return *ncbi;
                }
                return locations[0];
            }
            source make_source(LocationEntry const &location, std::string const &accession, Service::LocalInfo::FileInfo const &fileInfo) const {
                auto result = source();
                
                if ((result.haveLocalPath = fileInfo.have) != false) {
                    result.localPath = fileInfo.path;
                    if ((result.haveCachePath = !fileInfo.cachepath.empty()) != false)
                        result.cachePath = fileInfo.cachepath;
                    if ((result.haveSize = fileInfo.size != 0) != false)
                        result.fileSize = fileInfo.size;
                }
                result.accession = accession;
                result.service = location.service;
                result.remoteUrl = location.link;
                result.needCE = location.ceRequired;
                result.needPmt = location.payRequired;
                result.encrypted = !location.encryptedForProjectId.empty();
                result.projectId = location.encryptedForProjectId;
                result.haveAccession = true;
                
                return result;
            }
        };
        using Files = std::vector<FileEntry>;
        std::string accession;
        std::string status;
        std::string message;
        Files files;

        ResponseEntry(ncbi::JSONObject const &obj) {
            status = getString(obj, "status");
            message = getString(obj, status == "200" ? "msg" : "message");
            accession = getString(obj, status == "200" ? "bundle" : "accession");
            
            forEach(obj, "files", [&](ncbi::JSONValue const &value) {
                assert(value.isObject());
                auto const &entry = FileEntry(value.toObject());
                files.emplace_back(entry);
            });
        }
        Files::const_iterator matching(FileEntry const &entry, std::string const &type) const {
            return std::find_if(files.begin(), files.end(), [&](FileEntry const &x) {
                return x.object == entry.object && x.type == type;
            });
        }
        template <typename F>
        void process(std::string const &accession, Service const &service, F &&func) const
        {
            for (auto & file : files) {
                if (file.type != "sra") continue;
                if (hasSuffix(".pileup", file.name)) continue; // TODO: IS THIS CORRECT
                
                auto const vcache = matching(file, "vdbcache");
                if (vcache == files.end()) {
                    for (auto &location : file.locations) {
                        auto const &run_source = file.make_source(location, accession, service.localInfo2(accession, file.name));
                        func(data_source(run_source));
                    }
                }
                else {
                    for (auto &location : file.locations) {
                        auto const &run_source = file.make_source(location, accession, service.localInfo2(accession, file.name));
                        auto const &cache_source = file.make_source(vcache->best_matching(location), accession, service.localInfo2(accession, vcache->name));
                        func(data_source(run_source, cache_source));
                    }
                }
            }
        }
    };
    using Responses = std::vector<ResponseEntry>;
    Responses response;
    std::string status;
    std::string message;
    std::string nextToken;
    
    raw_response(ncbi::JSONObject const &obj) {
        status = "200";
        message = "OK";
#if DEBUG || _DEBUGGING
        auto const version = getString(obj, "version");
        assert(version == "2" || version == "unstable");
#endif
        getOptionalString(&status, obj, "status");
        getOptionalString(&message, obj, "message");
        getOptionalString(&nextToken, obj, "nextToken");
        
        forEach(obj, "result", [&](ncbi::JSONValue const &value) {
            assert(value.isObject());
            auto const &entry = ResponseEntry(value.toObject());
            response.emplace_back(entry);
        });
    }
};

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

data_sources data_sources::preload(std::vector<std::string> const &runs,
                                   ParamList const &parameters)
{
    auto result = data_sources(Service::CE_Token());
    auto const addSource = [&](data_source &&source) {
        result.addSource(std::move(source));
    };
    auto local = std::map<std::string, Service::LocalInfo>();
    auto const &service = Service::make();

    for (auto & run : runs) {
        local[run] = service.localInfo(run);
    }
    try {
        auto const response = get_SDL_response(service, runs, result.have_ce_token);
        LOG(7) << "SDL response:\n" << response << std::endl;

        auto const jvRef = ncbi::JSON::parse(ncbi::String(response));
        auto const &obj = jvRef->toObject();
        auto const version = getString(obj, "version");
        if (version == "2" || version == "unstable") {
            auto const &raw = raw_response(obj);
            LOG(7) << "Parsed SDL Response" << std::endl;
            
            for (auto &sdl_result : raw.response) {
                auto const localInfo = local.find(sdl_result.accession);
                
                LOG(6) << "Accession " << sdl_result.accession << " " << sdl_result.status << " " << sdl_result.message << std::endl;
                if (sdl_result.status == "200") {
                    sdl_result.process(sdl_result.accession, service, addSource);
                    local.erase(localInfo); // remove since we used it here
                }
                else if (sdl_result.status == "404" && localInfo->second.rundata) {
                    // use the local data (see below)
                }
                else {
                    std::cerr << "Accession " << sdl_result.accession << ": Error " << sdl_result.status << " " << sdl_result.message << std::endl;
                }
            }
        }
        else {
            throw SDL_unexpected_error(std::string("unexpected version ") + version);
        }
    }
    catch (vdb::exception const &e) {
        LOG(1) << "Failed to talk to SDL" << std::endl;
        LOG(2) << e.failedCall() << " returned " << e.resultCode() << std::endl;
    }
    catch (SDL_unexpected_error const &e) {
        throw e;
    }
    catch (...) {
        throw SDL_unexpected_error("unparsable response");
    }
    // try to make sources for local files
    for (auto const &info : local) {
        if (info.second.rundata) {
            source rsource = {};
            rsource.accession = info.first;
            rsource.localPath = info.second.rundata.path;
            rsource.haveLocalPath = true;
            rsource.cachePath = info.second.rundata.cachepath;
            rsource.haveCachePath = !rsource.cachePath.empty();
            if (!info.second.vdbcache)
                result.addSource(data_source(rsource));
            else {
                source vsource = {};
                vsource.accession = info.first;
                vsource.localPath = info.second.vdbcache.path;
                vsource.haveLocalPath = true;
                vsource.cachePath = info.second.vdbcache.cachepath;
                vsource.haveCachePath = !vsource.cachePath.empty();

                result.addSource(data_source(rsource, vsource));
            }
        }
    }
    return result;
}

#if DEBUG || _DEBUGGING
void data_sources::test_vdbcache() {
    auto const testJSON = R"###(
{
    "version": "unstable",
    "result": [
        {
            "bundle": "SRR850901",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub_files|SRR850901",
                    "type": "reference_fasta",
                    "name": "SRR850901.contigs.fasta",
                    "size": 5417080,
                    "md5": "e0572326534bbf310c0ac744bd51c1ec",
                    "modificationDate": "2016-11-19T08:00:46Z",
                    "locations": [
                        {
                            "link": "https://sra-download.ncbi.nlm.nih.gov/traces/sra7/SRZ/000850/SRR850901/SRR850901.contigs.fasta",
                            "service": "sra-ncbi",
                            "region": "public"
                        }
                    ]
                },
                {
                    "object": "srapub_files|SRR850901",
                    "type": "sra",
                    "name": "SRR850901.pileup",
                    "size": 842809,
                    "md5": "323d0b76f741ae0b71aeec0176645301",
                    "modificationDate": "2016-11-20T07:38:19Z",
                    "locations": [
                        {
                            "link": "https://sra-download.ncbi.nlm.nih.gov/traces/sra14/SRZ/000850/SRR850901/SRR850901.pileup",
                            "service": "sra-ncbi",
                            "region": "public"
                        }
                    ]
                },
                {
                    "object": "srapub|SRR850901",
                    "type": "sra",
                    "name": "SRR850901",
                    "size": 323741972,
                    "md5": "5e213b2319bd1af17c47120ee8b16dbc",
                    "modificationDate": "2016-11-19T07:35:20Z",
                    "locations": [
                        {
                            "link": "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos/sra-pub-run-2/SRR850901/SRR850901.2",
                            "service": "ncbi",
                            "region": "be-md"
                        }
                    ]
                },
                {
                    "object": "srapub|SRR850901",
                    "type": "vdbcache",
                    "name": "SRR850901.vdbcache",
                    "size": 17615526,
                    "md5": "2eb204cedf5eefe45819c228817ee466",
                    "modificationDate": "2016-02-08T16:31:35Z",
                    "locations": [
                        {
                            "link": "https://sra-download.ncbi.nlm.nih.gov/traces/sra11/SRR/000830/SRR850901.vdbcache",
                            "service": "sra-ncbi",
                            "region": "public"
                        }
                    ]
                }
            ]
        }
    ]
}
)###";
    auto const jvRef = ncbi::JSON::parse(ncbi::String(testJSON));
    auto const &obj = jvRef->toObject();
    auto const &raw = raw_response(obj);
    
    assert(raw.response.size() == 1);
    auto passed = false;
    auto files = 0;
    auto sras = 0;
    auto srrs = 0;
    
    for (auto &file : raw.response[0].files) {
        ++files;
        if (file.type != "sra") continue;
        ++sras;
        if (hasSuffix(".pileup", file.name)) continue;
        ++srrs;
        
        assert(file.locations.size() == 1);
        
        auto const vcache = raw.response[0].matching(file, "vdbcache");
        assert(vcache != raw.response[0].files.end());
        
        auto const &location = file.locations[0];
        auto const &vdbcache = vcache->best_matching(location);
        assert(vdbcache.service == "sra-ncbi");
        assert(vdbcache.region == "public");
        
        passed = true;
    }
    assert(files == 4);
    assert(sras == 2);
    assert(srrs == 1);
    assert(passed);
}

void data_sources::test_2() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
        {
            "bundle": "SRR000001",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000001",
                    "type": "sra",
                    "name": "SRR000001",
                    "size": 312527083,
                    "md5": "9bde35fefa9d955f457e22d9be52bcd9",
                    "modificationDate": "2015-04-08T02:54:13Z",
                    "locations": [
                        {
                            "link": "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos/sra-pub-run-5/SRR000001/SRR000001.4",
                            "service": "ncbi",
                            "region": "be-md"
                        }
                    ]
                }
            ]
        },
        {
            "bundle": "SRR000002",
            "status": 200,
            "msg": "ok",
            "files": [
                {
                    "object": "srapub|SRR000002",
                    "type": "sra",
                    "name": "SRR000002",
                    "size": 358999861,
                    "md5": "22c8b51d7caae31920df904a6b4164c0",
                    "modificationDate": "2012-01-19T20:14:00Z",
                    "locations": [
                        {
                            "link": "https://sra-downloadb.be-md.ncbi.nlm.nih.gov/sos/sra-pub-run-5/SRR000002/SRR000002.3",
                            "service": "ncbi",
                            "region": "be-md"
                        }
                    ]
                }
            ]
        }
    ]
}
)###";
    auto const jvRef = ncbi::JSON::parse(ncbi::String(testJSON));
    auto const &obj = jvRef->toObject();
    auto const &raw = raw_response(obj);
    
    assert(raw.response.size() == 2);
}

void data_sources::test_top_error() {
    auto const testJSON = R"###(
{
    "version": "2",
    "status": 400,
    "message": "Wrong service or/and region name"
})###";
    auto const jvRef = ncbi::JSON::parse(ncbi::String(testJSON));
    auto const &obj = jvRef->toObject();
    auto const &raw = raw_response(obj);
    
    assert(raw.response.size() == 0);
    assert(raw.status == "400");
}

void data_sources::test_empty() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
    ]
})###";
    auto const jvRef = ncbi::JSON::parse(ncbi::String(testJSON));
    auto const &obj = jvRef->toObject();
    auto const &raw = raw_response(obj);
    
    assert(raw.response.size() == 0);
}

void data_sources::test_inner_error() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
        {
            "accession": "SRR867664",
            "status": 404,
            "message": "No data at given location.region"
        }
    ]
})###";
    auto const jvRef = ncbi::JSON::parse(ncbi::String(testJSON));
    auto const &obj = jvRef->toObject();
    auto const &raw = raw_response(obj);
    
    assert(raw.response.size() == 1);
    assert(raw.response[0].status == "404");
}
#endif // DEBUG || _DEBUGGING

} // namespace sratools
