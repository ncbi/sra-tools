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
* ==============================================================================
*
* Communicate with CSVM server.
* Keep the list of submitted orders for tracking and to request user quotas.
*/

#include "orders.h" /* KDirectory_ReadFile */

#include <kapp/main.h> /* KMain */

#include <kfg/config.h> /* KConfigRelease */

#include <klib/json.h> /* KJsonValueWhack */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* KOutMsg */
#include <klib/printf.h> /* string_printf */
#include <klib/rc.h> /* RC */
#include <klib/text.h> /* String */

#include <kns/http.h> /* KHttpRelease */
#include <kns/manager.h> /* KNSManagerRelease */
#include <kns/stream.h> /* KStreamRelease */

#define FINI(type, obj) do { rc_t rc2 = type##Fini(obj); \
    if (rc2 && !rc) { rc = rc2; } } while (false)
#define RELEASE(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

/* CSVM endpoints */
typedef struct {
    char * post;
    char * status;
    char * stats;
} CSEndpoints;

typedef struct {
    KDirectory * dir;
    KNSManager * mgr;

    char * cvsmOrders;
    char * jwtCartBuilder;

    CSEndpoints aws;
    CSEndpoints gcp;
} CSManager;

/* Functions performed by sra-cold-store tool */
typedef enum {
    eStatus,
    eQuotas,
    eRestore,
} EFunction;

enum { /* used as bitmask in CSArgs::clouds */
    eAws = 1,
    eGcp = 2,
};

typedef struct {
    EFunction mode;

    const char * jwtCart;
    const char * fileType;
    const char * permFile;

    int clouds; /* bitmask of clouds when multiple clouds are queried */
    CloudProviderId provider; /* target cloud - is used for 'Order Submit' */
    const char * bucket;
    bool notifyByEmail;

    Args * args;
    uint32_t params;
} CSArgs;

typedef struct {
    char * perm; /* now jwtUser string */
    const char * cookie; /* don't free */
} CSPerm;

/* JwtCart: created by jwt_cart_builder.cgi */
typedef struct {
    const CSArgs * args;
    const CSPerm * perm;
    char * jwtCart;
    char * jwtCartWithEmail;
} CSJwtCart;

typedef struct {
    CloudProviderId provider;
    char * input;
    char * response;
} CSVMRequest;

typedef struct {
    int64_t total;
    int64_t remaining;
} CSQuotaData;

typedef struct {
    CSQuotaData delivery;
    CSQuotaData warmup;
} CSQuota;

typedef struct {
    CloudProviderId provider;
    char * email;
    CSQuota dbgap;
    CSQuota public;
} CSQuotas;

const char UsageDefaultName[] = "sra-cold-store";

#define BUCK_OPTION "bucket"
#define BUCK_ALIAS  "b"
static const char * BUCK_USAGE[] = { "Requestor bucket name.", NULL };

#define KART_OPTION "cart"
static const char * KART_USAGE[] = { "PATH to jwt cart file.", NULL };

#define CLOU_OPTION "cloud"
#define CLOU_ALIAS  "c"
static const char * CLOU_USAGE[] =
{ "Cloud: comma-separated list of: aws, gcp.", NULL };

#define FUNC_RESTORE "restore"
#define FUNC_STATUS  "status"
#define FUNC_QUOTAS  "quotas"
#define FUNC_OPTION "function"
#define FUNC_ALIAS  "f"
static const char * FUNC_USAGE[] = { "Function to perform "
    "("FUNC_RESTORE ", " FUNC_STATUS ", " FUNC_QUOTAS "). "
    "Default: " FUNC_STATUS ".", NULL };

#define NOTI_OPTION "notity"
#define NOTI_ALIAS  "n"
static const char * NOTI_USAGE[] =
{ "Notify by email about order status (yes, no). Default: yes.", NULL };

#define PERM_OPTION "perm"
static const char * PERM_USAGE[] = { "PATH to file with permissions.", NULL };

#define TYPE_OPTION "type"
#define TYPE_ALIAS  "T"
static const char * TYPE_USAGE[] = { "Specify file type to download.", NULL };

static OptDef OPTIONS[] = {
/*                                            max_count needs_value required */
{ FUNC_OPTION  , FUNC_ALIAS, NULL, FUNC_USAGE,    1    ,   true    , false  },
{ PERM_OPTION  , NULL      , NULL, PERM_USAGE,    1    ,   true    , false  },
{ BUCK_OPTION  , BUCK_ALIAS, NULL, BUCK_USAGE,    1    ,   true    , false  },
{ CLOU_OPTION  , CLOU_ALIAS, NULL, CLOU_USAGE,    1    ,   true    , false  },
{ KART_OPTION  , NULL      , NULL, KART_USAGE,    1    ,   true    , false  },
{ TYPE_OPTION  , TYPE_ALIAS, NULL, TYPE_USAGE,    1    ,   true    , false  },
{ NOTI_OPTION  , NOTI_ALIAS, NULL, NOTI_USAGE,    1    ,   true    , false  },
};

static rc_t CSEndpointsFini(CSEndpoints * self) {
    assert(self);

    free(self->post);
    free(self->stats);
    free(self->status);

    memset(self, 0, sizeof * self);

    return 0;
}

/* print orders file name */
static rc_t printOrdersFile(char * dst, size_t bsize,
    size_t * num_writ, String * ncbiHome)
{   return string_printf(dst, bsize, num_writ, "%S/csvm", ncbiHome); }

/* initialize orders file name */
static rc_t InitCvsmOrdersFileName(char ** self) {
    rc_t rc = 0;
    String * ncbiHome = NULL;
    KConfig * kfg = NULL;
    size_t num_writ = 0;

    const char * c = getenv("NCBI_VDB_CSVM_FILE");
    if (c != NULL) {
        *self = string_dup_measure(c, NULL);
        if (*self == NULL)
            return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
        else
            return 0;
    }

    assert(self);
    *self = NULL;

    if (rc == 0)
        rc = KConfigMake(&kfg, NULL);

    if (rc == 0)
        rc = KConfigReadString(kfg, "NCBI_HOME", &ncbiHome);

    if (rc == 0) {
        char * buffer = NULL;
        printOrdersFile(NULL, 0, &num_writ, ncbiHome);
        buffer = malloc(++num_writ);
        if (buffer == NULL)
            return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
        else
            rc = printOrdersFile(buffer, num_writ, NULL, ncbiHome);

        if (rc == 0)
            *self = buffer;
    }

    StringWhack(ncbiHome);
    RELEASE(KConfig, kfg);

    return rc;
}

/* https://confluence.ncbi.nlm.nih.gov/display/SRA/Managing+Cold+Storage+and+Restoration */
#define CART_BUILDER "https://www.ncbi.nlm.nih.gov" \
        "/Traces/study/backends/jwt_cart_builder/jwt_cart_builder.cgi"
#define AWS_POST \
  "https://jhvroljigl.execute-api.us-east-1.amazonaws.com/csvm-workorder-post"
#define GCP_POST \
  "https://csvm-cloud-run-service-jlirrojisq-uk.a.run.app/csvm-workorder-post"
#define AWS_STATUS \
  "https://jhvroljigl.execute-api.us-east-1.amazonaws.com/csvm-workorder-status"
#define GCP_STATUS \
  "https://csvm-cloud-run-service-jlirrojisq-uk.a.run.app/csvm-workorder-status"
#define AWS_STATS \
  "https://jhvroljigl.execute-api.us-east-1.amazonaws.com/csvm-requestor-stat"
#define GCP_STATS \
  "https://csvm-cloud-run-service-jlirrojisq-uk.a.run.app/csvm-requestor-stat"

static char * CSEndpointInit(const char * dflt, const char * env)
{
    const char * c = getenv(env);
    if (c == NULL)
        c = dflt;

    return string_dup_measure(c, NULL);
}

static rc_t CSEndpointsInit(CSEndpoints * self, CloudProviderId cloud) {
    rc_t rc = 0;

    assert(self);

    memset(self, 0, sizeof * self);

    switch (cloud) {
    case cloud_provider_aws:
        self->post = CSEndpointInit(AWS_POST, "NCBI_VDB_CSVM_AWS_POST");
        self->stats = CSEndpointInit(AWS_STATS, "NCBI_VDB_CSVM_AWS_STATS");
        self->status = CSEndpointInit(AWS_STATUS, "NCBI_VDB_CSVM_AWS_STATUS");
        break;
    case cloud_provider_gcp:
        self->post = CSEndpointInit(GCP_POST, "NCBI_VDB_CSVM_GCP_POST");
        self->stats = CSEndpointInit(GCP_STATS, "NCBI_VDB_CSVM_GCP_STATS");
        self->status = CSEndpointInit(GCP_STATUS, "NCBI_VDB_CSVM_GCP_STATUS");
        break;
    default:
        assert(0);  return 1;
    }

    if (self->post == NULL || self->stats == NULL || self->status == NULL)
        rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    if (rc != 0)
        CSEndpointsFini(self);

    return rc;
}

static rc_t CSManagerFini(CSManager * self) {
    rc_t rc = 0;

    assert(self);

    RELEASE(KDirectory, self->dir);
    RELEASE(KNSManager, self->mgr);

    FINI(CSEndpoints, &self->aws);
    FINI(CSEndpoints, &self->gcp);

    free(self->cvsmOrders);
    free(self->jwtCartBuilder);

    memset(self, 0, sizeof * self);

    return rc;
}

static rc_t CSManagerInit(CSManager * self) {
    rc_t rc = 0;

    char * e = getenv("NCBI_VDB_JWT_CART_BUILDER");

    assert(self);
    memset(self, 0, sizeof * self);

    if (e != NULL && e[0] != '\0')
        self->jwtCartBuilder = string_dup_measure(e, NULL);
    else
        self->jwtCartBuilder = string_dup_measure(CART_BUILDER, NULL);
    if (self->jwtCartBuilder == NULL)
        rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    if (rc == 0)
        rc = InitCvsmOrdersFileName(&self->cvsmOrders);

    if (rc == 0)
        CSEndpointsInit(&self->aws, cloud_provider_aws);
    if (rc == 0)
        CSEndpointsInit(&self->gcp, cloud_provider_gcp);

    if (rc == 0)
        rc = KDirectoryNativeDir(&self->dir);
    if (rc == 0)
        rc = KNSManagerMake(&self->mgr);

    if (rc != 0)
        CSManagerFini(self);

    return rc;
}

/* create a request to call jwt_cart_builder.cgi */
static rc_t CSManagerMakeJwtCartReq(const CSManager * self,
    const CSPerm * perm, KHttpRequest ** aReq)
{
    rc_t rc = 0;
    KHttpRequest * req = NULL;

    assert(self && aReq);
    *aReq = NULL;

    rc = KNSManagerMakeRequest(self->mgr, &req,
        0x01010000, NULL, self->jwtCartBuilder);
    if (rc != 0)
        return rc;

    assert(perm->perm || perm->cookie);

    if (perm->perm != NULL)
        rc = KHttpRequestAddPostParam(req, "user_token=%s", perm->perm);
    else if (perm->cookie != NULL) {
        char name[] = "WGA_SESSION=";
        size_t bsize = sizeof name + string_measure(perm->cookie, NULL);
        char * cookie = malloc(bsize);
        if (cookie == NULL)
            rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
        else
            rc = string_printf(cookie, bsize, NULL, "%s%s", name, perm->cookie);
        if (rc == 0) {
            rc = KHttpRequestAddHeader(req, "Cookie", cookie);
            free(cookie);
        }
    }

    if (rc == 0)
        *aReq = req;
    else
        KHttpRequestRelease(req);

    return rc;
}

/* copy KHttpResult to buffer */
static rc_t KHttpResult_Read(KHttpResult * self, char ** buffer) {
    rc_t rc = 0;
    KStream * s = NULL;
    uint32_t code = 0;
    char * buf = NULL;
    uint64_t pos = 0;
    uint64_t size = 4096;
    size_t num_read = 0;

    assert(buffer);
    *buffer = NULL;

    if (rc == 0)
        rc = KClientHttpResultStatus(self, &code, NULL, 0, NULL);
    if (rc == 0 && code != 200)
        return RC(rcExe, rcFile, rcReading, rcData, rcUnexpected);
    if (rc == 0)
        rc = KClientHttpResultGetInputStream(self, &s);
    if (rc == 0) {
        buf = malloc(size);
        if (buf == NULL)
            rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    }

    while (rc == 0) {
        rc = KStreamRead(s, buf + pos, size - pos, &num_read);
        if (rc == 0)
            pos += num_read;
        if (num_read == 0) {
            if (rc == 0)
                if (pos == size) {
                    ++size;
                    buf = realloc(buf, size);
                    if (buf == NULL)
                        rc = RC(rcExe,
                            rcStorage, rcAllocating, rcMemory, rcExhausted);
                }
            if (rc == 0)
                buf[pos] = '\0';
            break;
        }

        if (pos == size) {
            size *= 2;
            buf = realloc(buf, size);
            if (buf == NULL)
                rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
        }
    }

    RELEASE(KStream, s);

    if (rc != 0)
        free(buf);
    else
        *buffer = buf;

    return rc;
}

/*
https://bitbucket.ncbi.nlm.nih.gov/projects/SRAA/repos/jwt-cart-builder/browse
call jwt_cart_builder.cgi to build jwtCart*/
static rc_t BuildJwtCart(const CSManager * mgr,
    const CSArgs * args, const CSPerm * perm, char ** buffer)
{
    rc_t rc = 0;
    KHttpRequest * req = NULL;
    KHttpResult * rslt = NULL;
    uint32_t code = 0;

    assert(mgr && args);

    if (rc == 0)
        rc = CSManagerMakeJwtCartReq(mgr, perm, &req);

    if (rc == 0 && args->jwtCart == NULL) {
        if (args->params == 0) {
            rc = RC(rcExe, rcArgv, rcProcessing, rcQuery, rcEmpty);
            PLOGERR(klogErr, (klogErr, rc,
                "Nothing to restore. Try '$(n) --help' for more information.",
                "n=%s", UsageDefaultName));
        }
        else {
            uint32_t i = 0;
            for (i = 0; i < args->params && rc == 0; ++i) {
                char * value = NULL;
                rc = ArgsParamValue(args->args, i, (void*)&value);
                if (rc == 0)
                    rc = KHttpRequestAddPostParam(req, "acc=%s", value);
            }
        }
    }

    if (rc == 0)
        rc = KClientHttpRequestPOST(req, &rslt);
    if (rc == 0)
        rc = KHttpResultStatus(rslt, &code, NULL, 0, NULL);
    if (rc == 0)
        rc = KHttpResult_Read(rslt, buffer);
    if (rc != 0 || code != 200) {
        if (rc == 0)
            rc = RC(rcExe, rcFile, rcReading, rcData, rcUnexpected);
        PLOGERR(klogErr, (
            klogErr, rc, "Cannot build restore request: '$(c)'", "c=%d", code));
    }

    RELEASE(KHttpResult, rslt);
    RELEASE(KHttpRequest, req);

    return rc;
}

/* call jwt_cart_builder.cgi to add email to jwtCart */
static rc_t CSJwtCartBuildWithEmail(CSJwtCart * self,
    const CSManager * mgr, const CSPerm * perm)
{
    rc_t rc = 0;

    KHttpRequest * req = NULL;
    KHttpResult * rslt = NULL;

    assert(self && self->jwtCart);

    if (rc == 0)
        rc = CSManagerMakeJwtCartReq(mgr, perm, &req);
    if (rc == 0)
        rc = KHttpRequestAddPostParam(req, "with_email=%s", self->jwtCart);
    if (rc == 0)
        rc = KClientHttpRequestPOST(req, &rslt);

    assert(!self->jwtCartWithEmail);

    if (rc == 0)
        rc = KHttpResult_Read(rslt, &self->jwtCartWithEmail);
    if (rc == 0) {
        assert(self->jwtCartWithEmail);
        if (self->jwtCartWithEmail[0] == '{') {
            rc = RC(rcExe, rcQuery, rcExecuting, rcQuery, rcFailed);
            PLOGERR(klogErr, (klogErr, rc,
                "Cannot build restore request: '$(error)'",
                "error=%s", self->jwtCartWithEmail));
        }
    }

    RELEASE(KHttpResult, rslt);
    RELEASE(KHttpRequest, req);

    return rc;
}

static rc_t CSArgsFini(CSArgs * self) {
    rc_t rc = 0;

    assert(self);

    rc = ArgsWhack(self->args);

    memset(self, 0, sizeof * self);

    return rc;
}

static rc_t Args_Get(const Args *self, const char *nm, const char **v)
{
    uint32_t pcount = 0;
    rc_t rc = ArgsOptionCount(self, nm, &pcount);

    if (rc != 0) {
        PLOGERR(klogErr, (klogErr, rc,
            "Failure to get '$(N)' argument", "N=%s", nm));
        return rc;
    }

    if (pcount > 0) {
        const char * val = NULL;
        rc = ArgsOptionValue(self, nm, 0, (const void **)&val);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,
                "Failure to get '$(N)' argument value", "N=%s", nm));
            return rc;
        }

        assert(v);
        *v = val;
    }

    return 0;
}

static rc_t CSArgsInitBucket(CSArgs * self, const char * nm) {
    const char * arg = NULL;
    rc_t rc = Args_Get(self->args, nm, &arg);

    if (rc == 0 && arg != NULL) {
        bool withoutCloud = false;

        String gs;
        String s3;

        uint32_t m = string_measure(arg, NULL);
        CONST_STRING(&gs, "gs://");
        CONST_STRING(&s3, "s3://");

        if (m <= gs.size)
            withoutCloud = true;
        else {
            if (string_cmp(arg, gs.size, gs.addr, gs.size, gs.size) == 0)
                self->provider = cloud_provider_gcp;
            else if (string_cmp(arg, s3.size, s3.addr, s3.size, s3.size)
                == 0)
            {
                self->provider = cloud_provider_aws;
            }
            else
                withoutCloud = true;
        }

        if (withoutCloud) {
            if (self->clouds == eAws) {
                self->provider = cloud_provider_aws;
                self->bucket = arg;
            }
            else if (self->clouds == eGcp) {
                self->provider = cloud_provider_gcp;
                self->bucket = arg;
            }
            else if (self->clouds == 0) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                PLOGERR(klogErr, (klogErr, rc,
                    "Invalid '$(N)' argument value '$(V)'. Examples of valid"
                    " buckets: 'gs://bucket-name' or 's3://bucket-name'",
                    "N=%s,V=%s", nm, arg));
            }
            else {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                PLOGERR(klogErr, (klogErr, rc,
                    "Invalid '$(N)' argument value.", CLOU_OPTION));
            }
        }
        else
            self->bucket = arg + gs.size;
    }

    return rc;
}

static rc_t CSArgsInitCloud(CSArgs * self, const char * nm) {
    const char * arg = NULL;
    rc_t rc = Args_Get(self->args, nm, &arg);

    if (rc == 0 && arg != NULL) {
        bool valid = true;
        const char * val = arg;
        self->clouds = 0;
        while (rc == 0 && valid && val != NULL && * val != '\0') {
            switch (val[0]) {
            case 'a':
                if (val[1] == 'w' && val[2] == 's') {
                    self->clouds |= eAws;
                    val += 3;
                    if (*val == ',')
                        ++val;
                    continue;
                }
                else {
                    valid = false;
                    break;
                }
            case 'g':
                if (val[1] == 'c' && val[2] == 'p') {
                    self->clouds |= eGcp;
                    val += 3;
                    if (*val == ',')
                        ++val;
                    continue;
                }
                else {
                    valid = false;
                    break;
                }
            default:
                valid = false;
                break;
            }
        }

        if (rc == 0 && !valid) {
            rc = RC(
                rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            PLOGERR(klogErr, (klogErr, rc,
                "Invalid '$(N)' argument value '$(V)'.",
                "N=%s,V=%s", nm, arg));
        }
    }

    return rc;
}

static rc_t CSArgsInit(CSArgs * self, int argc, char * argv[]) {
    rc_t rc = 0;

    const char * arg = NULL;

    assert(self);
    memset(self, 0, sizeof * self);

    rc = ArgsMakeAndHandle(&self->args, argc, argv, 1,
        OPTIONS, sizeof OPTIONS / sizeof OPTIONS[0]);

    if (rc == 0) {
        self->mode = eStatus;
        rc = Args_Get(self->args, FUNC_OPTION, &arg);
        if (rc == 0 && arg != NULL) {
            if (arg[0] == 'r')
                self->mode = eRestore;
            else if (arg[0] == 'q')
                self->mode = eQuotas;
        }
    }

    if (rc == 0)
        rc = CSArgsInitCloud(self, CLOU_OPTION);

    if (rc == 0 && self->mode == eRestore) {
        const char * nm = NULL;
        const char * missed = NULL;

        if (rc == 0) {
            if (rc == 0)
                rc = Args_Get(self->args, KART_OPTION, &self->jwtCart);

            if (rc == 0)
                rc = Args_Get(self->args, TYPE_OPTION, &self->fileType);

            if (rc == 0) {
                self->notifyByEmail = true;
                rc = Args_Get(self->args, NOTI_OPTION, &arg);
                if (rc == 0 && arg != NULL && arg[0] == 'n')
                    self->notifyByEmail = false;
            }

            if (rc == 0) {
                nm = PERM_OPTION;
                rc = Args_Get(self->args, nm, &self->permFile);
                if (rc == 0 && missed == NULL && self->permFile == NULL)
                    missed = nm;
            }

            if (rc == 0) {
                nm = BUCK_OPTION;
                rc = CSArgsInitBucket(self, nm);
                if (rc == 0 && missed == NULL && self->bucket == NULL)
                    missed = nm;
            }
        }

        if (rc == 0 && missed != NULL) {
            rc = RC(rcExe, rcArgv, rcProcessing, rcArgv, rcInsufficient);
            PLOGERR(klogErr, (klogErr, rc,
                "'--$(N)' is required to perform restore", "N=%s", missed));
        }
    }

    if (rc == 0)
        rc = ArgsParamCount(self->args, &self->params);

    if (rc != 0)
        CSArgsFini(self);

    return rc;
}

static rc_t CSPermFini(CSPerm * self) {
    assert(self);

    free(self->perm);
/** don't free(self->cookie); */

    memset(self, 0, sizeof * self);

    return 0;
}

static rc_t CSPermInit(CSPerm * self,
    const KDirectory * dir, const CSArgs * args)
{
    rc_t rc = 0;

    assert(self && args);
    memset(self, 0, sizeof * self);

    self->cookie = getenv("WGA_SESSION");
    assert(args->permFile);

    /* ignore special value '/' of permFile: use cookie instead */
    if (args->permFile[0] != '/' && args->permFile[1] != '\0') {
        uint64_t size = ~0;
        rc = KDirectory_ReadFile(dir, args->permFile, &self->perm, &size);
        if (rc != 0)
            PLOGERR(klogErr, (
                klogErr, rc, "Cannot read '$(f)'", "f=%s", args->permFile));
        else if (size == 0) {
            rc = RC(rcExe, rcArgv, rcProcessing, rcFile, rcEmpty);
            PLOGERR(klogErr, (
                klogErr, rc, "'$(f)' is empty", "f=%s", args->permFile));
        }
    }

    if (rc == 0 && self->perm == NULL && self->cookie == NULL)
        rc = RC(rcExe, rcArgv, rcProcessing, rcArgv, rcInsufficient);

    return rc;
}

static rc_t CSJwtCartFini(CSJwtCart * self) {
    assert(self);

    free(self->jwtCart);
    free(self->jwtCartWithEmail);

    memset(self, 0, sizeof * self);

    return 0;
}

static void CSJwtCartInit(CSJwtCart * self,
    CSPerm * perm, const CSArgs * args)
{
    assert(self && perm);

    memset(self, 0, sizeof * self);

    self->args = args;
    self->perm = perm;
}

static rc_t CSJwtCartMake1(CSJwtCart * self,
    const CSManager * mgr, const CSArgs * args, const CSPerm * perm)
{
    rc_t rc = 0;

    assert(self && self->args && mgr);

    if (self->args->jwtCart == NULL)
         /* call jwt_cart_builder.cgi to build jwtCart */
        rc = BuildJwtCart(mgr, args, perm, &self->jwtCart);
    else /* or use jwtCart from command line */
        rc = KDirectory_ReadFile(mgr->dir, self->args->jwtCart,
            &self->jwtCart, NULL);

    return rc;
}

/* call jwt_cart_builder.cgi to add email to jwtCart */
static rc_t CSJwtCartMake2(CSJwtCart * self,
    const CSManager * mgr, const CSPerm * perm)
{   return CSJwtCartBuildWithEmail(self, mgr, perm); }

static rc_t CSVMRequestFini(CSVMRequest * self) {
    assert(self);

    free(self->input);
    free(self->response);

    memset(self, 0, sizeof * self);

    return 0;
}

/* create Post Work Order request string */
static rc_t printOrderPost(char * dst, size_t bsize, size_t * num_writ,
    const char * jwtCart, const CSArgs * args)
{
    const char * prefix = "";
    const char * fileType = "";
    const char * suffix = "";

    assert(args);

    if (args->fileType != NULL) {
        prefix = ", \"filetype\": \"";
        fileType = args->fileType;
        suffix = "\" ";
    }

    return string_printf(dst, bsize, num_writ,
        "{ "
        " \"jwt_cart\": \"%s\", "
        " \"target_bucket\": \"%s\", "
        " \"notify_by_email\": \"%s\""
        "  %s%s%s"
        "}",
        jwtCart, args->bucket,
        args->notifyByEmail ? "yes" : "no",
        prefix, fileType, suffix);
}

/* create Status of Work Order request string */
static rc_t printOrderStatus(char * dst, size_t bsize, size_t * num_writ,
    const char * email, const char * workorder)
{
    return string_printf(dst, bsize, num_writ,
        "{ "
        " \"email\": \"%s\", "
        " \"workorder\": \"%s\" "
        "}",
        email, workorder);
}

/* create Get Monthly Stats for Requestor request string */
static rc_t printQuotas(char * dst, size_t bsize, size_t * num_writ,
    const char * email, const char * workorder)
{
    return string_printf(dst, bsize, num_writ,
        "{ "
        " \"requestor_email\": \"%s\", "
        " \"ticket_id\": \"%s\" "
        "}",
        email, workorder);
}

/* initialize Post request input JSON string */
static rc_t CSVMRequestInitPost(CSVMRequest * self,
    const CSArgs * args, const CSJwtCart * jwtCart)
{
    rc_t rc = 0;

    size_t num_writ = 0;
    char * input = NULL;

    assert(self && args && jwtCart && self->input == NULL);
    memset(self, 0, sizeof * self);

    self->provider = args->provider;

    printOrderPost(NULL, 0, &num_writ, jwtCart->jwtCartWithEmail, args);
    input = malloc(++num_writ);
    if (input == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    rc = printOrderPost(input, num_writ, NULL, jwtCart->jwtCartWithEmail, args);
    if (rc == 0)
        self->input = input;
    else
        free(input);

    return rc;
}

/* initialize Status request input JSON string */
static rc_t CSVMRequestInitStatus(CSVMRequest * self,
    const char * email, const char * workorder)
{
    rc_t rc = 0;

    size_t num_writ = 0;
    char * input = NULL;

    assert(self);
    memset(self, 0, sizeof * self);

    printOrderStatus(NULL, 0, &num_writ, email, workorder);
    input = malloc(++num_writ);
    if (input == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    rc = printOrderStatus(input, num_writ, NULL, email, workorder);
    if (rc == 0)
        self->input = input;
    else
        free(input);

    return rc;
}

/* initialize Quotas request input JSON string */
static rc_t CSVMRequestInitQuotas(CSVMRequest * self, CloudProviderId provider,
    const char * email, const char * workorder)
{
    rc_t rc = 0;

    size_t num_writ = 0;
    char * input = NULL;

    assert(self);
    memset(self, 0, sizeof * self);

    self->provider = provider;

    printQuotas(NULL, 0, &num_writ, email, workorder);
    input = malloc(++num_writ);
    if (input == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);

    rc = printQuotas(input, num_writ, NULL, email, workorder);
    if (rc == 0)
        self->input = input;
    else
        free(input);

    return rc;
}

/* Submit request to CSVM and read response */
static rc_t CSVMRequestSubmit(CSVMRequest * self,
    const CSManager * mgr, const char * url)
{
    rc_t rc = 0;

    char * e = getenv("NCBI_VDB_CSVM");

    assert(self && mgr);

    if (e != NULL && e[0] != '\0') /*dbg: use env.var. to get request response*/
        rc = KDirectory_ReadFile(mgr->dir, e, &self->response, NULL);

    else {
        KHttpRequest * req = NULL;
        KHttpResult * rslt = NULL;

        if (rc == 0)
            rc = KNSManagerMakeRequest(mgr->mgr, &req, 0x01010000, NULL, url);

        if (rc == 0)
            rc = KHttpRequestAddHeader(req, "Content-Type", "application/json");
        if (rc == 0)
            rc = KHttpRequestAddPostParam(req, self->input);

        if (rc == 0)
            rc = KClientHttpRequestPOST(req, &rslt);

        if (rc == 0)
            rc = KHttpResult_Read(rslt, &self->response);

        RELEASE(KHttpResult, rslt);
        RELEASE(KHttpRequest, req);
    }

    return rc;
}

/* Parse CSVM response (order submit|status) and update Orders */
static rc_t CSVMRequestProcess(CSVMRequest * self,
    struct CSVMOrder * order, CSVMOrders * orders)
{
    rc_t rc = 0;

    char error[99] = "";

    KJsonValue * root = NULL;
    const char * email = NULL;
    const char * workorder = NULL;
    const char * status = NULL;

    assert(self);

    rc = KJsonValueMake(&root, self->response, error, sizeof error);

    if (rc == 0) {
        const KJsonObject * node = KJsonValueToObject(root);
        const KJsonValue * value = KJsonObjectGetMember(node, "error");
        if (value != NULL) {
            const char * error = NULL;
            rc = KJsonGetString(value, &error);
            if (rc != 0)
                LOGERR(klogErr, rc, "Cannot process CSVM response");
            else {
                rc = RC(rcExe, rcQuery, rcExecuting, rcQuery, rcFailed);
                PLOGERR(klogErr, (klogErr, rc,
                    "Failed to Post CSVM Work Order: '$(error)'",
                    "error=%s", error));
            }
        }

        else {
            value = KJsonObjectGetMember(node, "email");
            if (rc == 0)
                rc = KJsonGetString(value, &email);

            value = KJsonObjectGetMember(node, "workorder");
            if (rc == 0)
                rc = KJsonGetString(value, &workorder);

            value = KJsonObjectGetMember(node, "status");
            node = KJsonValueToObject(value);

            value = KJsonObjectGetMember(node, "code");
            if (rc == 0)
                rc = KJsonGetString(value, &status);
        }
    }

    if (rc == 0) {
        if (order == NULL) {
            int number = 0;
            rc = CSVMOrdersAdd(orders, self->provider, email, workorder,
                &number);
            if (rc == 0)
                OUTMSG(("Submitted order %d '%s'\n", number, workorder));
        }
        else {
            int number = 0;
            if (CSVMOrderUpdate(order, status, &number))
                orders->dirty = true;
            OUTMSG(("%d %s: %s\n", number, workorder, status));
        }
    }

    KJsonValueWhack(root);

    return rc;
}

/* Parse total/remaining from Quotas JSON response */
static rc_t CSQuotaDataInit(CSQuotaData * self,
    const KJsonValue * data)
{
    rc_t rc = 0;

    const KJsonObject * node = KJsonValueToObject(data);

    const KJsonValue * value = KJsonObjectGetMember(node, "total");

    assert(self);

    if (rc == 0)
        rc = KJsonGetNumber(value, &self->total);

    value = KJsonObjectGetMember(node, "remaining");
    if (rc == 0)
        rc = KJsonGetNumber(value, &self->remaining);

    return rc;
}

/* Parse delivery/warmup from Quotas JSON response */
static rc_t CSQuotaInit(CSQuota * self, const KJsonValue * statistics) {
    rc_t rc = 0;

    const KJsonObject * node = KJsonValueToObject(statistics);

    const KJsonValue * quotas = KJsonObjectGetMember(node, "quotas");
    const KJsonValue * data = NULL;

    assert(self);

    node = KJsonValueToObject(quotas);

    data = KJsonObjectGetMember(node, "delivery");
    if (rc == 0)
        rc = CSQuotaDataInit(&self->delivery, data);

    data = KJsonObjectGetMember(node, "warmup");
    if (rc == 0)
        rc = CSQuotaDataInit(&self->warmup, data);

    return rc;
}

static rc_t CSQuotasInit(CSQuotas * self,
    CloudProviderId provider, const char * email)
{
    assert(self);

    memset(self, 0, sizeof *self);

    self->provider = provider;

    self->email = string_dup_measure(email, NULL);
    if (self->email == NULL)
        return RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    else
        return 0;
}

static rc_t CSQuotasFini(CSQuotas * self) {
    assert(self);

    free(self->email);

    memset(self, 0, sizeof *self);

    return 0;
}

/* Print Quota data to user */
static rc_t CSQuotaPrint(const CSQuota * self, const char * type) {
    assert(self);

    return KOutMsg("%s: delivery: %ld/%ld, warmup: %ld/%ld", type,
        self->delivery.remaining, self->delivery.total,
        self->warmup.remaining, self->warmup.total);
}

/* Print Quotas row to user */
static rc_t CSQuotasPrint(const CSQuotas * self) {
    rc_t rc = 0, r2 = 0;

    const char * c = "";

    assert(self);

    switch (self->provider) {
    case cloud_provider_aws: c = "AWS "; break;
    case cloud_provider_gcp: c = "GCP "; break;
    default: assert(0); break;
    }
    r2 = KOutMsg("%s", c); if (r2 != 0 && rc == 0)
        rc = r2;

    if (self->email != NULL) {
        r2 = KOutMsg("%s ", self->email); if (r2 != 0 && rc == 0)
            rc = r2;
    }

    r2 = CSQuotaPrint(&self->public, "public"); if (r2 != 0 && rc == 0)
        rc = r2;

    if (self->dbgap.warmup.total != 0) {
        r2 = KOutMsg("; "); if (r2 != 0 && rc == 0)
            rc = r2;
        r2 = CSQuotaPrint(&self->dbgap, "dbgap"); if (r2 != 0 && rc == 0)
            rc = r2;
    }

    r2 = KOutMsg("\n"); if (r2 != 0 && rc == 0)
        rc = r2;

    return rc;
}

/* Parse Quotas CSVM response ant print it to user */
static rc_t CSVMRequestProcessStats(CSVMRequest * self) {
    rc_t rc = 0;
    KJsonValue * root = NULL;

    assert(self);

    rc = KJsonValueMake(&root, self->response, NULL, 0);

    if (rc == 0) {
        const KJsonObject * node = KJsonValueToObject(root);
        const char * email = NULL;
        const char * error = NULL;
        const KJsonValue * value = KJsonObjectGetMember(node,
            "requestor_email");
        const KJsonValue * scopes = KJsonObjectGetMember(node, "scopes");
        const KJsonValue * statistics = KJsonObjectGetMember(node,
            "statistics");

        CSQuotas q;

        KJsonGetString(value, &email);

        value = KJsonObjectGetMember(node, "error");

        CSQuotasInit(&q, self->provider, email);

        if (value != NULL) {
            rc = KJsonGetString(value, &error);
            if (rc != 0)
                LOGERR(klogErr, rc, "Cannot process CSVM response");
            else {
                rc = RC(rcExe, rcQuery, rcExecuting, rcQuery, rcFailed);
                PLOGERR(klogErr, (klogErr, rc,
                    "Failed to Get CSVM Stats: '$(error)'", "error=%s", error));
            }
        }

        else if (statistics != NULL) {
            node = KJsonValueToObject(statistics);
            value = KJsonObjectGetMember(node, "error");
            if (value != NULL) {
                if (rc == 0)
                    rc = KJsonGetString(value, &error);
                if (rc != 0)
                    LOGERR(klogErr, rc, "Cannot process CSVM response");
                else {
                    rc = RC(rcExe, rcQuery, rcExecuting, rcQuery, rcFailed);
                    PLOGERR(klogErr, (klogErr, rc,
                        "Failed to Get CSVM Stats: '$(error)'",
                        "error=%s", error));
                }
            }
            else
                rc = CSQuotaInit(&q.public, statistics);
        }

        else if (scopes != NULL) {
            const KJsonObject * scopesNod = KJsonValueToObject(scopes);
            const KJsonValue * scope = KJsonObjectGetMember(scopesNod, "dbgap");
            
            node = KJsonValueToObject(scope);

            statistics = KJsonObjectGetMember(node, "statistics");
            if (rc == 0)
                rc = CSQuotaInit(&q.dbgap, statistics);

            scope = KJsonObjectGetMember(scopesNod, "public");
            node = KJsonValueToObject(scope);

            statistics = KJsonObjectGetMember(node, "statistics");

            if (rc == 0)
                rc = CSQuotaInit(&q.public, statistics);
        }

        if (rc == 0)
            rc = CSQuotasPrint(&q);

        FINI(CSQuotas, &q);
    }

    KJsonValueWhack(root);
    return rc;
}

rc_t CC UsageSummary(const char * progname) {
    return KOutMsg(
        "Usage:\n"
        " Submit restore order:\n"
        "  %s --" FUNC_OPTION " " FUNC_RESTORE " --" PERM_OPTION " <PATH> "
                                             "--" BUCK_OPTION " <bucket_name>\n"
        "              [--" KART_OPTION " <PATH>] [options] [<accession> ...]\n"
        "\n"
        " Print orders status:\n"
        "  %s --" FUNC_OPTION " " FUNC_STATUS "\n"
        "\n"
        " Print quotas information:\n"
        "  %s --" FUNC_OPTION " " FUNC_QUOTAS "\n"
        "\n"
        "Summary:\n"
        "  Manage restoration from cloud cold store\n"
        "\n", progname, progname, progname);
}

rc_t CC Usage(const Args * args) {
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    uint32_t i = 0;
    rc_t rc = 0, r2 = 0;

    if (args == NULL)
        rc = RC(rcExe, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram(args, &fullpath, &progname);

    if (rc != 0)
        progname = fullpath = UsageDefaultName;

    r2 = UsageSummary(progname); if (rc == 0 && r2 != 0)
        rc = r2;

    r2 = KOutMsg("OPTIONS:\n"); if (rc == 0 && r2 != 0)
        rc = r2;

    for (i = 0; i < sizeof OPTIONS / sizeof OPTIONS[0]; ++i) {
        const OptDef * opt = &OPTIONS[i];
        const char * alias = opt->aliases;
        const char * param = NULL;
        if (alias != NULL) {
            if (strcmp(alias, BUCK_ALIAS) == 0)
                param = "name";
            else if (strcmp(alias, CLOU_ALIAS) == 0)
                param = "value";
            else if (strcmp(alias, FUNC_ALIAS) == 0)
                param = FUNC_RESTORE "|" FUNC_STATUS "|" FUNC_QUOTAS;
            else if (strcmp(alias, NOTI_ALIAS) == 0)
                param = "yes|no";
            else if (strcmp(alias, TYPE_ALIAS) == 0)
                param = "value";
        }
        else if (strcmp(opt->name, KART_OPTION) == 0 ||
            strcmp(opt->name, PERM_OPTION) == 0)
        {
            param = "PATH";
        }
        HelpOptionLine(opt->aliases, opt->name, param, opt->help);
    }

    r2 = KOutMsg("\n"); if (rc == 0 && r2 != 0)
        rc = r2;

    HelpOptionsStandard();

    HelpVersion(fullpath, KAppVersion());

    return rc;
}

/* Unique ID of user in cloud to request quotas*/
typedef struct {
    BSTNode n;
    CloudProviderId provider; /* user can work with multiple clouds  */
    const char * email;       /* dirrerent emails can be used */
} BSTreeNode;
static void BSTreeNodeInit(BSTreeNode * self, CloudProviderId provider,
    const char * email)
{
    assert(self);
    memset(self, 0, sizeof * self);
    self->provider = provider;
    self->email = email;
}
static int64_t CC bstCmp(const void * item, const BSTNode * n) {
    const BSTreeNode * node = item;
    const BSTreeNode * sn = (const BSTreeNode*)n;
    assert(node && sn);
    if (node->provider != sn->provider)
        return node->provider - sn->provider;
    return strcmp(node->email, sn->email);
}
static int64_t CC bstSort(const BSTNode* item, const BSTNode* n)
{    return bstCmp(item, n); }
static void CC bstWhack(BSTNode * n, void * ignore)
{   free(n); }

/* Collect cloud user ID-s */
typedef struct {
    int clouds; /* remaining clouds that were requested but not processed */
    const CSManager * mgr;
    BSTree tree; /* cloud user ID-s */
    rc_t rc;
} CSOrdersQuotaData;
static void CSOrdersQuotaDataInit(CSOrdersQuotaData * self,
    const CSManager * mgr, int clouds)
{
    assert(self);
    memset(self, 0, sizeof * self);
    self->mgr = mgr;
    self->clouds = clouds;
}

/* Submit and process Quotas request */
static rc_t CSManagerQuota(const CSManager * self, CloudProviderId provider,
    const char * email, const char * workorder)
{
    CSVMRequest request;
    rc_t rc = CSVMRequestInitQuotas(&request,
        provider, email, workorder);

    assert(self);

    if (rc == 0) {
        const char * url = NULL;
        switch (provider) {
        case cloud_provider_aws: url = self->aws.stats; break;
        case cloud_provider_gcp: url = self->gcp.stats; break;
        default: assert(0); rc = 1; break;
        }

        if (rc == 0) {
            rc = CSVMRequestSubmit(&request, self, url);
            if (rc != 0)
                LOGERR(klogErr, rc, "Failed to make stats request");
        }

        if (rc == 0)
            rc = CSVMRequestProcessStats(&request);
    }

    FINI(CSVMRequest, &request);

    return rc;
}

/* Prepare, submit and process Quotas request for unique cloud user ID-s */
static void CC CSOrdersQuotaProcess(SLNode * n, void * data) {
    rc_t rc = 0;

    struct CSVMOrder * o = (struct CSVMOrder*)n;
    CSOrdersQuotaData * d = (CSOrdersQuotaData*)data;

    CloudProviderId provider = cloud_provider_none;
    const char * email = NULL;
    const char * workorder = NULL;
    BSTreeNode * sn = NULL;

    BSTreeNode node;

    assert(o && d);

    CSVMOrderData(o, NULL, &provider, &email, &workorder, NULL);

    BSTreeNodeInit(&node, provider, email);

    sn = (BSTreeNode*)BSTreeFind(&d->tree, &node, bstCmp);
    if (sn != NULL)
        return;

    sn = calloc(1, sizeof *sn);
    if (sn == NULL)
        d->rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    else {
        BSTreeNodeInit(sn, provider, email);
        BSTreeInsert(&d->tree, (BSTNode*)sn, bstSort);
    }

    rc = CSManagerQuota(d->mgr, provider, email, workorder);
    if (rc != 0)
        d->rc = rc;
    else {
        if (provider == cloud_provider_aws)
            d->clouds &= ~eAws;
        else if (provider == cloud_provider_gcp)
            d->clouds &= ~eGcp;
    }
}

/* Request default quotas (user email or workorder is now known) */
static rc_t CSManagerQuotasDefault(const CSManager * self, int clouds) {
    rc_t rc = 0, r2 = 0;

    while (clouds > 0) {
        CloudProviderId provider = cloud_provider_none;
        if (clouds & eAws) {
            provider = cloud_provider_aws;
            clouds &= ~eAws;
        }
        else if (clouds & eGcp) {
            provider = cloud_provider_gcp;
            clouds &= ~eGcp;
        }
        else {
            assert(0);
            return 1;
        }

        r2 = CSManagerQuota(self, provider, "@", "");
        if (r2 != 0 && rc == 0)
            rc = r2;
    }

    return rc;
}

/* Request CVVM for user quotas */
static rc_t CSManagerQuotas(const CSManager * self,
    CSVMOrders * orders, const CSArgs * args)
{
    rc_t rc = 0;

    assert(self && orders && args);

    if (SLListHead(&(orders->list)) == NULL)
        /* no order is found: request default quotas */
        if (args->clouds == 0)
            rc = KOutMsg("Specify cloud to query statistics\n");
        else
            rc = CSManagerQuotasDefault(self, args->clouds);
    else { /* orders exist */
        /* request quotas for unique cloud user ID-s */
        CSOrdersQuotaData d;
        CSOrdersQuotaDataInit(&d, self, args->clouds);
        SLListForEach(&(orders->list), CSOrdersQuotaProcess, &d);

        /* request default quotas
           for remaining clouds that were requested but not processed */
        rc = CSManagerQuotasDefault(self, d.clouds);
        if (d.rc != 0 && rc == 0)
            rc = d.rc;

        BSTreeWhack(&d.tree, bstWhack, NULL);
    }

    return rc;
}

typedef struct {
    rc_t rc;
    const CSManager * mgr;
    CSVMOrders * orders;
} CSVMOrdersStatusData;

static void CSVMOrdersStatusDataInit(CSVMOrdersStatusData * self,
    const CSManager * mgr, CSVMOrders * orders)
{
    assert(self);

    memset(self, 0, sizeof * self);

    self->mgr = mgr;
    self->orders = orders;
}

static void CC CSVMOrdersToSaveAdd(SLNode * n, void * data) {
    struct CSVMOrder * o = (struct CSVMOrder*)n;
    CSVMOrdersStatusData * d = (CSVMOrdersStatusData*)data;

    EStatus status = eSubmitted;
    CloudProviderId provider = cloud_provider_none;
    const char * email = NULL;
    const char * workorder = NULL;
    int number = 0;

    assert(o && d && d->mgr);

    CSVMOrderData(o, &status, &provider, &email, &workorder, &number);

    if (status == eSubmitted) {
        rc_t rc = 0;

        CSVMRequest request;

        const char * url = NULL;
        switch (provider) {
        case cloud_provider_aws: url = d->mgr->aws.status; break;
        case cloud_provider_gcp: url = d->mgr->gcp.status; break;
        default: assert(0); rc = 1; break;
        }

        rc = CSVMRequestInitStatus(&request, email, workorder);

        if (rc == 0) {
            rc = CSVMRequestSubmit(&request, d->mgr, url);
            if (rc != 0)
                LOGERR(klogErr, rc, "Failed to make status request");
        }

        if (rc == 0)
            rc = CSVMRequestProcess(&request, o, d->orders);

        FINI(CSVMRequest, &request);

        if (rc != 0 && d->rc == 0)
            d->rc = rc;
    }

    else if (status == eCompleted)
        OUTMSG(("%d %s: %s\n", number, workorder, "completed"));
}

/* Request CVVM for status of every known order */
static rc_t CSManagerStatus(const CSManager * self,
    CSVMOrders * orders)
{
    rc_t rc = 0;
    assert(self && orders);
    if (SLListHead(&(orders->list)) == NULL)
        ;/*OUTMSG(("No orders found\n"));*/
    else {
        CSVMOrdersStatusData otc;
        CSVMOrdersStatusDataInit(&otc, self, orders);
        SLListForEach(&(orders->list), CSVMOrdersToSaveAdd, &otc);
        rc = otc.rc;
    }
    return rc;
}

/* Send to CSVM a request to restore */
static rc_t CSManagerRestore(const CSManager * self,
    CSVMOrders * orders, const CSArgs * args)
{
    rc_t rc = 0;
    CSJwtCart jwtCart;
    CSVMRequest order;
    CSPerm perm;
    memset(&jwtCart, 0, sizeof jwtCart);
    memset(&order, 0, sizeof order);
    memset(&perm, 0, sizeof perm);
    assert(self && orders && args);
    if (rc == 0)
        rc = CSPermInit(&perm, self->dir, args);
    if (rc == 0)
        CSJwtCartInit(&jwtCart, &perm, args);
    if (rc == 0)
        rc = CSJwtCartMake1(&jwtCart, self, args, &perm);
    if (rc == 0)
        rc = CSJwtCartMake2(&jwtCart, self, &perm);
    if (rc == 0)
        rc = CSVMRequestInitPost(&order, args, &jwtCart);
    if (rc == 0) {
        const char * url = NULL;
        switch (args->provider) {
        case cloud_provider_aws: url = self->aws.post; break;
        case cloud_provider_gcp: url = self->gcp.post; break;
        default: assert(0); return 1;
        }
        rc = CSVMRequestSubmit(&order, self, url);
        if (rc != 0)
            LOGERR(klogErr, rc, "Failed to make order request");
    }
    if (rc == 0)
        rc = CSVMRequestProcess(&order, NULL, orders);
    FINI(CSVMRequest, &order);
    FINI(CSJwtCart, &jwtCart);
    FINI(CSPerm, &perm);
    return rc;
}

rc_t KMain(int argc, char * argv[]) {
    rc_t rc = 0, r2 = 0;

    CSArgs args;
    CSManager mgr;
    CSVMOrders orders;

    memset(&args, 0, sizeof args);
    memset(&mgr, 0, sizeof mgr);
    memset(&orders, 0, sizeof orders);

    if (rc == 0)
        rc = CSArgsInit(&args, argc, argv);

    if (rc == 0)
        rc = CSManagerInit(&mgr);

    if (rc == 0)
        rc = CSVMOrdersLoad(&orders, mgr.dir, mgr.cvsmOrders);

    if (rc == 0) {
        if (args.mode == eStatus)
            rc = CSManagerStatus(&mgr, &orders);
        else if (args.mode == eQuotas)
            rc = CSManagerQuotas(&mgr, &orders, &args);
        else if (args.mode == eRestore)
            rc = CSManagerRestore(&mgr, &orders, &args);
        else
            assert(0);
    }

    r2 = CSVMOrdersFini(&orders, mgr.dir, mgr.cvsmOrders);
    if (r2 != 0 && rc == 0)
        rc = r2;

    FINI(CSArgs, &args);
    FINI(CSManager, &mgr);

    return rc;
}
