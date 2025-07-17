/*===========================================================================
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

#include <kfg/kfg-priv.h> /* KConfigFixMainResolverCgiNode */

#include <klib/printf.h> /* string_printf */
#include <klib/strings.h> /* SDL_CGI */

#include "util.hpp" // CStdIn

#include <sstream> // ostringstream

#include <climits> /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

using std::string;

rc_t CStdIn::Read(char *buffer, size_t bsize, size_t *num_read) {
    size_t dummy = 0;
    if (num_read == NULL) {
if (DEBUG_LOCAL) OUTMSG(("<<< Read: num_read == NULL\n"));
        num_read = &dummy;
    }

    if (bsize == 0) {
        *num_read = 0;
        return 0;
    }

    rc_t rc = KFileRead(m_Self, m_Pos, buffer, bsize, num_read);
    if (rc == 0) {
        m_Pos += *num_read;
        size_t last = *num_read;
        if (*num_read >= bsize) {
            last = bsize - 1;
        }

        while (true) {
            buffer[last] = '\0';
            if (last == 0) {
                break;
            }
            --last;
            if (buffer[last] != '\n' && buffer[last] != '\r') {
                break;
            }
            --*num_read;
        }
    }

if (DEBUG_LOCAL) OUTMSG(("<<< Read: num_read = %d\n", *num_read));

return rc;
}

rc_t CKDirectory::CanWriteFile(const CString &dir, bool verbose) const {
    bool ok = true;
    rc_t rc = 0;
    char path[PATH_MAX] = "";
    if (verbose) {
        OUTMSG(("checking whether %S is writable... ", dir.Get()));
    }
    for (int i = 0; i < 10000 && rc == 0; ++i) {
        size_t path_len = 0;
        rc = string_printf(path, sizeof path, &path_len,
            "%S/.tmp%d.tmp", dir.Get(), i);
        if (rc != 0) {
            break;
        }
        assert(path_len <= sizeof path);
        if (Exists(path)) {
            KDirectoryRemove(m_Self, false, path);
        }
        else {
            KFile *f = NULL;
            rc = KDirectoryCreateFile(m_Self,
                &f, false, m_PrivateAccess, kcmCreate, path);
            if (rc == 0) {
                rc = KFileWrite(f, 0, path, path_len, NULL);
            }
            RELEASE(KFile, f);
            const KFile *cf = NULL;
            if (rc == 0) {
                rc = KDirectoryOpenFileRead(m_Self, &cf, path);
            }
            char buffer[PATH_MAX] = "";
            size_t num_read = 0;
            if (rc == 0) {
                rc = KFileRead(cf, 0, buffer, sizeof buffer, &num_read);
            }
            if (rc == 0) {
                if (path_len != num_read || string_cmp(path,
                    path_len, buffer, num_read, sizeof buffer) != 0)
                {
                    if (verbose) {
                        OUTMSG(("no\n"));
                    }
                    OUTMSG(("Warning: "
                        "NCBI Home directory is not writable"));
                    ok = false;
                }
            }
            RELEASE(KFile, cf);
            if (rc == 0) {
                KDirectoryRemove(m_Self, false, path);
            }
            break;
        }
    }
    if (verbose && ok) {
        if (rc == 0) {
            OUTMSG(("yes\n"));
        }
        else {
            OUTMSG(("failed\n"));
        }
    }
    return rc;
}

rc_t CKDirectory::CheckAccess(const CString &path,
    bool &updated, bool isPrivate, bool verbose) const
{
    updated = false;
    const String *str = path.Get();
    if (str == NULL) {
        return 0;
    }
    uint32_t access = 0;
    if (verbose) {
        OUTMSG(("checking %S file mode... ", path.Get()));
    }
    rc_t rc = KDirectoryAccess(m_Self, &access, str->addr);
    if (rc != 0) {
        OUTMSG(("failed\n"));
    }
    else {
        if (verbose) {
            OUTMSG(("%o\n", access));
        }
        if (isPrivate) {
            if (access != m_PrivateAccess) {
                uint32_t access = 0777;
                if (verbose) {
                    OUTMSG(("updating %S to %o... ", str, access));
                }
                rc = KDirectorySetAccess(m_Self, false,
                    m_PrivateAccess, access, str->addr);
                if (rc == 0) {
                    OUTMSG(("ok\n"));
                    updated = true;
                }
                else {
                    OUTMSG(("failed\n"));
                }
            }
        }
        else {
            if ((access & m_PrivateAccess) != m_PrivateAccess) {
                uint32_t access = 0700;
                if (verbose) {
                    OUTMSG(("updating %S to %o... ", str, access));
                }
                rc = KDirectorySetAccess(m_Self, false,
                    m_PrivateAccess, access, str->addr);
                if (rc == 0) {
                    OUTMSG(("ok\n"));
                    updated = true;
                }
                else {
                    OUTMSG(("failed\n"));
                }
            }
        }
    }
    return rc;
}

rc_t CKDirectory::CreateNonExistingDir(bool verbose,
    uint32_t access, const char *path, va_list args) const
{
    char str[PATH_MAX] = "";
    rc_t rc = string_vprintf(str, sizeof str, NULL, path, args);
    if (rc != 0) {
        OUTMSG(("error: cannot generate path string\n"));
        return rc;
    }

    return CreateNonExistingDir(str, access, verbose, true);
}

rc_t CKDirectory::CreateNonExistingDir(const string &path,
    uint32_t access, bool verbose, bool checkExistance) const
{
    const char *str = path.c_str();

    if (checkExistance) {
        if (verbose) {
            OUTMSG(("checking whether %s exists... ", str));
        }
        if (Exists(str)) {
            if (verbose) {
                OUTMSG(("found\n"));
            }
            return 0;
        }
    }

    if (verbose) {
        OUTMSG(("creating... "));
    }

    rc_t rc = KDirectoryCreateDir(m_Self, access,
        (kcmCreate | kcmParents), str);
    if (verbose) {
        if (rc == 0) {
            OUTMSG(("ok\n"));
        }
        else {
            OUTMSG(("failed\n"));
        }
    }

    return rc;
}

rc_t CKDirectory::CreateNonExistingDir(const CString &path,
    uint32_t access, bool verbose) const
{
    const String *str = path.Get();
    if (str == NULL) {
        return 0;
    }

    if (verbose) {
        OUTMSG(("checking whether %S exists... ", str));
    }

    if (Exists(str->addr)) {
        if (verbose) {
            OUTMSG(("found\n"));
        }
        return 0;
    }

    if (verbose) {
        OUTMSG(("creating... "));
    }

    rc_t rc = KDirectoryCreateDir(m_Self, access,
        (kcmCreate | kcmParents), str->addr);
    if (verbose) {
        if (rc == 0) {
            OUTMSG(("ok\n"));
        }
        else {
            OUTMSG(("failed\n"));
        }
    }

    return rc;
}

CKConfig::CKConfig(bool verbose)
    : m_Self(NULL), m_Updated(false)
    , m_RepositoryRemoteAuxDisabled ("repository/remote/aux/NCBI/disabled")
    , m_RepositoryRemoteMainDisabled("repository/remote/main/CGI/disabled")
    , m_RepositoryUserRoot          ("repository/user/main/public/root")
{
    if (verbose)
        OUTMSG(("loading configuration... "));
    rc_t rc = KConfigMakeLocal(&m_Self, NULL);
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

rc_t CKConfig::Commit(void)
{
    if (!m_Updated) {
        return 0;
    }

    rc_t rc = KConfigCommit(m_Self);
    if ( rc == 0 )
    {
        m_Updated = false;
    }
    return rc;
}

rc_t CKConfig::CreateRemoteRepositories(bool fix) {
     rc_t rc = 0;
#ifdef NAMESCGI
     bool updated = NodeExists("/repository_remote/CGI/resolver-cgi/trace");

     {
        const string name("/repository/remote/main/CGI/resolver-cgi");
        if (!updated || !NodeExists(name)) {
            rc_t r2 = UpdateNode(name,
                "https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi");
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }

    if (fix) {
        const string name("/repository/remote/main/CGI/disabled");
        if (NodeExists(name)) {
            rc_t r2 = UpdateNode(name.c_str(), "false");
            if (r2 != 0 && rc == 0) {
                rc = r2;
            }
        }
    }

    {
        const string name("/repository/remote/protected/CGI/resolver-cgi");
        if (!updated || !NodeExists(name)) {
            rc_t r2 = UpdateNode(name,
                "https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi");
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }

    if (fix) {
        const string name("/repository/remote/disabled");
        if (NodeExists(name)) {
            rc_t r2 = UpdateNode(name.c_str(), "false");
            if (r2 != 0 && rc == 0) {
                rc = r2;
            }
        }
    }
#endif
    {
        const string name("/repository/remote/main/SDL.2/resolver-cgi");
        if (!NodeExists(name)) {
            rc_t r2 = UpdateNode(name, SDL_CGI);
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }
#ifdef NAMESCGI
    {
        const string name("/repository/remote/protected/SDL.2/resolver-cgi");
        if (!NodeExists(name)) {
            rc_t r2 = UpdateNode(name, SDL_CGI);
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }
#endif
    return rc;
}

rc_t CKConfig::CreateUserRepository(string repoName, bool fix) {
    if (repoName.size() == 0)
        repoName = "public";

    CString cRoot(ReadString("/repository/user/default-path"));

    string root;
    if (cRoot.Empty())
        root = "$(HOME)/ncbi";
    else
        root = cRoot.GetString();

    std::ostringstream s;
    s << "/repository/user/" << (repoName == "public" ? "main" : "protected")
        << "/" << repoName;
    string repoNode(s.str());
    string name(repoNode + "/root");

    rc_t rc = 0;

    {
        string name ( repoNode + "/apps/file/volumes/flat" );
        if ( ! NodeExists ( name ) ) {
            rc_t r2 = UpdateNode ( name, "files" );
            if ( r2 != 0 && rc == 0 )
                rc = r2;
        }
    }
    {
        string name ( repoNode + "/apps/sra/volumes/sraFlat" );
        if ( ! NodeExists ( name ) ) {
            rc_t r2 = UpdateNode ( name, "sra" );
            if ( r2 != 0 && rc == 0 )
                rc = r2;
        }
    }

    if ( repoName != "public" )
        return rc;

    {
        string name(repoNode + "/apps/sraPileup/volumes/withExtFlat");
        if (!NodeExists(name)) {
            rc_t r2 = UpdateNode(name, "sra");
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }
    {
        string name(repoNode + "/apps/sraRealign/volumes/withExtFlat");
        if (!NodeExists(name)) {
            rc_t r2 = UpdateNode(name, "sra");
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }
    {
        string name ( repoNode + "/apps/nakmer/volumes/nakmerFlat" );
        if ( ! NodeExists ( name ) ) {
            rc_t r2 = UpdateNode ( name, "nannot" );
            if ( r2 != 0 && rc == 0 )
                rc = r2;
        }
    }
    {
        string name ( repoNode + "/apps/nannot/volumes/nannotFlat" );
        if ( ! NodeExists ( name ) ) {
            rc_t r2 = UpdateNode ( name, "nannot" );
            if ( r2 != 0 && rc == 0 )
                rc = r2;
        }
    }
    {
        string name ( repoNode + "/apps/refseq/volumes/refseq" );
        if ( ! NodeExists ( name ) ) {
            rc_t r2 = UpdateNode ( name, "refseq" );
            if ( r2 != 0 && rc == 0 )
                rc = r2;
        }
    }
    {
        string name ( repoNode + "/apps/wgs/volumes/wgsFlat" );
        if ( ! NodeExists ( name ) ) {
            rc_t r2 = UpdateNode ( name, "wgs" );
            if ( r2 != 0 && rc == 0 )
                rc = r2;
        }
    }

    return rc;
}

rc_t CKConfig::DisableRemoteRepository(bool disable) {
    const char *value = disable ? "true" : "false";
    rc_t rc = UpdateNode(m_RepositoryRemoteMainDisabled, value);

    rc_t r2 = UpdateNode(m_RepositoryRemoteAuxDisabled, value);
    if (r2 != 0 && rc == 0) {
        rc = r2;
    }

    return rc;
}

bool CKConfig::IsRemoteRepositoryDisabled(void) const {
    const CString disabled(ReadString(m_RepositoryRemoteMainDisabled));

    if (disabled.Equals("true")) {
        const CString disabled(ReadString(m_RepositoryRemoteAuxDisabled));
        return disabled.Equals("true");
    }

    return false;
}

bool CKConfig::NodeExists(const string &path) const {
    const KConfigNode *n = OpenNodeRead(path.c_str());
    if (n == NULL) {
        return false;
    }
    KConfigNodeRelease(n);
    return true;
}

const KConfigNode* CKConfig::OpenNodeRead(const char *path, ...) const {
    va_list args;
    va_start(args, path);

    const KConfigNode *node = NULL;
    rc_t rc = KConfigVOpenNodeRead(m_Self, &node, path, args);

    va_end(args);

    if (rc != 0) {
        return NULL;
    }

    return node;
}

const String* CKConfig::ReadString(const char *path) const {
    String *result = NULL;
    rc_t rc = KConfigReadString(m_Self, path, &result);

    if (rc != 0) {
        return NULL;
    }
    return result;
}

void CKConfig::Reload(bool verbose) {
    if (verbose) {
        OUTMSG(("reloading configuration... "));
    }

    rc_t rc = KConfigRelease(m_Self);
    m_Self = NULL;

    if (rc == 0) {
        rc = KConfigMakeLocal(&m_Self, NULL);
    }

    if (rc == 0) {
        if (verbose) {
            OUTMSG(("ok\n"));
        }
        m_Updated = false;
    }
    else {
        if (verbose) {
            OUTMSG(("failed\n"));
        }
        throw rc;
    }
}

rc_t CKConfig::UpdateNode(const char *path,
    const char *buffer, bool verbose, size_t size)
{
    if (DEBUG_LOCAL) {
        OUTMSG(("CKConfig::UpdateNode(%s, %s, %d)\n", path, buffer, size));
    }

    if (verbose) {
        OUTMSG(("%s = ... ", path));
    }

    if (size == (size_t)~0) {
        size = string_size(buffer);
    }

    KConfigNode *node = NULL;
    rc_t rc = KConfigOpenNodeUpdate(m_Self, &node, path);
// TODO do not write empty node if node itself is empty
    if (rc == 0) {
        rc = KConfigNodeWrite(node, buffer, size);
    }
    if (rc == 0) {
        m_Updated = true;
    }
    RELEASE(KConfigNode, node);

    if (rc == 0) {
        if (verbose) {
            OUTMSG(("\"%s\"\n", buffer));
        }
    }
    else {
        if (verbose) {
            OUTMSG(("failed: %R\n", buffer, rc));
        }
        else {
            OUTMSG(("%s = ... failed: %R\n", path, rc));
        }
    }

    return rc;
}

rc_t CKConfig::UpdateUserRepositoryRootPath(const CString &path) {
    const String *str = path.Get();

    if (str == NULL) {
        return 0;
    }
    return UpdateUserRepositoryRootPath(str->addr, str->size);
}

rc_t StringRelease(const String *self) {
    free((void*)self);
    return 0;
}

rc_t CSplitter::Test(void) {
    String s;
    {   StringInit(&s, NULL, 0, 0);
        CSplitter p(&s);
        assert(!p.HasNext());
    }
    {   CONST_STRING(&s, "");
        CSplitter p(&s);
        assert(!p.HasNext());
    }
    {
        CONST_STRING(&s, "a");
        CSplitter p(&s);
        assert(p.HasNext());
        assert(p.Next() == "a");
        assert(!p.HasNext());
    }
    {
        CONST_STRING(&s, "a:");
        CSplitter p(&s);
        assert(p.HasNext());
        assert(p.Next() == "a");
        assert(!p.HasNext());
    }
    {
        CONST_STRING(&s, "a::");
        CSplitter p(&s);
        assert(p.HasNext());
        assert(p.Next() == "a");
        assert(!p.HasNext());
    }
    {
        CONST_STRING(&s, "::a::");
        CSplitter p(&s);
        assert(p.HasNext());
        assert(p.Next() == "a");
        assert(!p.HasNext());
    }
    {
        CONST_STRING(&s, "aa:bbb");
        CSplitter p(&s);
        assert(p.HasNext());
        assert(p.Next() == "aa");
        assert(p.HasNext());
        assert(p.Next() == "bbb");
        assert(!p.HasNext());
    }   return 0;
}

CUserConfigData::CUserConfigData(const CUserRepositories &repos,
        const CString &dflt)
    : m_DefaultRoot(dflt.GetString())
    , m_CurrentRoot(repos.GetMainPublicRoot())
    , m_CacheEnabled(repos.IsMainPublicCacheEnabled())
{}

rc_t CKDirectory::CreateNonExistingPublicDir(bool verbose,
    const char *path, ...) const
{
    va_list args;
    va_start(args, path);

    rc_t rc = CreateNonExistingDir(verbose, m_PublicAccess, path, args);

    va_end(args);

    return rc;
}

bool CKDirectory::IsDirectory(const char *path, ...) const {
    va_list args;
    va_start(args, path);

    KPathType type = KDirectoryVPathType(m_Self, path, args);

    va_end(args);

    return type == kptDir;
}

bool CKDirectory::Exists(const CString &path) const {
    const String *str = path.Get();

    if (str == NULL) {
        return false;
    }
    return Exists(str->addr);
}

rc_t CKDirectory::CheckPublicAccess(const CString &path, bool verbose) const {
    bool updated = false;
    return CheckAccess(path, updated, false, verbose);
}

std::string CKDirectory::Canonical ( const char * path ) const {
    char resolved [ PATH_MAX ] = "";
    rc_t rc =
        KDirectoryResolvePath ( m_Self, true, resolved, sizeof resolved, path );
    if ( rc != 0 )
        return "";
    size_t size  = string_measure ( path, NULL );
    if ( string_cmp
        ( path, size, resolved, string_measure ( resolved, NULL ), size ) == 0 )
        return "";
    return resolved;
}


rc_t CKConfig::UpdateNode(bool verbose,
    const char *value, const char *name, ...)
{
    va_list args;
    va_start(args, name);

    char dst[4096] = "";
    size_t num_writ = 0;
    rc_t rc = string_vprintf(dst, sizeof dst, &num_writ, name, args);
    if (rc == 0) {
        rc = UpdateNode(dst, value, verbose);
    }

    va_end(args);

    return rc;
}

#ifdef NAMESCGI
rc_t CKConfig::FixResolverCgiNodes ( void ) {
    rc_t rc = KConfigFixMainResolverCgiNode ( m_Self );
    rc_t r2 = KConfigFixProtectedResolverCgiNode  ( m_Self );
    if ( rc == 0 && r2 == 0 ) {
        m_Updated = true;
    } else {
        if ( rc == 0 ) {
            rc = r2;
        }
    }
    return rc;
}
#endif

CApp::CApp(const CKDirectory &dir, const CKConfigNode &rep,
        const string &root, const string &name)
    : m_HasVolume(false)
    , m_AppVolPath("apps/" + name + "/volumes")
{
    const KConfigNode *vols = rep.OpenNodeRead(m_AppVolPath);
    KNamelist *typeNames = NULL;
    rc_t rc = KConfigNodeListChildren(vols, &typeNames);
    if (rc != 0) {
        return;
    }
    uint32_t count = 0;
    rc = KNamelistCount(typeNames, &count);
    if (rc == 0) {
        for (uint32_t idx = 0; idx < count; ++idx) {
            const char *typeName = NULL;
            rc = KNamelistGet(typeNames, idx, &typeName);
            if (rc != 0) {
                continue;
            }
            const KConfigNode *alg = NULL;
            rc = KConfigNodeOpenNodeRead(vols, &alg, typeName);
            if (rc != 0) {
                continue;
            }
            String *volList = NULL;
            rc = KConfigNodeReadString(alg, &volList);
            if (rc == 0) {
                if (volList != NULL && volList->addr != NULL) {
                    m_Volumes[typeName]
                        = CAppVolume(typeName, volList->addr);
                }
                CSplitter volArray(volList);
                while (volArray.HasNext()) {
                    const string vol(volArray.Next());
                    if (dir.IsDirectory("%s/%s", root.c_str(), vol.c_str()))
                    {
                        m_HasVolume = true;
                        break;
                    }
                }
            }
            RELEASE(String, volList);
            RELEASE(KConfigNode, alg);
        }
    }
    RELEASE(KNamelist, typeNames);
    RELEASE(KConfigNode, vols);
}

rc_t CRepository::Update(CKConfig &kfg, string &node, bool verbose) {
    char root[4096] = "";
    rc_t rc = string_printf(root, sizeof root, NULL, "/repository/%s/%s/%s",
        m_Category.c_str(), m_SubCategory.c_str(), m_Name.c_str());
    if (rc != 0) {
        OUTMSG(("ERROR\n"));
        return rc;
    }

    if (DEBUG_LOCAL) {
        OUTMSG(("CRepository::Update: root = %s\n", root));
    }

    node.assign(root);

    for (CApps::TCI it = m_Apps.begin(); it != m_Apps.end(); ++it) {
        rc_t r2 = (*it).second->Update(kfg, root, verbose);
        if (r2 != 0 && rc == 0) {
            rc = r2;
        }
    }

    rc_t r2 = kfg.UpdateNode(verbose, m_Root.c_str(), "%s/root", root);
    if (r2 != 0 && rc == 0) {
        rc = r2;
    }

    return rc;
}

CRepository::CRepository(const string &category, const string &type,
        const string &name, const string &root)
    : m_Disabled(false)
    , m_Category(category)
    , m_SubCategory(type)
    , m_Name(name)
    , m_Root(root)
{}

CRepository::CRepository(const CKDirectory &dir, const CKConfigNode &repo,
        const string &category, const string &subCategory,
        const string &name)
    : m_Disabled(false)
    , m_Category(category)
    , m_SubCategory(subCategory)
    , m_Name(name)
    , m_Root(repo.ReadString("root"))
{
    m_Apps.Update(dir, repo, m_Root, "sra");
    m_Apps.Update(dir, repo, m_Root, "refseq");
    string disabled(repo.ReadString("disabled"));
    if (disabled == "true") {
        m_Disabled = true;
    }
}

string CRepository::Dump(void) const {
    char node[4096] = "";
    rc_t rc = string_printf(node, sizeof node, NULL,
        "/repository/%s/%s/%s", m_Category.c_str(),
        m_SubCategory.c_str(), m_Name.c_str(), m_Root.c_str());
    if (rc != 0) {
        OUTMSG(("ERROR\n"));
        return "";
    }

    for (CApps::TCI it = m_Apps.begin(); it != m_Apps.end(); ++it) {
        (*it).second->Dump(node);
    }

    if (m_Root.size() > 0) {
        OUTMSG(("%s/root = \"%s\"\n", node, m_Root.c_str()));
    }

    return node;
}

bool CRepository::Is(const string &subCategory, const string &name)
    const
{
    if (name.size() <= 0) {
        return m_SubCategory == subCategory;
    }

    return m_SubCategory == subCategory && m_Name == name;
}

string CRemoteRepository::GetRoot(const string &stack, char needle)
{
    if (EndsWith(stack, needle)) {
        return "";
    }
    else {
        return stack;
    }
}

string CRemoteRepository::GetCgi(const string &stack, char needle)
{
    if (EndsWith(stack, needle)) {
        return stack;
    }
    else {
        return "";
    }
}

bool CRemoteRepository::EndsWith(const string &stack, char needle) {
    if (stack.size() <= 0) {
        return false;
    }
    return stack[stack.size() - 1] == needle;
}

rc_t CRemoteRepository::Fix(CKConfig &kfg, bool disable, bool verbose) {
    if (verbose) {
        OUTMSG(("checking %s %s remote repository\n",
            GetCategory().c_str(), GetSubCategory().c_str()));
    }

    Disable(disable);

    if (Is("main")) {
        m_ResolverCgi
            = "https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi";
        ClearApps();
    }
    else {
        m_ResolverCgi = "";
        FixApps();
        SetRoot("https://ftp-trace.ncbi.nlm.nih.gov/sra");
    }

    return Update(kfg);
}

rc_t CUserRepository::Fix(CKConfig &kfg,
    const CUserConfigData *data, const string *root)
{
    Disable(false);

    if (DEBUG_LOCAL) {
        OUTMSG((__FILE__ " CUserRepository::Fix: data = %d\n", data));
    }

    if (data != NULL) {
        m_CacheEnabled = data->GetCacheEnabled();
        const string root(data->GetCurrentRoot());
        if (root.size() > 0) {
            if (DEBUG_LOCAL) {
                OUTMSG((__FILE__ " CUserRepository::Fix: SetRoot to %s\n",
                    root.c_str()));
            }
            SetRoot(root);
        }
    }
    else {
        m_CacheEnabled = true;
        assert(root);
        SetRoot(*root);
    }

    FixApps();

    return Update(kfg, DEBUG_LOCAL);
}

rc_t CUserRepositories::Load(const CKConfig &kfg, const CKDirectory &dir) {
    const string category("main");

    OUTMSG(("loading %s user repository... ", category.c_str()));
    const KConfigNode *userNode = kfg.OpenNodeRead
        ("/repository/user/%s", category.c_str());
    if (userNode == NULL) {
        OUTMSG(("not found\n"));
        return 0;
    }
    const CKConfigNode user(userNode);
    KNamelist *userNames = NULL;
    rc_t rc  = KConfigNodeListChildren(userNode, &userNames);
    if (rc != 0) {
        OUTMSG(("failed\n"));
    }
    uint32_t count = 0;
    if (rc == 0) {
        rc = KNamelistCount(userNames, &count);
    }
    if (rc != 0) {
        OUTMSG(("failed\n"));
    }
    for (uint32_t idx = 0; idx < count && rc == 0; ++idx) {
        const char *userName = NULL;
        rc = KNamelistGet(userNames, idx, &userName);
        if (rc != 0) {
            rc = 0;
            continue;
        }
        OUTMSG(("%s ", userName));
        const KConfigNode *userRepo = user.OpenNodeRead(userName);
        if (userRepo == NULL) {
            continue;
        }
        CKConfigNode node(userRepo);
        push_back(new CUserRepository(dir, node, category, userName));
    }
    RELEASE(KNamelist, userNames);

    if (rc == 0) {
        OUTMSG(("ok\n"));
    }
    else {
        OUTMSG(("failed\n"));
    }

    return rc;
}

const CUserRepository *CUserRepositories::FindMainPublic(void) const {
    for (TCI it = begin(); it != end(); ++it) {
        CUserRepository *r = *it;
        assert(r);
        if (DEBUG_LOCAL) {
            OUTMSG(("MainPublic not found\n"));
        }
        if (r->Is("main", "public")) {
            return r;
        }
    }

    return NULL;
}

rc_t CUserRepositories::MkAppVolumes(const CKDirectory &dir, bool verbose)
    const
{
    rc_t rc = 0;

    for (TCI it = begin(); it != end(); ++it) {
        CUserRepository *r = *it;
        assert(r);
        const string root(r->GetRoot());
        if (root.size() <= 0) {
            continue;
        }

        for (CApps::TCI it = r->AppsBegin(); it != r->AppsEnd(); ++it) {
            const CApp *a = (*it).second;
            assert(a);
            //a->Dump("a->Dump");
            for (CApp::TCAppVolumesCI it = a->VolumesBegin();
                it != a->VolumesEnd(); ++it)
            {
                const CAppVolume &v((*it).second);
                const string path(v.GetPath());
                if (path.size() <= 0) {
                    continue;
                }
                CSplitter s(path);
                while (s.HasNext()) {
                    rc_t r2 = dir.CreateNonExistingPublicDir
                        (verbose, "%s/%s", root.c_str(), s.Next().c_str());
                    if (r2 != 0 && rc == 0) {
                        rc = r2;
                    }
                }
            }
        }
    }

    return rc;
}

rc_t CRemoteRepositories::Load(const CKConfig &kfg, const CKDirectory &dir) {
    rc_t rc = 0;
    for (int i = 0; i < 2; ++i) {
        string category;
        switch (i) {
            case 0:
                category = "main";
                break;
            case 1:
                category = "aux";
                break;
            default:
                assert(0);
                break;
        }
        OUTMSG(("loading %s remote repository... ", category.c_str()));
        const KConfigNode *remoteNode = kfg.OpenNodeRead
            ("/repository/remote/%s", category.c_str());
        if (remoteNode == NULL) {
            OUTMSG(("not found\n"));
            continue;
        }
        const CKConfigNode remote(remoteNode);
        KNamelist *remoteNames = NULL;
        rc  = KConfigNodeListChildren(remoteNode, &remoteNames);
        if (rc != 0) {
            OUTMSG(("failed\n"));
        }
        uint32_t count = 0;
        if (rc == 0) {
            rc = KNamelistCount(remoteNames, &count);
        }
        if (rc != 0) {
            OUTMSG(("failed\n"));
        }
        for (uint32_t idx = 0; idx < count && rc == 0; ++idx) {
            const char *remoteName = NULL;
            rc = KNamelistGet(remoteNames, idx, &remoteName);
            if (rc != 0) {
                rc = 0;
                continue;
            }
            OUTMSG(("%s ", remoteName));
            const KConfigNode *remoteRepo = remote.OpenNodeRead(remoteName);
            if (remoteRepo == NULL) {
                continue;
            }
            CKConfigNode node(remoteRepo);
            push_back(new CRemoteRepository(dir,
                node, category, remoteName));
        }
        RELEASE(KNamelist, remoteNames);
        if (rc == 0) {
            OUTMSG(("ok\n"));
        }
        else {
            OUTMSG(("failed\n"));
        }
    }
    return rc;
}

void CRemoteRepositories::Fix(CKConfig &kfg, bool disable, bool verbose) {
    CRemoteRepository *main = NULL;
    CRemoteRepository *protectd = NULL;

    for (TCI it = begin(); it != end(); ++it) {
        CRemoteRepository *r = *it;
        assert(r);
        bool toDisable = disable;
        const string category(r->GetCategory());
        if (category == "aux") {
            continue;
        }
        else if (category == "main") {
            main = r;
        }
        else if (category == "protected") {
            protectd = r;
            toDisable = false;
        }
        r->Fix(kfg, toDisable, verbose);
    }

    const string cgi
        ("https://trace.ncbi.nlm.nih.gov/Traces/names/names.fcgi");
    if (main == NULL) {
        main = new CRemoteRepository("main", "CGI", cgi);
        main->Fix(kfg, disable);
        push_back(main);
    }

    if (protectd == NULL) {
        protectd = new CRemoteRepository("protected", "CGI", cgi);
        if (verbose) {
            OUTMSG(("creating %s %s remote repository\n",
                protectd->GetSubCategory().c_str(),
                protectd->GetName().c_str()));
        }
        protectd->Fix(kfg, false);
        push_back(protectd);
    }
}
