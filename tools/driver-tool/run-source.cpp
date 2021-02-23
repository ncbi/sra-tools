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

#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <algorithm>

#include "support2.hpp"
#include "globals.hpp"
#include "constants.hpp"
#include "debug.hpp"
#include "proc.hpp"
#include "util.hpp"
#include "opt_string.hpp"
#include "run-source.hpp"
#include "sratools.hpp"
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

struct SDL_unexpected_error : public std::runtime_error
{
    SDL_unexpected_error(std::string const &what) : std::runtime_error(std::string("unexpected response from SDL: ") + what) {}
};

static std::string getString(ncbi::JSONObject const &obj, char const *const member)
{
    try {
        auto const &value = obj.getValue(member);

        if (!value.isNull() && !value.isArray() && !value.isObject())
            return value.toString().toSTLString();
    }
    catch (...) {}
    throw SDL_unexpected_error(std::string("expected a string value labeled ") + member);
}

static opt_string getOptionalString(ncbi::JSONObject const &obj, char const *const member)
{
    auto result = opt_string();
    try {
        auto const &value = obj.getValue(member); // if throws, return false

        if (value.isNull())
            return result;

        if (!value.isArray() && !value.isObject()) {
            auto const &string = value.toString();
            result = opt_string(string.toSTLString());
            return result;
        }
    }
    catch (...) {
        // member not found
        return result;
    }
    // member existed but value is an array or object
    throw SDL_unexpected_error(std::string("expected a scalar value labeled ") + member);
}

static bool getOptionalBool(ncbi::JSONObject const &obj, char const *const member, bool const default_value)
{
    try {
        auto const &value = obj.getValue(member);
        if (value.isBoolean())
            return value.toBoolean();
    }
    catch (...) {
        // member not found
        return default_value;
    }
    // member existed but value is not a boolean
    throw SDL_unexpected_error(std::string("expected a boolean value labeled ") + member);
}

template <typename F>
static void forEach(ncbi::JSONObject const &obj, char const *member, F && f)
{
    if (!obj.exists(member)) return;

    auto const &array = obj.getValue(member).toArray();
    auto const n = array.count();
    for (auto i = decltype(n)(0); i < n; ++i) {
        auto const &value = array.getValue(i);
        f(value);
    }
}

static Service::Response get_SDL_response(Service const &query, std::vector<std::string> const &runs, bool const haveCE)
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

    return query.response(url_string, version_string);
}

static inline std::string guess_region(opt_string const &region_, std::string const &service)
{
    if (region_) return region_.value();
    if (service == "ncbi")
        return "be-md";
    throw std::runtime_error("no default region for service");
}

static inline std::string guess_type(opt_string const &type_, opt_string const &object)
{
    if (type_) return type_.value();
    if (object) {
        auto const &name = object.value();
        auto const pipe_symbol_at = name.find('|');
        if (pipe_symbol_at != std::string::npos) {
            auto const type_from_name = name.substr(0, pipe_symbol_at);
            if (type_from_name == "wgs")
                return "sra";
        }
    }
    throw std::runtime_error("no type");
}

/// @brief holds SDL version 2 response
/// @Note member names generally match the corresponding member names in the SDL response JSON
struct Response2 {
    /// @brief holds a single element of the result array
    struct ResultEntry {
        /// @brief holds a single file (element of the files array)
        struct FileEntry {
            /// @brief holds a single element of the locations array
            struct LocationEntry {
                std::string link;           // url to data
                std::string service;        // who is providing the data
                std::string region;         // as defined by service
                opt_string region_;         // as defined by service
                opt_string expirationDate;
                opt_string projectId;       // encryptedForProjectId
                bool ceRequired;            // compute environment required
                bool payRequired;           // data from this location is not free
                
                LocationEntry(ncbi::JSONObject const &obj)
                : link(getString(obj, "link"))
                , service(getString(obj, "service"))
                , region_(getOptionalString(obj, "region"))
                , expirationDate(getOptionalString(obj, "expirationDate"))
                , projectId(getOptionalString(obj, "encryptedForProjectId"))
                , ceRequired(getOptionalBool(obj, "ceRequired", false))
                , payRequired(getOptionalBool(obj, "payRequired", false))
                {
                    region = guess_region(region_, service);
                }

                /// @brief is the data from this location readable using the encryption from other location
                /// @Note if not encrypted then this location is always readable
                bool readableWith(LocationEntry const &other) const {
                    return !projectId ? true : !other.projectId ? false : projectId.value() == other.projectId.value();
                }
            };
            using Locations = std::vector<LocationEntry>;
            opt_string type_;
            std::string type;
            std::string name;
            opt_string object;
            opt_string size;
            opt_string md5;
            opt_string format;
            opt_string modificationDate;
            Locations locations;
            
            FileEntry(ncbi::JSONObject const &obj)
            : type_(getOptionalString(obj, "type"))
            , name(getString(obj, "name"))
            , object(getOptionalString(obj, "object"))
            , size(getOptionalString(obj, "size"))
            , md5(getOptionalString(obj, "md5"))
            , format(getOptionalString(obj, "format"))
            , modificationDate(getOptionalString(obj, "modificationDate"))
            {
                type = guess_type(type_, object);
                forEach(obj, "locations", [&](ncbi::JSONValue const &value) {
                    assert(value.isObject());
                    auto const &entry = LocationEntry(value.toObject());
                    locations.emplace_back(entry);
                });
            }

            /// @brief find the location that is provided by NCBI/NLM/NIH
            /// @Note might be encrypted
            Locations::const_iterator from_ncbi() const {
                return std::find_if(locations.begin(), locations.end(), [&](LocationEntry const &x) {
                    return x.service == "sra-ncbi" || x.service == "ncbi";
                });
            }

            /// @brief find the location that is the same service and region
            Locations::const_iterator matching(LocationEntry const &other) const {
                return std::find_if(locations.begin(), locations.end(), [&](LocationEntry const &x) {
                    return x.service == other.service && x.region == other.region;
                });
            }

            /// @brief find the location that is the best match for another location
            /// @Note same service/region, else NCBI and compatible encryption, else any compatible encryption
            Locations::const_iterator best_matching(LocationEntry const &other) const {
                auto const match = matching(other);
                if (match != locations.end())
                    return match;

                auto const ncbi = from_ncbi();
                if (ncbi != locations.end() && ncbi->readableWith(other))
                    return ncbi;

                return std::find_if(locations.begin(), locations.end(), [&](LocationEntry const &x) {
                    return x.readableWith(other);
                });
            }

            /// @brief This is part of the interface to the rest of the program
            source make_source(LocationEntry const &location, std::string const &accession, Service::LocalInfo::FileInfo const &fileInfo) const {
                auto result = source();
                
                if ((result.haveLocalPath = fileInfo.have) != false) {
                    result.localPath = fileInfo.path;
                    if ((result.haveCachePath = !fileInfo.cachepath.empty()) != false)
                        result.cachePath = fileInfo.cachepath;
                    if ((result.haveSize = (fileInfo.size != 0)) != false)
                        result.fileSize = std::to_string(fileInfo.size);
                }
                result.accession = accession;
                result.service = location.service;
                result.remoteUrl = location.link;
                result.needCE = location.ceRequired;
                result.needPmt = location.payRequired;
                result.encrypted = location.projectId;
                result.projectId = result.encrypted ? location.projectId.value() : std::string();
                result.haveAccession = true;
                
                return result;
            }
        };
        using Files = std::vector<FileEntry>;
        std::string query;
        std::string status;
        std::string message;
        Files files;

        ResultEntry(ncbi::JSONObject const &obj)
        : status(getString(obj, "status"))
        {
            message = getString(obj, "msg");
            query = getString(obj, "bundle");
            
            forEach(obj, "files", [&](ncbi::JSONValue const &value) {
                assert(value.isObject());
                auto const &entry = FileEntry(value.toObject());
                files.emplace_back(entry);
            });
        }

        /// @brief find the file that matches but has a different type
        Files::const_iterator matching(FileEntry const &entry, std::string const &type) const {
            return std::find_if(files.begin(), files.end(), [&](FileEntry const &x) {
                return x.object == entry.object && x.type == type;
            });
        }

        /// @brief Filter files and join to matching vdbcache file.
        ///
        /// It is probably best to do this here in the version specific code.
        ///
        /// @param accession containing accession, aka bundle.
        /// @param response for local file lookups.
        /// @param func callback, is called for each data source object.
        ///
        /// @Note This is the *primary* means by which information is handed out to the rest of the program.
        template <typename F>
        void process(std::string const &accession, Service::Response const &response, F &&func) const
        {
            for (auto & file : files) {
                if (file.type != "sra") continue;
                if (ends_with(".pileup", file.name)) continue; // TODO: IS THIS CORRECT

                auto const vcache = matching(file, "vdbcache");
                if (vcache == files.end()) {
                    // there is no vdbcache with this object, nothing to join
                    for (auto &location : file.locations) {
                        // if the user had run prefetch and not moved the
                        // downloaded files, this should find them.
                        auto const &run_source = file.make_source(location, accession, response.localInfo(accession, file.name, file.type));
                        func(std::move(data_source(run_source)));
                    }
                }
                else {
                    // there is a vdbcache with this object, do the join
                    for (auto &location : file.locations) {
                        auto const &run_source = file.make_source(location, accession, response.localInfo(accession, file.name, file.type));
                        auto const &best = vcache->best_matching(location);
                        if (best != vcache->locations.end()) {
                            auto const &cache_source = file.make_source(*best, accession, response.localInfo(accession, vcache->name, vcache->type));
                            func(std::move(data_source(run_source, cache_source)));
                        }
                        else {
                            // there was no matching vdbcache for this source
                            func(std::move(data_source(run_source)));
                        }
                    }
                }
            }
        }
    };
    using Results = std::vector<ResultEntry>;
    Results result;
    std::string status;
    std::string message;
    opt_string nextToken;
    
    Response2(ncbi::JSONObject const &obj)
    : status(getOptionalString(obj, "status").value_or("200"))  ///< there is no status when there is no error
    , message(getOptionalString(obj, "message").value_or("OK")) ///< there is no message when there is no error
    , nextToken(getOptionalString(obj, "nextToken"))
    {
#if DEBUG || _DEBUGGING
        // caller should have checked this first
        auto const version = getString(obj, "version");
        assert(version == "2" || version == "unstable");
#endif
        forEach(obj, "result", [&](ncbi::JSONValue const &value) {
            assert(value.isObject());
            auto const &entry = ResultEntry(value.toObject());
            result.emplace_back(entry);
        });
    }
};

/// @brief get run environment variables
Dictionary data_source::get_environment() const
{
    auto result = Dictionary();
    auto const names = env_var::names();

    if (!haveVdbCache && run.isSimple())
        goto RETURN_ENV_VARS;

    result[names[env_var::REMOTE_URL]] = run.remoteUrl;
    if (run.haveCachePath)
        result[names[env_var::CACHE_URL]] = run.cachePath;
    if (run.haveLocalPath)
        result[names[env_var::LOCAL_URL]] = run.localPath;
    if (run.haveSize)
        result[names[env_var::SIZE_URL]] = run.fileSize;
    if (run.needCE)
        result[names[env_var::REMOTE_NEED_CE]] = "1";
    if (run.needPmt)
        result[names[env_var::REMOTE_NEED_PMT]] = "1";

    if (haveVdbCache) {
        result[names[env_var::REMOTE_VDBCACHE]] = vdbcache.remoteUrl;
        if (vdbcache.haveCachePath)
            result[names[env_var::CACHE_VDBCACHE]] = vdbcache.cachePath;
        if (vdbcache.haveLocalPath)
            result[names[env_var::LOCAL_VDBCACHE]] = vdbcache.localPath;
        if (vdbcache.haveSize)
            result[names[env_var::SIZE_VDBCACHE]] = vdbcache.fileSize;
        if (vdbcache.needCE)
            result[names[env_var::CACHE_NEED_CE]] = "1";
        if (haveVdbCache && vdbcache.needPmt)
            result[names[env_var::CACHE_NEED_PMT]] = "1";
    }
RETURN_ENV_VARS:
    return result;
}

#define SETENV(VAR, VAL) env_var::set(env_var::VAR, VAL.c_str())
#define SETENV_IF(EXPR, VAR, VAL) env_var::set(env_var::VAR, (EXPR) ? VAL.c_str() : NULL)
#define SETENV_BOOL(VAR, VAL) env_var::set(env_var::VAR, (VAL) ? "1" : NULL)

/// @brief set/unset CE Token environment variable
void data_sources::set_ce_token_env_var() const {
    SETENV_IF(have_ce_token, CE_TOKEN, ce_token_);
}

std::pair<std::vector<std::string>, std::vector<std::string>> split_by_type(std::vector<std::string> const &runs)
{
    std::vector<std::string> sra, nonsra;
    std::set<std::string> seen;

    for (auto && i : runs) {
        if (!seen.insert(i).second) continue;

        if (isSRAPattern(i))
            sra.push_back(i);
        else
            nonsra.push_back(i);
    }
    return {sra, nonsra};
}

data_sources::data_sources(std::vector<std::string> const &runs)
{
    have_ce_token = false;

    auto notfound = std::set<std::string>(runs.begin(), runs.end());
    for (auto const &run : notfound) {
        source s = {};
        s.accession = run;
        s.localPath = run;
        s.haveLocalPath = true;
        addSource(data_source(s));
    }
}

data_sources::data_sources(std::vector<std::string> const &runs, bool withSDL)
{
    auto const havePerm = perm != nullptr;
    auto const canSendCE = config->canSendCEToken();
    if (logging_state::is_dry_run()) ; else assert(!(havePerm && !canSendCE));

    auto const &ceToken = Service::CE_Token();
    if (logging_state::is_dry_run()) ; else assert(!(havePerm && ceToken.empty()));

    have_ce_token = canSendCE && !ceToken.empty();
    if (have_ce_token) ce_token_ = ceToken;
    auto not_processed = std::set<std::string>(runs.begin(), runs.end());

    auto run_query = [&](std::vector<std::string> const &terms) {
        auto const &service = Service::make();
        auto const &response = get_SDL_response(service, terms, have_ce_token);
        LOG(8) << "SDL response:\n" << response << std::endl;

        auto const jvRef = ncbi::JSON::parse(ncbi::String(response.responseText()));
        auto const &obj = jvRef->toObject();
        auto const version = getString(obj, "version");
        auto const version_is = [&](std::string const &vers) {
            return version == vers || (version == "unstable" && vers == resolver::unstable_version());
        };

        if (version_is("2")) {
            auto const &raw = Response2(obj);
            LOG(7) << "Parsed SDL Response" << std::endl;

            for (auto &sdl_result : raw.result) {
                auto const &query = sdl_result.query;

                LOG(6) << "Query " << query << " " << sdl_result.status << " " << sdl_result.message << std::endl;
                if (sdl_result.status == "200") {
                    unsigned added = 0;

                    sdl_result.process(query, response, [&](data_source &&source) {
                        if (havePerm && source.encrypted()) {
                            std::cerr << "Accession " << source.accession() << " is encrypted for " << source.projectId() << std::endl;
                        }
                        else {
                            addSource(std::move(source));
                            added += 1;
                        }
                    });
                    if (added == 0) {
                        std::cerr
                        << "Accession " << query << " might be available in a different region or on a different cloud provider." << std::endl
                        << "Or you can get an ngc file from dbGaP, and rerun with --ngc <file>." << std::endl;
                    }
                    else {
                        auto const localInfo = not_processed.find(query);
                        not_processed.erase(localInfo); // remove since we found and processed it
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
        else {
            throw SDL_unexpected_error(std::string("unexpected version ") + version);
        }
    };
    if (withSDL) {
        try {
            std::set<std::string> seen;
            for (auto && i : runs) {
                if (!seen.insert(i).second) continue;
                if (pathExists(i)) continue;

                run_query({i});
            }
        }
        catch (vdb::exception const &e) {
            LOG(1) << "Failed to talk to SDL" << std::endl;
            LOG(2) << e.failedCall() << " returned " << e.resultCode() << std::endl;

            std::cerr << e.msg << "." << std::endl;
            exit(EX_USAGE);

        }
        catch (SDL_unexpected_error const &e) {
            LOG(1) << e.what() << std::endl;
            throw std::logic_error("Error communicating with NCBI");
        }
        catch (...) {
            throw std::logic_error("Error communicating with NCBI");
        }
    }
    // make "file" sources for unprocessed or unrecognized query items
    for (auto const &run : not_processed) {
        source s = {};
        s.accession = run;
        s.localPath = run;
        s.haveLocalPath = true;
        addSource(data_source(s));
    }
}

data_sources data_sources::preload(std::vector<std::string> const &runs,
                                   ParamList const &parameters)
{
    return logging_state::testing_level() != 2 ? data_sources(runs, config->canUseSDL()) : data_sources(runs);
}

#if DEBUG || _DEBUGGING
// these tests all use asserts because these are all hard-coded values

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
    auto const &raw = Response2(obj);
    
    assert(raw.result.size() == 1);
    auto passed = false;
    auto files = 0;
    auto sras = 0;
    auto srrs = 0;
    
    for (auto &file : raw.result[0].files) {
        ++files;
        if (file.type != "sra") continue;
        ++sras;
        if (ends_with(".pileup", file.name)) continue;
        ++srrs;
        
        assert(file.locations.size() == 1);
        
        auto const vcache = raw.result[0].matching(file, "vdbcache");
        assert(vcache != raw.result[0].files.end());
        
        auto const &location = file.locations[0];
        auto const &vdbcache = vcache->best_matching(location);
        assert(vdbcache != vcache->locations.end());
        assert(vdbcache->service == "sra-ncbi");
        assert(vdbcache->region == "public");
        
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
    auto const &raw = Response2(obj);
    
    assert(raw.result.size() == 2);
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
    auto const &raw = Response2(obj);
    
    assert(raw.result.size() == 0);
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
    auto const &raw = Response2(obj);
    
    assert(raw.result.size() == 0);
}

void data_sources::test_inner_error() {
    auto const testJSON = R"###(
{
    "version": "2",
    "result": [
        {
            "bundle": "SRR867664",
            "status": 404,
            "msg": "No data at given location.region"
        }
    ]
})###";
    auto const jvRef = ncbi::JSON::parse(ncbi::String(testJSON));
    auto const &obj = jvRef->toObject();
    auto const &raw = Response2(obj);
    
    assert(raw.result.size() == 1);
    assert(raw.result[0].status == "404");
}
#endif // DEBUG || _DEBUGGING

} // namespace sratools
