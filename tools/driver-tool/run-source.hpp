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
#include "parse_args.hpp"

namespace sratools {

struct source {
    std::string accession, localPath, remoteUrl, service, cachePath, fileSize;
    bool needCE = false, needPmt = false;
    bool haveLocalPath = false, haveCachePath = false, haveSize = false, haveAccession = false;
    
    std::string const &key() const {
        assert(haveAccession || haveLocalPath);
        return haveAccession ? accession : localPath;
    }
};

class data_source {
    data_source() {}
    source run, vdbcache;
    bool haveVdbCache;
public:
    data_source(source const &run) : run(run), haveVdbCache(false) {}
    data_source(source const &run, source const &vcache)
    : run(run)
    , vdbcache(vcache)
    , haveVdbCache(true)
    {}
    
    std::string const &key() const { return run.key(); }
    
    static data_source local_file(std::string const &file) {
        source result = {};
        result.localPath = file;
        result.haveLocalPath = true;
        
        return data_source(result);
    }
    void set_environment() const;
    std::string const &service() const { return run.service; }
};

/// @brief contains the responce from srapath names function
class data_sources {
public:
    using container = std::vector<data_source>;
private:
    // std::vector<data_source> sources;
    std::map<std::string, std::vector<data_source>> sources;
    std::string ce_token_;
    bool have_ce_token;

    /// @brief a lot can happen here
    ///
    /// NB. This will short-curcuit on a local file
    ///
    /// @param responseJSON response from srapath
    data_sources(std::string const &responseJSON);
    data_sources() {}
    
    /// @brief add a data sources, creates container if needed
    ///
    /// @param source the data source
    void addSource(data_source && source)
    {
        auto const iter = sources.find(source.key());
        if (iter != sources.end())
            iter->second.emplace_back(source);
        else
            sources.insert({source.key(), container({source})});
    }
        
#if DEBUG || _DEBUGGING
    static void test_local_and_remote() {
        static char const responseJSON[] =
        "{\"count\": 1,\"CE-Token\": null,\"responses\": [{\"accession\": \"SRR10063844\", \"itemType\": \"sra\", \"size\": 37644943, \"local\": \"/netmnt/traces04/sra44/SRR/009827/SRR10063844\", \"remote\": [{ \"path\": \"https://sra-download.ncbi.nlm.nih.gov/traces/sra44/SRR/009827/SRR10063844\", \"service\": \"sra-ncbi\", \"CE-Required\": false, \"Payment-Required\": false }]}]}";
        auto const sources = data_sources(responseJSON);
        assert(sources.sourcesFor("SRR10063844").empty() == false);
    }
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

    container const &sourcesFor(std::string const &accession) const
    {
        static auto const empty = container();
        auto const iter = sources.find(accession);
        return (iter != sources.end()) ? iter->second : empty;
    }
    
    static data_sources preload(std::vector<std::string> const &runs, ParamList const &parameters);
    
#if DEBUG || _DEBUGGING
    static void test() {
        test_local_and_remote();
    }
#endif
};

} // namespace sratools
