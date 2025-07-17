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

#include <cmath>

#include "configure.h" /* configure */
#include "interactive.h" /* run_interactive */
#include "util.hpp" // CNoncopyable
#include "vdb-config-model.hpp" // vdbconf_model

#include <klib/rc.h> /* RC */
#include <klib/vector.h> /* Vector */

#include <sstream> // stringstream

#include <climits> /* PATH_MAX */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

using std::string;

////////////////////////////////////////////////////////////////////////////////
class CConfigurator : CNoncopyable {
    CKConfig m_Cfg;
    CKDirectory m_Dir;
    rc_t CheckNcbiHome(bool &updated, bool verbose) const {
        if (verbose) {
            OUTMSG(("checking NCBI home... "));
        }

        const CString ncbi(m_Cfg.ReadNcbiHome());

        if (verbose) {
            OUTMSG(("%S\n", ncbi.Get()));
        }

        rc_t rc = 0;
        if (!ncbi.Empty()) {
            rc = m_Dir.CreateNonExistingPrivateDir(ncbi, verbose);
            if (rc == 0) {
                rc = m_Dir.CheckPrivateAccess(ncbi, updated, verbose);
            }
            if (rc == 0) {
                rc = m_Dir.CanWriteFile(ncbi, verbose);
            }
        }

        return rc;
    }
    rc_t CheckRepositories(bool fix) {
        {
            const string name("/repository/user/default-path");
            CString node(m_Cfg.ReadString(name.c_str()));
            if (node.Empty()) {
                CString home(m_Cfg.ReadString("HOME"));
                if (!home.Empty()) {
/* this rc is ignored */ m_Cfg.UpdateNode(name.c_str(),
                    (home.GetString() + "/ncbi").c_str());
                }
                else {
                    m_Cfg.UpdateNode(name.c_str(), "$(HOME)/ncbi");
                }
            }
            else {
                const string canonical
                    ( m_Dir . Canonical ( node . GetCString () ) );
                if ( ! canonical . empty () )
                    m_Cfg.UpdateNode ( name, canonical );
            }
        }
        rc_t rc = 0;
        if (fix) {
            const string name("/repository/site/disabled");
            if (m_Cfg.NodeExists(name)) {
                rc_t r2 = m_Cfg.UpdateNode(name.c_str(), "false");
                if (r2 != 0 && rc == 0)
                    rc = r2;
            }
        }
        const KRepositoryMgr *mgr = NULL;
        rc = KConfigMakeRepositoryMgrRead(m_Cfg.Get(), &mgr);
        KRepositoryVector repositories;
        memset(&repositories, 0, sizeof repositories);
        if (rc == 0) {
            rc = KRepositoryMgrRemoteRepositories(mgr, &repositories);
            if (rc == 0)
                KRepositoryVectorWhack(&repositories);
            else if
                (rc == SILENT_RC(rcKFG, rcNode, rcOpening, rcPath, rcNotFound))
            {
                rc = m_Cfg.CreateRemoteRepositories();
            }
        }
        if ( rc == 0 && fix )
            rc = m_Cfg.CreateRemoteRepositories(fix);

        if (rc == 0) {
#ifdef NAMESCGI
            m_Cfg . FixResolverCgiNodes ( );
#endif

            bool noUser = false;
            rc = KRepositoryMgrUserRepositories(mgr, &repositories);
            if (rc == 0) {
                uint32_t len = 0;
                if (rc == 0)
                    len = VectorLength(&repositories);
                if (len == 0)
                    noUser = true;
                else {
                    uint32_t i = 0;
                    noUser = true;
                    for (i = 0; i < len; ++i) {
                        KRepository *repo = static_cast<KRepository*>
                            (VectorGet(&repositories, i));
                        if (repo != NULL) {
                            char name[PATH_MAX] = "";
                            size_t size = 0;
                            rc = KRepositoryName(repo,
                                name, sizeof name, &size);
                            if (rc == 0) {
                                const char p[] = "public";
                                if (strcase_cmp(p, sizeof p - 1, name,
                                    size, sizeof name) == 0)
                                {
                                    noUser = false;
                                }
                                if (fix) {
                                    rc = m_Cfg.CreateUserRepository (name, fix);
                                }
                            }

                            char root[PATH_MAX] = "";
                            rc = KRepositoryRoot ( repo,
                                                   root, sizeof root, &size);
                            if (rc == 0) {
                                const string canonical
                                    ( m_Dir . Canonical ( root ) );
                                if ( ! canonical . empty () ) {
                                    rc = KRepositorySetRoot ( repo,
                                        canonical . c_str (),
                                        canonical . size () );
                                    if ( rc == 0 )
                                        m_Cfg . Updated ();
                                }
                            }
                        }
                    }
                    rc = 0;
                }
                KRepositoryVectorWhack(&repositories);
            }
            else if
                (rc == SILENT_RC(rcKFG, rcNode, rcOpening, rcPath, rcNotFound))
            {
                noUser = true;
            }
            if (noUser) {
                rc = m_Cfg.CreateUserRepository();
            }
        }
        RELEASE(KRepositoryMgr, mgr);
        return rc;
    }

    rc_t CheckConfig(bool fix) {
        rc_t rc = CheckRepositories(fix);
        if (rc == 0) {
            const string name("/tools/ascp/max_rate");
            CString node(m_Cfg.ReadString(name.c_str()));
            if (node.Empty()) {
                rc = m_Cfg.UpdateNode(name.c_str(), "450m");
            }
        }
        if (rc == 0) {
            CString config_default(m_Cfg.ReadDefaultConfig());
            if (!config_default.Empty() && !config_default.Equals("false")) {
                m_Cfg.UpdateNode("/config/default", "false");
            }
        }
        if (rc == 0) {
            m_Cfg.Commit();
        }
        return rc;
    }

protected:
    vdbconf_model *m_Config;

public:
    CConfigurator(bool fix = false, bool verbose = false): m_Config(NULL) {
#define TODO 1
        bool updated = false;
        rc_t rc = CheckNcbiHome(updated, verbose);
        if (rc == 0 && updated) {
            m_Cfg.Reload(verbose);
        }
        if (rc == 0) {
            rc = CheckConfig(fix);
            if (rc == 0) {
                m_Cfg.Reload(verbose);
            }
        }
        if (rc == 0) {
            m_Config = new vdbconf_model( m_Cfg );
            if (m_Config == NULL) {
                rc = TODO;
            }
        }
        if (rc != 0) {
            throw rc;
        }
    }
    virtual ~CConfigurator(void) { delete m_Config; }
    virtual rc_t Configure(void) {
        OUTMSG(("Fixed default configuration\n"));
        return 0;
    }
};
struct SUserRepo {
    bool cacheEnabled;
//  bool enabled;
    string root;
    SUserRepo(const vdbconf_model *kfg, int32_t id = -1) { Reload (kfg, id); }
    void Reload(const vdbconf_model *kfg, int32_t id = -1) {
        assert(kfg);
        if (id < 0) {
            cacheEnabled = kfg->is_user_cache_enabled();
//          enabled = kfg->is_user_enabled();
            root = kfg->get_public_location();
        }
        else {
            cacheEnabled = kfg->is_protected_repo_cached(id);
            root = kfg->get_repo_location(id);
        }
    }
};
struct SData {
    bool done;
    bool updated;
    bool site;
    struct SCrntData {
    private:
        const vdbconf_model *m_Kfg;
    public:
        bool site_enabled;
        bool remote_enabled;
        bool cache_disabled;
        SUserRepo userR;
        SCrntData(const vdbconf_model *kfg)
            : m_Kfg(kfg), userR(kfg)
        {
            Reload();
        }
        void Reload(void) {
            assert(m_Kfg);
            site_enabled = m_Kfg->is_site_enabled();
            remote_enabled = m_Kfg->is_remote_enabled();
            cache_disabled = ! m_Kfg->is_global_cache_enabled();
            userR.Reload(m_Kfg);
        }
    } crnt;
    SData(const vdbconf_model *kfg)
        : done(false)
        , updated(false)
        , site(kfg->does_site_repo_exist())
        , crnt(kfg)
    {}
};
struct STrinity {
private:
    string ToString(int i) { std::stringstream s; s << i; return s.str(); }
public:
//  const string enabled;
    const string cached;
    const string root;
    STrinity(const string &src)
//      : enabled(src.substr(0, 1))
        : cached (src.substr(0, 1))
        , root   (src.substr(1, 1))
    {}
    STrinity(int i)
//      : enabled(ToString(i + 0))
        : cached (ToString(i + 0))
        , root   (ToString(i + 1))
    {}
    void Print(void) const
    { OUTMSG(("%s %s\n", cached.c_str(), root.c_str())); }
};
class CTextualConfigurator : public CConfigurator {
    CStdIn m_Stdin;
    string Input(const string &prompt, const string &value) {
        OUTMSG(("\n\n%s:\n%s\n\nEnter the new path and Press <Enter>\n"
            "Press <Enter> to accept the path\n> ",
            prompt.c_str(), value.c_str()));
        char buffer[PATH_MAX] = "";
        size_t num_read = 0;
        rc_t rc = m_Stdin.Read(buffer, sizeof buffer, &num_read);
        if (rc == 0 && num_read > 0) {
            return string(buffer, num_read);
        }
        else {
            return "";
        }
    }

    void ProcessCancel(SData &d) {
        if (!d.updated) {
            d.done = true;
            return;
        }
        OUTMSG(("\nChanges in your configuration were not saved\n\n"));
        while (true) {
            OUTMSG((
                "To save changes and exit  : Enter Y and Press <Enter>\n"
                "To ignore changes and exit: Enter N and Press <Enter>\n"
                "To cancel and continue    : Press <Enter>\n"
                "\n"
                "Your choice > "));
            char answer = toupper(m_Stdin.Read1());
            switch (answer) {
                case '\0':
                    return;
                case  'Y':
                    OUTMSG(("Saving...\n"));
                    m_Config->commit();
                //  no break;
                case  'N':
                    OUTMSG(("Exiting..."));
                    d.done = true;
                    return;
                default:
                    OUTMSG(("Unrecognized input\n"));
            }
        }
    }
    enum EChoice {
        eCancel,
        eExit,
        eRemote,
        eSite,
        eUnknown,
        eUserCacheEnable,
        eGlobalCacheEnable,
//      eUserEnable,
        eUserRoot,
    };
    struct SChoice {
        EChoice choice;
        int32_t id;
        SChoice(EChoice c, int32_t i = -1) : choice(c), id(i)
        {}
    };
    class CSymGen {
        static const string magic;
    public:
        static STrinity Id2Seq(uint32_t id) {
            int d = id * 2 - (int)magic.size();
            if (d < 0) {
                return STrinity(magic.substr(id * 2, 2));
            }
            else {
                return STrinity(d + 10);
            }
        }
        static SChoice Seq2Choice(string /* copy */ s, uint32_t maxId) {
            if (s.length() <= 0 || s.length() > 2) {
                return SChoice(eUnknown);
            }
            else if (s.length() == 1) {
                size_t p = magic.find(s[0]);
                if (p == string::npos) {
                    return SChoice(eUnknown);
                }
                else {
                    EChoice c = eUnknown;
                    switch (p % 2) {
                        case 0: c = eUserCacheEnable; break;
                        case 1: c = eUserRoot       ; break;
                        default: assert(0); break;
                    }
                    return SChoice(c, (int)p / 2);
                }
            }
            else {
                assert(s.length() == 2);
                if (!isdigit(s[0]) || !!isdigit(s[0]) || s[0] == '0') {
                    return SChoice(eUnknown);
                }
                int id = (s[0] - '0') * 10 + s[1] - '0' + (int)magic.size();
                EChoice c = eUnknown;
                switch (id % 2) {
                    case 0: c = eUserCacheEnable; break;
                    case 1: c = eUserRoot       ; break;
                    default: assert(0); break;
                }
                return SChoice(c, id / 2);
            }
        }
        static void Ask(void) { OUTMSG(("magic.len = %d\n", magic.size())); }
    };
    SChoice Inquire(const SData &d) {
        OUTMSG(("     vdb-config interactive\n\n  data source\n\n"));
        OUTMSG(("   NCBI SRA: "));
        if (d.crnt.remote_enabled) {
            OUTMSG(("enabled (recommended) (1)\n\n"));
        }
        else {
            OUTMSG(("disabled (not recommended) (1)\n\n"));
        }
        if (d.site) {
        OUTMSG(("   site    : "));
            if (d.crnt.site_enabled) {
                OUTMSG(("enabled (recommended) (2)\n\n"));
            }
            else {
                OUTMSG(("disabled (not recommended) (2)\n\n"));
            }
        }
        OUTMSG(("\n  local workspaces: local file caching: "));
        if (d.crnt.cache_disabled) {
            OUTMSG(("disabled (not recommended) (6)\n"));
        }
        else {
            OUTMSG(("enabled (recommended) (6)\n"));
        }
        OUTMSG(("\n  Open Access Data\n"));
/*      if (d.crnt.userR.enabled) {
            OUTMSG(("enabled (recommended) (6)\n"));
        }
        else {
            OUTMSG(("disabled (not recommended) (6)\n"));
        }*/
        if (d.crnt.userR.cacheEnabled) {
            OUTMSG(("cached (recommended) (3)\n"));
        }
        else {
            OUTMSG(("not cached (not recommended) (3)\n"));
        }
        OUTMSG(("location: '%s' (4)\n", d.crnt.userR.root.c_str()));

        uint32_t id = 0;

        OUTMSG(("\n\n"
"To cancel and exit      : Press <Enter>\n"));
        if (d.updated) {
            OUTMSG(("To save changes and exit: Enter Y and Press <Enter>\n"));
        }
        OUTMSG((
"To update and continue  : Enter corresponding symbol and Press <Enter>\n"));
        OUTMSG(("\nYour choice > "));
        char answer = toupper(m_Stdin.Read1());
        switch (answer) {
            case '\0': return SChoice(eCancel);
            case  '1': return SChoice(eRemote);
            case  'Y': return SChoice(eExit);
            case  '2': //            case  'O':
                return d.site ? SChoice(eSite) : SChoice(eUnknown);
            case  '6': return SChoice(eGlobalCacheEnable);
            case  '3': return SChoice(eUserCacheEnable);
            case  '4': return SChoice(eUserRoot);
            default  : return CSymGen::Seq2Choice(string(1, answer), id);
        }
    }
    bool SetRoot(int32_t id, const string &old) {
        const string name(id < 0 ? "Public" : "dbGaP");
        const string prompt("Path to " + name + " Repository");
        bool flushOld = false, reuseNew = false, ask = true;
        string root;
        while (true) {
            if (ask) {
                root = Input(prompt, old);
                if (root.size() == 0) {
                    OUTMSG(("\nRoot path to '%s' repository was not changed\n",
                        name.c_str()));
                    return false;
                }
                OUTMSG(("\nChanging root path to '%s' repository to '%s'\n",
                    name.c_str(), root.c_str()));
                ask = false;
            }
            ESetRootState s = m_Config->change_repo_location
                (flushOld, root, reuseNew, id);
            switch (s) {
                case eSetRootState_OK:
                    return true;
                case eSetRootState_NewPathEmpty:
                case eSetRootState_Error:
                    assert(0);
                    return false;
                case eSetRootState_MkdirFail:
                    OUTMSG(("Error: cannot make directory '%s'\n",
                        root.c_str()));
                    ask = true;
                    break;
                case eSetRootState_NotChanged:
                    OUTMSG(("Keeping '%s' path unchanged\n", root.c_str()));
                    return false;
                case eSetRootState_NewNotDir:
                    OUTMSG(("Error: '%s' exists and is not a directory\n",
                        root.c_str()));
                    ask = true;
                    break;
                case eSetRootState_NotUnique:
                    OUTMSG(("Error: there is another repository in '%s'\n",
                        root.c_str()));
                    ask = true;
                    break;
                case eSetRootState_NewDirNotEmpty: {
                    OUTMSG(("Warning: directory '%s' is not empty\n"
                        "Would you like to use it? (y/N)? > ", root.c_str()));
                    char answer = toupper(m_Stdin.Read1());
                    if (answer == 'Y') {
                        reuseNew = true;
                    }
                    else {
                        ask = true;
                    }
                    break;
                }
                case eSetRootState_OldNotEmpty: {
                    OUTMSG(("Warning: your repository '%s' is not empty\n"
                        "Would you like to clear it? (y/N)? > ", old.c_str()));
                    char answer = toupper(m_Stdin.Read1());
                    if (answer == 'Y') {
                        OUTMSG(("Clearing the old repository...\n"));
                        flushOld = true;
                    }
                    else {
                        ask = true;
                    }
                    break;
                }
            }
        }
    }
    virtual rc_t Configure(void) {
        assert(m_Config);
        SData d(m_Config);
        while (!d.done) {
            d.crnt.Reload();
            SChoice answer = Inquire(d);
            OUTMSG(("\n"));
            switch (answer.choice) {
                case eSite:
                    if (d.crnt.site_enabled) {
                        OUTMSG(("WARNING: DISABLING SITE REPOSITORY!!!"));
                    }
                    else {
                        OUTMSG(("Enabling site repository..."));
                    }
                    m_Config->set_site_enabled(!d.crnt.site_enabled);
                    d.updated = true;
                    break;
                case eRemote:
                    if (d.crnt.remote_enabled) {
                        OUTMSG(("WARNING: DISABLING REMOTE REPOSITORY!!!"));
                    }
                    else {
                        OUTMSG(("Enabling remote repository..."));
                    }
                    m_Config->set_remote_enabled(!d.crnt.remote_enabled);
                    d.updated = true;
                    break;
                case eGlobalCacheEnable:
                    if (d.crnt.cache_disabled) {
                        OUTMSG(("Enabling local file caching..."));
                    }
                    else {
                        OUTMSG(("WARNING: DISABLING LOCAL FILE CACHING!!!"));
                    }
                    m_Config->set_global_cache_enabled(d.crnt.cache_disabled);
                    d.updated = true;
                    break;
                case eUserCacheEnable:
                    if (answer.id < 0) {
                        if (d.crnt.userR.cacheEnabled) {
                            OUTMSG(("WARNING: "
                                "DISABLING USER REPOSITORY CACHING!!!"));
                        }
                        else {
                            OUTMSG(("Enabling user repository caching..."));
                        }
                        m_Config->set_user_cache_enabled
                            (!d.crnt.userR.cacheEnabled);
                        d.updated = true;
                    }
                    break;
                case eUserRoot: {
                    string root(d.crnt.userR.root);
                    d.updated = SetRoot(answer.id, root);
                    break;
                }
                case eExit:
                    OUTMSG(("Saving..."));
                    m_Config->commit();
                    d.done = true;
                    break;
                case eCancel:
                    OUTMSG(("Canceling...\n\n"));
                    ProcessCancel(d);
                    break;
                default:
                    OUTMSG(("Unrecognized input. Continuing..."));
                    break;
            }
            OUTMSG(("\n\n\n"));
        }
        return 0;
    }
public:
    CTextualConfigurator(void) {}
};
const string CTextualConfigurator::CSymGen::magic("56789ABCDEFGHIJKLMNOPQRSTUVWXZ");

class CVisualConfigurator : public CConfigurator
{
    virtual rc_t Configure( void )
    {
        if ( m_Config == NULL )
        {
            return TODO;
        }
        /* here we can switch between:
             - the old view : run_interactive() just repositories and caching
             - the new view : run_interactive2() with cloud settings and repositories
         */
        return run_interactive( *m_Config );
    }
};

rc_t configure(EConfigMode mode) {
    rc_t rc = 0;
    CConfigurator *c = NULL;
    try {
        switch (mode) {
            case eCfgModeDefault:
                c = new CConfigurator(true);
                break;
            case eCfgModeTextual:
                c = new CTextualConfigurator;
                break;
            default:
                c = new CVisualConfigurator;
                break;
        }
        rc = c->Configure();
    }
    catch (rc_t re) {
        if (rc == 0) {
            rc = re;
        }
    }
    catch (...) {
        if (rc == 0) {
            rc = TODO;
        }
    }
    delete c;
    return rc;
}

