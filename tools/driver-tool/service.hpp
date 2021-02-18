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

#pragma once

#include <string>
#include <vector>
#include <exception>
#include <stdexcept>

namespace vdb {
class exception : public std::runtime_error {
    std::string from;
    uint32_t rc;
public:

    const std::string msg;

    uint32_t resultCode() const { return rc; }
    std::string const &failedCall() const { return from; }
    exception(uint32_t rc, char const *from, char const *what)
    : std::runtime_error(what)
    , from(from)
    , rc(rc)
    {}
    exception(uint32_t rc, std::string const &from, std::string const &what,
        const std::string &message = "")
    : std::runtime_error(what)
    , from(from)
    , rc(rc)
    , msg(message)
    {}
};

class Service {
    void *obj;
    explicit Service(void *);
public:
    struct LocalInfo {
        struct FileInfo {
            std::string path, cachepath;
            size_t size;
            bool have;

            operator bool() const { return have; }
        } rundata, vdbcache;
    };

    class Response {
        void *obj;
        std::string text;

        Response(void *obj, char const *cstr)
        : obj(obj)
        , text(cstr)
        {}
        friend class Service;
    public:
        std::string const &responseText() const { return text; }

        Service::LocalInfo::FileInfo localInfo(  std::string const &accession
                                               , std::string const &name
                                               , std::string const &type) const;

        ~Response();

        friend std::ostream &operator <<(std::ostream &os, Response const &rhs);
    };
    static Service make();
    ~Service();
    
    void add(std::string const &term) const;
    void add(std::vector<std::string> const &terms) const;
    void setLocation(std::string const &value) const;
    void setPermissionsFile(std::string const &path) const;
    void setNGCFile(std::string const &path) const;

    Response response(std::string const &url, std::string const &version) const;

    static bool haveCloudProvider();
    static std::string CE_Token();
};
}
