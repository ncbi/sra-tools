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

namespace sratools {

struct source {
    std::string accession, localPath, remoteUrl, service, cachePath, fileSize;
    bool needCE, needPmt;
    bool haveLocalPath, haveCachePath, haveSize;
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
    using iterator = container::iterator;
    using const_iterator = container::const_iterator;
private:
    std::vector<data_source> sources;
    std::string ce_token_;
    bool have_ce_token;
public:
    /// @brief a lot can happen here
    ///
    /// NB. This will short-curcuit on a local file
    ///
    /// @param qry the run (or accession) to query
    data_sources(std::string const &qry);
    
    /// @brief informative only
    std::string const &ce_token() const {
        return ce_token_;
    }
    
    /// @brief set/unset CE Token environment variable
    void set_ce_token_env_var() const;

    /// @brief allows for-in loop
    const_iterator begin() const {
        return sources.begin();
    }
    /// @brief allows for-in loop
    const_iterator end() const {
        return sources.end();
    }
};

} // namespace sratools
