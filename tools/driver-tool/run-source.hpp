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

#pragma once
#include <string>
#include <vector>
#include "parse_args.hpp"

namespace sratools {

/// @brief Contains the source info for a VDB database.
struct source {
    std::string accession, localPath, remoteUrl, service, cachePath, fileSize;
    std::string projectId;
    bool needCE = false, needPmt = false;
    bool haveLocalPath = false, haveCachePath = false, haveSize = false, haveAccession = false;
    bool encrypted;
    
    std::string const &key() const {
        assert(haveAccession || haveLocalPath);
        return haveAccession ? accession : localPath;
    }
    bool isSimple() const {
        return (haveAccession || haveLocalPath)
            && accession == localPath
            && !(needCE || needPmt || haveCachePath || haveSize);
    }
};

/// @brief Contains the source info for a run and any associated vdbcache file.
class data_source {
    data_source() {}
    source run, vdbcache;
    bool haveVdbCache;
public:
    explicit data_source(source const &run) : run(run), haveVdbCache(false) {}
    data_source(source const &run, source const &vcache)
    : run(run)
    , vdbcache(vcache)
    , haveVdbCache(true)
    {}
    
    std::string const &key() const { return run.key(); }
    
    static data_source local_file(std::string const &file, std::string const &cache = "") {
        source result = {};
        result.localPath = file;
        result.haveLocalPath = true;
        result.cachePath = cache;
        result.haveCachePath = !cache.empty();

        return data_source(result);
    }
    void set_environment() const;
    Dictionary get_environment() const;
    std::string const &service() const { return run.haveLocalPath ? run.localPath : run.service; }
    bool encrypted() const { return run.encrypted; }
    std::string const &accession() const { return run.accession; }
    std::string const &projectId() const { return run.projectId; }
};

/// @brief Contains the response from SDL and/or local file info.
class data_sources {
public:
    using container = std::vector<data_source>;
private:
    // std::vector<data_source> sources;
    std::map<std::string, std::vector<data_source>> sources;
    std::string ce_token_;
    bool have_ce_token;

    data_sources(std::vector<std::string> const &runs);
    data_sources(std::vector<std::string> const &runs, bool withSDL);
    
    /// @brief add a data sources, creates container if needed
    ///
    /// @param source the data source
    void addSource(data_source && source)
    {
        auto const iter = sources.find(source.key());
        if (iter != sources.end())
            iter->second.emplace_back(std::move(source));
        else {
            auto key = source.key();
            sources.insert({key, container({std::move(source)})});
        }
    }
    
#if DEBUG || _DEBUGGING
    static void test_empty();
    static void test_vdbcache();
    static void test_2();
    static void test_top_error();
    static void test_inner_error();
#endif

public:
    /// @brief true if there are no sources
    bool empty() const {
        return sources.empty();
    }
    
    /// @brief informative only
    std::string const &ce_token() const {
        return ce_token_;
    }
    
    /// @brief set/unset CE Token environment variable
    void set_ce_token_env_var() const;

    /// @brief the data sources for an accession
    container const &sourcesFor(std::string const &accession) const
    {
        static auto const empty = container();
        auto const iter = sources.find(accession);
        return (iter != sources.end()) ? iter->second : empty;
    }

    /// @brief Call SDL with accesion/query list and process the results.
    /// Can use local file info if no response from SDL.
    static data_sources preload(std::vector<std::string> const &runs, ParamList const &parameters = {});
    
#if DEBUG || _DEBUGGING
    static void test() {
        test_empty();
        test_vdbcache();
        test_2();
        test_top_error();
        test_inner_error();
    }
#endif
};

} // namespace sratools
