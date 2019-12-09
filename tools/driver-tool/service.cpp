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

#include <exception>
#include <cstdint>

#include "debug.hpp"
#include "service.hpp"

namespace vdb {

    #include <klib/text.h>
    #include <vfs/services.h>
    #include <vfs/services-priv.h>
    #include <cloud/cloud.h>
    #include <cloud/manager.h>

    class Cloud {
        class Manager {
            vdb::CloudMgr *mgr;
        public:
            Manager() {
                auto const rc = vdb::CloudMgrMake(&mgr, nullptr, nullptr);
                if (rc)
                    throw vdb::exception(rc, "CloudMgrMake", "");
            }
            ~Manager() { vdb::CloudMgrRelease(mgr); }
            
            vdb::Cloud *current() const {
                vdb::Cloud *result = nullptr;
                auto const rc = vdb::CloudMgrGetCurrentCloud(mgr, &result);
                if (rc)
                    throw vdb::exception(rc, "CloudMgrGetCurrentCloud", "");
                return result;
            }
        };
        vdb::Cloud *obj;
    public:
        ~Cloud() { vdb::CloudRelease(obj); }
        Cloud() {
            auto const &mgr = Manager();
            obj = mgr.current();
        }
        std::string token() const {
            auto result = std::string();
            
            vdb::String const *s;
            auto const rc = vdb::CloudMakeComputeEnvironmentToken(obj, &s);
            if (rc == 0 && s) {
                result.assign(s->addr, StringSize(s));
                free((void *)s);
            }
            return result;
        }
    };
    
    extern "C" {
        extern rc_t VPathMakeString(VPath const *, String const **);
        extern rc_t VPathRelease(VPath const *);
    }
    class Path {
        VPath const *self;
    public:
        explicit Path(VPath const *self) : self(self) {}
        ~Path() { VPathRelease(self); }
        operator std::string() const {
            auto result = std::string();
            String const *s = nullptr;
            auto const rc = VPathMakeString(self, &s);
            if (rc == 0 && s) {
                result.assign(s->addr, StringSize(s));
                free((void *)s);
            }
            return result;
        }
    };

    Service Service::make() {
        vdb::KService *obj;
        auto const rc = vdb::KServiceMake(&obj);
        if (rc)
            throw vdb::exception(rc, "KServiceMake", "");
        return Service(obj);
    }

    void Service::add(std::string const &term) const {
        auto const rc = vdb::KServiceAddId((vdb::KService *)obj, term.c_str());
        if (rc)
            throw vdb::exception(rc, "KServiceAddId", term);
    }

    void Service::add(std::vector<std::string> const &terms) const {
        for (auto &term : terms)
            add(term);
    }

    void Service::setLocation(std::string const &location) const {
        auto const rc = vdb::KServiceSetLocation((vdb::KService *)obj, location.c_str());
        if (rc)
            throw vdb::exception(rc, "KServiceSetLocation", location);
    }

    void Service::setPermissionsFile(std::string const &path) const {
        auto const rc = vdb::KServiceSetJwtKartFile((vdb::KService *)obj, path.c_str());
        if (rc)
            throw vdb::exception(rc, "KServiceSetJwtKartFile", path);
    }

    void Service::setNGCFile(std::string const &path) const {
        auto const rc = vdb::KServiceSetNgcFile((vdb::KService *)obj, path.c_str());
        if (rc)
            throw vdb::exception(rc, "KServiceSetNgcFile", path);
    }

#if 0
    std::string Service::response(std::string const &url, std::string const &version) const {
        KSrvResponse const *resp = nullptr;
        auto const rc = vdb::KServiceNamesExecuteExt((vdb::KService *)obj, 0, url.c_str(), version.c_str(), &resp);
        if (rc == 0) {
            auto const result = std::string(KServiceGetResponseCStr((vdb::KService *)obj));
            KSrvResponseRelease(resp);
            return result;
        }
        throw vdb::exception(rc, "KServiceNamesExecuteExt", "");
    }

    Service::LocalInfo::FileInfo Service::localInfo2(std::string const &accession, std::string const &name) const
    {
        LocalInfo::FileInfo info = {};
        VPath *local = nullptr, *cache = nullptr;

        // KServiceQueryLocation(obj, accession.c_str(), name.c_str(), &local, &cache);
        if (local) {
            info.have = true;
            info.path = VPathToString(local);
            if (cache)
                info.cachepath = VPathToString(cache);
        }
        VPathRelease(local);
        VPathRelease(cache);
        return info;
    }

    Service::LocalInfo Service::localInfo(std::string const &accession) const
    {
        LocalInfo info = {};

        info.rundata = localInfo2(accession, accession);
        if (info.rundata)
            info.vdbcache = localInfo2(accession, accession + ".vdbcache");

        return info;
    }
#else
    Service::Response Service::response(std::string const &url, std::string const &version) const {
        KSrvResponse const *resp = nullptr;
        auto const rc = vdb::KServiceNamesExecuteExt((vdb::KService *)obj, 0, url.c_str(), version.c_str(), &resp);
        if (rc == 0) {
            auto const cstr = KServiceGetResponseCStr((vdb::KService *)obj);
            return Response((void *)resp, cstr);
        }
        throw vdb::exception(rc, "KServiceNamesExecuteExt", "");
    }
    Service::LocalInfo::FileInfo Service::Response::localInfo2(std::string const &accession, std::string const &name) const {
        Service::LocalInfo::FileInfo info = {};
        VPath const *vlocal = nullptr, *vcache = nullptr;
        rc_t rc1 = 0, rc2 = 0;
        auto const rc = vdb::KSrvResponseGetLocation((KSrvResponse const *)obj, accession.c_str(), name.c_str(), &vlocal, &rc1, &vcache, &rc2);

        if (rc == 0 && rc1 == 0 && vlocal) {
            Path local(vlocal);
            info.have = true;
            info.path = local;
            if (vcache) {
                Path cache(vcache);
                info.cachepath = cache;
            }
        }
        return info;
    }
    Service::LocalInfo Service::Response::localInfo(std::string const &accession) const {
        Service::LocalInfo info = {};

        info.rundata = localInfo2(accession, accession);
        if (info.rundata)
            info.vdbcache = localInfo2(accession, accession + ".vdbcache");

        return info;
    }

    Service::Response::~Response() {
        vdb::KSrvResponseRelease((vdb::KSrvResponse const *)(obj));
    }
    
    std::ostream &operator <<(std::ostream &os, Service::Response const &rhs) {
        return os << rhs.text;
    }

#endif

    std::string Service::CE_Token() {
        try {
            auto const &cloud = Cloud();
            return cloud.token();
        }
        catch (vdb::exception const &e) {
            switch (e.resultCode()) {
            case 3017889624:
                LOG(2) << "No cloud token, not in a cloud." << std::endl;
                break;
            default:
                LOG(1) << "Failed to get cloud token" << std::endl;
                LOG(2) << e.failedCall() << " returned " << e.resultCode() << std::endl;
                break;
            }
            return "";
        }
    }

    Service::~Service() { vdb::KServiceRelease((vdb::KService *)obj); }
    Service::Service(void *obj) : obj(obj) {}
}
