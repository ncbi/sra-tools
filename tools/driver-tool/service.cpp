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
#include "util.hpp"

#include <klib/status.h> /* STSMSG */
#include <klib/text.h>

#include <vfs/services.h>
#include <vfs/services-priv.h>
#include <cloud/cloud.h>
#include <cloud/manager.h>
#include <vfs/path.h>
#include <vfs/manager.h>

namespace vdb {
    extern "C" rc_t VDBManagerGetQualityString(void *, char const **);

    class Cloud {
        class Manager {
            CloudMgr *mgr;
        public:
            Manager() {
                auto const rc = CloudMgrMake(&mgr, nullptr, nullptr);
                if (rc)
                    throw exception(rc, "CloudMgrMake", "");
            }
            ~Manager() { CloudMgrRelease(mgr); }

            bool haveCloudProvider() const {
                ::Cloud *cloud = nullptr;
                CloudMgrGetCurrentCloud(mgr, &cloud);
                if (cloud) {
                    CloudRelease(cloud);
                    return true;
                }
                return false;
            }
            ::Cloud *current() const {
                ::Cloud *result = nullptr;
                auto const rc = CloudMgrGetCurrentCloud(mgr, &result);
                if (rc)
                    throw exception(rc, "CloudMgrGetCurrentCloud", "");
                return result;
            }
        };
        ::Cloud *obj;
    public:
        ~Cloud() { CloudRelease(obj); }
        Cloud() {
            auto const &mgr = Manager();
            obj = mgr.current();
        }
        static bool haveProvider() {
            auto const &mgr = Manager();
            return mgr.haveCloudProvider();
        }
        std::string token() const {
            auto result = std::string();
            
            String const *s;
            auto const rc = CloudMakeComputeEnvironmentToken(obj, &s);
            if (rc == 0 && s) {
                result.assign(s->addr, StringSize(s));
                free((void *)s);
            }
            return result;
        }
    };

    namespace VFS {
        class Manager {
            VFSManager *self;
        public:
            ~Manager() { VFSManagerRelease(self); }
            Manager() : self(nullptr) {
                auto const rc = VFSManagerMake(&self);
                if (rc)
                    throw vdb::exception(rc, "VFSManagerMake", "can't make a VFS Manager!");
            }
            VPath const *makeSystemPath(std::string const &path) const {
                auto result = (VPath *)(nullptr);
                auto const rc = VFSManagerMakeSysPath(self, &result, path.c_str());
                if (rc == 0)
                    return result;
                throw vdb::exception(rc, "VFSManagerMakeSysPath", "can't make a VPath");
            }
        };
        static Manager const manager = Manager();
    }

    class Path {
        VPath const *self;
        static VFS::Manager const manager;
    public:
        explicit Path(VPath const *self) : self(self) {}
        explicit Path(std::string const &path) : self(VFS::manager.makeSystemPath(path)) {}

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
        Service::QualityType qualityType() const {
            switch (VPathGetQuality(self)) {
            case eQualFull:
                return Service::full;
            case eQualNo:
                return Service::none;
            default:
                return Service::unknown;
            }
        }
        bool exists() const {
            return pathExists(*this);
        }
    };

    Service Service::make() {
        KService *obj;
        auto const rc = KServiceMake(&obj);
        if (rc)
            throw exception(rc, "KServiceMake", "");
        return Service(obj);
    }

    void Service::add(std::string const &term) const {
        auto const rc = KServiceAddId((KService *)obj, term.c_str());
        if (rc)
            throw exception(rc, "KServiceAddId", term,
                "Cannot process '" + term + "'");
    }

    void Service::add(std::vector<std::string> const &terms) const {
        for (auto &term : terms)
            add(term);
    }

    void Service::setLocation(std::string const &location) const {
        auto const rc = KServiceSetLocation((KService *)obj, location.c_str());
        if (rc)
            throw exception(rc, "KServiceSetLocation", location,
                "Cannot use '" + location + "' as location");
    }

    void Service::setPermissionsFile(std::string const &path) const {
        auto const rc = KServiceSetJwtKartFile((KService *)obj, path.c_str());
        if (rc)
            throw exception(rc, "KServiceSetJwtKartFile", path,
                "Cannot use '" + path + "' as jwt cart file");
    }

    void Service::setNGCFile(std::string const &path) const {
        auto const rc = KServiceSetNgcFile((KService *)obj, path.c_str());
        if (rc)
            throw exception(rc, "KServiceSetNgcFile", path,
                "Cannot use '" + path + "' as ngc file");
    }

    Service::QualityType Service::preferredQualityType() {
        char const *quality = NULL;

        VDBManagerGetQualityString(NULL, &quality);
        if (quality != NULL) {
            switch (quality[0]) {
            case 'Z':
                return none;
            case 'R':
                return full;
            }
        }
        return unknown;
    }

    Service::Response Service::response(std::string const &url, std::string const &version) const {
        KSrvResponse const *resp = nullptr;
        KService * service = static_cast <KService*> (obj);

        auto const rc = KServiceNamesExecuteExt(service, 0,
            url.c_str(), version.c_str(), &resp);
        if (rc == 0) {
            auto const cstr = KServiceGetResponseCStr(service);
            return Response((void *)resp, cstr);
        }

        throw exception(rc, "KServiceNamesExecuteExt", "",
            "Failed to call external services");
    }

    Service::FileInfo Service::Response::localInfo(  std::string const &accession
                                                   , std::string const &name
                                                   , std::string const &type) const
    {
        Service::FileInfo info = {};
        VPath const *vlocal = nullptr, *vcache = nullptr;
        rc_t rc1 = 0, rc2 = 0;
        auto const rc = KSrvResponseGetLocation2((KSrvResponse const *)obj, accession.c_str(), name.c_str(), type.c_str(), &vlocal, &rc1, &vcache, &rc2);

        if (rc == 0 && rc1 == 0 && vlocal) {
            Path local(vlocal);
            info.have = true;
            info.path = local;
            info.qualityType = local.qualityType();
            if (vcache) {
                Path cache(vcache);
                info.cachepath = cache;
            }
        }
        return info;
    }

    Service::Response::~Response() {
        KSrvResponseRelease((KSrvResponse const *)(obj));
    }
    
    std::ostream &operator <<(std::ostream &os, Service::Response const &rhs) {
        return os << rhs.text;
    }

    Service::FileInfo Service::_localInfo(std::string const &accession, std::string const &path) const {
        Service::FileInfo info = {};

        info.path = accession;
        try {
            auto const local = Path(path);
            info.have = true;
            info.qualityType = local.qualityType();
        }
        catch (...) {
            info.have = false;
            info.qualityType = unknown;
        }
        return info;
    }

    Service::FileInfo Service::localInfo(std::string const &accession, std::string const &extension) const {
        Service::FileInfo info = {};

        return info;
    }

    /// Builds accession file paths as commonly made by `prefetch`. E.g.
    /// `/home/repo/SRR000001/SRR000001.sra` but without the extension.
    /// Different extensions will be tested.
    static std::string accessionToPath(std::string const &accession) {
        auto const sep = accession.find_last_of('/');
        if (sep == std::string::npos)
            return accession + '/' + accession;
        else
            return accession + accession.substr(sep);
    }

    /// Note: `accession` what the user typed on the command line.
    /// It has already been verified as a path to something in the file system.
    /// The default mode for `prefetch` is to create accession folder in the current directory
    /// that contain the accession data file and any associated files. The path to one of these
    /// folders is most likely what `accession` is.
    Service::FileInfo Service::localInfo(std::string const &accession) const {
        Service::FileInfo info = {};
        auto const path = accessionToPath(accession);

        /// Add extension to path, check existance and quality type.
        auto check = [&](char const *ext) -> bool {
            try {
                auto const local = Path(path + ext);
                if (local.exists()) {
                    info.have = true;
                    info.qualityType = local.qualityType();
                    return true;
                }
            }
            catch (...) {}
            return false;
        };

        info.path = accession;
        try {
            auto const local = Path(accession);
            info.path = local;
            info.have = true;
            info.qualityType = local.qualityType();
        }
        catch (...) {
            info.have = false;
            info.qualityType = unknown;
        }
        if (!info.have || info.qualityType != unknown) {
            // wow the user gave us a path to a likely data file.
            return info;
        }

        // In case the user has both available, we still
        // want to honor the preference and not blindly
        // pick the full quality version.
        if (preferredQualityType() == full && check(".sra"))
            return info;

        static char const *const noqualTypes[] = { ".sralite", ".realign", ".noqual", nullptr };
        for (auto ext = noqualTypes; *ext; ++ext) {
            if (check(*ext))
                return info;
        }
        check(".sra");
        return info;
    }

    bool Service::haveCloudProvider() {
        return Cloud::haveProvider();
    }
    std::string Service::CE_Token() {
        try {
            auto const &cloud = Cloud();
            return cloud.token();
        }
        catch (exception const &e) {
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

    Service::~Service() { KServiceRelease((KService *)obj); }
    Service::Service(void *obj) : obj(obj) {}
}
