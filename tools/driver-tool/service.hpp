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
 *  KService
 *
 */

#include <string>
#include <vector>
#include <exception>

namespace vdb {
class exception : public std::runtime_error {
    uint32_t rc;
    std::string from;
public:
    uint32_t resultCode() const { return rc; }
    std::string const &failedCall() const { return from; }
    exception(uint32_t rc, char const *from, char const *what)
    : rc(rc)
    , from(from)
    , std::runtime_error(what)
    {}
    exception(uint32_t rc, std::string const &from, std::string const &what)
    : rc(rc)
    , from(from)
    , std::runtime_error(what)
    {}
};

class Service {
    void *obj;
    Service(void *);
public:
    static Service make();
    ~Service();
    
    void add(std::string const &term) const;
    void add(std::vector<std::string> const &terms) const;
    void setLocation(std::string const &value) const;
    void setPermissionsFile(std::string const &path) const;
    void setNGCFile(std::string const &path) const;
    std::string response(std::string const &url, std::string const &version) const;
    
    enum DataType { run, vdbcache };
    
    struct LocalInfo {
        struct FileInfo {
            std::string path, cachepath;
            size_t size;
            bool have;

            operator bool() const { return have; }
        } rundata, vdbcache;
    };
    LocalInfo localInfo(std::string const &accession) const;
    LocalInfo::FileInfo localInfo2(std::string const &accession, std::string const name) const;
    static std::string CE_Token();
};
}
