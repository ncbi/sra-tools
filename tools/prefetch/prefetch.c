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

/********** includes **********/

#include <cloud/manager.h> /* CloudMgrMake */

#include <kapp/args.h> /* ArgsParamCount */
#include <kapp/main.h> /* Quitting */

#include <kdb/manager.h> /* kptDatabase */

#include <kfg/kart.h> /* Kart */
#include <kfg/repository.h> /* KRepositoryMgr */

#include <vdb/dependencies.h> /* VDBDependenciesRemoteAndCache */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/vdb-priv.h> /* VDBManagerPathTypeUnreliable */

#include <vfs/manager.h> /* VFSManagerMakePathWithExtension */
#include <vfs/manager-priv.h> /* VResolverCacheForUrl */
#include <vfs/path.h> /* VPathRelease */
#include <vfs/resolver-priv.h> /* VResolverQueryWithDir */
#include <vfs/services-priv.h> /* KServiceNamesQueryExt */

#include <kns/ascp.h> /* AscpOptions */
#include <kns/manager.h>
#include <kns/stream.h> /* KStreamRelease */
#include <kns/kns-mgr-priv.h> /* KNSManagerMakeReliableClientRequest */
#include <kns/http.h>

#include <kfs/directory.h> /* KDirectoryPathType */
#include <kfs/file.h> /* KFile */
#include <kfs/gzip.h> /* KFileMakeGzipForRead */
#include <kfs/md5.h> /* KFileMakeMD5Read */
#include <kfs/subfile.h> /* KFileMakeSubRead */
#include <kfs/cacheteefile.h> /* KDirectoryMakeCacheTee */

#include <klib/container.h> /* BSTree */
#include <klib/data-buffer.h> /* KDataBuffer */
#include <klib/out.h> /* OUTMSG */
#include <klib/printf.h> /* string_printf */
#include <klib/progressbar.h> /* make_progressbar */
#include <klib/rc.h>
#include <klib/status.h> /* STSMSG */
#include <klib/text.h> /* String */

#include <strtol.h> /* strtou64 */
#include <sysalloc.h>

#include <assert.h>
#include <ctype.h> /* isdigit */
#include <stdlib.h> /* free */
#include <string.h> /* memset */
#include <time.h> /* time */

#include <stdio.h> /* printf */

#include "kfile-no-q.h"
#include "PrfMain.h"
#include "PrfRetrier.h"
#include "PrfOutFile.h"

#define USE_CURL 0
#define ALLOW_STRIP_QUALS 0

#define rcResolver   rcTree
static bool NotFoundByResolver(rc_t rc) {
    if (GetRCModule(rc) == rcVFS) {
        if (GetRCTarget(rc) == rcResolver) {
            if (GetRCContext(rc) == rcResolving) {
                if (GetRCState(rc) == rcNotFound) {
                    return true;
                }
            }
        }
    }
    return false;
}

typedef struct {
    /* "plain" command line argument */
    const char *desc;

    const KartItem *item;

#if _DEBUGGING
    const char *textkart;
#endif

    const char *jwtCart;

    Resolved resolved;
    int number;
    
    bool isDependency;
    char * seq_id;

    PrfMain *mane; /* just a pointer, no refcount here, don't release it */
} Item;

typedef struct {
    const char *obj;
    bool done;
    Kart *kart;
    bool isKart;
    const char *jwtCart;
} Iterator;

typedef struct {
    BSTNode n;
    Item *i;
} KartTreeNode;

/********** String extension **********/
static rc_t StringRelease(const String *self) {
    free((String*)self);
    return 0;
}

static char* StringCheck(const String *self, rc_t rc) {
    if (rc == 0 && self != NULL
        && self->addr != NULL && self->len != 0 && self->addr[0] != '\0')
    {
        return string_dup(self->addr, self->size);
    }
    return NULL;
}

static
bool _SchemeIsFasp(const String *self) {
    const char fasp[] = "fasp";
    return _StringIsXYZ(self, NULL, fasp, sizeof fasp - 1);
}

/********** KDirectory extension ***********/
static rc_t _KDirectoryMkLockName(const KDirectory *self,
    const String *prefix, char *out, size_t sz)
{
    rc_t rc = 0;
    size_t num_writ = 0;

    assert(prefix);

    rc = string_printf(out, sz, &num_writ, "%S.lock", prefix);
    DISP_RC2(rc, "string_printf(lock)", prefix->addr);

    if (rc == 0 && num_writ > sz) {
        rc = RC(rcExe, rcFile, rcCopying, rcBuffer, rcInsufficient);
        PLOGERR(klogInt, (klogInt, rc,
            "bad string_printf($(s).lock) result", "s=%S", prefix));
        return rc;
    }

    return rc;
}

static
rc_t _KDirectoryCleanCache(KDirectory *self, const String *local)
{
    rc_t rc = 0;

    char cache[PATH_MAX] = "";
    size_t num_writ = 0;

    assert(self && local && local->addr);

    if (rc == 0) {
        rc = string_printf(cache, sizeof cache, &num_writ, "%S.cache", local);
        DISP_RC2(rc, "string_printf(.cache)", local->addr);
    }

    if (rc == 0 && KDirectoryPathType(self, "%s", cache) != kptNotFound) {
        STSMSG(STS_DBG, ("removing %s", cache));
        rc = KDirectoryRemove(self, false, "%s", cache);
    }

    return rc;
}

static rc_t _KDirectoryClean(KDirectory *self, const String *cache,
    const char *lock)
{
    rc_t rc = 0;
    rc_t rc2 = 0;

    assert(self && cache);

#if 0
    char tmpName[PATH_MAX] = "";
    const char *dir = tmpName;
    const char *tmpPfx = NULL;
    size_t tmpPfxLen = 0;

    rc = _KDirectoryMkTmpPrefix(self, cache, tmpName, sizeof tmpName);
    if (rc == 0) {
        char *slash = strrchr(tmpName, '/');
        if (slash != NULL) {
            if (strlen(tmpName) == slash + 1 - tmpName) {
                rc = RC(rcExe,
                    rcDirectory, rcSearching, rcDirectory, rcIncorrect);
                PLOGERR(klogInt, (klogInt, rc,
                    "bad file name $(path)", "path=%s", tmpName));
            }
            else {
                *slash = '\0';
                tmpPfx = slash + 1;
            }
        }
        else
            tmpPfx = tmpName;
        tmpPfxLen = strlen(tmpPfx);
    }

    if (rc == 0 && false ) {
        uint32_t count = 0;
        uint32_t i = 0;
        KNamelist *list = NULL;
        STSMSG(STS_DBG, ("listing %s for old temporary files", dir));
        rc = KDirectoryList(self, &list, NULL, NULL, "%s", dir);
        if (rc == SILENT_RC(rcFS, rcDirectory, rcListing, rcPath, rcNotFound))
            rc = 0;
        DISP_RC2(rc, "KDirectoryList", dir);

        if (rc == 0 && list != NULL) {
            rc = KNamelistCount(list, &count);
            DISP_RC2(rc, "KNamelistCount(KDirectoryList)", dir);
        }

        for (i = 0; i < count && rc == 0; ++i) {
            const char *name = NULL;
            rc = KNamelistGet(list, i, &name);
            if (rc != 0)
                DISP_RC2(rc, "KNamelistGet(KDirectoryList)", dir);
            else {
                if (strncmp(name, tmpPfx, tmpPfxLen) == 0) {
                    rc_t rc3 = 0;
                    STSMSG(STS_DBG, ("removing %s", name));
                    rc3 = KDirectoryRemove(self, false,
                        "%s%c%s", dir, '/', name);
                    if (rc2 == 0 && rc3 != 0) {
                        rc2 = rc3;
                    }
                }
            }
        }

        RELEASE(KNamelist, list);
    }
#endif

    if (lock != NULL && KDirectoryPathType(self, "%s", lock) != kptNotFound) {
        rc_t rc3 = 0;
        STSMSG(STS_DBG, ("removing %s", lock));
        rc3 = KDirectoryRemove(self, false, "%s", lock);
        if (rc2 == 0 && rc3 != 0)
            rc2 = rc3;
    }

    if (rc == 0 && rc2 != 0)
        rc = rc2;

    return rc;
}

/********** VResolver extension **********/
static rc_t KService_ProcessId(KService * self,
    const KartItem * item, const char * id, rc_t aRc, bool * dbgap)
{
    rc_t rc = 0;
    bool numeric = true;
    int i = 0; assert(id);
    for (i = 0; id[i] != '\0'; ++i)
        if (!isdigit(id[i])) {
            numeric = false;
            break;
        }

    assert(dbgap);
    *dbgap = false;

    if (!numeric) {
        *dbgap = false;
        return rc;
    }

    if (numeric) {
        char ticket[4096] = "";
        rc_t rc = KartItemGetTicket(item, ticket, sizeof ticket, NULL);
        if (rc == 0)
            *dbgap = true;
    }

    return rc;
}
static rc_t V_ResolverRemote(const VResolver *self,
    Resolved * resolved, VRemoteProtocols protocols,
    struct VPath const ** cache,
    const char * odir, const char * ofile, const Item * item )
{
    rc_t rc = 0;

    const VPath **local = NULL;

    uint32_t l = 0;
    const char * id = item -> desc;
    KService * service = NULL;
    const KSrvRespObj * obj = NULL;
    KSrvRespObjIterator * it = NULL;
    KSrvRespFile * file = NULL;
    const char * cgi = NULL;

    assert(resolved && item && item->mane);

    local = &resolved->local.path;

    rc = KServiceMake ( & service );
    if ( rc == 0 && item -> seq_id != NULL ) {
        assert ( item -> isDependency  );
        id = item -> seq_id;
    }

    if ( id == NULL )
        id = resolved -> name;

    assert ( id );

    if ( rc == 0 ) {
        if ( resolved -> project != 0 || resolved -> dbgapProject ) {
            bool dbgap = false;
            rc = KServiceAddProject ( service, resolved -> project );
            if (rc != 0)
                rc = KService_ProcessId(service, item->item, id, rc, &dbgap);
            if ( rc == 0 ) {
                char path [ 512 ] = "dbgap|";
                size_t s = strlen(path);
                char * p = path + s;
                if (!dbgap) {
                    s = 0;
                    p = path;
                }

                rc = VPathReadPath ( resolved -> accession,
                                        p, sizeof path - s, NULL );
                if (rc == 0) {
                    if (dbgap)
                        rc = KServiceAddObject(service, path);
                    else
                        rc = KServiceAddId(service, path);
                }
            }
        }
        else { /* to investigate for dbGaP project 0 */
            uint32_t project = 0;
            rc_t r = VResolverGetProject ( self, & project );
            if ( r == 0 && project != 0 )
                rc = KServiceAddProject ( service, project );
            if ( rc == 0 && item -> jwtCart == NULL ) {
/*              rc = KService_ProcessId(service, item->item, id, rc); */
                rc = KServiceAddId ( service, id );
            }
        }
    }

    if (rc == 0 && item->mane->fileType != NULL)
        rc = KServiceSetFormat(service, item->mane->fileType);

    if (rc == 0 && item->mane->location != NULL)
        rc = KServiceSetLocation(service, item->mane->location);

    if (rc == 0 && item->mane->jwtCart != NULL) {
        uint32_t pcount = 0;
        uint32_t i = 0;
        rc = KServiceSetJwtKartFile(service, item->mane->jwtCart);
        if (rc != 0)
            PLOGERR(klogErr, (klogErr, rc,
                "cannot use '$(perm)' as jwt cart file",
                "perm=%s", item->mane->jwtCart));
        if (rc == 0)
            rc = ArgsParamCount(item->mane->args, &pcount);
        for (i = 0; i < pcount && rc == 0; ++i) {
            const char *obj = NULL;
            rc = ArgsParamValue(item->mane->args, i, (const void **)&obj);
            if (rc == 0)
                rc = KServiceAddId(service, obj);
        }
    }

    if (rc == 0 && item->mane->ngc != NULL) {
        rc = KServiceSetNgcFile(service, item->mane->ngc);
        if (rc != 0)
            PLOGERR(klogErr, (klogErr, rc,
                "cannot use '$(ngc)' as ngc file",
                "ngc=%s", item->mane->ngc));
    }

    if ( rc == 0 )
        rc = KServiceNamesQueryExt ( service, protocols, cgi,
            NULL, odir, ofile, &resolved->response );

    if ( rc == 0 )
        l = KSrvResponseLength  (resolved->response );

    if ( rc == 0 && l > 0 )
        rc = KSrvResponseGetObjByIdx (resolved->response, 0, & obj );
    if ( rc == 0 && l > 0 )
        rc = KSrvRespObjMakeIterator ( obj, & it );
    if (rc == 0 && l > 0) {
        RELEASE(KSrvRespObjIterator, resolved->respIt);
        resolved->respIt = it;
        rc = KSrvRespObjIteratorNextFile(it, &file);
    }
    if ( rc == 0 && l > 0 ) {
        KSrvRespFileIterator * fi = NULL;
        String fasp;
        String http;
        String https;
        String scheme;
        CONST_STRING(&fasp, "fasp");
        CONST_STRING(&http, "http");
        CONST_STRING(&https, "https");

        RELEASE(KSrvRespFile, resolved->respFile);
        resolved->respFile = file;

        rc = KSrvRespFileMakeIterator ( file, & fi );
        while ( rc == 0 ) {
            const VPath * path = NULL;
            rc = KSrvRespFileIteratorNextPath ( fi, & path );
            if ( rc == 0 ) {
                bool ascp = false;
                VPathStr * v = NULL;
                if (path == NULL)
                    break;
                memset(&scheme, 0, sizeof scheme);
                rc = VPathGetScheme(path, &scheme);
                if (rc == 0) {
                    if (StringEqual(&scheme, &https))
                        v = &resolved->remoteHttps;
                    else if (StringEqual(&scheme, &fasp)) {
                        v = &resolved->remoteFasp;
                        ascp = true;
                    }
                    else if (StringEqual(&scheme, &http))
                        v = &resolved->remoteHttp;
                    assert(v);
                    assert(path);
                    if (v->path != NULL)
                        continue;
                    RELEASE(VPath, v->path);
                    v->path = path;
                }

                if ( rc == 0 ) {
                    char path [ PATH_MAX ] = "";
                    size_t len = 0;
                    rc = VPathReadUri ( v -> path,
                                        path, sizeof path, & len );
                    DISP_RC2 ( rc, "VPathReadUri(VResolverRemote)",
                                    resolved -> name );
                    if ( rc == 0 ) {
                        String local_str;
                        char * query = string_chr ( path, len, '?' );
                        if ( ascp && query != NULL ) {
                            * query = '\0';
                            len = query - path;
                        }
                        else {
                            query = string_chr(path, len, '#');
                            if (ascp && query != NULL) {
                                *query = '\0';
                                len = query - path;
                            }
                        }
                        StringInit ( & local_str,
                                        path, len, ( uint32_t ) len );
                        RELEASE ( String, v -> str );
                        rc = StringCopy ( & v -> str, & local_str );
                        DISP_RC2 ( rc, "StringCopy(VResolverRemote)",
                                        resolved -> name );
                    }
                }
            }
            else if ( NotFoundByResolver ( rc ) )
                PLOGERR ( klogErr, (klogErr, rc,
                            "'$(acc)' cannot be found.", "acc=%s",
                            resolved -> name ) );
            else
                DISP_RC2 ( rc, "Cannot resolve remote",
                            resolved -> name );
        }
        RELEASE ( KSrvRespFileIterator, fi );
    }
    if ( rc == 0 && l > 0 ) {
        if ( rc == 0 ) {
            rc = KSrvRespFileGetCache ( file, cache );
            if ( rc != 0 && NotFoundByResolver(rc) )
                rc = 0;
        }
        if ( rc == 0 ) {
            rc = KSrvRespFileGetLocal ( file, local );
            if ( rc != 0 && NotFoundByResolver(rc) )
                rc = 0;
        }
    }
    RELEASE ( KSrvRespObj, obj );
    RELEASE ( KService, service );
    return rc;
}

static rc_t _VResolverRemote(VResolver *self, Resolved * resolved,
    VRemoteProtocols protocols, const Item * item)
{
    rc_t rc = 0;
    const VPath *vcache = NULL;
    const char * dir = NULL;
    const PrfMain * mane = NULL;

    const char *name = NULL;
    const String **cache = NULL;

    assert(item);

    assert ( resolved );
    name = resolved -> name;
    cache = & resolved -> cache;

    mane = item -> mane;
    assert ( mane );
    dir = mane->outDir;

    assert ( item -> mane );
    rc = V_ResolverRemote(self, resolved, protocols,
                          &vcache, dir, item -> mane -> outFile, item );
    if (rc == 0 && cache != NULL) {
        String path_str;
        if (mane->outFile != NULL)
            StringInitCString ( & path_str, mane -> outFile );
        else if (vcache == NULL) {
            rc = RC(rcExe, rcResolver, rcResolving, rcPath, rcNotFound);
            PLOGERR(klogInt, (klogInt, rc,
                "cannot get cache location for '$(acc)'", "acc=%s", name));
        }

        if (rc == 0 && mane->outFile == NULL) {
            rc = VPathGetPath(vcache, &path_str);
            DISP_RC2(rc, "VPathGetPath(VResolverCache)", name);
        }

        if (rc == 0) {
            if (*cache != NULL)
                free((void*)*cache);
            rc = StringCopy(cache, &path_str);
            DISP_RC2(rc, "StringCopy(VResolverCache)", name);
        }
    }

    RELEASE(VPath, vcache);

    return rc;
}

/********** VPathStr **********/
static rc_t VPathStrFini(VPathStr *self) {
    rc_t rc = 0;

    assert(self);

    VPathRelease(self->path);

    RELEASE(String, self->str);

    memset(self, 0, sizeof *self);

    return rc;
}

static
rc_t VPathStrInitStr(VPathStr *self, const char *str, size_t len)
{
    String s;
    assert(self);
    if (len == 0) {
        len = string_size(str);
    }
    StringInit(&s, str, len, (uint32_t)len);
    VPathStrFini(self);
    return StringCopy(&self->str, &s);
}

static rc_t VPathStrInit(VPathStr *self, const VPath * path) {
    rc_t rc = VPathStrFini(self);

    if (path == NULL)
        return rc;

    if (rc != 0)
        return rc;
    else {
        String cache;

        self->path = path;

        rc = VPathGetPath(path, &cache);
        if (rc != 0)
            return rc;

        rc = StringCopy(&self->str, &cache);
    }

    return rc;
}

/********** NumIterator **********/

typedef enum {
    eNIBegin,
    eNINumber,
    eNIInterval,
    eNIDash,
    eNIComma,
    eNIBad,
    eNIForever,
    eNIEnd
} ENumIteratorState;

typedef struct {
    ENumIteratorState state;
    bool skip;
    const char *s;
    int32_t crnt;
    int32_t intEnd;
} NumIterator;

static void NumIteratorInit(NumIterator *self, const char *row) {
    assert(self);
    memset(self, 0, sizeof *self);
    self->crnt = self->intEnd = -1;
    self->s = row;
}

static int32_t NumIteratorGetNum(NumIterator *self) {
    int32_t n = 0;
    for (n = 0; *(self->s) >= '0' && *(self->s) <= '9'; ++(self->s)) {
        n = n * 10 + *(self->s) - '0';
    }
    return n;
}

static bool NumIteratorNext(NumIterator *self, int64_t crnt) {
    char c = '\0';
    assert(self);
    while (true) {
        self->skip = false;
        switch (self->state) {
            case eNIBegin:
            case eNIComma:
                if (self->s == NULL || *(self->s) == '\0') {
                    if (self->state == eNIBegin) {
                        self->state = eNIForever;
                        continue;
                    }
                    else {
                        self->state = eNIEnd;
                        continue;
                    }
                }
                c = *(self->s);
                ++(self->s);
                if (c == ',') {
                    self->state = eNIComma;
                    continue;
                }
                else if (c == '-') {
                    self->state = eNIDash;
                    continue;
                }
                else if (c >= '0' && c <= '9') {
                    --(self->s);
                    self->crnt = NumIteratorGetNum(self);
                    self->state = eNINumber;
                    if (self->crnt < crnt) {
                        continue;
                    }
                    else {
                        if (self->crnt > crnt) {
                            self->skip = true;
                        }
                        return true;
                    }
                    continue;
                }
                else {
                    self->state = eNIBad;
                    continue;
                }
            case eNIInterval:
                if (crnt <= self->intEnd) {
                    return true;
                }
          /* no break here */
            case eNINumber:
                if (self->crnt >= crnt) {
                    if (self->crnt > crnt) {
                        self->skip = true;
                    }
                    return true;
                }
                if (self->s == NULL || *(self->s) == '\0') {
                    self->state = eNIEnd;
                    continue;
                }
                c = *(self->s);
                ++(self->s);
                if (c == ',') {
                    self->state = eNIComma;
                    continue;
                }
                else if (c == '-') {
                    self->state = eNIDash;
                    continue;
                }
                else {
                    self->state = eNIBad;
                    continue;
                }
            case eNIDash:
                if (self->s == NULL || *(self->s) == '\0') {
                    self->state = eNIForever;
                    continue;
                }
                c = *(self->s);
                ++(self->s);
                if (c == ',' || c == '-') {
                    self->state = eNIForever;
                    continue;
                }
                else if (c >= '0' && c <= '9') {
                    --(self->s);
                    self->intEnd = NumIteratorGetNum(self);
                    self->state = eNIInterval;
                    if (crnt <= self->intEnd) {
                        return true;
                    }
                    else {
                        continue;
                    }
                }
                else {
                    self->state = eNIBad;
                    continue;
                }
            case eNIForever:
                return true;
            case eNIBad:
            case eNIEnd:
                return false;
        }
    }
}

/********** Resolved **********/
static rc_t ResolvedFini(Resolved *self) {
    rc_t rc = 0;
    rc_t rc2 = 0;

    assert(self);

    rc  = VPathStrFini(&self->local);
    rc2 = VPathStrFini(&self->remoteHttp);
    rc2 = VPathStrFini(&self->remoteHttps);
    rc2 = VPathStrFini(&self->remoteFasp);
    if (rc == 0 && rc2 != 0) {
        rc = rc2;
    }
    rc2 = VPathStrFini(&self->path);

    RELEASE(KFile, self->file);
    RELEASE(VPath, self->accession);
    RELEASE(VResolver, self->resolver);

    RELEASE(KartItem, self->kartItem);

    RELEASE(String, self->cache);

    RELEASE(KSrvResponse, self->response);
    RELEASE(KSrvRespObjIterator, self->respIt);
    RELEASE(KSrvRespFile, self->respFile);

    free(self->name);

    memset(self, 0, sizeof *self);

    return rc != 0 ? rc : rc2;
}

static void ResolvedReset(Resolved *self, ERunType type) {
    assert(self);

    memset(self, 0, sizeof *self);

    self->type = type;
}

/** isLocal is set to true when the object is found locally.
    i.e. does not need need not be [re]downloaded */
static rc_t ResolvedLocal(const Resolved *self,
    const PrfMain *mane, bool *isLocal, EForce force)
{
    rc_t rc = 0;
    const KDirectory *dir = NULL;
    uint64_t sRemote = 0;
    uint64_t sLocal = 0;
    const KFile *local = NULL;
    char path[PATH_MAX] = "";

    bool transFile = false;
    KPathType type = kptNotFound;

    assert(isLocal && self && mane);
    dir = mane -> dir;

    *isLocal = false;

    if (self->local.str == NULL) {
        return 0;
    }

    rc = VPathReadPath(self->local.path, path, sizeof path, NULL);
    DISP_RC(rc, "VPathReadPath");

    type = KDirectoryPathType(dir, "%s", path) & ~kptAlias;
    if (type == kptDir) {
        if ((KDirectoryPathType(dir, "%s/%s.sra.tmp",
            path, path) & ~kptAlias) == kptFile)
        {
            transFile = true;
        }
        else {
            uint32_t projectId = 0;
            if (VPathGetProjectId(self->remoteHttps.path, & projectId))
                if ((KDirectoryPathType(dir, "%s/%s_dbGaP-%d.sra.tmp",
                    path, path, projectId) & ~kptAlias) == kptFile)
                {
                    transFile = true;
                }
        }
    }

    if (rc == 0 && type != kptFile) {
        if (transFile); /* ignore it: will resume */
        else if (force == eForceNo) {
            STSMSG(STS_TOP,
                ("%s (not a file) is found locally: consider it complete",
                 path));
            *isLocal = true;
        }
        else {
            STSMSG(STS_TOP,
                ("%s (not a file) is found locally and will be redownloaded",
                 path));
        }
        return 0;
    }

    if (rc == 0) {
        rc = KDirectoryOpenFileRead(dir, &local, "%s", path);
        DISP_RC2(rc, "KDirectoryOpenFileRead", path);
    }
    if (rc == 0) {
        rc = KFileSize(local, &sLocal);
        DISP_RC2(rc, "KFileSize", path);
    }

    if (self->respFile != NULL)
        rc = KSrvRespFileGetSize(self->respFile, & sRemote);
    else if (rc == 0) {
        if (self->file != NULL) {
            rc = KFileSize(self->file, &sRemote);
            DISP_RC2(rc, "KFileSize(remote)", self->name);
        }
        else
            sRemote = self->remoteSz;
    }

    if (rc == 0) {
        if (sRemote == 0) {
            if (sLocal != 0) {
                if (force == eForceNo) {
                    *isLocal = true;
                    STSMSG(STS_INFO, ("%s (%,lu) is found", path, sLocal));
                }
                else {
                    STSMSG(STS_INFO,
                        ("%s (%,lu) is found and will be redownloaded",
                        path, sLocal));
                }
            }
            else if (sLocal == 0) {
                STSMSG(STS_INFO,
                    ("an empty %s (%,lu) is found and will be redownloaded",
                    path, sLocal));
            }
        }
        else if (sRemote == sLocal) {
            if (force == eForceNo) {
                *isLocal = true;
                STSMSG(STS_INFO, ("%s (%,lu) is found and is complete",
                    path, sLocal));
            }
            else {
                STSMSG(STS_INFO, ("%s (%,lu) is found and will be redownloaded",
                    path, sLocal));
            }
        }
        else { /* double check the size
    ( in case returned by resolver for decrypted != the real size ) */
            const VPath * http = NULL;
            rc = KSrvRespFileGetHttp(self->respFile, &http);
            if (rc == 0 && http != NULL) {
                char path[PATH_MAX] = "";
                size_t len = 0;
                rc = VPathReadUri(http, path, sizeof path, &len);
                if (rc == 0) {
                    const KFile * file = NULL;
                    rc = KNSManagerMakeHttpFile(mane->kns, &file, NULL,
                        0x01010000, "%s", path);
                    if (rc == 0)
                        rc = KFileSize(file, &sRemote);
                    RELEASE(KFile, file);
                }
            }
            RELEASE(VPath, http);
            if (rc == 0 && sRemote == sLocal) {
                if (force == eForceNo) {
                    *isLocal = true;
                    STSMSG(STS_INFO, ("%s (%,lu) is found and is complete",
                        path, sLocal));
                }
            }
            else
                STSMSG(STS_TOP, (
                    "%s (%,lu) is incomplete. Expected size is %,lu. "
                    "It will be re-downloaded", path, sLocal, sRemote));
        }
    }

    RELEASE(KFile, local);

    return rc;
}

static rc_t PrfMainDownloadStream(const PrfMain * self, PrfOutFile * pof,
    KClientHttpRequest * req, uint64_t size, progressbar * pb, rc_t * rwr,
    rc_t * rw)
{
    int i = 0;

    rc_t rc = 0, r2 = 0;
    KClientHttpResult * rslt = NULL;
    KStream * s = NULL;

    assert(self && rw && rwr &&pof && pof->cache);

    for (i = 0; i < 3; ++i) {
        rc = KClientHttpRequestGET(req, &rslt);
        DISP_RC2(rc, "Cannot KClientHttpRequestGET", pof->cache->addr);

        if (rc == 0) {
            rc = KClientHttpResultGetInputStream(rslt, &s);
            DISP_RC2(rc, "Cannot KClientHttpResultGetInputStream",
                pof->cache->addr);
        }

        if (rc != 0)
            break;
        else if (s != NULL)
            break;
        else /* retry 3 times to call GET if returned KStream is NULL */
            RELEASE(KClientHttpResult, rslt);
    }

    if (s == NULL) {
        *rw = RC(rcExe, rcFile, rcCopying, rcTransfer, rcNull);
        return rc;
    }

    for (*rw = 0; *rw == 0 && rc == 0; ) {
        size_t num_read = 0, num_writ = 0;

        rc = Quitting();
        if (rc != 0)
            break;

        *rw = KStreamRead(s, self->buffer, self->bsize, &num_read);
#ifdef TESTING_FAILURES
        if (pof->pos > 0 && *rw == 0) *rw = 1;
#endif
        if (*rw != 0 || num_read == 0) {
            if (pof->pos > 0) {
                if (KStsLevelGet() > 0)
                    DISP_RC2(*rw, "Cannot KStreamRead: "
                        "switching to KFileRead...", pof->cache->addr);
            }
            else
                DISP_RC2(*rw, "Cannot KStreamRead", pof->cache->addr);
            break;
        }

        if (self->dryRun)
            break;

        *rwr = KFileWriteAll(
            pof->file, pof->pos, self->buffer, num_read, &num_writ);
        DISP_RC2(*rwr, "Cannot KFileWrite", pof->tmpName);
        if (*rwr == 0 && num_writ != num_read)
            *rwr = RC(rcExe, rcFile, rcCopying, rcTransfer, rcIncomplete);
        if (*rwr != 0 && rc == 0)
            rc = *rwr;

        if (rc == 0) {
            pof->pos += num_writ;
            if (pb != NULL)
                update_progressbar(pb, 100 * 100 * pof->pos / size);
            r2 = PrfOutFileCommitTry(pof);
            if (rc == 0 && r2 != 0 && pof->_fatal)
                rc = r2;
        }
    }

    RELEASE(KClientHttpResult, rslt);
    RELEASE(KStream, s);

    return rc;
}

static rc_t PrfMainDownloadFile(const PrfMain * self, PrfOutFile * pof,
    const KFile * in, uint64_t size, progressbar * pb, rc_t * rwr,
    PrfRetrier * retrier)
{
    rc_t rc = 0, r2 = 0;
#ifdef TESTING_FAILURES
    bool already = false;
    rc_t testRc = 1;
#endif

    assert(self && retrier && rwr);

    while (rc == 0) {
        size_t num_read = 0, num_writ = 0;

        rc = Quitting();
        if (rc != 0)
            break;

        rc = KFileRead(
            in, pof->pos, self->buffer, retrier->curSize, &num_read);
#ifdef TESTING_FAILURES
        if (!already&&rc == 0)rc = testRc; else already = true;
#endif
        if (rc != 0) {
            rc = PrfRetrierAgain(retrier, rc, pof->pos != 0);
            if (rc != 0)
                break;
            else
                continue;
        }
        else if (num_read == 0)
            break;

        *rwr = KFileWriteAll(
            pof->file, pof->pos, self->buffer, num_read, &num_writ);
        DISP_RC2(*rwr, "Cannot KFileWrite", pof->tmpName);
        if (*rwr == 0 && num_writ != num_read)
            rc = RC(rcExe, rcFile, rcCopying, rcTransfer, rcIncomplete);
        if (*rwr != 0 && rc == 0)
            rc = *rwr;

        if (rc == 0) {
            pof->pos += num_writ;
            PrfRetrierReset(retrier, pof->pos);
            if (pb != NULL)
                update_progressbar(pb, 100 * 100 * pof->pos / size);
            r2 = PrfOutFileCommitTry(pof);
            if (rc == 0 && r2 != 0 && pof->_fatal)
                rc = r2;
        }
    }

    return rc;
}

static rc_t PrfMainDownloadHttpFile(Resolved *self,
    PrfMain *mane, const VPath * path, PrfOutFile * pof)
{
    rc_t rc = 0, rw = 0, r2 = 0, rwr = 0;
    const KFile *in = NULL;
    uint64_t size = 0;

    progressbar * pb = NULL;

    const VPathStr * remote = NULL;

    KStsLevel lvl = STS_INFO;

    char spath[PATH_MAX] = "";
    size_t len = 0;

    String src;
    memset(& src, 0, sizeof src);

    assert(self && mane && pof);

    assert(!mane->eliminateQuals);

    if (mane->dryRun)
        lvl = STAT_USR;

    rc = VPathReadUri(path, spath, sizeof spath, &len);
    if (rc != 0) {
        DISP_RC(rc, "VPathReadUri(PrfMainDownloadHttpFile)");
        return rc;
    }
    else
        StringInit(&src, spath, len, (uint32_t)len);

    remote = self -> remoteHttp . path != NULL ? & self -> remoteHttp
                                               : & self -> remoteHttps;
    assert(remote);

    if (rc == 0 && !mane->dryRun)
        rc = PrfOutFileOpen(pof, mane->force == eForceALL);

    assert ( src . addr );

    if (rc == 0 && !mane->dryRun && mane->stripQuals) {
        if (in == NULL) {
            rc = _KFileOpenRemote(&in, mane->kns, path, & src, !self->isUri);
            if (rc != 0 && !self->isUri)
                PLOGERR(klogInt, (klogInt, rc, "failed to open file "
                    "'$(path)'", "path=%S", & src));
        }

        if (mane->stripQuals) {
            const KFile * kfile = NULL;

            rc = KSraFileNoQuals(in, &kfile);
            if (rc == 0) {
                KFileRelease(in);
                in = kfile;
            }
        }
    }
    
    if (rc == 0)
        STSMSG(lvl, ("%S -> %s", &src, pof->tmpName));
    else
        rwr = rc;

    if (rc == 0 && mane->showProgress && !mane->dryRun) {
        r2 = 0;
        if (in == NULL)
            r2 = _KFileOpenRemote(&in, mane->kns, path,
                &src, !self->isUri);
        if (r2 == 0)
            rc = KFileSize(in, &size);
        if (r2 == 0)
            rc = make_progressbar(&pb, 2);
    }

    if (rc == 0 && !PrfOutFileIsLoaded(pof)) {
        bool reliable = ! self -> isUri;
        ver_t http_vers = 0x01010000;
        KClientHttpRequest * kns_req = NULL;

        bool ceRequired = false;
        bool payRequired = false;
        const String * ce_token = NULL;

        VPathGetCeRequired(path, &ceRequired);
        VPathGetPayRequired(path, &payRequired);

        if (ceRequired) {
            CloudMgr * m = NULL;
            Cloud * cloud = NULL;
            rc_t rc = CloudMgrMake(&m, NULL, NULL);
            if (rc == 0)
                rc = CloudMgrGetCurrentCloud(m, &cloud);
            if (rc == 0)
                CloudMakeComputeEnvironmentToken(cloud, &ce_token);
            RELEASE(Cloud, cloud);
            RELEASE(CloudMgr, m);
        }

        if (reliable)
            if (ceRequired && ce_token != NULL)
                rc = KNSManagerMakeReliableClientRequest(mane->kns,
                    &kns_req, http_vers, NULL, "%S&ident=%S", &src, ce_token);
            else
                rc = KNSManagerMakeReliableClientRequest(mane->kns,
                    &kns_req, http_vers, NULL, "%S", &src);
        else
            if (ceRequired && ce_token != NULL)
                rc = KNSManagerMakeClientRequest(mane->kns,
                    &kns_req, http_vers, NULL, "%S&ident=%S", &src, ce_token);
            else
                rc = KNSManagerMakeClientRequest ( mane -> kns,
                    & kns_req, http_vers, NULL, "%S", & src );
        DISP_RC2 ( rc, "Cannot KNSManagerMakeClientRequest", src . addr );

        RELEASE(String, ce_token);

        if (rc == 0) {
            if (payRequired)
                KHttpRequestSetCloudParams(kns_req, ceRequired, payRequired);

            rc = PrfMainDownloadStream(mane, pof, kns_req, size, pb, &rwr, &rw);
        }

        RELEASE ( KClientHttpRequest, kns_req );
    }

    if (rc == 0 && (rw != 0 || PrfOutFileIsLoaded (pof))
       /* && pof->pos > 0 :
       sometimes KClientHttpResultGetInputStream() returns NULL
       and streaming fails: try KFile anyway */
        && Quitting() == 0)
    {
#ifdef TESTING_FAILURES
        bool already = false;
        rc_t testRc = 1;
#endif
        PrfRetrier retrier;
        if (in == NULL)
            rc = _KFileOpenRemote(&in, mane->kns, path,
                &src, !self->isUri);
        PrfRetrierInit(&retrier, mane, path,
            &src, self->isUri, &in, size, pof->pos);
        rc = PrfMainDownloadFile(mane, pof, in, size, pb, &rwr, &retrier);
    }

    if (!mane->dryRun) {
        if (rwr == 0)
            PrfOutFileCommitDo(pof);

        r2 = PrfOutFileClose(pof);
        if (r2 != 0 && rc == 0)
            rc = r2;
    }

    destroy_progressbar(pb);

    if (rc == 0 && !mane->dryRun)
        STSMSG(STS_INFO, ("%s (%ld)", pof->tmpName, pof->pos));

    RELEASE(KFile, in);

    if ( rc == 0 && rw != 0 )
        rc = rw;

    return rc;
}

static rc_t PrfMainDownloadCacheFile(Resolved *self,
                        PrfMain *mane, const char *to, bool elimQuals)
{
    rc_t rc = 0;
    const KFile *out = NULL;

    const VPathStr * remote = NULL;

    assert(self && mane);
    assert(!mane->stripQuals);

    remote = self -> remoteHttp . path != NULL ? & self -> remoteHttp
                                               : & self -> remoteHttps;

    assert(remote -> str);

    if (self->file == ((void*)0)) {
        rc = _KFileOpenRemote(&self->file, mane->kns,
            remote->path, remote -> str, !self->isUri);
        if (rc != 0) {
            PLOGERR(klogInt, (klogInt, rc, "failed to open file for $(path)",
                              "path=%S", remote -> str));
            return rc;
        }
    }
    
    rc = KDirectoryMakeCacheTee(mane->dir, &out, self->file, 0, "%s", to);
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc, "failed to open cache file for $(path)",
                          "path=%S", to));
        return rc;
    }
    
    STSMSG(STS_INFO, ("%S -> %s", remote -> str, to));

    rc = KSraReadCacheFile( out, elimQuals );
    if (rc != 0) {
        PLOGERR(klogInt, (klogInt, rc, "failed to read cache file at $(path)",
                          "path=%S", to));
    }
  
    RELEASE(KFile, out);
    
    if (rc != 0)
        return rc;
    
    if (rc == 0)
        STSMSG(STS_INFO, ("%s", to));
    
    return rc;
}

/*  https://sra-download.ncbi.nlm.nih.gov/srapub/SRR125365.sra
anonftp@ftp-private.ncbi.nlm.nih.gov:/sra/sra-instant/reads/ByR.../SRR125365.sra
*/
static rc_t PrfMainDownloadAscp(const Resolved *self, PrfMain *mane,
    const char *to, const VPath * path)
{
    rc_t rc = 0;

    const char *src = NULL;
    AscpOptions opt;

    char spath[PATH_MAX] = "";
    size_t len = 0;

    assert ( self && mane );

    if ( self -> isUri && ! ( mane -> ascp && mane -> asperaKey ) ) {
        rc_t rc = RC ( rcExe, rcFile, rcCopying, rcFile, rcNotFound );
        LOGERR ( klogErr, rc,
                 "cannot run aspera download: ascp or key file is not found" );
        return rc;
    }

    assert(mane->ascp && mane->asperaKey);

    memset(&opt, 0, sizeof opt);

    rc = VPathReadUri(path, spath, sizeof spath, &len);
    if (rc != 0) {
        DISP_RC(rc, "VPathReadUri(PrfMainDownloadAscp)");
        return rc;
    }
    else {
        String str;
        char *query = string_chr(spath, len, '?');
        if (query != NULL) {
            *query = '\0';
            len = query - spath;
        }
        StringInit(&str, spath, len, (uint32_t)len);
        if (!_StringIsFasp(&str, &src))
            return RC(rcExe, rcFile, rcCopying, rcSchema, rcInvalid);
    }

    if (mane->ascpParams != NULL) {
        opt.ascp_options = mane->ascpParams;
    }
    else if (mane->ascpMaxRate != NULL) {
        size_t sz = string_copy(opt.target_rate, sizeof opt.target_rate,
            mane->ascpMaxRate->addr, mane->ascpMaxRate->size);
        if (sz < sizeof opt.target_rate) {
            return RC(rcExe, rcFile, rcCopying, rcBuffer, rcInsufficient);
        }
    }

    opt.name = self->name;
    opt.src_size = self->remoteSz;
    opt.heartbeat = mane->heartbeat;
    opt.quitting = Quitting;
    opt.dryRun = mane->dryRun;

    return aspera_get(mane->ascp, mane->asperaKey, src, to, &opt);
}

typedef enum {
    eVinit,
    eVyes,
    eVno,
    eVskipped,
} EValidate;

static rc_t POFValidate(PrfOutFile * self,
    const VPath * remote, const VPath * cache, bool checkMd5,
    EValidate * vSz, EValidate * vMd5, bool * encrypted)
{
    rc_t rc = 0, rd = 0;

    const KFile * f = NULL;
    const KFile ** fd = &f;
    char buf[10240];
    size_t nr = 0;

    uint64_t s = VPathGetSize(remote);
    const uint8_t * md5 = VPathGetMd5(remote);

    assert(self && vSz && vMd5 && encrypted);
    *vSz = *vMd5 = eVskipped;
    *encrypted = false;

    assert(self->cache);

    {
        KStsLevel lvl = STS_DBG;
        if (self->pos > 20 * 1024 * 1024 * 1024L)
            lvl = STS_TOP;
        else if (self->pos > 1024 * 1024 * 1024L)
            lvl = STS_INFO;
        else
            lvl = STS_DBG;
        if (self->_vdbcache)
            STSMSG(lvl, ("  verifying '%s.vdbcache'...", self->_name));
        else
            STSMSG(lvl, ("  verifying '%s'...", self->_name));
    }

    rd = KDirectoryOpenFileRead(
        self->_dir, &f, "%.*s", self->cache->size, self->cache->addr);
    if (rd == 0 && s > 0) {
        uint64_t size = 0;
        rc = KFileSize(f, &size);
        if (rc == 0 && size > 7) {
            rc = KFileRead(f, 0, buf, 8, &nr);
#define MAGIC "NCBInenc"
            if (rc == 0 &&
                string_cmp(buf, sizeof MAGIC - 1, MAGIC, sizeof MAGIC - 1,
                    sizeof MAGIC - 1) == 0)
            {
                *encrypted = true;
            }
        }
        if (rc == 0 && *encrypted) {
            VFSManager * mgr = NULL;
            rc = VFSManagerMake(&mgr);
            if (rc == 0) {
                rc = VFSManagerOpenFileReadDecrypt(mgr, fd, cache);
      /* TODO
         1) if rc == 1948584216
                (path not found while opening node within configuration module)
               then
                if (--verify yes) then log validate error "cannot get password".
         2) make --verify 3-state: yes, no, default
         3) add test in asm-trace/test/prefetch:
         3.1) prefetch kart-withGaP, no pwd -> skip validation (no pwd found)
         3.2) prefetch kart-withGaP --verify yes, no pwd -> fail (no pwd found)
         3.3) prefetch kart-withGaP, with pwd -> ok
         3.4) prefetch kart-withGaP --verify yes, with pwd -> ok */
            }
            RELEASE(VFSManager, mgr);
            assert(fd);
            if (rc == 0)
                rc = KFileSize(*fd, &size);
        }
        if (rc == 0) {
            if (size == s)
                *vSz = eVyes;
            else {
                *vSz = eVno;
                self->invalid = true;
            }
            if (size > 0x20000000 && *encrypted) /* don't check md5 for large */
                checkMd5 = false;       /*  encrypted files: it takes forever */
        }
    }

    if (rd == 0 && md5 != NULL && checkMd5) {
        const KFile * f2 = NULL;
        rc_t r2 = 0;
        assert(fd);
        r2 = KFileMakeMD5Read(&f2, *fd, md5);
        if (r2 == 0) {
            r2 = KFileRead(f2, ~0, buf, sizeof buf, &nr);
            if (r2 != 0) {
                *vMd5 = eVno;
                self->invalid = true;
            }
            else
                *vMd5 = eVyes;
        }
        RELEASE(KFile, f2);
        *fd = NULL; /* MD5FileRelease releases underlying KFile */
        if (rc == 0 && r2 != 0)
            rc = r2;
    }

    RELEASE(KFile, f);
    if (rc == 0 && rd != 0)
        rc = rd;

    return rc;
}

static rc_t PrfMainDoDownload(Resolved *self, const Item * item,
    bool isDependency, const VPath * path, PrfOutFile * pof)
{
    bool canceled = false;
    rc_t rc = 0;
    PrfMain * mane = NULL;
    assert(item);
    mane = item->mane;
    assert(mane && pof);
    {
        char spath[PATH_MAX] = "";
        KStsLevel lvl = STS_DBG;
        rc_t r = VPathReadUri(path, spath, sizeof spath, NULL);
        if (mane->dryRun)
            lvl = STAT_USR;
        if (r != 0)
            STSMSG(lvl, ("########## VPathReadUri(remote)=%R)", r));
        else {
            uint64_t s = VPathGetSize(path);
            char * query = strstr(spath, "tic=");
            if (query != NULL) {
                if (*(query - 1) == '?' || *(query - 1) == '#')
                    --query;
                *query = '\0';
            }
            STSMSG(lvl, ("########## remote(%s:%,ld)", spath, s));
        }
    }
    if (rc == 0) {
        String scheme;
        rc = VPathGetScheme(path, &scheme);
        if (rc == 0) {
          rc_t rd = 0;
          bool ascp = _SchemeIsFasp(&scheme);
          if (!mane->noAscp) {
            if (ascp) {
                STSMSG(STS_TOP, (" Downloading via fasp..."));
                if (mane->forceAscpFail)
                    rc = 1;
                else if (mane->eliminateQuals) {
                    LOGMSG(klogErr, "Cannot eliminate qualities "
                        "during fasp download");
                    rc = 1;
                }
                else if (mane->eliminateQuals) {
                    LOGMSG(klogErr, "Cannot remove QUALITY columns "
                        "during FASP download");
                    rc = 1;
                }
                else
                    rd = PrfMainDownloadAscp(self, mane, pof->tmpName, path);
                if (rd == 0)
                    STSMSG(STS_TOP, (" FASP download succeed"));
                else {
                    rc_t rc = Quitting();
                    if (rc != 0)
                        canceled = true;
                    else
                        STSMSG(STS_TOP, (" FASP download failed"));
                }
            }
          }
          if (!ascp && /*(rc != 0 && GetRCObject(rc) != rcMemory&&*/
            !canceled && !mane->noHttp) /*&& !self->isUri))*/
          {
            bool https = true;
            if (scheme.size == 4)
                https = false;
            STSMSG(STS_TOP,
                (" Downloading via %s...", https ? "HTTPS" : "HTTP"));
            if (mane->eliminateQuals)
                rd = PrfMainDownloadCacheFile(self, mane,
                    pof->tmpName, mane->eliminateQuals && !isDependency);
            else
                rd = PrfMainDownloadHttpFile(self, mane, path, pof);
            if (rd == 0) {
                STSMSG(STS_TOP, (" %s download succeed",
                    https ? "HTTPS" : "HTTP"));
            }
            else {
                rc_t rc = Quitting();
                if (rc != 0)
                    canceled = true;
                else
                    STSMSG(STS_TOP, (" %s download failed",
                        https ? "HTTPS" : "HTTP"));
            }
          }
          if ( rc == 0 && rd != 0 )
            rc = rd;
        }
    }
    return rc;
}

static rc_t PrfMainDownload(Resolved *self, const Item * item,
                         bool isDependency, const VPath *vdbcache)
{
    rc_t rc = 0, r2 = 0, rv = 0;
    KFile *flock = NULL;
    PrfMain * mane = NULL;

    char lock[PATH_MAX] = "";

    const VPath * vcache = NULL;
    const VPath * vremote = NULL;

    const char * name = NULL;

    PrfOutFile pof;

    String cache;
    memset( & cache, 0, sizeof cache );

    assert(self && item);

    name = self->name;
    if (self->respFile != NULL) {
        const char * acc = NULL;
        rc_t r2 = KSrvRespFileGetName(self->respFile, &acc);
        if (r2 == 0 && acc != NULL)
            name = acc;
    }

    mane = item -> mane;
    assert ( mane );

    rc = PrfOutFileInit(&pof, mane->resume, name, vdbcache != NULL);
    if (rc != 0)
        return rc;

    if (rc == 0) {
        if (self->respFile != NULL) {
            rc = KSrvRespFileGetCache(self->respFile, &vcache);
            if (rc == 0) {
                if (vdbcache != NULL) {
                    VPath * clocal = NULL;
                    rc = VFSManagerMakePathWithExtension(
                        mane->vfsMgr, &clocal, vcache, ".vdbcache");
                    if (rc == 0) {
                        RELEASE(VPath, vcache);
                        vcache = clocal;
                    }

                }
                if (rc == 0) {
                    rc = VPathGetPath(vcache, &cache);
                    if (rc != 0) {
                        DISP_RC(rc, "VPathGetPath(PrfMainDownload)");
                        return rc;
                    }
                }
            }
        }
        else {
            assert(self->cache);
            cache = *self->cache;
        }

        assert(cache.size && cache.addr);

        {
            KStsLevel lvl = STS_DBG;
            if (mane->dryRun)
                lvl = STAT_USR;
            STSMSG(lvl, ("########## cache(%S)", &cache));
        }

        if (mane->force != eForceAll && mane->force != eForceALL &&
            PrfMainHasDownloaded(mane, cache.addr))
        {
            STSMSG(STS_INFO, ("%s has just been downloaded", cache.addr));
            return 0;
        }
    }

    if (rc == 0)
        rc = _KDirectoryMkLockName(mane->dir, & cache, lock, sizeof lock);

    if (rc == 0) {
        rc = PrfOutFileMkName(&pof, &cache);
        if (rc != 0)
            return rc;
    }

    if (KDirectoryPathType(mane->dir, "%s", lock) != kptNotFound) {
        if (mane->force != eForceAll && mane->force != eForceALL) {
            KTime_t date = 0;
            rc = KDirectoryDate(mane->dir, &date, "%s", lock);
            if (rc == 0) {
                time_t t = time(NULL) - date;
                if (t < 60 * 60 * 24) { /* 24 hours */
                    STSMSG(STS_DBG, ("%s found: canceling download", lock));
                    rc = RC(rcExe, rcFile, rcCopying, rcLock, rcExists);
                    PLOGERR(klogWarn, (klogWarn, rc,
                        "Lock file $(file) exists: download canceled",
                        "file=%s", lock));
                    return rc;
                }
                else {
                    STSMSG(STS_DBG, ("%s found and ignored as too old", lock));
                    rc = _KDirectoryClean(mane->dir, pof.cache, lock);
                }
            }
            else {
                STSMSG(STS_DBG, ("%s found", lock));
                DISP_RC2(rc, "KDirectoryDate", lock);
            }
        }
        else {
            STSMSG(STS_DBG, ("%s found and forced to be ignored", lock));
            rc = _KDirectoryClean(mane->dir, pof.cache, lock);
        }
    }
    else {
        STSMSG(STS_DBG, ("%s not found", lock));
    }

    if (rc == 0) {
        STSMSG(STS_DBG, ("creating %s", lock));
        rc = KDirectoryCreateFile(mane->dir, &flock,
            false, 0664, kcmInit | kcmParents, "%s", lock);
        DISP_RC2(rc, "Cannot CreateFile", lock);
    }

    assert(!mane->noAscp || !mane->noHttp);

    if (self->respFile != NULL) {
        rc_t rd = 0;
        KSrvRespFileIterator * fi = NULL;
        rc = KSrvRespFileMakeIterator(self->respFile, &fi);
        while (rc == 0) {
            if (vdbcache == NULL) {
                rc = KSrvRespFileIteratorNextPath(fi, &vremote);
                if (rc == 0) {
                    if (vremote == NULL) {
                        rc = rd;
                        break;
                    }
                }
                rd = PrfMainDoDownload(self, item, isDependency, vremote, &pof);
            }
            else {
                rc = VPathAddRef(vdbcache);
                if (rc != 0)
                    break;
                else {
                    vremote = vdbcache;
                    rd = PrfMainDoDownload(self, item, isDependency, vdbcache,
                        &pof);
                }
            }
            if (rd == 0 && vdbcache == NULL) {
                const VPath * vdbcache = NULL;
                rc_t rc = VPathGetVdbcache(vremote, & vdbcache, NULL);
                if (rc == 0 && vdbcache != NULL) {
                    STSMSG(STS_TOP, ("%d.2) Downloading '%s.vdbcache'...",
                        item->number, name));
                    if (PrfMainDownload(self, item, isDependency, vdbcache)
                        == 0)
                    {
                        STSMSG(STS_TOP, (
                            "%d.2) '%s.vdbcache' was downloaded successfully",
                            item->number, name));
                    }
                    else
                        STSMSG(STS_TOP, ("%d) failed to download %s.vdbcache",
                            item->number, name));
                    RELEASE(VPath, vdbcache);
                }
            }
            if (rd == 0)
                break;
            RELEASE ( VPath, vremote );
        }
        RELEASE(KSrvRespFileIterator, fi);
    }
    else {
        do {
            if (self->remoteFasp.path != NULL) {
                rc = PrfMainDoDownload(self, item,
                    isDependency, self->remoteFasp.path, &pof);
                if (rc == 0)
                    break;
            }
            if (self->remoteHttp.path != NULL) {
                rc = PrfMainDoDownload(self, item,
                    isDependency, self->remoteHttp.path, &pof);
                if (rc == 0)
                    break;
            }
            if (self->remoteHttps.path != NULL) {
                rc = PrfMainDoDownload(self, item,
                    isDependency, self->remoteHttps.path, &pof);
                if (rc == 0)
                    break;
            }
        } while (false);
    }

    RELEASE(KFile, flock);
    
    if (rc == 0) {
        KStsLevel lvl = STS_DBG;
        if (mane->dryRun)
            lvl = STAT_USR;
        STSMSG(lvl, ("renaming %s -> %S", pof.tmpName, & cache));
        if (!mane->dryRun) {
            rc = KDirectoryRename(mane->dir, true, pof.tmpName, cache.addr);
            if (rc != 0)
                PLOGERR(klogInt, (klogInt, rc, "cannot rename $(from) to $(to)",
                    "from=%s,to=%S", pof.tmpName, & cache));
        }
    }

    if (rc == 0 && !mane->dryRun) {
        EValidate size = eVinit;
        EValidate md5 = eVinit;
        bool encrypted = false;
        rv = POFValidate(
            &pof, vremote, vcache, mane->validate, &size, &md5, &encrypted);
        if (rv != 0)
            LOGERR(klogInt, rc, "failed to verify");
        else {
            if (size == eVyes && md5 == eVyes)
                STSMSG(STS_TOP, (" '%s%s' is valid",
                    name, vdbcache == NULL ? "" : ".vdbcache"));
            else if (size == eVyes && encrypted)
                STSMSG(STS_TOP, (" size of '%s%s' is correct",
                    name, vdbcache == NULL ? "" : ".vdbcache"));
            else {
                if (size == eVno) {
                    STSMSG(STS_TOP, (" '%s%s': size does not match",
                        name, vdbcache == NULL ? "" : ".vdbcache"));
                    rv = RC(rcExe, rcFile, rcValidating, rcSize, rcUnequal);
                }
                if (md5 == eVno) {
                    STSMSG(STS_TOP, (" '%s%s': md5 does not match",
                        name, vdbcache == NULL ? "" : ".vdbcache"));
                    rv = RC(rcExe, rcFile, rcValidating, rcChecksum, rcUnequal);
                }
            }
        }
    }

    if (rc == 0 && rv == 0)
        rc = PrfMainDownloaded(mane, cache.addr);

    if (rc == 0) {
        r2 = _KDirectoryCleanCache(mane->dir, & cache);
        if (rc == 0 && r2 != 0)
            rc = r2;
    }

    r2 = _KDirectoryClean(mane->dir, &cache, lock);
    if (rc == 0 && r2 != 0)
        rc = r2;

    r2 = PrfOutFileWhack(&pof, rc == 0);
    if (rc == 0 && r2 != 0)
        rc = r2;

    if (rc == 0 && rv != 0)
        rc = rv;

    RELEASE(VPath, vcache);
    RELEASE(VPath, vremote);

    return rc;
}

/********** Item **********/
static rc_t ItemRelease(Item *self) {
    rc_t rc = 0;

    if (self == NULL)
        return 0;

    rc = ResolvedFini(&self->resolved);
    RELEASE(KartItem, self->item);

    free ( self -> seq_id );

    memset(self, 0, sizeof *self);

    free(self);

    return rc;
}

static rc_t ItemInit(Item *self, const char *obj) {
    assert(self);
    self->desc = obj;
    return 0;
}

static char* ItemName(const Item *self) {
    char *c = NULL;
    assert(self);
    if (self->desc != NULL)
        return string_dup_measure(self->desc, NULL);
    else if (self->jwtCart != NULL)
        return string_dup_measure(self->jwtCart, NULL);
    else {
        rc_t rc = 0;
        const String *elem = NULL;
        assert(self->item);
        /*
        rc = KartItemItemDesc(self->item, &elem);
        c = StringCheck(elem, rc);
        if (c != NULL) {
            return c;
        }
*/
        rc = KartItemAccession(self->item, &elem);
        c = StringCheck(elem, rc);
        if (c != NULL) {
            return c;
        }

        rc = KartItemItemId(self->item, &elem);
        return StringCheck(elem, rc);
    }
}

static rc_t ItemSetDependency(Item *self,
    const VDBDependencies *deps, uint32_t idx)
{
    Resolved * resolved = NULL;
    const VPath * cache = NULL;
    const VPath * remote = NULL;
    rc_t cacheRc = 0;
    rc_t remoteRc = 0;
    rc_t rc = VDBDependenciesRemoteAndCache(deps, idx,
        &remoteRc, &remote, &cacheRc, &cache);
    assert(self);
    resolved = &self->resolved;
    if (rc == 0) {
        if (remoteRc != 0) {
            rc = remoteRc;
            PLOGERR(klogInt, (klogInt, rc, "cannot get remote "
                "location for '$(id)'", "id=%s", self->seq_id));
        }
        else if (cacheRc != 0) {
            rc = cacheRc;
            PLOGERR(klogInt, (klogInt, rc, "cannot get cache "
                "location for '$(id)'", "id=%s", self->seq_id));
        }
        else {
            VPathStr * v = NULL;
            String fasp;
            String http;
            String https;
            String scheme;
            CONST_STRING(&fasp, "fasp");
            CONST_STRING(&http, "http");
            CONST_STRING(&https, "https");
            memset(&scheme, 0, sizeof scheme);
            rc = VPathGetScheme(remote, &scheme);
            if (rc == 0) {
                if (StringEqual(&scheme, &https))
                    v = &resolved->remoteHttps;
                else if (StringEqual(&scheme, &fasp)) {
                    v = &resolved->remoteFasp;
                }
                else if (StringEqual(&scheme, &http))
                    v = &resolved->remoteHttp;
                assert(v && v->path == NULL);
                v->path = remote;
            }
            if (rc == 0) {
                char path[PATH_MAX] = "";
                size_t len = 0;
                rc = VPathReadUri(v->path,
                    path, sizeof path, &len);
                DISP_RC2(rc, "VPathReadUri(VResolverRemote)", resolved->name);
                if (rc == 0) {
                    String local_str;
                    StringInit(&local_str, path, len, (uint32_t)len);
                    assert(!v->str);
                    rc = StringCopy(&v->str, &local_str);
                    DISP_RC2(rc, "StringCopy(VResolverRemote)", resolved->name);
                }
            }
            if (rc == 0) {
                String path_str;
                rc = VPathGetPath(cache, &path_str);
                DISP_RC2(rc, "VPathGetPath(VResolverCache)", resolved->name);

                if (rc == 0) {
                    assert(!resolved->cache);
                    rc = StringCopy(&resolved->cache, &path_str);
                    DISP_RC2(rc, "StringCopy(VResolverCache)", resolved->name);
                }
            }
        }
    }
    return rc;
}

static rc_t _KartItemToVPath(const KartItem *self,
    const VFSManager *vfs, VPath **path)
{
    uint64_t oid = 0;
    rc_t rc = KartItemItemIdNumber(self, &oid);
    if (rc == 0) {
        rc = VFSManagerMakeOidPath(vfs, path, (uint32_t)oid);
    }
    else {
        char path_str[PATH_MAX] = "";
        const String *accession = NULL;
        rc = KartItemAccession(self, &accession);
        if (rc == 0) {
            rc =
                string_printf(path_str, sizeof path_str, NULL, "%S", accession);
        }
        if (rc == 0) {
            rc = VFSManagerMakePath(vfs, path, path_str);
        }
    }
    return rc;
}

static rc_t _ItemSetResolverAndAccessionInResolved(Item *item,
    VResolver *resolver, const KRepositoryMgr *repoMgr,
    const VFSManager *vfs)
{
    Resolved *resolved = NULL;
    rc_t rc = 0;
    const KRepository *p_protected = NULL;

    assert(item && resolver && repoMgr && vfs);

    resolved = &item->resolved;

    if (item->desc != NULL) {
        rc = VFSManagerMakePath(vfs, &resolved->accession, "%s", item->desc);
        DISP_RC2(rc, "VFSManagerMakePath", item->desc);
        if (rc == 0)
            rc = VResolverAddRef(resolver);
        if (rc == 0) {
            resolved->resolver = resolver;
            resolved->isUri = item->isDependency
                ? false : VPathFromUri (resolved->accession);
            /* resolved->isUri is set to true just when
                resolved->accession is a full URI to download */
        }
        if (rc == 0) {
            if (resolved->isUri) {
                VPathStr * remote = NULL;
                String fasp;
                String http;
                String https;
                String scheme;
                CONST_STRING ( & fasp , "fasp"  );
                CONST_STRING ( & http , "http"  );
                CONST_STRING ( & https, "https" );
                memset ( & scheme, 0, sizeof scheme );
                rc = VPathGetScheme ( resolved -> accession, & scheme );
                if ( StringEqual ( & scheme, & http ) )
                    remote = & resolved -> remoteHttp;
                else if ( StringEqual ( & scheme, & https ) )
                    remote = & resolved -> remoteHttps ;
                else if ( StringEqual ( & scheme, & fasp ) )
                    remote = & resolved -> remoteFasp ;
                if ( remote != NULL ) {
                    char path[PATH_MAX] = "";
                    size_t len = 0;
                    remote -> path = resolved -> accession;
                    rc = VPathReadUri(resolved->accession,
                                      path, sizeof path, &len);
                    DISP_RC2(rc, "VPathReadUri(VResolverRemote)",
                                 resolved->name);
                    if (rc == 0) {
                        String local_str;
                        char *query = string_chr(path, len, '?');
                        if (query != NULL) {
                            *query = '\0';
                            len = query - path;
                        }
                        StringInit(&local_str, path, len, (uint32_t)len);
                        rc = StringCopy(& remote -> str, &local_str);
                        DISP_RC2(rc, "StringCopy(VResolverRemote)",
                                     resolved->name);
                    }
                }
                resolved->accession = NULL;
                if ( rc == 0 ) {
                    if ( remote != NULL && remote -> str->size > 0 &&
                         remote -> str->addr[remote -> str->size-1]
                            == '/' )
                    {
                        rc = VFSManagerMakePath ( vfs, &resolved->accession,
                                                       "ncbi-file:index.html" );
                        DISP_RC2(rc, "VFSManagerMakePath", "index.html");
                    }
                    else {
                        rc = VFSManagerExtractAccessionOrOID
                            (vfs, &resolved->accession, remote -> path);
                        if ( rc != 0 ) {
                            const char * start = remote -> str->addr;
                            size_t size = remote -> str->size;
                            const char * end = start + size;
                            const char * slash
                                = string_rchr ( start, size, '/' );
                            const char * scol = NULL;
                            String scheme;
                            String fasp;
                            CONST_STRING ( & fasp, "fasp" );
                            rc = VPathGetScheme ( remote -> path, & scheme );
                            if ( rc == 0 ) {
                                if ( StringEqual ( & scheme, & fasp ) )
                                    scol = string_rchr ( start, size, ':' );
                                if ( slash != NULL )
                                    start = slash + 1;
                                if ( scol != NULL && scol > start )
                                    start = scol + 1;
                                rc = VFSManagerMakePath ( vfs,
                                    &resolved->accession, "%.*s",
                                    ( uint32_t ) ( end - start ), start );
                            }
                        }
                        DISP_RC2(rc, "ExtractAccession", remote -> str->addr);
                    }
                }
            }
            else { /* to investigate for dbGaP project 0 */
                uint32_t projectId = 0;
                rc_t r = KRepositoryMgrCurrentProtectedRepository(repoMgr,
                                                                  &p_protected);
                if (r == 0)
                    r = KRepositoryProjectId(p_protected, &projectId);
                if (r == 0)
                    resolved->project = projectId;
                RELEASE (KRepository, p_protected);
            }
        }
    }
    else if (item->jwtCart != NULL);
    else {
        rc = KartItemProjIdNumber(item->item, &resolved->project);
        if (rc != 0) {
            DISP_RC(rc, "KartItemProjIdNumber");
            return rc;
        }
        /* no support for kart files with items within public project
           (project 0 is protected '1000 genomes' project) */
        resolved->dbgapProject = true;

        rc = _KartItemToVPath(item->item, vfs, &resolved->accession);
        if (rc != 0) {
            DISP_RC(rc, "invalid kart file row");
            return rc;
        }
        else {
                rc = VResolverAddRef(resolver);
                if (rc == 0)
                    resolved->resolver = resolver;
        }
    }

    return rc;
}

/* resolve locations */
static rc_t _ItemResolveResolved(VResolver *resolver,
    VRemoteProtocols protocols, Item *item, const KRepositoryMgr *repoMgr,
    const VFSManager *vfs, KNSManager *kns,
    size_t minSize, size_t maxSize)
{
    Resolved *resolved = NULL;
    rc_t rc = 0;
    rc_t rc2 = 0;

    const VPathStr * remote = NULL;

    uint32_t i;
    bool has_proto [ eProtocolMask + 1 ];
    memset ( has_proto, 0, sizeof has_proto );
    for ( i = 0; i < eProtocolMaxPref; ++ i )
        has_proto [ ( protocols >> ( i * 3 ) ) & eProtocolMask ] = true;

    assert(resolver && item);

    resolved = &item->resolved;

    VPathStrFini(&resolved->local);

    VPathStrFini(&resolved->remoteFasp);
    VPathStrFini(&resolved->remoteHttp);
    VPathStrFini(&resolved->remoteHttps);

    assert(resolved->accession == NULL);

    rc = _ItemSetResolverAndAccessionInResolved(item,
        resolver, repoMgr, vfs);

    assert ( item -> mane );

    if (rc == 0) {
        rc2 = 0;
        resolved->remoteSz = 0;
        {
            rc2 = _VResolverRemote(resolved->resolver, resolved, protocols,
                item);
            if ( rc2 == 0 ) {
                if ( resolved -> remoteHttp . path != NULL )
                    remote = & resolved -> remoteHttp;
                else if ( resolved -> remoteHttps . path != NULL )
                    remote = & resolved -> remoteHttps;
                else if ( resolved -> remoteFasp . path != NULL )
                    remote = & resolved -> remoteFasp;
                if ( resolved->local.path != NULL ) {
                    rc = VPathMakeString(resolved->local.path,
                                            &resolved->local.str);
                    DISP_RC2(rc, "VPathMakeString(VResolverLocal)",
                                    resolved->name);
                }
            }
            else  if (rc == 0)
                rc = rc2;
        }
        if (rc == 0) {
            rc_t rc3 = 0;
            if (resolved->file == NULL) {
                bool reliable = ! resolved->isUri;
                assert ( remote );
                rc3 = _KFileOpenRemote(&resolved->file, kns,
                    remote->path, remote -> str, reliable);
                if ( !resolved->isUri )
                    DISP_RC2(rc3, "cannot open remote file",
                                remote -> str->addr);
            }

            if (rc3 == 0 && resolved->file != NULL) {
                rc3 = KFileSize(resolved->file, &resolved->remoteSz);
                if (rc3 != 0)
                    DISP_RC2(rc3, "cannot get remote file size",
                        remote -> str->addr);
                else if (item->jwtCart == NULL) {
                    if (resolved->remoteSz >= maxSize)
                        return rc;
                    else if (resolved->remoteSz < minSize)
                        return rc;
                }
            }
        }
    }

    if (rc == 0) {
        rc2 = 0;
        if (resolved->file == NULL) {
            assert ( remote -> str );
            if (!_StringIsFasp(remote -> str, NULL)) {
                rc2 = _KFileOpenRemote(&resolved->file, kns,
                    remote->path, remote -> str, !resolved->isUri);
            }
        }
        if (rc2 == 0 && resolved->file != NULL
            && resolved->remoteSz == 0)
        {
            rc2 = KFileSize(resolved->file, &resolved->remoteSz);
            DISP_RC2(rc2, "KFileSize(remote)", resolved->name);
        }
    }

    return rc;
}

/* Resolved: resolve locations */
static rc_t ItemInitResolved(Item *self, VResolver *resolver, KDirectory *dir,
    bool ascp, const KRepositoryMgr *repoMgr,
    const VFSManager *vfs, KNSManager *kns)
{
    Resolved *resolved = NULL;
    rc_t rc = 0;
    VRemoteProtocols protocols = ascp ? eProtocolHttpHttpsFasp
                                      : eProtocolHttpHttps;

    const VPathStr * remote = NULL;

    assert(self && self->mane);

    resolved = &self->resolved;
    resolved->name = ItemName(self);

    assert(resolved->type != eRunTypeUnknown);

    if (!self->isDependency &&
        self->desc != NULL) /* object name is specified (not kart item) */
    {
        if ( self -> mane -> outFile == NULL ) {
            bool local = false;
            KPathType type
                = KDirectoryPathType(dir, "%s", self->desc) & ~kptAlias;
            if (type == kptFile)
                local = true;
            else if (type == kptDir) {
                if ((KDirectoryPathType(dir, "%s/%s.sra",
                    self->desc, self->desc) & ~kptAlias) == kptFile)
                {
                    if (self->mane->force == eForceNo)
                        local = true;
                }
            }

            if (local) {
                rc = VPathStrInitStr(&resolved->path, self->desc, 0);
                resolved->existing = true;
                if (resolved->type != eRunTypeDownload) {
                    uint64_t s = -1;
                    const KFile *f = NULL;
                    rc = KDirectoryOpenFileRead(dir, &f, "%s", self->desc);
                    if (rc == 0)
                        rc = KFileSize(f, &s);
                    if (s != -1)
                        resolved->remoteSz = s;
                    else
                        OUTMSG(("%s\tunknown\n", self->desc));
                    RELEASE(KFile, f);
                }
                else
                    STSMSG(STS_TOP,
                        ("'%s' is a local non-kart file", self->desc));

                return 0;
            }
        }
    }

    if (!self->isDependency)
        rc = _ItemResolveResolved(resolver, protocols, self,
            repoMgr, vfs, kns, self->mane->minSize, self->mane->maxSize);

    if ( resolved -> remoteHttp . path != NULL )
        remote = & resolved -> remoteHttp;
    else if ( resolved -> remoteHttps . path != NULL )
        remote = & resolved -> remoteHttps;
    else if ( resolved -> remoteFasp . path != NULL )
        remote = & resolved -> remoteFasp;

    if (rc == 0) {
        if (self->jwtCart == NULL) {
            if (resolved->remoteSz >= self->mane->maxSize) {
                resolved->oversized = true;
                return rc;
            }
            if (resolved->remoteSz < self->mane->minSize) {
                resolved->undersized = true;
                return rc;
            }
        }

        if (resolved->local.str == NULL
            && (resolved->cache == NULL || remote == NULL
                                        || remote -> str == NULL))
        {
            rc = RC(rcExe, rcPath, rcValidating, rcParam, rcNull);
            PLOGERR(klogInt, (klogInt, rc,
                "bad VResolverResolve($(acc)) result",
                "acc=%s", resolved->name));
        }
    }

    return rc;
}

/* resolve: locate */
static rc_t ItemResolve(Item *item, int32_t row) {
    Resolved *self = NULL;
    static int n = 0;
    rc_t rc = 0;
    bool ascp = false;

    assert(item && item->mane);

    self = &item->resolved;
    assert(self->type);

    ++n;
    if (row > 0 &&
        item->desc == NULL) /* desc is NULL for kart items */
    {
        n = row;
    }

    item->number = n;

    ascp = PrfMainUseAscp(item->mane);
    if (self->type == eRunTypeList) {
        ascp = false;
    }

    rc = ItemInitResolved(item, item->mane->resolver, item->mane->dir, ascp,
        item->mane->repoMgr, item->mane->vfsMgr, item->mane->kns);

    return rc;
}

static bool maxSzPrntd = false;

static void logMaxSize(size_t maxSize) {
    if (maxSzPrntd) {
        return;
    }
        
    maxSzPrntd = true;

    if (maxSize == 0) {
/*      OUTMSG(("Maximum file size download limit is unlimited\n")); */
            return;
    }

    if (maxSize / 1024 < 10) {
        PLOGMSG(klogWarn, (klogWarn,
            "Maximum file size download limit is $(size)B\n",
            "size=%zu", maxSize));
        return;
    }

    maxSize /= 1024;
    if (maxSize / 1024 < 10) {
        PLOGMSG(klogWarn, (klogWarn,
            "Maximum file size download limit is $(size)KB\n",
            "size=%zu", maxSize));
        return;
    }

    maxSize /= 1024;
    if (maxSize / 1024 < 10) {
        PLOGMSG(klogWarn, (klogWarn,
            "Maximum file size download limit is $(size)MB\n",
            "size=%zu", maxSize));
        return;
    }

    maxSize /= 1024;
    if (maxSize / 1024 < 10) {
        PLOGMSG(klogWarn, (klogWarn,
            "Maximum file size download limit is $(size)GB\n",
            "size=%zu", maxSize));
        return;
    }

    maxSize /= 1024;
    PLOGMSG(klogWarn, (klogWarn,
        "Maximum file size download limit is $(size)TB\n",
        "size=%zu", maxSize));
}

static void logBigFile(int n, const char *name, size_t size) {
    if (size / 1024 < 10) {
        STSMSG(STS_TOP,
            ("%d) '%s' (%,zuB) is larger than maximum allowed: skipped\n",
                n, name, size));
        return;
    }

    size /= 1024;
    if (size / 1024 < 10) {
        STSMSG(STS_TOP,
            ("%d) '%s' (%,zuKB) is larger than maximum allowed: skipped\n",
                n, name, size));
        return;
    }

    size /= 1024;
    if (size / 1024 < 10) {
        STSMSG(STS_TOP,
            ("%d) '%s' (%,zuMB) is larger than maximum allowed: skipped\n",
                n, name, size));
        return;
    }

    size /= 1024;
    if (size / 1024 < 10) {
        STSMSG(STS_TOP,
            ("%d) '%s' (%,zuGB) is larger than maximum allowed: skipped\n",
                n, name, size));
        return;
    }

    size /= 1024;
    STSMSG(STS_TOP,
        ("%d) '%s' (%,zuTB) is larger than maximum allowed: skipped\n",
            n, name, size));
}

/* download if not found; obey size restriction */
static rc_t ItemDownload(Item *item) {
    bool isLocal = false;
    int n = 0;
    rc_t rc = 0;
    Resolved *self = NULL;
    const char * name = NULL;

    assert(item && item->mane);

    n = item->number;
    self = &item->resolved;
    name = self->name;

    assert(self->type);

    if (rc == 0) {
        uint64_t sz = 0;
        bool skip = false;
        bool undersized = self->undersized;
        bool oversized = self->oversized;
        if (self->respFile != NULL) {
            const VPath * local = NULL;

            rc_t r = KSrvRespFileGetSize(self->respFile, & sz);
            if (r == 0) {
                oversized = sz >= item->mane->maxSize;
                undersized = sz < item->mane->minSize;
                if (item->jwtCart == NULL) {
                    self->oversized = oversized;
                    self->undersized = undersized;
                }
            }

            r = KSrvRespFileGetName(self->respFile, &name);

            r = KSrvRespFileGetLocal(self->respFile, & local);
            if (r == 0)
                rc = VPathStrInit(&self->local, local);
        }
        if (self->existing) { /* the path is a path to an existing local file */
            rc = VPathStrInitStr(&self->path, item->desc, 0);
            if (rc == 0)
                rc = PrfOutFileConvert(item->mane->dir, item->desc);
            return rc;
        }
        if (undersized) {
            STSMSG(STS_TOP,
               ("%d) '%s' (%,zu KB) is smaller than minimum allowed: skipped\n",
                n, name, sz / 1024));
            skip = true;
            item->mane->undersized = true;
        }
        else if (oversized) {
            logMaxSize(item->mane->maxSize);
            logBigFile(n, name, sz);
            skip = true;
            item->mane->oversized = true;
        }

        rc = ResolvedLocal(self, item->mane, &isLocal,
            skip ? eForceNo : item->mane->force);

        if (rc == 0) {
            if (skip && !isLocal) {
                return rc;
            }
        }
    }

    {
        KStsLevel lvl = STS_DBG;
        if (item->mane->dryRun)
            lvl = STAT_USR;
        STSMSG(lvl, ("########## local(%s)",
            self->local.str ? self->local.str->addr : "NULL"));
    }

    if (rc == 0) {
        if (isLocal) {
            if (self->inOutDir) {
                const char * start = self->cache->addr;
                size_t size = self->cache->size;
                const char * end = start + size;
                const char * sep = string_rchr ( start, size, '/' );
                if ( sep != NULL )
                    start = sep + 1;
                STSMSG(STS_TOP, ("%d) '%s' is found locally (%.*s)",
                    n, name, ( uint32_t ) ( end - start ), start));
            }
            else
                STSMSG(STS_TOP, ("%d) '%s' is found locally", n, name));
            if (self->local.str != NULL) {
                VPathStrFini(&self->path);
                rc = StringCopy(&self->path.str, self->local.str);
            }
        }
        else if (self->remoteFasp.str == NULL && item->mane->noHttp) {
            rc = RC(rcExe, rcFile, rcCopying, rcFile, rcNotFound);
            PLOGERR(klogErr, (klogErr, rc,
                "cannot download '$(name)' using requested transport",
                "name=%s", name));
        }
        else {
            bool notFound = false;
            const char * name = self->name;
            if (self->respFile != NULL) {
                const char * acc = NULL;
                rc_t r2 = KSrvRespFileGetName(self->respFile, &acc);
                if (r2 == 0 && acc != NULL)
                    name = acc;
            }
            STSMSG(STS_TOP, ("%d) Downloading '%s'...", n, name));
            notFound =
                KDirectoryPathType(item->mane->dir, "%s", name) == kptNotFound;
            rc = PrfMainDownload(self, item, item->isDependency, NULL);
            if (item->mane->dryRun && notFound
                && KDirectoryPathType(item->mane->dir, "%s", name)
                    == kptDir)
            {
                KNamelist * list = NULL;
                rc = KDirectoryList(item->mane->dir,
                    &list, NULL, NULL, "%s", name);
                if (rc == 0) {
                    uint32_t count = 0;
                    rc = KNamelistCount(list, &count);
                    if (rc == 0 && count == 0)
                        KDirectoryRemove(item->mane->dir, false, "%s", name);
                }
                RELEASE(KNamelist, list);
            }
            if (rc == 0) {
                if (self->inOutDir) {
                    const char * start = self->cache->addr;
                    size_t size = self->cache->size;
                    const char * end = start + size;
                    const char * sep = string_rchr ( start, size, '/' );
                    if ( sep != NULL )
                        start = sep + 1;
                    if ( item->mane->outDir != NULL )
                        STSMSG(STS_TOP,
                            ("%d) '%s' was downloaded successfully (%s/%.*s)",
                            n, name, item->mane->outDir,
                            ( uint32_t ) ( end - start ), start));
                    else
                        STSMSG(STS_TOP,
                            ("%d) '%s' was downloaded successfully (%.*s)",
                            n, name,
                            ( uint32_t ) ( end - start ), start));
                }
                else
                    STSMSG(STS_TOP, ("%d) '%s' was downloaded successfully",
                                       n, name));
                if (self->cache != NULL) {
                    VPathStrFini(&self->path);
                    rc = StringCopy(&self->path.str, self->cache);
                }
            }
            else if (rc != SILENT_RC(rcExe,
                rcProcess, rcExecuting, rcProcess, rcCanceled))
            {
                STSMSG(STS_TOP, ("%d) failed to download %s", n, name));
            }
        }
    }
    else {
        STSMSG(STS_TOP, ("%d) cannot locate '%s'", n, self->name));
    }

    return rc;
}

static rc_t ItemPrintSized(const Item *self, int32_t row, size_t size) {
    rc_t rc = 0;
    const String *projId = NULL;
    const String *itemId = NULL;
    const String *accession = NULL;
    const String *name = NULL;
    const String *itemDesc = NULL;
    assert(self && row);
    if (self->desc != NULL) { /* object */
        OUTMSG(("%d\t%s\t%,zuB\n", row, self->desc, size));
    }
    else {                    /* kart item */
        if (rc == 0) {
            rc = KartItemProjId(self->item, &projId);
        }
        if (rc == 0) {
            rc = KartItemItemId(self->item, &itemId);
        }
        if (rc == 0) {
            rc = KartItemAccession(self->item, &accession);
        }
        if (rc == 0) {
            rc = KartItemName(self->item, &name);
        }
        if (rc == 0) {
            rc = KartItemItemDesc(self->item, &itemDesc);
        }
        if (rc == 0) {
           rc = OUTMSG(("%d\t%S|%S|%S|%S|%S\t%,zuB\n",
               row, projId, itemId, accession, name, itemDesc, size));
        }
    }

    return rc;
}

static rc_t ItemPostDownload(Item *item, int32_t row);

/* resolve: locate; download if not found */
static
rc_t ItemResolveResolvedAndDownloadOrProcess(Item *self, int32_t row)
{
    rc_t rc = ItemResolve(self, row);
    if (rc != 0)
        return rc;

    assert(self);

    if (self->resolved.type == eRunTypeList)
        return ItemPrintSized(self, row, self->resolved.remoteSz);
    else if (self->resolved.type != eRunTypeDownload)
        return rc;

    if (self->resolved.respFile != NULL) {
        do {
            rc_t r1 = 0;
            rc_t rd = ItemDownload(self);
            if (rd != 0) {
                if (rc == 0)
                    rc = rd;
            }
            else if (self->resolved.type == eRunTypeDownload && !self->isDependency
                && !self->mane->dryRun)
            {
                rd = ItemPostDownload(self, row);
                if (rd != 0 && rc == 0)
                    rc = rd;
            }

            r1 = VPathStrFini(&self->resolved.local);
            if (r1 != 0 && rc == 0)
                rc = r1;
            RELEASE(String, self->resolved.cache);
            r1 = VPathStrFini(&self->resolved.remoteHttp);
            if (r1 != 0 && rc == 0) {
                rc = r1;
                break;
            }
            r1 = VPathStrFini(&self->resolved.remoteHttps);
            if (r1 != 0 && rc == 0)
                rc = r1;
            r1 = VPathStrFini(&self->resolved.remoteFasp);
            if (r1 != 0 && rc == 0)
                rc = r1;
            RELEASE(KFile, self->resolved.file);
            self->resolved.remoteSz = 0;
            self->resolved.undersized = self->resolved.oversized
                = self->resolved.existing = /*self->resolved.downloaded =*/ false;
            r1 = VPathStrFini(&self->resolved.path);
            if (r1 != 0 && rc == 0)
                rc = r1;
            RELEASE(KSrvRespFile, self->resolved.respFile);
            if (rc != 0)
                break;

            r1 = KSrvRespObjIteratorNextFile(self->resolved.respIt,
                &self->resolved.respFile);
            if (r1 != 0 && rc == 0) {
                rc = r1;
                break;
            }
            if (self->resolved.respFile == NULL) {
                uint32_t l = KSrvResponseLength(self->resolved.response);
                if (++self->resolved.respObjIdx < l) {
                    const KSrvRespObj * obj = NULL;
                    rc = KSrvResponseGetObjByIdx(self->resolved.response,
                        self->resolved.respObjIdx, &obj);
                    if (rc == 0)
                        rc = KSrvRespObjMakeIterator(
                            obj, &self->resolved.respIt);
                    if (rc == 0)
                        rc = KSrvRespObjIteratorNextFile(
                            self->resolved.respIt, &self->resolved.respFile);
                    RELEASE(KSrvRespObj, obj);
                }
            }
        } while (self->resolved.respFile != NULL);
    }
    else { /* resolver was not called: checking a local fiie */
        rc = ItemDownload(self);
        if (rc == 0 && self->resolved.type == eRunTypeDownload)
            rc = ItemPostDownload(self, row);
    }

    return rc;
}

static rc_t ItemDownloadDependencies(Item *item) {
    Resolved *resolved = NULL;
    rc_t rc = 0;
    const VDBDependencies *deps = NULL;
    uint32_t count = 0;
    uint32_t i = 0;
    ESrvFileFormat ff = eSFFInvalid;

    assert(item && item->mane);

    if (item->isDependency)
        return 0;

    resolved = &item->resolved;

    assert(resolved);

    if (resolved->respFile != NULL &&
        KSrvRespFileGetFormat(resolved->respFile, &ff) == 0)
    {
        if (ff == eSFFVdbcache) {
            STSMSG(STS_DBG, ("skipped dependencies check for '%s.vdbcache'",
                resolved->name));
            return 0;
        }
    }

    if (resolved->path.str != NULL)
        rc = PrfMainDependenciesList(item->mane, resolved, &deps);

    /* resolve dependencies (refseqs) */
    if (rc == 0 && deps != NULL) {
        rc = VDBDependenciesCount(deps, &count);
        if (rc == 0) {
            STSMSG(STS_TOP, ("'%s' has %d%s dependenc%s", resolved->name,
                count, item->mane->check_all ? "" : " unresolved",
                count == 1 ? "y" : "ies"));
        }
        else {
            DISP_RC2(rc, "Failed to check dependencies", resolved->name);
        }
    }

    for (i = 0; i < count && rc == 0; ++i) {
        bool local = true;
        const char *seq_id = NULL;

        if (rc == 0) {
            rc = VDBDependenciesLocal(deps, &local, i);
            DISP_RC2(rc, "VDBDependenciesLocal", resolved->name);
            if (local) {
                continue;
            }
        }

        if (rc == 0) {
            rc = VDBDependenciesSeqId(deps, &seq_id, i);
            DISP_RC2(rc, "VDBDependenciesSeqId", resolved->name);
        }

        if (rc == 0) {
            size_t num_writ = 0;
            char ncbiAcc[512] = "";

            assert(seq_id);

            rc = string_printf(ncbiAcc, sizeof ncbiAcc, &num_writ,
                "ncbi-acc:%s?vdb-ctx=refseq", seq_id);
            DISP_RC2(rc, "string_printf(?vdb-ctx=refseq)", seq_id);
            if (rc == 0 && num_writ > sizeof ncbiAcc) {
                rc = RC(rcExe, rcFile, rcCopying, rcBuffer, rcInsufficient);
                PLOGERR(klogInt, (klogInt, rc,
                    "bad string_printf($(s)?vdb-ctx=refseq) result",
                    "s=%s", seq_id));
            }
    
            if (rc == 0) {
                Item *ditem = calloc(1, sizeof *ditem);
                if (ditem == NULL)
                    return RC(rcExe,
                        rcStorage, rcAllocating, rcMemory, rcExhausted);

                ditem->desc = ncbiAcc;
                ditem->mane = item->mane;
                ditem->isDependency = true;
                ditem->seq_id = string_dup_measure ( seq_id, NULL );
                if ( ditem->seq_id == NULL )
                    return RC(rcExe,
                        rcStorage, rcAllocating, rcMemory, rcExhausted);

                ResolvedReset(&ditem->resolved, eRunTypeDownload);

                rc = ItemSetDependency(ditem, deps, i);

                if (rc == 0)
                    rc = ItemResolveResolvedAndDownloadOrProcess(ditem, 0);

                RELEASE(Item, ditem);
            }
        }
    }

    RELEASE(VDBDependencies, deps);

    return rc;
}

static rc_t ItemPostDownload(Item *item, int32_t row) {
    rc_t rc = 0;
    Resolved *resolved = NULL;
    KPathType type = kptNotFound;
    assert(item);
    resolved = &item->resolved;
    if (resolved->type == eRunTypeList) {
        return rc;
    }
    else if (resolved->oversized) {
        item->mane->oversized = true;
    }
    else if (resolved->undersized) {
        item->mane->undersized = true;
    }

    if (resolved->path.str != NULL) {
        const char * path = NULL;
        assert(item->mane);
        rc = _VDBManagerSetDbGapCtx(item->mane->mgr, resolved->resolver);
        path = resolved -> path . str -> addr;
        type = VDBManagerPathTypeUnreliable
            ( item->mane->mgr, "%S", resolved->path.str) & ~kptAlias;

        assert ( path );

        if (type != kptDatabase) {
            if (type == kptTable) {
                 STSMSG(STS_DBG, ("...'%s' is a table", path));
            }
            else {
                 STSMSG(STS_DBG, ("...'%s' is not recognized "
                     "as a database or a table", path));
            }
            return rc;
         }
        else {
            STSMSG(STS_DBG, ("...'%s' is a database", path));
        }
    }

    rc = ItemDownloadDependencies(item);
    if (true) {
        rc_t rc2 = Quitting();
        if (rc == 0 && rc2 != 0) {
            rc = rc2;
        }
    }
    return rc;
}

static rc_t ItemProcess(Item *item, int32_t row) {
    /* resolve: locate; download if not found */
    return ItemResolveResolvedAndDownloadOrProcess(item, row);
}

const char UsageDefaultName[] = "prefetch";
rc_t CC UsageSummary(const char *progname) {
    return OUTMSG((
        "Usage:\n"
        "  %s [options] <SRA accession> [...]\n"
        "  Download SRA files and their dependencies\n"
        "\n"
        "  %s [options] --cart <kart file>\n"
        "  Download cart file\n"
        "\n"
        "  %s [options] <URL> --output-file <FILE>\n"
        "  Download URL to FILE\n"
        "\n"
        "  %s [options] <URL> [...] --output-directory <DIRECTORY>\n"
        "  Download URL or URL-s to DIRECTORY\n"
        "\n"
        "  %s [options] <SRA file> [...]\n"
        "  Check SRA file for missed dependencies "
                                           "and download them\n"
        "\n"
        , progname, progname, progname, progname, progname));
}

/******************************************************************************/

/*********** Iterator **********/
static rc_t
IteratorInit(Iterator *self, const char *obj, const PrfMain *mane)
{
    rc_t rc = 0;

    KPathType type = kptNotFound;

    assert(self && mane);
    memset(self, 0, sizeof *self);

    if (obj == NULL && mane->kart != NULL) {
        type = KDirectoryPathType(mane->dir, "%s", mane->kart);
        if ((type & ~kptAlias) != kptFile) {
            rc = RC(rcExe, rcFile, rcOpening, rcFile, rcNotFound);
            DISP_RC(rc, mane->kart);
            return rc;
        }
        rc = KartMakeWithNgc(mane->dir, mane->kart, &self->kart, &self->isKart,
            mane->ngc);
        if (rc != 0) {
            if (!self->isKart)
                rc = 0;
            else
                PLOGERR(klogErr, (klogErr, rc, "'$(F)' is not a kart file",
                    "F=%s", mane->kart));
        }
        return rc;
    }
#if _DEBUGGING
    else if (obj == NULL && mane->textkart != NULL) {
        type = KDirectoryPathType(mane->dir, "%s", mane->textkart);
        if ((type & ~kptAlias) != kptFile) {
            rc = RC(rcExe, rcFile, rcOpening, rcFile, rcNotFound);
            DISP_RC(rc, mane->textkart);
            return rc;
        }
        rc = KartMakeText(mane->dir, mane->textkart, &self->kart,
            &self->isKart);
        if (rc != 0) {
            if (!self->isKart) {
                rc = 0;
            }
            else {
                PLOGERR(klogErr, (klogErr, rc, "'$(F)' is not a text kart file",
                    "F=%s", mane->textkart));
            }
        }
        return rc;
    }
#endif

    if (obj == NULL && mane->jwtCart != NULL) {
        type = KDirectoryPathType(mane->dir, "%s", mane->jwtCart);
        if ((type & ~kptAlias) != kptFile) {
            rc = RC(rcExe, rcFile, rcOpening, rcFile, rcNotFound);
            DISP_RC(rc, mane->jwtCart);
        }
        else
            self->jwtCart = mane->jwtCart;
        return rc;
    }

    assert(obj);
    type = KDirectoryPathType(mane->dir, "%s", obj);
    if ((type & ~kptAlias) == kptFile) {
        type = VDBManagerPathType(mane->mgr, "%s", obj);
        if ((type & ~kptAlias) == kptFile) {
            rc = KartMakeWithNgc(mane->dir, obj, &self->kart, &self->isKart,
                mane->ngc);
            if (rc != 0) {
                if (self->isKart)
                    PLOGERR(klogErr, (klogErr, rc,
                        "Cannot open kart '$(kart)' with ngc '$(ngc)'",
                        "kart=%s,ngc=%s", obj, mane->ngc));
                else
                    rc = 0;
            }
        }
    }

    if (rc == 0 && !self->isKart) {
        self->obj = obj;
    }

    return rc;
}

static rc_t IteratorNext(Iterator *self, Item **next, bool *done) {
    rc_t rc = 0;

    assert(self && next && done);

    *next = NULL;

    if (self->done) {
        *done = true;
        return 0;
    }

    *done = false;

    *next = calloc(1, sizeof **next);
    if (*next == NULL) {
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    if (self->isKart) {
        rc = KartMakeNextItem(self->kart, &(*next)->item);
        if (rc != 0) {
            if (rc ==
                SILENT_RC(rcKFG, rcNode, rcAccessing, rcNode, rcNotFound))
            {
                LOGERR(klogErr, rc, "Cannot read kart file. "
                    "Did you specify --" NGC_OPTION " command line option?");
            }
            else
                LOGERR(klogErr, rc, "Invalid kart file: cannot read next row");
        }
        else if ((*next)->item == NULL) {
            RELEASE(Item, *next);
            *next = NULL;
            *done = true;
        }

        if (rc == 0 && *done) {
            self->done = true;
        }
    }
    else if (self->jwtCart != NULL) {
        (*next)->jwtCart = self->jwtCart;
        self->done = true;
    }
    else {
        rc = ItemInit(*next, self->obj);
        self->done = true;
    }

    return rc;
}

static void IteratorFini(Iterator *self) {
    rc_t rc = 0;

    assert(self);

    RELEASE(Kart, self->kart);
}

/********** KartTreeNode **********/
static void CC bstKrtWhack(BSTNode *n, void *ignore) {
    KartTreeNode *sn = (KartTreeNode*) n;

    assert(sn);

    ItemRelease(sn->i);

    memset(sn, 0, sizeof *sn);

    free(sn);
}

static int64_t CC bstKrtSort(const BSTNode *item, const BSTNode *n) {
    const KartTreeNode *sn1 = (const KartTreeNode*)item;
    const KartTreeNode *sn2 = (const KartTreeNode*)n;

    assert(sn1 && sn2 && sn1->i && sn2->i);

    if (sn1->i->resolved.remoteSz < sn2->i->resolved.remoteSz)
        return -1;
    else if (sn1->i->resolved.remoteSz > sn2->i->resolved.remoteSz)
        return 1;

    return 0;
}

static void CC bstKrtDownload(BSTNode *n, void *data) {
    rc_t rc = 0;

    const KartTreeNode *sn = (const KartTreeNode*) n;
    assert(sn && sn->i);

    rc = ItemDownload(sn->i);

    if (rc == 0)
        rc = ItemPostDownload(sn->i, sn->i->number);
}

/*********** Process one command line argument **********/
static rc_t PrfMainRun ( PrfMain * self, const char * arg, const char * realArg,
                      uint32_t pcount, bool * multiErrorReported )
{
    ERunType type = eRunTypeDownload;
    rc_t rc = 0;
    Iterator it;
    assert(self && realArg && multiErrorReported);
    memset(&it, 0, sizeof it);

    if (rc == 0)
        rc = IteratorInit(&it, arg, self);

    if (false && rc == 0 && it.isKart) {
        rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcUnsupported);
        LOGERR(klogErr, rc, "Your kart file is out of date. "
            "Please login to the dbGaP portal to download a new one.");
    }

    if ( rc == 0 ) {
        if ( it . kart != NULL ) {
            if ( self -> outFile ) {
                rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInvalid );
                LOGERR ( klogErr, rc, "Cannot specify --" OUT_FILE_OPTION
                                    " when downloading kart file" );
            }
        }
        else {
            if ( self -> orderOrOutFile != NULL )
                self -> outFile = self -> orderOrOutFile;
            if ( self -> outFile != NULL && pcount > 1 ) {
                if ( ! * multiErrorReported ) {
                    rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInvalid );
                    LOGERR ( klogErr, rc, "Cannot specify --" OUT_FILE_OPTION
                                        " when downloading multiple objects" );
                    * multiErrorReported = true;
                }
                else
                    rc = SILENT_RC ( rcExe, rcArgv, rcParsing,
                                     rcParam, rcInvalid );
                return rc;
            }
        }
    }

    if (self->list_kart_sized)
        type = eRunTypeList;
    else if (self->order == eOrderSize) {
        if (rc == 0 && it.kart == NULL)
            type = eRunTypeDownload;
        else
            type = eRunTypeGetSize;
    }
    else
        type = eRunTypeDownload;

    if (rc == 0) {
        BSTree trKrt;
        BSTreeInit(&trKrt);

        if (self->dryRun)
            STSMSG(0, ("A dry run was requested - skipping actual download"));

        if (self->list_kart) {
            if (it.kart != NULL) {
                if (self->list_kart_numbered) {
                    rc = KartPrintNumbered(it.kart);
                }
                else {
                    rc = KartPrint(it.kart);
                }
            }
            else {
                PLOGMSG(klogWarn, (klogWarn,
                    "'$(F)' is invalid or not a kart file",
                    "F=%s", realArg));
            }
        }
        else {
            size_t total = 0;
            const char *row = self->rows;
            int64_t n = 1;
            NumIterator nit;
            NumIteratorInit(&nit, row);
            if (type == eRunTypeList) {
                self->maxSize = ~0;
                if (it.kart != NULL) {
                    OUTMSG((
                      "row\tproj-id|item-id|accession|name|item-desc\tsize\n"));
                }
            }
            else {
                if (it.kart != NULL) {
                    OUTMSG(("Downloading kart file '%s'\n", realArg));
                    if (type == eRunTypeGetSize) {
                        OUTMSG(("Checking sizes of kart files...\n"));
                    }
                }
                else if (self->jwtCart != NULL)
                    OUTMSG(("Downloading jwt cart file '%s'\n", self->jwtCart));
                OUTMSG(("\n"));
            }

            for (n = 1; ; ++n) {
                rc_t rc2 = 0;
                rc_t rc3 = 0;
                bool done = false;
                Item *item = NULL;
                rc_t rcq = Quitting();
                if (rcq != 0) {
                    if (rc == 0) {
                        rc = rcq;
                    }
                    break;
                }
                rc2 = IteratorNext(&it, &item, &done);
                if (rc2 != 0 || done) {
                    if (rc == 0 && rc2 != 0) {
                        rc = rc2;
                    }
                    break;
                }
                done = ! NumIteratorNext(&nit, n);
                if (done) {
                    break;
                }
                if (!nit.skip) {
                    item->mane = self;
                    ResolvedReset(&item->resolved, type);

                    rc3 = ItemProcess(item, (int32_t)n);
                    if (rc3 != 0) {
                        if (rc == 0) {
                            rc = rc3;
                        }
                    }
                    else {
                        if (item->resolved.undersized &&
                            type == eRunTypeGetSize)
                        {
                            STSMSG(STS_TOP,
               ("%d) '%s' (%,zu KB) is smaller than minimum allowed: skipped\n",
                n, item->resolved.name, item->resolved.remoteSz / 1024));
                        }
                        else if (item->resolved.oversized &&
                             type == eRunTypeGetSize)
                        {
                            logMaxSize(self->maxSize);
                            logBigFile(n, item->resolved.name,
                                          item->resolved.remoteSz);
                        }
                        else {
                            total += item->resolved.remoteSz;

                            if (item != NULL) {
                                if (type == eRunTypeGetSize) {
                                    KartTreeNode *sn = calloc(1, sizeof *sn);
                                    if (sn == NULL) {
                                        return RC(rcExe, rcStorage,
                                           rcAllocating, rcMemory, rcExhausted);
                                    }
                                    if (item->resolved.remoteSz == 0) {
                                        /* remoteSz is unknown:
                     add it to the end of download list preserving kart order */
                                        item->resolved.remoteSz
                                            = (~0ul >> 1) + n + 1;
                                    }
                                    sn->i = item;
                                    item = NULL;
                                    BSTreeInsert(&trKrt, (BSTNode*)sn,
                                        bstKrtSort);
                                }
                            }
                        }
                    }
                }

                RELEASE(Item, item);
            }

            if ( rc == 0 ) {
                if (type == eRunTypeList) {
                    if (it.kart != NULL && total > 0) {
                        OUTMSG
                            (("--------------------\ntotal\t%,zuB\n\n", total));
                    }
                }
                else if (type == eRunTypeGetSize) {
                    OUTMSG (("\nDownloading the files...\n\n", realArg));
                    BSTreeForEach (&trKrt, false, bstKrtDownload, NULL);
                }
            }
        }
        BSTreeWhack(&trKrt, bstKrtWhack, NULL);
    }
    if (it.isKart) {
        if (self->list_kart) {
            rc_t rc2 = OUTMSG(("\n"));
            if (rc2 != 0 && rc == 0) {
                rc = rc2;
            }
        }
        else if (rc == 0) {
            uint16_t number = 0;
            rc = KartItemsProcessed(it.kart, &number);
            if (rc == 0 && number == 0) {
                PLOGMSG(klogWarn, (klogWarn,
                    "kart file '$(F)' is empty", "F=%s", realArg));
            }
        }
    }
    IteratorFini(&it);

    return rc;
}

/*********** KMain **********/
rc_t CC KMain(int argc, char *argv[]) {
    rc_t rc = 0;
    bool insufficient = false;
    uint32_t pcount = 0;

    PrfMain pars;

    rc = PrfMainInit(argc, argv, &pars);

    if (rc == 0)
        rc = ArgsParamCount(pars.args, &pcount);
    if (rc == 0 && pcount == 0 && pars.jwtCart == NULL && pars.kart == NULL
#if _DEBUGGING
        && pars.textkart == NULL
#endif
        )
    {
        rc = UsageSummary(UsageDefaultName);
        insufficient = true;
    }

    if (rc == 0) {
        bool multiErrorReported = false;
        uint32_t i = ~0;

        /* JWT cart is processed here.
     All command line parameters are applied as accession filters to the cart */
        if (pars.jwtCart != NULL) {
            rc = PrfMainRun(&pars, NULL, pars.jwtCart, 1, &multiErrorReported);
        }
        else if (pars.kart != NULL) {
            if (pars.outFile != NULL) {
                LOGERR(klogWarn,
                    RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid),
                    "Cannot specify both --" OUT_FILE_OPTION
                    " and --" KART_OPTION ": "
                    "--" OUT_FILE_OPTION " is ignored");
                pars.outFile = NULL;
            }
            rc = PrfMainRun(&pars, NULL, pars.kart, 1, &multiErrorReported);
        }
#if _DEBUGGING
        else if (pars.textkart != NULL) {
            if (pars.outFile != NULL) {
                LOGERR(klogWarn,
                    RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid),
                    "Cannot specify both --" OUT_FILE_OPTION
                    " and --" TEXTKART_OPTION ": "
                    "--" OUT_FILE_OPTION " is ignored");
                pars.outFile = NULL;
            }
            rc = PrfMainRun(&pars, NULL, pars.textkart, 1, &multiErrorReported);
        }
        else
#endif

        /* All command line parameters are precessed here
           unless JWT cart is specified. */
        for (i = 0; i < pcount && pars.jwtCart == NULL; ++i) {
            const char *obj = NULL;
            rc_t rc2 = ArgsParamValue(pars.args, i, (const void **)&obj);
            DISP_RC(rc2, "ArgsParamValue");
            if (rc2 == 0) {
                rc2 = PrfMainRun(&pars, obj, obj, pcount, &multiErrorReported);
                if (rc2 != 0 && rc == 0)
                    rc = rc2;
            }
        }

        if (pars.undersized || pars.oversized) {
            OUTMSG(("\n"));
            if (pars.undersized) {
                OUTMSG((
               "Download of some files was skipped because they are too small\n"
                ));
            }
            if (pars.oversized) {
                OUTMSG((
               "Download of some files was skipped because they are too large\n"
                ));
            }
            OUTMSG((
               "You can change size download limit by setting\n"
             "--" MINSZ_OPTION " and --" SIZE_OPTION " command line arguments\n"
            ));
        }
    }

    {
        rc_t rc2 = PrfMainFini(&pars);
        if (rc2 != 0 && rc == 0) {
            rc = rc2;
        }
    }

    if ( rc == 0 && insufficient ) {
        rc = RC ( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
    }

    return rc;
}
