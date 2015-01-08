#ifndef _hpp_tools_vdb_config_util_
#define _hpp_tools_vdb_config_util_

/*==============================================================================
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
*/

#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/text.h> /* String */

#include <kfg/config.h> /* KConfig */
#include <kfs/directory.h> /* KDirectory */

#include <kfs/file.h> /* KFile */

#include <map>
#include <string>
#include <vector>

#include <cstring>

static const bool DEBUG(false);

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)


class CNoncopyable {
public:
    CNoncopyable(void) {}
protected:
    ~CNoncopyable(void) {}
private:
    // Prohibit copy constructor and assignment operator
    CNoncopyable(const CNoncopyable&);
    const CNoncopyable& operator=(const CNoncopyable&);
};

rc_t StringRelease(const String *self);

class CString : CNoncopyable {
    const String *m_Self;

public:
    CString(const String *self) : m_Self(self) {}

    ~CString() { free((void*)m_Self); }

    const String* Get(void) const { return m_Self; }
    std::string GetString(void) const {
        const char *s = GetCString();
        if (s == NULL)
            return "";
        return s;
    }
    const char* GetCString(void) const {
        if (Empty())
            return NULL;
        return m_Self->addr;
    }

    bool Empty(void) const {
        return
            m_Self == NULL || m_Self->addr == NULL || m_Self->addr[0] == '\0';
    }

    bool Equals(const CString &aString) const {
        const String *string = aString.Get();

        if (m_Self == NULL && string == NULL)
            return true;
        return StringEqual(m_Self, string);
    }

    bool Equals(const char *buffer, size_t bsize = ~0) const {
        if (bsize == (size_t)~0) {
            bsize = string_size(buffer);
        }

        if (m_Self == NULL) {
            return buffer == NULL || bsize == 0;
        }

        String s;
        StringInit(&s, buffer, bsize, (uint32_t)bsize + 1);

        return StringEqual(m_Self, &s);
    }
};

class CStdIn : CNoncopyable {
    const KFile *m_Self;
    uint64_t m_Pos;

public:
    CStdIn(void) : m_Self(NULL), m_Pos(0) {
        rc_t rc = KFileMakeStdIn(&m_Self);
        if (rc != 0) {
            throw rc;
        }
    }

    ~CStdIn(void) { KFileRelease(m_Self); }

    char Read1(void) {
        char buf[9] = "";
        rc_t rc = Read(buf, sizeof buf);
        if (rc != 0) {
            return -1;
        }
        return buf[0];
    }

    rc_t Read(char *buffer, size_t bsize, size_t *num_read = NULL);
};

class CKDirectory : CNoncopyable {
    KDirectory *m_Self;
    const uint32_t m_PrivateAccess;
    const uint32_t m_PublicAccess;
public:
    CKDirectory(void)
        : m_Self(NULL), m_PrivateAccess(0700), m_PublicAccess(0775)
    {
        rc_t rc  = KDirectoryNativeDir(&m_Self);
        if (rc != 0)
            throw rc;
    }
    ~CKDirectory() { KDirectoryRelease(m_Self); }
    rc_t CheckPrivateAccess(const CString &path, bool &updated, bool verbose)
        const
    {   return CheckAccess(path, updated, true, verbose); }
    rc_t CheckPublicAccess(const CString &path, bool verbose = false) const;
    rc_t CreateNonExistingPrivateDir(const CString &path, bool verbose = false)
        const
    {   return CreateNonExistingDir(path, m_PrivateAccess, verbose); }
    rc_t CreateNonExistingPublicDir(bool verbose, const char *path, ...) const;
    rc_t CreateNonExistingPublicDir(const std::string &path,
        bool verbose = false) const
    {   return CreateNonExistingDir(path, m_PublicAccess, verbose, true); }
    rc_t CreateNonExistingPublicDir(const CString &path, bool verbose = false)
        const
    {   return CreateNonExistingDir(path, m_PublicAccess, verbose); }
    rc_t CreatePublicDir(const std::string &path, bool verbose = false) {
        return CreateNonExistingDir(path, m_PublicAccess, verbose, false);
    }
    rc_t CanWriteFile(const CString &dir, bool verbose = false) const;
    bool Exists(const std::string &path) const { return Exists(path.c_str()); }
    bool Exists(const CString &path) const;
    bool IsDirectory(const std::string &path) const {
        return IsDirectory(path.c_str());
    }
    bool IsDirectory(const char *path, ...) const;
private:
    bool Exists(const char *path) const
    {   return KDirectoryPathType(m_Self, path) != kptNotFound; }
    rc_t CreateNonExistingDir(const CString &path,
        uint32_t access, bool verbose) const;
    rc_t CreateNonExistingDir(const std::string &path,
        uint32_t access, bool verbose, bool chekExistance) const;
    rc_t CreateNonExistingDir(bool verbose,
        uint32_t access, const char *path, va_list args) const;
    rc_t CheckAccess(const CString &path, bool &updated, bool isPrivate,
        bool verbose = false) const;
};

class CKConfigNode : CNoncopyable {
    const KConfigNode *m_Self;

public:
    CKConfigNode(const KConfigNode *self)
        : m_Self(self)
    {}

    ~CKConfigNode(void) {
        KConfigNodeRelease(m_Self);
    }

    const KConfigNode* OpenNodeRead(const std::string &path) const {
        const KConfigNode *node = NULL;
        rc_t rc = KConfigNodeOpenNodeRead(m_Self, &node, path.c_str());
        if (rc != 0) {
            return NULL;
        }
        return node;
    }

    bool ReadBool(const std::string &path) const {
        return ReadString(path) == "true";
    }

    std::string ReadString(const std::string &path) const {
        const KConfigNode *node = NULL;
        rc_t rc = KConfigNodeOpenNodeRead(m_Self, &node, path.c_str());
        if (rc != 0) {
            return "";
        }

        String *result = NULL;
        rc = KConfigNodeReadString(node, &result);
        std::string r;
        if (rc == 0) {
            assert(result);
            r = result->addr;
        }

        RELEASE(String, result);
        RELEASE(KConfigNode, node);

        return r;
    }
};

class CKConfig : CNoncopyable {
    KConfig *m_Self;
    bool m_Updated;
    const char *m_RepositoryRemoteAuxDisabled;
    const char *m_RepositoryRemoteMainDisabled;
    const char *m_RepositoryUserRoot;
public:
    CKConfig(bool verbose = false)
        : m_Self(NULL), m_Updated(false)
        , m_RepositoryRemoteAuxDisabled ("repository/remote/aux/NCBI/disabled")
        , m_RepositoryRemoteMainDisabled("repository/remote/main/CGI/disabled")
        , m_RepositoryUserRoot          ("repository/user/main/public/root")
    {
        if (verbose)
            OUTMSG(("loading configuration... "));
        rc_t rc = KConfigMake(&m_Self, NULL);
        if (rc == 0) {
            if (verbose)
                OUTMSG(("ok\n"));
        }
        else {
            if (verbose)
                OUTMSG(("failed\n"));
            throw rc;
        }
    }
    ~CKConfig(void)
    { KConfigRelease(m_Self); }
    KConfig* Get(void) const { return m_Self; }
    void Reload(bool verbose = false);
    rc_t Commit(void) const;
    const KConfigNode* OpenNodeRead(const char *path, ...) const;
    bool IsRemoteRepositoryDisabled(void) const;
    const String* ReadDefaultConfig(void) const
    {   return ReadString("/config/default"); }
    const String* ReadHome(void) const { return ReadString("HOME"); }
    const String* ReadNcbiHome(void) const { return ReadString("NCBI_HOME"); }
    const String* ReadUserRepositoryRootPath(void) const
    {   return ReadString(m_RepositoryUserRoot); }
    rc_t DisableRemoteRepository(bool disable);
    rc_t UpdateUserRepositoryRootPath(const CString &path);
    rc_t UpdateUserRepositoryRootPath(const char *buffer, size_t size) {
        return UpdateNode(m_RepositoryUserRoot, buffer, false, size);
    }
    bool NodeExists(const std::string &path) const;
    rc_t UpdateNode(bool verbose, const char *value, const char *name, ...);
    rc_t UpdateNode(const char *path, const char *buffer,
        bool verbose = false, size_t size = ~0);
    rc_t CreateRemoteRepositories(bool fix = false);
    rc_t CreateUserRepositories(bool fix = false);
    const String* ReadString(const char *path) const;
};

class CSplitter : CNoncopyable {
    const char *m_Start;
    size_t m_Size;

public:
    CSplitter(const std::string &s) : m_Start(s.c_str()), m_Size(s.size()) {}
    CSplitter(const String *s) : m_Start(NULL), m_Size(0) {
        if (s == NULL) {
            return;
        }
        m_Start = s->addr;
        m_Size = s->len;
        SkipEmpties();
    }
    bool HasNext(void) const { return m_Size != 0; }
    const std::string Next(void) {
        if (!HasNext()) {
            return "";
        }
        const char *s = m_Start;
        const char *end = string_chr(m_Start, m_Size, ':');
        size_t n = m_Size;
        if (end != NULL) {
            n = end - m_Start;
            m_Start = end + 1;
            if (m_Size >= n + 1) {
               m_Size -= n + 1;
            }
            else {
               m_Size = 0;
            }
        }
        else {
            m_Start = NULL;
            m_Size = 0;
        }
        SkipEmpties();
        return std::string(s, n);
    }
    static rc_t Test(void);

private:
    void SkipEmpties(void) {
        while (m_Size > 0 && m_Start != NULL && *m_Start == ':') {
            --m_Size;
            ++m_Start;
        }
    }
};

class CAppVolume {
    std::string m_Type;
    std::string m_Path;

public:
    CAppVolume(const std::string &type = "", const std::string &path = "")
        : m_Type(type)
        , m_Path(path)
    {}

    std::string GetPath(void) const { return m_Path; }

    rc_t Update(CKConfig &kfg, const char *node,
        const std::string &appVolPath, bool verbose = false) const
    {
        if (DEBUG) {
            OUTMSG(("CAppVolume::Update(%s, %s)\n", node, appVolPath.c_str()));
        }

        return kfg.UpdateNode(verbose, m_Path.c_str(),
            "%s/%s/%s", node, appVolPath.c_str(), m_Type.c_str());
    }

    void Dump(const char *node, const std::string &appVolPath) const {
        OUTMSG(("%s/%s/%s = \"%s\"\n", node, appVolPath.c_str(),
            m_Type.c_str(), m_Path.c_str()));
    }
};

class CApp : CNoncopyable {
    bool m_HasVolume;
    const std::string m_AppVolPath;
    typedef std::map<const std::string, CAppVolume> TCAppVolumes;
    TCAppVolumes m_Volumes;

public:
    typedef TCAppVolumes::const_iterator TCAppVolumesCI;

    CApp(const std::string &root, const std::string &name,
            const std::string &type, const std::string &path)
        : m_HasVolume(false), m_AppVolPath("apps/" + name + "/volumes")
    {
        Update(root, name, type, path);
    }
    CApp(const CKDirectory &dir, const CKConfigNode &rep,
            const std::string &root, const std::string &name);

    void Update(const std::string &root, const std::string &name,
        const std::string &type, const std::string &path)
    {
        m_Volumes[type] = CAppVolume(type, path);
    }
    void Update(const CKDirectory &dir, const CKConfigNode &rep,
        const std::string &root, const std::string &name) const
    {
        assert(0);
    }
    rc_t Update(CKConfig &kfg, const char *node, bool verbose = false) const {
        rc_t rc = 0;
        for (TCAppVolumesCI it = m_Volumes.begin(); it != m_Volumes.end(); ++it)
        {
            rc_t r2 = (*it).second.Update(kfg, node, m_AppVolPath, verbose);
            if (r2 != 0 && rc == 0) {
                rc = r2;
            }
        }
        return rc;
    }
    void Dump(const char *node) const {
        for (TCAppVolumesCI it = m_Volumes.begin(); it != m_Volumes.end(); ++it)
        {
            (*it).second.Dump(node, m_AppVolPath);
        }
    }

    TCAppVolumesCI VolumesBegin(void) const { return m_Volumes.begin(); }
    TCAppVolumesCI VolumesEnd(void) const { return m_Volumes.end(); }
};

class CApps : public std::map<const std::string, CApp*> {
public:
    typedef std::map<const std::string, CApp*>::const_iterator TCI;
    typedef std::map<const std::string, CApp*>::iterator TI;

    ~CApps(void) {
        for (TCI it = begin(); it != end(); ++it) {
            delete((*it).second);
        }
    }

    void Update(const CKDirectory &dir, const CKConfigNode &rep,
        const std::string &root, const std::string &name)
    {
        TCI it = find(name);
        if (it == end()) {
            insert(std::pair<const std::string, CApp*>
                (name, new CApp(dir, rep, root, name)));
        }
        else {
            (*it).second->Update(dir, rep, root, name);
        }
    }

    void Update(const std::string &root, const std::string &name,
        const std::string &type, const std::string &path)
    {
        if (DEBUG) {
            OUTMSG(("CApps.Update(%s)\n", name.c_str()));
        }

        TI it = find(name);
        if (it == end()) {
            if (DEBUG) {
                OUTMSG(("CApps.Update(not found)\n"));
            }
            insert(std::pair<const std::string, CApp*>
                (name, new CApp(root, name, type, path)));
        }
        else {
            if (DEBUG) {
                OUTMSG(("CApps.Update(found)\n"));
            }
            (*it).second->Update(root, name, type, path);
        }
    }
};

class CRepository : CNoncopyable {
    bool m_Disabled;
    const std::string m_Category;    // user/site/remote
    const std::string m_SubCategory; // main/aux/protected
    const std::string m_Name;
    std::string m_Root;
    CApps m_Apps;

    bool RootExists(void) const { return m_Root.size() > 0; }

protected:
    CRepository(const std::string &category, const std::string &type,
            const std::string &name, const std::string &root);
    CRepository(const CKDirectory &dir, const CKConfigNode &repo,
            const std::string &category, const std::string &subCategory,
            const std::string &name);

    void SetRoot(const std::string &root) { m_Root = root; }
    void FixFileVolume(void)
    {   m_Apps.Update(m_Root, "file", "flat", "files"); }
    void FixNakmerVolume(const std::string &name,
        const std::string &path)
    {   m_Apps.Update(m_Root, "nakmer", name, path); }
    void FixNannotVolume(const std::string &name,
        const std::string &path)
    {   m_Apps.Update(m_Root, "nannot", name, path); }
    void FixRefseqVolume(void)
    {   m_Apps.Update(m_Root, "refseq", "refseq", "refseq"); }
    void FixRefseqVolume(const std::string &name,
        const std::string &path)
    {   m_Apps.Update(m_Root, "sra", name, path); }
    void FixWgsVolume(const std::string &name)
    {   m_Apps.Update(m_Root, "wgs", name, "wgs"); }
    rc_t Update(CKConfig &kfg, std::string &node, bool verbose = false);

public:
    void ClearApps(void) { m_Apps.clear(); }
    virtual std::string Dump(void) const;
    bool Is(const std::string &subCategory, const std::string &name = "")
        const;
    std::string GetCategory(void) const { return m_Category; }
    std::string GetSubCategory(void) const { return m_SubCategory; }
    std::string GetName(void) const { return m_Name; }
    std::string GetRoot(void) const { return m_Root; }
    void Disable(bool disable) { m_Disabled = disable; }

    CApps::TCI AppsBegin(void) const { return m_Apps.begin(); }
    CApps::TCI AppsEnd(void) const { return m_Apps.end(); }
};

class CRemoteRepository : public CRepository {
    std::string m_ResolverCgi;

    static bool EndsWith(const std::string &stack, char needle);
    static std::string GetCgi(const std::string &stack, char needle);
    static std::string GetRoot(const std::string &stack, char needle);
    void FixApps(void) {
        FixNakmerVolume("fuseNAKMER", "sadb");
        FixNannotVolume("fuseNANNOT", "sadb");
        FixRefseqVolume();
        FixRefseqVolume("fuse1000", "sra-instant/reads/ByRun/sra");
        FixWgsVolume("fuseWGS");
    }
    rc_t Update(CKConfig &kfg, bool verbose = false) {
        std::string node;
        rc_t rc = CRepository::Update(kfg, node, verbose);
        if (rc != 0) {
            return rc;
        }
        return kfg.UpdateNode(verbose,
            m_ResolverCgi.c_str(), "%s/resolver-cgi", node.c_str());
    }

public:
    CRemoteRepository(const CKDirectory &dir, const CKConfigNode &repo,
            const std::string &type, const std::string &name)
        : CRepository(dir, repo, "remote", type, name)
        , m_ResolverCgi(repo.ReadString("resolver-cgi"))
    {}
    CRemoteRepository(const std::string &category,
            const std::string &name, const std::string &root)
        : CRepository("remote", category, name, GetRoot(root, 'i'))
        , m_ResolverCgi(GetCgi(root, 'i'))
    {
        if (m_ResolverCgi.size() == 0) {
            FixApps();
        }
    }

    virtual std::string Dump(void) const {
        std::string node(CRepository::Dump());
        if (m_ResolverCgi.size() > 0) {
            OUTMSG(("%s/resolver-cgi = \"%s\"\n",
                node.c_str(), m_ResolverCgi.c_str()));
        }
        return node;
    }
    rc_t Fix(CKConfig &kfg, bool disable, bool verbose = false);
};

class CUserRepositories;
class CUserConfigData :  CNoncopyable {
    std::string m_DefaultRoot;
    std::string m_CurrentRoot;
    bool m_CacheEnabled;

public:
    CUserConfigData(const CUserRepositories &repos, const CString &dflt);

    std::string GetCurrentRoot(void) const { return m_CurrentRoot; }
    std::string GetDefaultRoot(void) const { return m_DefaultRoot; }
    bool GetCacheEnabled(void) const { return m_CacheEnabled; }

    void SetCurrentRoot(const std::string &path) { m_CurrentRoot = path; }
    void SetCacheEnabled(bool enabled) { m_CacheEnabled = enabled; }
};

class CUserRepository : public CRepository {
    bool m_CacheEnabled;

    void FixApps(void) {
        FixFileVolume();
        FixNakmerVolume("nakmerFlat", "nannot");
        FixNannotVolume("nannotFlat", "nannot");
        FixRefseqVolume();
        FixRefseqVolume("sraFlat", "sra");
        FixWgsVolume("wgsFlat");
    }
    rc_t Fix(CKConfig &kfg,
        const CUserConfigData *data, const std::string *root);
    rc_t Update(CKConfig &kfg, bool verbose = false) {
        std::string node;
        rc_t rc = CRepository::Update(kfg, node, verbose);
        if (rc != 0) {
            return rc;
        }
        return kfg.UpdateNode(verbose, m_CacheEnabled ? "true" : "false",
            "%s/cache-enabled", node.c_str());
    }

public:
    CUserRepository(const CKDirectory &dir, const CKConfigNode &repo,
            const std::string &type, const std::string &name)
        : CRepository(dir, repo, "user", type, name)
        , m_CacheEnabled(repo.ReadBool("cache-enabled"))
    {}
    CUserRepository(const std::string &category,
            const std::string &name, const std::string &root)
        : CRepository("user", category, name, root), m_CacheEnabled(true)
    {
        FixApps();
    }

    virtual std::string Dump(void) const {
        const std::string node(CRepository::Dump());
        OUTMSG(("%s/cache-enabled = \"%s\"\n", node.c_str(),
            m_CacheEnabled ? "true" : "false"));
        return node;
    }

    bool IsCacheEnabled(void) const { return m_CacheEnabled; }
    void Fix(CKConfig &kfg, const std::string &root) { Fix(kfg, NULL, &root); }
    void Fix(CKConfig &kfg, const CUserConfigData *data) {
        Fix(kfg, data, NULL);
    }
};

class CUserRepositories : std::vector<CUserRepository*> {
    typedef std::vector<CUserRepository*>::const_iterator TCI;
    typedef std::vector<CUserRepository*>::iterator TI;
    const CUserRepository *FindMainPublic(void) const;

public:
    ~CUserRepositories(void) {
        for (TCI it = begin(); it != end(); ++it) {
            free(*it);
        }
    }

    rc_t Load(const CKConfig &kfg, const CKDirectory &dir);
    std::string GetMainPublicRoot(void) const {
        const CUserRepository *r = FindMainPublic();
        if (r == NULL) {
            return "";
        }
        return r->GetRoot();
    }
    bool IsMainPublicCacheEnabled(void) const {
        const CUserRepository *r = FindMainPublic();
        if (r == NULL) {
            return true;
        }
        return r->IsCacheEnabled();
    }
    void Fix(CKConfig &kfg, const CUserConfigData &data) {
        bool publicMainFound = false;
        for (TI it = begin(); it != end(); ++it) {
            CUserRepository *r = *it;
            assert(r);
            if (r->Is("main", "public")) {
                r->Fix(kfg, &data);
                publicMainFound = true;
            }
            else {
                r->Fix(kfg, data.GetDefaultRoot());
            }
        }
        if (!publicMainFound) {
            CUserRepository *r = new CUserRepository("main", "public", "");
            r->Fix(kfg, &data);
            push_back(r);
        }
    }
    rc_t MkAppVolumes(const CKDirectory &dir, bool verbose = false)
        const;
};

class CRemoteRepositories : std::vector<CRemoteRepository*> {
    typedef std::vector<CRemoteRepository*>::const_iterator TCI;

public:
    ~CRemoteRepositories(void) {
        for (TCI it = begin(); it != end(); ++it) { free(*it); }
    }

    rc_t Load(const CKConfig &kfg, const CKDirectory &dir);

    bool IsDisabled(void) const { return false; }
    void Fix(CKConfig &kfg, bool disable, bool verbose = false);
};


#endif // _hpp_tools_vdb_config_util_
