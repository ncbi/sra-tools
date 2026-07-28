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
* =========================================================================== */

#include <klib/container.h> /* BSTree */
#include <klib/log.h> /* PLOGERR */

struct VDBDependencies;

typedef enum {
    eRunTypeUnknown,
    eRunTypeList,     /* list sizes */
    eRunTypeDownload,
    eRunTypeGetSize    /* download ordered by sizes */
} ERunType;

typedef struct {
    const struct VPath *path;
    const struct String *str;
} VPathStr;

typedef struct {
    ERunType type;
    char *name;     /* name to resolve */
    char *id; /* numeric ID for kart items */

    VPathStr      local;
    const struct String *cache;

    VPathStr      remoteHttp;
    VPathStr      remoteHttps;
    VPathStr      remoteFasp;

    const struct VPath * remote;

    const struct KFile *file;
    uint64_t remoteSz;

    bool undersized; /* remoteSz < min allowed size */
    bool oversized; /* remoteSz >= max allowed size */

    bool existing; /* the path is a path to an existing local file */

 /* path to the resolved object : absolute path or resolved(local or cache) */
    VPathStr path;

    struct VPath *accession;
    bool isUri; /* accession is URI */
    bool inOutDir; /* cache location is in the output directory ow cwd */

    uint64_t project;
    bool dbgapProject;
    /* project > 0 : protected
       project = 0 & dbgapProject   : protected (1000 genomes)
       project = 0 & ! dbgapProject : public  */

    const struct KartItem *kartItem;

    struct VResolver *resolver;

    const struct KSrvResponse * response;
    uint32_t respObjIdx;
    struct KSrvRespObjIterator * respIt;
    struct KSrvRespFile * respFile;
} Resolved;

typedef enum {
    eOrderSize,
    eOrderOrig
} EOrder;

typedef enum {
    eForceNo, /* do not download found and complete objects */
    eForceYes,/* force download of found and complete objects */
    eForceAll, /* force download; ignore locks, keep transaction file */
    eForceALL, /* force download; ignore locks and transaction file */
} EForce;

typedef enum {
    eDefault,
    eFalse,
    eTrue,
} ETernary;

typedef struct PrfMain {
    struct Args *args;
    bool check_all;
    ETernary check_refseqs;

    bool list_kart;
    bool list_kart_numbered;
    bool list_kart_sized;
    EOrder order;

    const char *rows;

    EForce force;
    bool resume;
    bool validate;

    struct KConfig *cfg;
    struct KDirectory *dir;

    const struct KRepositoryMgr *repoMgr;
    const struct VDBManager *mgr;
    struct VFSManager *vfsMgr;
    struct KNSManager *kns;

    struct VResolver *resolver;

    void  *buffer;
    size_t bsize;

    bool undersized; /* remoteSz < min allowed size */
    bool oversized; /* remoteSz >= max allowed size */

    BSTree downloaded;

    uint64_t minSize;
    uint64_t maxSize;
    uint64_t heartbeat;
    bool showProgress;

    bool noAscp;
    bool noHttp;

    bool forceAscpFail;

    bool ascpChecked;
    const char *ascp;
    const char *asperaKey;
    struct String *ascpMaxRate;
    const char *ascpParams; /* do not free! */

//  bool stripQuals;     /* this will download file without quality columns */
    bool eliminateQuals; /* this will download cache file with eliminated
                            quality columns which could filled later */

    bool dryRun; /* Dry run the app: don't download, only check resolving */

    const char * location; /* do not free! */
    const char * outDir;  /* do not free! */
    const char * outFile; /* do not free! */
    const char * orderOrOutFile; /* do not free! */
    const char * fileType;  /* do not free! */
    const char * ngc;  /* do not free! */
    const char * jwtCart;  /* do not free! */

    const char * kart;
#if _DEBUGGING
    const char * textkart;
#endif

    ETernary fullQuality; // Current preference
} PrfMain;

bool _StringIsXYZ(const struct String *self, const char **withoutScheme,
    const char * scheme, size_t scheme_size);
bool _StringIsFasp(const struct String *self, const char **withoutScheme);

rc_t _KFileOpenRemote(const struct KFile **self, struct KNSManager *kns,
    const struct VPath *vpath, const struct String *path, bool reliable);

rc_t _VDBManagerSetDbGapCtx(
    const struct VDBManager *self, struct VResolver *resolver);

bool PrfMainHasDownloaded(const PrfMain *self, const char *local);
rc_t PrfMainDownloaded(PrfMain *self, const char *path);
bool PrfMainUseAscp(PrfMain *self);
rc_t PrfMainDependenciesList(const PrfMain *self,
    const Resolved *resolved, const struct VDBDependencies **deps);
rc_t PrfMainInit(int argc, char *argv[], PrfMain *self);
rc_t PrfMainFini(PrfMain *self);

extern const char UsageDefaultName[];
rc_t CC UsageSummary(const char *progname);

#define DISP_RC(rc, err) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, err))

#define DISP_RC2(rc, msg, name) (void)((rc == 0) ? 0 : \
   PLOGERR(klogInt, (klogInt,rc, "$(msg) [$(name)]","msg=%s,name=%s",msg,name)))

#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 != 0 && rc == 0) { rc = rc2; } obj = NULL; } while (false)

#define STS_TOP  1
#define STS_INFO 2
#define STS_DBG  3
#define STS_FIN  4

#define ELIM_QUALS_OPTION "eliminate-quals"
#define KART_OPTION "cart"
#define MINSZ_OPTION "min-size"
#define NGC_OPTION "ngc"
#define OUT_FILE_OPTION "output-file"
#define SIZE_OPTION "max-size"
#if _DEBUGGING
#define TEXTKART_OPTION "text-kart"
#endif
