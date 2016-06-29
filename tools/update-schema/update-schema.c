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

#include <kapp/main.h>
#include <vdb/vdb-priv.h> /* VTableStoreSchema */
#include <vdb/manager.h> /* VDBManager */
#include <vdb/table.h>
#include <vdb/schema.h> /* VSchema */
#include <kdb/meta.h>
#include <klib/out.h> /* OUTMSG */
#include <klib/log.h> /* LOGERR */
#include <klib/rc.h>
#include <insdc/sra.h> /* SRA_PLATFORM_ ... */
#include <assert.h>
#include <string.h> /* memset */

#define DISP_RC(rc, msg) (void)((rc == 0) ? 0 : LOGERR(klogInt, rc, msg))
#define DISP_RC2(rc, name, msg) (void)((rc == 0) ? 0 : \
    PLOGERR(klogInt, (klogInt,rc, "$(msg): $(name)","msg=%s,name=%s",msg,name)))
#define DESTRUCT(type, obj) do { rc_t rc2 = type##Release(obj); \
    if (rc2 && !rc) { rc = rc2; } obj = NULL; } while (false)

typedef struct CmdArgs {
    const char* run;
    const char* schema;
    const char* sPlatform;
    uint8_t platform;
    Args* args;
    uint32_t count;
    uint32_t i;
} CmdArgs;
typedef struct Db {
    VDBManager* mgr;
    VSchema* schema;
    VTable* tbl;
    KMetadata* meta;
    bool frozen;
    bool updatedPlatf;
    bool deletedSchema;
} Db;
#define OPTION_PLATF "platform"
#define ALIAS_PLATF  "p"
static const char* platf_usage[] = { "Platform: LS454 | ILLUMINA | HELICOS",
    " | ABI_SOLID | COMPLETE_GENOMICS", " | PACBIO_SMRT | ION_TORRENT | CAPILLARY | OXFORD_NANOPORE", NULL };
#define OPTION_SCHEMA "schema"
#define ALIAS_SCHEMA  "s"
static const char* schema_usage[] = { "Schema", NULL };
#define OPTION_RUN   "out"
#define ALIAS_RUN    "o"
static const char* run_usage   [] = { "Run", NULL };
OptDef Options[] =  {
      { OPTION_RUN   , ALIAS_RUN   , NULL, run_usage   , 1, true , true }
    , { OPTION_SCHEMA, ALIAS_SCHEMA, NULL, schema_usage, 1, true , true }
    , { OPTION_PLATF , ALIAS_PLATF , NULL, platf_usage , 1, true , true }
};

rc_t CC UsageSummary (const char * progname)
{    
    OUTMSG (("\n"
             "Usage:\n"
             "  %s -o <run> [ -s <schema-table-or-db-spec>\n"
             "      [ -p <platform-name> ] ] <schema-file> ...\n"
             "\n"
             "Summary:\n"
             "  Update table schema\n"
             "\n", progname));
    return 0;
}

const char UsageDefaultName[] = "check-blob-size";
#define def_name UsageDefaultName

rc_t CC Usage(const Args* args)
{
    rc_t rc = 0;
    const char* progname = def_name;
    const char* fullpath = def_name;
    assert(args);
    rc = ArgsProgram(args, &fullpath, &progname);
    if (rc)
    {    progname = fullpath = def_name; }
    UsageSummary(progname);
    OUTMSG(("\nOptions\n"));
    HelpOptionLine(ALIAS_RUN, OPTION_RUN   , "run"              , run_usage);
    HelpOptionLine
               (ALIAS_SCHEMA, OPTION_SCHEMA, "schema-table-spec", schema_usage);
    HelpOptionLine(ALIAS_PLATF, OPTION_PLATF, "platform-name"   , platf_usage);
    HelpOptionsStandard();
    HelpVersion(fullpath, KAppVersion());
    return rc;
}

/* MINIUSAGE(def_name) */
static rc_t CmdArgsInit(int argc, char** argv, CmdArgs* cmdArgs)
{
    rc_t rc = 0;
    Args* args = NULL;
    assert(argv && cmdArgs);

    memset(cmdArgs, 0, sizeof *cmdArgs);

    rc = ArgsMakeAndHandle(&cmdArgs->args,
        argc, argv, 1, Options, sizeof Options / sizeof Options[0]);
    DISP_RC(rc, "while calling ArgsMakeAndHandle");
    args = cmdArgs->args;

    while (rc == 0) {
        uint32_t pcount = 0;

        /* run parameter */
        rc = ArgsOptionCount(args, OPTION_RUN, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing run name");
            break;
        }
        if (pcount < 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc, "Missing run parameter");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many run parameters");
            break;
        }
        rc = ArgsOptionValue (args, OPTION_RUN, 0, (const void **)&cmdArgs->run);
        if (rc) {
            LOGERR(klogErr, rc, "Failure retrieving run name");
            break;
        }

        /* platform parameter */
        rc = ArgsOptionCount(args, OPTION_PLATF, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing platform");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many platform parameters");
            break;
        }
        assert(SRA_PLATFORM_UNDEFINED == 0);
        if (pcount == 1) {
            uint8_t p = SRA_PLATFORM_UNDEFINED;
            const char* arg = NULL;
            rc = ArgsOptionValue(args, OPTION_PLATF, 0, (const void **)&arg);
            if (rc) {
                LOGERR(klogErr, rc, "Failure retrieving platform");
                break;
            }
            if      (!strcmp(arg, "454"))
            {   p = SRA_PLATFORM_454; }
            else if (!strcmp(arg, "LS454"))
            {   p = SRA_PLATFORM_454; }
            else if (!strcmp(arg, "ILLUMINA"))
            {   p = SRA_PLATFORM_ILLUMINA; }
            else if (!strcmp(arg, "HELICOS"))
            {   p = SRA_PLATFORM_HELICOS; }
            else if (!strcmp(arg, "ABI_SOLID"))
            {   p = SRA_PLATFORM_ABSOLID; }
            else if (!strcmp(arg, "COMPLETE_GENOMICS"))
            {   p = SRA_PLATFORM_COMPLETE_GENOMICS; }
            else if (!strcmp(arg, "PACBIO_SMRT"))
            {   p = SRA_PLATFORM_PACBIO_SMRT; }
            else if (!strcmp(arg, "ION_TORRENT"))
            {   p = SRA_PLATFORM_ION_TORRENT; }
            else if (!strcmp(arg, "CAPILLARY"))
            {   p = SRA_PLATFORM_CAPILLARY; }
            else if (!strcmp(arg, "OXFORD_NANOPORE"))
            {   p = SRA_PLATFORM_OXFORD_NANOPORE; }
            else {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                PLOGERR(klogInt, (klogInt, rc, "Invalid platform: $(name)",
                    "name=%s", arg));
                break;
            }
            cmdArgs->platform = p;
            cmdArgs->sPlatform = arg;
        }

        /* schema parameter */
        rc = ArgsOptionCount(args, OPTION_SCHEMA, &pcount);
        if (rc) {
            LOGERR(klogErr, rc, "Failure parsing schema");
            break;
        }
        if (pcount > 1) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcAmbiguous);
            LOGERR(klogErr, rc, "Too many schema parameters");
            break;
        }
        else if (pcount == 1) {
            rc = ArgsOptionValue(args, OPTION_SCHEMA, 0, (const void **)&cmdArgs->schema);
            if (rc) {
                LOGERR(klogErr, rc, "Failure retrieving schema");
                break;
            }
        }
        else if (/* pcount < 0 && */ cmdArgs->platform) {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR
                (klogErr, rc, "Schema is required when platform is specified");
        }

        break;
    }

    if (rc == 0) {
        do {
            rc = ArgsParamCount(args, &cmdArgs->count);
            if (rc) {
                DISP_RC(rc, "while calling ArgsParamCount");
                break;
            }
            if (cmdArgs->count < 1) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                LOGERR(klogErr, rc, "Missing schema parameter");
                break;
            }
        } while (false);
    }

    if (rc != 0)
    {   MiniUsage(cmdArgs->args); }

    return rc;
}

static rc_t CmdArgsGetNextParam(CmdArgs* args, const char** param)
{
    rc_t rc = 0;
    assert(args && param);
    *param = NULL;
    if (args->i >= args->count) /* no more parameters to give :( */
    {   return rc; }
    rc = ArgsParamValue(args->args, args->i, (const void **)param);
    if (rc) {
        PLOGERR(klogInt, (klogInt, rc,
            "while calling ArgsParamValue($(i))", "i=%d", args->i));
    }
    ++args->i;
    return rc;
}

static rc_t DbConstruct(Db* db, CmdArgs* args) {
    rc_t rc = 0;
    assert(db && args);
    memset(db, 0, sizeof *db);
    if (rc == 0) {
        rc = VDBManagerMakeUpdate(&db->mgr, NULL);
        DISP_RC(rc, "While calling VDBManagerMakeUpdate");
    }
    if (rc == 0) {
        rc = VDBManagerMakeSchema(db->mgr, &db->schema);
        DISP_RC(rc, "While calling VDBManagerMakeSchema");
    }
    while (rc == 0) {
        const char* param = NULL;
        rc = CmdArgsGetNextParam(args, &param);
        if (param == NULL)
        {   break; }
        rc = VSchemaParseFile(db->schema, "%s", param);
        DISP_RC2(rc, param, "While calling VSchemaParseFile");
    }
    if (rc == 0) {
        rc =
            VDBManagerOpenTableUpdate(db->mgr, &db->tbl, db->schema, args->run);
        DISP_RC2(rc, args->run, "While calling VDBManagerOpenTableUpdate");
    }
    if (rc == 0) {
        rc = VTableOpenMetadataUpdate(db->tbl, &db->meta);
        DISP_RC(rc, "While calling VTableOpenMetadataUpdate");
    }
    return rc;
}

static rc_t DbBeginTran(Db* self)
{
    rc_t rc = 0;
    assert(self);
    if (rc == 0) {
        rc = KMetadataFreeze(self->meta);
        DISP_RC(rc, "While calling KMetadataFreeze");
        if (rc == 0)
        {   self->frozen = true; }
    }
    return rc;
}

static rc_t DbUpdatePlatform(Db* self, const CmdArgs* args)
{
    rc_t rc = 0;
    bool toUpdate = false;
    const char path[] = "col/PLATFORM/row";
    const KMDataNode* rNode = NULL;
    KMDataNode* uNode = NULL;
    assert(self && args);
    if (rc == 0) {
        rc = KMetadataOpenNodeRead(self->meta, &rNode, "%s", path);
        DISP_RC2(rc, path, "While calling KMetadataOpenNodeRead");
    }
    if (rc == 0) {
        uint8_t p = SRA_PLATFORM_UNDEFINED;
        rc = KMDataNodeReadB8(rNode, &p);
        DISP_RC2(rc, path, "While reading KMetaNode");
        if (rc == 0 && p == args->platform) {
            PLOGMSG(klogWarn, (klogWarn,
                "PLATFORM is set already to $(PLATFORM)",
                "PLATFORM=%s", args->sPlatform));
        }
        else { toUpdate = true; }
    }
    DESTRUCT(KMDataNode, rNode);
    if (toUpdate) {
        if (rc == 0) {
            rc = KMetadataOpenNodeUpdate(self->meta, &uNode, "%s", path);
            DISP_RC2(rc, path, "While calling KMetadataOpenNodeUpdate");
        }
        if (rc == 0) {
            rc = KMDataNodeWriteB8(uNode, &args->platform);
            DISP_RC2(rc, path, "While writing KMetaNode");
            if (rc == 0)
            {   self->updatedPlatf = true; }
        }
    }
    DESTRUCT(KMDataNode, uNode);
    return rc;
}

static rc_t DbDeleteSchema(Db* self)
{
    rc_t rc = 0;
    KMDataNode* uNode = NULL;
    const char path[] = "schema";
    assert(self);
    if (rc == 0) {
        rc = KMetadataOpenNodeUpdate(self->meta, &uNode, "%s", path);
        DISP_RC2(rc, path, "While calling KMetadataOpenNodeUpdate");
    }
    if (rc == 0) {
        rc = KMDataNodeDropAll(uNode);
        DISP_RC2(rc, path, "While calling KMDataNodeDropAll");
        if (rc == 0)
        {   self->deletedSchema = true; }
    }
    DESTRUCT(KMDataNode, uNode);
    return rc;
}

static rc_t DbCloseTbl(Db* self)
{
    rc_t rc = 0;
    assert(self);
    DESTRUCT(KMetadata, self->meta);
    DESTRUCT(VTable, self->tbl);
    return rc;
}

static rc_t DbDestruct(Db* self) {
    rc_t rc = 0;
    assert(self);
    DESTRUCT(KMetadata, self->meta);
    DESTRUCT(VTable, self->tbl);
    DESTRUCT(VSchema, self->schema);
    DESTRUCT(VDBManager, self->mgr);
    return rc;
}

static rc_t DbStoreSchema(Db* self, const CmdArgs* args)
{
    rc_t rc = 0;
    bool metaUpdated = false;
    assert(self && args);
    if (self->updatedPlatf || self->deletedSchema)
    {   metaUpdated = true; }
    if (metaUpdated) {
        if (rc == 0) {
            rc = DbCloseTbl(self);
            DISP_RC(rc, "While calling VTableRelease");
        }
        if (rc == 0) {
            rc = VDBManagerCreateTable(self->mgr,
                                       &self->tbl, self->schema, args->schema, kcmOpen, "%s", args->run);
            DISP_RC2(rc, args->run, "While calling VDBManagerCreateTable");
        }
    }
    else {
        rc = VTableStoreSchema(self->tbl);
        DISP_RC(rc, "While calling VTableStoreSchema");
    }
    return rc;
}

static rc_t DbRollbackTran(Db* self)
{
    rc_t rc = 0;
    assert(self);
    return rc;
}

rc_t CC KMain(int argc, char* argv[])
{
    rc_t rc = 0;
    CmdArgs args;
    Db db;
    memset(&db, 0, sizeof db);
    if (rc == 0)
    {   rc = CmdArgsInit(argc, argv, &args); }
    if (rc == 0)
    {   rc = DbConstruct(&db, &args); }
    if (rc == 0) {
        rc = DbBeginTran(&db);
        if (rc == 0 && args.platform)
        {   rc = DbUpdatePlatform(&db, &args); }
        if (rc == 0 && args.schema)
        {   rc = DbDeleteSchema(&db); }
        if (rc == 0)
        {   rc = DbStoreSchema(&db, &args); }
        if (rc != 0)
        {   DbRollbackTran(&db); }
    }
    {
        rc_t rc2 = DbDestruct(&db);
        if (rc2 != 0 && rc == 0)
        {   rc = rc2; }
    }
    return rc;
}

/* EOF */
