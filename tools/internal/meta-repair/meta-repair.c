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
* =============================================================================$
*/

#include <kapp/args.h> /* ArgsWhack */
#include <kapp/log-xml.h> /* XMLLogger_Release */
#include <kapp/main.h> /* KAppVersion */
#include <kfs/directory.h> /* KDirectoryRelease */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* KOutMsg  */
#include <klib/printf.h> /* RC */
#include <klib/rc.h> /* RC */
#include <klib/status.h> /* STSMSG */
#include <vdb/database.h> /* VDatabaseRelease */
#include <vdb/manager.h> /* VDBManagerRelease */
#include <vdb/table.h> /* VTableRelease */
#include <stdio.h> /* snprintf */
#include <inttypes.h>
#include <time.h> /* time */

#define DR_OPTION "dryrun"
#define DR_ALIAS ""
static const char *DR_USAGE[] = {
    "Dry run the application: don't execute commands.", NULL };

#define IN_OPTION "input"
#define IN_ALIAS "i"
static const char *IN_USAGE[]
=  { "Input: required. Directory will be updated.", NULL };

#define FX_OPTION "fix-file"
#define FX_ALIAS "F"
static const char *FX_USAGE[] =  { "File with fix data.", NULL };

#define FS_OPTION "force"
#define FS_ALIAS "f"
static const char *FS_USAGE[]
    =  { "Forces an existing target to be overwritten.", NULL };

#define LG_OPTION "print-output"
#define LG_ALIAS "P"
static const char *LG_USAGE[] =  { "Print output of executed commands.", NULL };

#define MD_OPTION "mode"
#define MD_ALIAS "m"
static const char *MD_USAGE[] =  { "Run mode: required.", NULL };

#define OD_OPTION "output-directory"
#define OD_ALIAS  "O"
static const char* OD_USAGE[] = { "Create output as unkared directory.", NULL };

#define OU_OPTION "output-file"
#define OU_ALIAS  "o"
static const char *OU_USAGE[] = { "Create output as kar arhive.", NULL };

#define PR_OPTION "show_progress"
#define PR_ALIAS  "p"
//static const char *PR_USAGE[] = { "Show the percentage of completion.", NULL };

#define RP_OPTION "info"
#define RP_ALIAS  "I"
static const char *RP_USAGE[] = { "Print report for all fields "
    "examined for mismatch even if the old value is correct.", NULL };

#define TM_OPTION "temp"
#define TM_ALIAS  "t"
static const char *TM_USAGE[]
= { "Where to put temporary files. Default is current directory.", NULL };

#define UP_OPTION "update"
#define UP_ALIAS  "u"
static const char* UP_USAGE[] = { "Confirm update of input directory.", NULL };

#define VF_OPTION "verify"
#define VF_ALIAS  "C"
static const char* VF_USAGE[] = { "Run sra-stat on output targets.", NULL };

#define XM_OPTION "xml-log"
#define XM_ALIAS  "z"
//static const char *XM_USAGE[]    = { "Produce XML-formatted log file. Default is text.", NULL };

static OptDef OPTIONS[] = {
/*                                 max_count needs_value required
    { PR_OPTION, PR_ALIAS, NULL, PR_USAGE, 1,   false,    false }, */
    { MD_OPTION, MD_ALIAS, NULL, MD_USAGE, 1,   true ,    true  },
    { IN_OPTION, IN_ALIAS, NULL, IN_USAGE, 1,   true ,    true  },
    { FX_OPTION, FX_ALIAS, NULL, FX_USAGE, 1,   true ,    false },
    { OU_OPTION, OU_ALIAS, NULL, OU_USAGE, 1,   true ,    false },
    { OD_OPTION, OD_ALIAS, NULL, OD_USAGE, 1,   true ,    false },
    { FS_OPTION, FS_ALIAS, NULL, FS_USAGE, 1,   false,    false },
    { UP_OPTION, UP_ALIAS, NULL, UP_USAGE, 1,   false,    false },
    { TM_OPTION, TM_ALIAS, NULL, TM_USAGE, 1,   true ,    false },
    { VF_OPTION, VF_ALIAS, NULL, VF_USAGE, 1,   false,    false },
    { RP_OPTION, RP_ALIAS, NULL, RP_USAGE, 1,   false,    false },
    { LG_OPTION, LG_ALIAS, NULL, LG_USAGE, 1,   false,    false },
    { DR_OPTION, DR_ALIAS, NULL, DR_USAGE, 1,   false,    false },
};

char const *USAGE_PARAMS[] = {
   /* PR_OPTION    NULL                     , */
   /* MD_OPTION */ "check|fix|check-and-fix",
   /* IN_OPTION */ "acc|dir|file"           ,
   /* FX_OPTION */ "path"                   ,
   /* OU_OPTION */ "file"                   ,
   /* OD_OPTION */ "directory"              ,
   /* FS_OPTION */ NULL                     ,
   /* UP_OPTION */ NULL                     ,
   /* TM_OPTION */ "path"                   ,
   /* VF_OPTION */ NULL                     ,
   /* RP_OPTION */ NULL                     ,
   /* LG_OPTION */ NULL                     ,
   /* DR_OPTION */ NULL                     ,
};

enum {
    eCheck = 1,
    eFix   = 2,
};

typedef enum {
    eNotSet,
    eAcc,
    aAccAsDirSra,
    aAccAsDirSralite,
    eFile,
    eDir,
} EInType;

typedef struct {
    bool dry;
    bool force;
    bool update;
    bool verify;
    bool logKids;
    bool progress;
    bool report;
    unsigned mode;
    const char* outDir;
    const char *outFile;

    const char *tmp;
    bool tmpCreated;

    uint32_t verbose;
    const char *xml;
    const XMLLogger *logger;

    const char *in;
    EInType     inType;
    const char *inFile;
    const char *inDir;
    char        inFileBuffer[4096];
    char        inDirBuffer [4096];

    const char *acc;
    char        prefetched[4096];
    const char *unkared;

    const char *fixFile;
    char fix[9999];

    const char *nextFix;
    uint64_t actual;
    uint64_t expected;
    char name[4096];

    const char *tblOpt;
    KDirectory *dir;
} Repair;

static void RepairSetAcc(Repair *self) {
    char *last = NULL;
    assert(self);
    last = strrchr(self->in, '/');
    if (last == NULL)
        self->acc = self->in;
    else
        self->acc = last + 1;
}

static void RepairCheckAD
(Repair *self, const char *dir, const char *path, bool prefetched)
{
    KPathType type = 0;
    EInType detected = eNotSet;
    char *last = NULL;

    assert(self);

    last = strrchr(path, '/');
    if (last == NULL)
        self->acc = path;
    else {
        if (last[1] == '\0') {
            *last = '\0';
            if (last != path)
                last = strrchr(path, '/');
        }
        if (last == NULL)
            self->acc = path;
        else
            self->acc = last + 1;
    }

    STSMSG(2, ("Accession: %s\n", self->acc));

    if (prefetched) {
        int const n = snprintf(self->prefetched, sizeof(self->prefetched), "%s/%s", dir, self->acc);
        assert(n < sizeof(self->prefetched));
    }
    if (dir != NULL) {
        int const n = snprintf(self->inFileBuffer, sizeof(self->inFileBuffer), "%s/%s/%s.sra", dir, self->acc, self->acc);
        assert(n < sizeof(self->inFileBuffer));
    }
    else {
        int const n = snprintf(self->inFileBuffer, sizeof(self->inFileBuffer), "%s/%s.sra", path, self->acc);
        assert(n < sizeof(self->inFileBuffer));
    }
    
    type = KDirectoryPathType(self->dir, self->inFileBuffer);
    if ((type & ~kptAlias) == kptFile) {
        detected = self->inType = aAccAsDirSra;
        STSMSG(1, ("Input is an AD with sra.\n"));
    }
    else {
        strcat(self->inFileBuffer, "lite");
        type = KDirectoryPathType(self->dir, self->inFileBuffer);
        if ((type & ~kptAlias) == kptFile) {
            detected = self->inType = aAccAsDirSralite;
            STSMSG(1, ("Input is an AD with sralite.\n"));
        }
    }

    if (detected)
        self->inFile = self->inFileBuffer;
    else
        STSMSG(1, ("Input is a directory.\n"));
}

static rc_t RepairInit(Repair *self, int argc, char *argv[], Args **args) {
    rc_t rc = 0;

    assert(self && args);
    memset(self, 0, sizeof *self);

    rc = ArgsMakeAndHandle(args, argc, argv, 2, OPTIONS,
        sizeof OPTIONS / sizeof OPTIONS[0], XMLLogger_Args, XMLLogger_ArgsQty);
    if (rc != 0)
        return rc;

    STSMSG(1, ("Initilizing...\n"));

    rc = KDirectoryNativeDir(&self->dir);
    if (rc != 0)
        return rc;

    assert(*args);

    do {
        uint32_t pcount = 0;
        const char *val = NULL;
        KPathType type = 0;

/* FX_OPTION */
        rc = ArgsOptionCount(*args, FX_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            rc = ArgsOptionValue
                (*args, FX_OPTION, 0, (const void**)&self->fixFile);
        if (rc != 0)
            return rc;

/* DR_OPTION */
        rc = ArgsOptionCount(*args, DR_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->dry = true;

/* FS_OPTION */
        rc = ArgsOptionCount(*args, FS_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->force = true;

/* UP_OPTION */
        rc = ArgsOptionCount(*args, UP_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->update = true;

/* VF_OPTION */
        rc = ArgsOptionCount(*args, VF_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->verify = true;

/* LG_OPTION */
        rc = ArgsOptionCount(*args, LG_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->logKids = true;

/* PR_OPTION *
        rc = ArgsOptionCount(*args, PR_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->progress = true; */

/* RP_OPTION */
        rc = ArgsOptionCount(*args, RP_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            self->report = true;

/* OD_OPTION */
        rc = ArgsOptionCount(*args, OD_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            rc = ArgsOptionValue
                (*args, OD_OPTION, 0, (const void**)&self->outDir);
        if (rc != 0)
            return rc;

/* OU_OPTION */
        rc = ArgsOptionCount(*args, OU_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            rc = ArgsOptionValue
                (*args, OU_OPTION, 0, (const void**)&self->outFile);
        if (rc != 0)
            return rc;

/* TM_OPTION */
        rc = ArgsOptionCount(*args, TM_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0) {
            rc = ArgsOptionValue(*args, TM_OPTION, 0, (const void**)&self->tmp);
            if (rc != 0)
                return rc;
            if ((KDirectoryPathType(self->dir, self->tmp) & ~kptAlias)
                == kptNotFound)
            {
                rc = KDirectoryCreateDir(
                    self->dir, 0700, kcmParents, self->tmp);
                if (rc == 0)
                    self->tmpCreated = true;
                else
                    return rc;
            }
        }
        else self->tmp = ".";
        STSMSG(1, ("Work directory is '%s'.\n", self->tmp));

/* OPTION_VERBOSE */
        rc = ArgsOptionCount(*args, OPTION_VERBOSE, &self->verbose);
        if (rc != 0)
            return rc;
        if (rc != 0)
            return rc;

/* XM_OPTION */
        rc = ArgsOptionCount(*args, XM_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0) {
            rc = ArgsOptionValue(*args, XM_OPTION, 0, (const void**)&self->xml);
            if (rc == 0) {
                rc = XMLLogger_Make(&self->logger, NULL, *args);
                if (rc != 0) {
                    LOGERR(klogErr, rc, "cannot initialize XMLLogger");
                    rc = 0;
                }
            }
        }

/* MD_OPTION */
        rc = ArgsOptionCount(*args, MD_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            rc = ArgsOptionValue(*args, MD_OPTION, 0, (const void**)&val);
        if (rc != 0)
            return rc;
        if (strcmp(val, "fix") == 0)
            self->mode = eFix;
        else if (strcmp(val, "check") == 0)
            self->mode = eCheck;
        else if (strcmp(val, "check-and-fix") == 0)
            self->mode = eCheck | eFix;
        else {
            rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
            LOGERR(klogErr, rc, "Invalid '" MD_OPTION "' argument value");
            return rc;
        }

/* IN_OPTION */
        rc = ArgsOptionCount(*args, IN_OPTION, &pcount);
        if (rc != 0)
            return rc;
        if (pcount > 0)
            rc = ArgsOptionValue(*args, IN_OPTION, 0, (const void**)&self->in);
        if (rc != 0)
            return rc;
        STSMSG(2, ("Input: %s.\n", self->in));
        type = KDirectoryPathType(self->dir, self->in);
        switch (type & ~kptAlias) {
            case kptFile:
                self->inType = eFile;
                RepairSetAcc(self);
                STSMSG(1, ("Input is a file.\n"));
                break;
            case kptDir:
                self->inType = eDir;
                RepairCheckAD(self, NULL, self->in, false);
                break;
            default:
                if (strchr(self->in, '/') == NULL) {
                    self->inType = eAcc;
                    RepairSetAcc(self);
                    break;
                }
                else {
                    rc = RC(rcExe, rcFile, rcOpening, rcFile, rcNotFound);
                    PLOGERR(klogErr,
                        (klogErr, rc, "Failed to find $(F)", "F=%s", self->in));
                    return rc;
                }
        }

/* VERIFY */
        if (self->mode & eFix) {
            if (self->outFile == NULL && self->outDir == NULL) {
                if (self->inType == eDir) {
                    if (!self->update) {
                        rc = RC
                            (rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                        LOGERR(klogErr, rc,
                            "Confirm updating of existing directory by using --"
                            UP_OPTION);
                    }
                }
                else {
                    rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
                    LOGERR(klogErr, rc,
                        "--" OU_OPTION " or --" OD_OPTION " is required when "
                        "--" MD_OPTION " is fix or check-and-fix");
                }
            }
            else if (self->outDir != NULL && self->inType == eDir) {
                rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcExcessive);
                LOGERR(klogErr, rc, "--" OD_OPTION
                    " cannot be used when input is a directory. "
                    "It will be updated in place.");
            }
            else if
                (self->outFile != NULL && strcmp(self->in, self->outFile) == 0)
            {
                rc = RC(rcExe, rcArgv, rcParsing, rcName, rcDuplicate);
                LOGERR(klogErr, rc, "input and output have to be different");
            }
            else if
                (self->outDir != NULL && strcmp(self->in, self->outDir) == 0)
            {
                rc = RC(rcExe, rcArgv, rcParsing, rcName, rcDuplicate);
                LOGERR(klogErr, rc, "input and output have to be different");
            }
            else if (self->outFile != NULL
                && KDirectoryPathType(self->dir, self->outFile) != kptNotFound
                && !self->force)
            {
                rc = RC(rcExe, rcFile, rcCreating, rcFile, rcExists);
                LOGERR(klogErr, rc, "output file exists") ;
            }
            else if (self->outDir != NULL
                && KDirectoryPathType(self->dir, self->outDir) != kptNotFound
                && !self->force)
            {
                rc = RC(rcExe, rcFile, rcCreating, rcFile, rcExists);
                LOGERR(klogErr, rc, "output directory exists");
            }
        }

        {
            KLogLevel l = KLogLevelGet();
            rc_t r = ArgsOptionCount(*args, OPTION_LOG_LEVEL, &pcount);
            if (r == 0) {
                 if (pcount == 0) {
                     if (l < klogInfo)
                         KLogLevelSet(klogInfo);
                 }
                 else if (l < klogInfo)
                     LOGMSG(klogWarn, "log-level was set: "
                                      "mismatch will not be shown");
            }
        }
    } while (0);

    return rc;
}

static rc_t Find(const char *cmd, const char *error) {
    rc_t rc = 0;
    int i = 0;
    STSMSG(3, ("%s\n", cmd));
    i = system(cmd);
    if (i != 0) {
        rc = RC(rcExe, rcProcess, rcExecuting, rcProcess, rcFailed);
        LOGERR(klogErr, rc, error);
    }
    return rc;
}

static
rc_t RepairExecuteImpl(const Repair *self, const char *command, bool silent) {
    rc_t rc = 0;
#if 0
    int i = system(command);
    if (i != 0) {
        rc = RC(rcExe, rcProcess, rcExecuting, rcProcess, rcFailed);
        if (!silent)
            PLOGERR(klogErr,
                (klogErr, rc, "Cannot run $(C)", "C=%s", command));
    }
    return rc;
#endif
    int i = 0;
    char buf[9999] = "";
    size_t pos = 0;
    size_t part = 0;
    size_t remaining = sizeof buf - part;
    FILE *fp = 0;
    char cmd[4096] = "";
    KLogLevel lvl = KLogLevelGet();
    i = snprintf(cmd, sizeof(cmd), "%s -L%d", command, lvl);
    assert(i < sizeof(cmd));
    i = 0;
    if (self->verbose > 0) {
        int i = 0;
        strcat(cmd, " -");
        for (i = 0; i < self->verbose; ++i)
            strcat(cmd, "v");
    }
    strcat(cmd, " 2>&1");
    STSMSG(2, ("%s\n", cmd));
    if (self->dry)
        return rc;
    fp = popen(cmd, "r");
    if (fp == NULL) {
        rc = RC(rcExe, rcProcess, rcOpening, rcCmd, rcFailed);
        if (!silent)
            PLOGERR(klogErr, (
                klogErr, rc, "Cannot run $(C)", "C=%s", command));
    } else {
        while (true) {
            size_t read = 0;
            assert(remaining > 0);
            read = fread(&buf[pos], 1, remaining - 1 , fp);
            if (read > 0) {
                if (self->logKids)
                    PLOGMSG(klogInfo,
                        (klogInfo, "$(L)", "L=%.*s", (int)read, buf + pos));
                buf[read + pos] = '\0';
            }
            else
                break;
            assert(remaining >= pos);
            remaining -= read;
            pos += read;
            if (remaining < 2) {
                size_t n = sizeof buf / 2;
                memmove(buf, buf + n, pos - n + 1);
                pos = n;
                remaining = sizeof buf - n;
            }
        }
        i = pclose(fp);
        if (i / 256 != 0) {
            rc = RC(rcExe, rcProcess, rcExecuting, rcCmd, rcFailed);
            if (!silent)
                PLOGERR(klogErr, (
                    klogErr, rc, "Failure of executing $(C): $(L)", "C=%s,L=%s",
                    command, buf));
        }
    }
    return rc;
}

static rc_t RepairExecute(const Repair *self, const char *command)
{   return RepairExecuteImpl(self, command, false); }

static rc_t RepairExecuteSilent(const Repair *self, const char *command)
{   return RepairExecuteImpl(self, command, true); }

#define KAR      "kar"
#define KDBMETA  "kdbmeta"
#define LOCK     "vdb-lock"
#define PREFETCH "prefetch"
#define SRA_STAT "sra-stat"
#define REPAIR   "--repair-data"
#define UNLOCK   "vdb-unlock"

static rc_t RepairCheckBins(const Repair *self) {
    rc_t rc = 0;
    char command[256] = "";
    int n;
    STSMSG(1, ("Checking for required tools...\n"));
    assert(self);
    if (self->dry)
        return rc;
    if (self->mode & eCheck) {
/* grep is found *
        rc = Find("grep --help > /dev/null 2>&1", "Cannot find grep");
        if (rc != 0)
            return rc; */

/* sra-stat is found */
        n = snprintf(command, sizeof(command), SRA_STAT " -h > /dev/null 2>&1");
        assert(n < sizeof(command));
        rc = Find(command, "Cannot find " SRA_STAT);
        if (rc != 0)
            return rc;

/* sra-stat supports --repair-data */
        if (self->verbose < 4)
            n = snprintf(command, sizeof(command), SRA_STAT " " REPAIR " > /dev/null 2>&1");
        else
            n = snprintf(command, sizeof(command), SRA_STAT " " REPAIR " 2>&1 > /dev/null");
        assert(n < sizeof(command));
        rc = Find(command, SRA_STAT " does not support " REPAIR);
        if (rc != 0)
            return rc;
    }

    if (self->mode & eFix) {
/* kar is found */
        n = snprintf(command, sizeof(command), KAR " -h > /dev/null 2>&1");
        assert(n < sizeof(command));
        rc = Find(command, "Cannot find " KAR);
        if (rc != 0)
            return rc;

/* kdbmeta is found */
        n = snprintf(command, sizeof(command), KDBMETA " -h > /dev/null 2>&1");
        assert(n < sizeof(command));
        rc = Find(command, "Cannot find " KDBMETA);
        if (rc != 0)
            return rc;

 /* vdb-lock is found */
        n = snprintf(command, sizeof(command), LOCK " -h > /dev/null 2>&1");
        assert(n < sizeof(command));
        rc = Find(command, "Cannot find " LOCK);
        if (rc != 0)
            return rc;

 /* vdb-unlock is found */
        n = snprintf(command, sizeof(command), UNLOCK " -h > /dev/null 2>&1");
        assert(n < sizeof(command));
        rc = Find(command, "Cannot find " UNLOCK);
        if (rc != 0)
            return rc;

        if (self->mode & eCheck && self->inType == eAcc) {
/* prefetch  is found */
            n = snprintf(command, sizeof(command), PREFETCH " -h > /dev/null 2>&1");
            assert(n < sizeof(command));
            rc = Find(command, "Cannot find " PREFETCH);
            if (rc != 0)
                return rc;
        }
    }

    return rc;
}

static rc_t RepairGetFix(Repair *self) {
    rc_t rc = 0;

    assert(self);

    if (! (self->mode & eCheck)){
        FILE *fp = NULL;
        char cmd[999] = "";

        STSMSG(1, ("Looking for fix data...\n"));

        if (self->fixFile == NULL) {
            rc_t rc = RC(rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            LOGERR(klogErr, rc,
                "--" FX_OPTION " has to be provided if --mode == fix");
            return rc;
        }

        if (self->fixFile[0] == '-' && self->fixFile[1] == '\0') {
            STSMSG(2, ("Reading fix data from %s.\n", "stdin"));
            fp = stdin;
        } else {
            STSMSG(2, ("Reading fix data from %s.\n", self->fixFile));
            fp = fopen(self->fixFile, "r");
        }

        if (fp == NULL) {
            rc = RC(rcExe, rcFile, rcOpening, rcError, rcUnexpected);
            LOGERR(klogErr, rc, "Failed to open --" FX_OPTION);
        } else {
            self->fix[0] = '\0';
            while (fgets(cmd, sizeof cmd, fp) != NULL) {
                const char *m = strstr(cmd, "{MISMATCH} ");
                if (m != NULL)
                    strcat(self->fix, m);
            }
            if (fp != stdin)
                fclose(fp);
        }
    }

    return rc;
}

static rc_t RepairCheck(Repair* self, const char* what, bool* succeed) {
    rc_t rc = 0;
    int i = 0;
    FILE* fp = NULL;
    char cmd[1024] = "";
    const char* key = "";
    const char* val = "";
    const char* info = "";
    const char* progress = "";

    if (succeed != NULL)
        *succeed = true;

    if (!(self->mode & eCheck))
        return rc;

    STSMSG(1, ("Running " SRA_STAT " of %s for metadata mismatch...\n", what));

    if (self->report)
        info = "--info";
    if (self->progress)
        progress = "-p";
#if 0
    if (self->xml != NULL) {
        key = "-z";
        val = self->xml;
    }
#endif

    i = snprintf(cmd, sizeof(cmd), SRA_STAT " " REPAIR " %s %s %s%s -L5 %s 2>&1",
        info, progress, key, val, what);
    assert(i < sizeof(cmd));
    i = 0;
    STSMSG(2, ("%s\n", cmd));

    if (succeed == NULL)
        self->fix[0] = '\0';

    if (self->dry)
        return rc;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        rc = RC(rcExe, rcProcess, rcOpening, rcCmd, rcFailed);
        LOGERR(klogErr, rc, "Failed to run " SRA_STAT " " REPAIR);
        return rc;
    }

    while (fgets(cmd, sizeof cmd, fp) != NULL) {
        const char* e = strstr(cmd, "Examined ");
        const char* m = strstr(cmd, "{MISMATCH} ");
        if (self->logKids)
            LOGMSG(klogInfo, cmd);
        else if (e != NULL && self->report)
            LOGMSG(klogInfo, cmd);
        if (m != NULL) {
            if (succeed != NULL)
                *succeed = false;
            else
                strcat(self->fix, m);
            if (!self->logKids)
                LOGMSG(klogInfo, m);
        }
    }

    i = pclose(fp);

    if (i / 256 != 0) {
        bool failed = false;
        if (succeed == NULL) {
            if (self->fix[0] == '\0')
                failed = true;
        }
        else {
            if (*succeed)
                failed = true;
            *succeed = false;
        }
        if (failed) {
            rc = RC(rcExe, rcProcess, rcExecuting, rcCmd, rcFailed);
            PLOGERR(klogErr, (klogErr, rc,
                "Failure of executing $(C): $(L)", "C=%s,L=%s", cmd, cmd));
        }
    }

    return rc;
}

static rc_t RepairPrefetch(Repair *self) {
    rc_t rc = 0;
    int n;
    char command[4115] = "";

    STSMSG(1, ("Input not found: prefetching...\n"));

    assert(self && self->inType == eAcc);

    n = snprintf(command, sizeof(command), PREFETCH " %s -O%s", self->in, self->tmp);
    assert(n < sizeof(command));

    rc = RepairExecute(self, command);
    if (rc == 0)
        RepairCheckAD(self, self->tmp, self->in, true);

    return rc;
}

static rc_t RepairUnlock(const Repair *self, const char *path) {
    KPathType type = 0;

    assert(self);

    type = KDirectoryPathType(self->dir, "%s/lock", path);

    if ((type & ~kptAlias) == kptFile) {
        char command[4123] = "";
        int const n = snprintf(command, sizeof(command), UNLOCK " %s", path);
        assert(n < sizeof(command));
        STSMSG(1, ("Unlocking...\n"));
        return RepairExecute(self, command);
    }

    return 0;
}

static rc_t RepairUnkar(Repair *self) {
    rc_t rc = 0;
    char command[8192];
    int n;

    assert(self);

    if (self->outDir == NULL) {
        n = snprintf(self->inDirBuffer, sizeof(self->inDirBuffer), "%s/%s.tmp", self->tmp, self->acc);
        assert(n < sizeof(self->inDirBuffer));
    }
    else {
        KPathType type = KDirectoryPathType(self->dir, self->outDir);
        if (type != kptNotFound) {
            STSMSG(1, ("Unlocking %s...\n", self->outDir));
            n = snprintf(command, sizeof(command), UNLOCK " %s", self->outDir);
            assert(n < sizeof(command));
            rc = RepairExecute(self, command);

            if (rc == 0) {
                STSMSG(2, ("Removing %s...\n", self->outDir));
                rc = KDirectoryRemove(self->dir, true, self->outDir);
                if (rc != 0) {
                    PLOGERR(klogErr, (klogErr, rc,
                        "...failed to remove $(F)", "F=%s", self->outDir));
                    return rc;
                }
            }
        }
        n = snprintf(self->inDirBuffer, sizeof(self->inDirBuffer), "%s", self->outDir);
        assert(n < sizeof(self->inDirBuffer));
    }

    if (KDirectoryPathType(self->dir, self->inDirBuffer) == kptDir)
        RepairUnlock(self, self->inDirBuffer);

    STSMSG(1, ("Unkaring...\n"));

    n = snprintf(command, sizeof(command), KAR " -f --extract %s -d %s",
        self->inFile, self->inDirBuffer);
    assert(n < sizeof(command));

    rc = RepairExecute(self, command);
    if (rc != 0)
        return rc;

    self->unkared = self->inDir = self->inDirBuffer;

    return rc;
}

static rc_t RepairCheckTable(Repair *self) {
    rc_t rc = 0, r2 = 0;
    const struct VDBManager *m = NULL;
    const VTable *t = NULL;

    STSMSG(1, ("Checking for -TSEQUENCE...\n"));

    assert(self);

    self->tblOpt = "";

    rc = VDBManagerMakeRead(&m, NULL);

    if (rc == 0) {
        rc = VDBManagerOpenTableRead(m, &t, NULL, "%s", self->inDir);
        if (rc == 0)
            STSMSG(2, ("...will update the run table directly\n"));
        else if (GetRCObject(rc) == (enum RCObject)rcTable
                    && GetRCState(rc) == rcIncorrect)
        {
            const VDatabase *d = NULL;
            rc = VDBManagerOpenDBRead(m, &d, NULL, self->inDir);
            if (rc == 0) {
                self->tblOpt = "-TSEQUENCE";
                STSMSG(2, ("...will update SEQUENCE table of the run\n"));
            }
            else
                LOGERR(klogErr, rc, "Cannot find metadata in run");
            r2 = VDatabaseRelease(d);
            if (r2 != 0 && rc == 0)
                rc = r2;
        }
    }

    r2 = VTableRelease(t);
    if (r2 != 0 && rc == 0)
        rc = r2;

    r2 = VDBManagerRelease(m);
    if (r2 != 0 && rc == 0)
        rc = r2;

    return rc;
}

static rc_t RepairFindNextFix(Repair *self) {
    rc_t rc = 0;

    assert(self);

    self->name[0] = '\0';
    self->actual = self->expected = 0;
    if (self->nextFix != NULL) {
        int n = sscanf(self->nextFix,
                       "{MISMATCH} Name:%s Expected:%" PRIu64 ", Actual:%" PRIu64 ".",
                       self->name, &self->expected, &self->actual);
        if (n != 3)
            return RC(rcExe, rcData, rcProcessing, rcData, rcUnexpected);
        else {
            size_t l = strlen(self->name);
            if (l > 0 && self->name[l - 1] == ',')
                self->name[l - 1] = '\0';
        }

        if (self->nextFix != NULL && self->nextFix[0] != '\0') {
            ++self->nextFix;
            self->nextFix = strstr(self->nextFix, "{MISMATCH}");
        }
    }

    return rc;
}

static rc_t RepairParseFix(Repair *self) {
    rc_t rc = 0;
    const char *m = NULL;

    STSMSG(1, ("Parsing fix data...\n"));

    assert(self);

    m = strstr(self->fix, "{MISMATCH} Case:");
    if (m != NULL) {
        rc = RC(rcExe, rcData, rcProcessing, rcCondition, rcUnexpected);
        PLOGERR(klogErr,
            (klogErr, rc, "Unknown mismatch case: $(C)", "C=%s", m));
        return rc;
    }

    self->nextFix = strstr(self->fix, "{MISMATCH}");
    return RepairFindNextFix(self);
}

static rc_t RepairDoFix(const Repair *self) {
    rc_t rc = 0;
    char hex[17] = "";
    char val[33] = "";
    int i = 0, j = 0;
    char command[8192] = "";

    STSMSG(1, ("Running " KDBMETA " to update metadata...\n"));

    assert(self);

    i = snprintf(hex, sizeof(hex), "%016" PRIX64 "", self->expected);
    assert(i < sizeof(hex));
    for (i = j = 0; i < 8; ++i) {
        val[j++] = '\\';
        val[j++] = 'x';
        val[j++] = hex[(7 - i) * 2    ];
        val[j++] = hex[(7 - i) * 2 + 1];
    }
    val[j] = '\0';

    i = snprintf(command, sizeof(command), KDBMETA " %s %s %s=\"%s\"",
            self->inDir, self->tblOpt, self->name, val);
    assert(i < sizeof(command));
    rc = RepairExecute(self, command);
    if (rc == 0)
        PLOGMSG(klogInfo, (klogInfo, "Updated $(F) from $(O) to $(N)",
            "F=%s,O=%lu,N=%lu", self->name, self->actual, self->expected));

    return rc;
}

static rc_t RepaitUpdateHistory(const Repair *self) {
    rc_t rc = 0;
    time_t rawtime = 0;
    struct tm *info = NULL;
    char datestring[80] = "";
    const char obj1[] = "SOFTWARE/meta-repair@date";
    const char obj2[] = "SOFTWARE/meta-repair@name=meta-repair";
    char obj3[80] = "";
    char command[999] = "";

    STSMSG(1, ("Running " KDBMETA " to update SOFTWARE history metadata...\n"));

    assert(self);

    time(&rawtime);
    info = gmtime(&rawtime);
    strftime(datestring, sizeof datestring, "%b %e %Y", info);

    rc = string_printf(obj3, sizeof obj3, NULL,
        "SOFTWARE/meta-repair@vers=%V", KAppVersion());

    if (rc == 0)
        rc = string_printf(command, sizeof command, NULL,
            KDBMETA " %s \"%s=%s\" %s %s",
            self->inDir, obj1, datestring, obj2, obj3);

    if (rc == 0)
        rc = RepairExecute(self, command);

    return rc;
}

static rc_t RepairKar(const Repair *self) {
    rc_t rc = 0;
    char command[999] = "";

    STSMSG(1, ("Karing...\n"));

    assert(self);

    rc = string_printf(command, sizeof command, NULL,
                    KAR " %s --create %s -d %s",
        self->force ? "-f" : "", self->outFile, self->inDir);

    if (rc == 0)
        rc = RepairExecute(self, command);

    return rc;
}

static rc_t RepaitFinish(Repair *self) {
    rc_t rc = RepaitUpdateHistory(self);

    assert(self);

    if (rc == 0) {
        char command[4123] = "";
        int const n = snprintf(command, sizeof(command), LOCK " %s", self->inDir);
        assert(n < sizeof(command));
        STSMSG(1, ("Locking...\n"));
        rc = RepairExecute(self, command);
    }

    if (rc == 0 && self->verify) {
        bool succeed = false;
        if (self->outDir != NULL)
            rc = RepairCheck(self, self->outDir, &succeed);
        else if (self->outFile == NULL)
            rc = RepairCheck(self, self->in, &succeed);
        else
            succeed = true;
        if (!succeed)
            rc = RC(rcExe, rcData, rcValidating, rcData, rcUnequal);
    }

    if (rc == 0 && self->outFile != NULL) {
        rc = RepairKar(self);
        if (rc == 0 && self->verify) {
            bool succeed = false;
            rc = RepairCheck(self, self->outFile, &succeed);
            if (!succeed)
                rc = RC(rcExe, rcData, rcValidating, rcData, rcUnequal);
        }
    }

    return rc;
}

static rc_t RepairNextFix(Repair *self)
{   return RepairFindNextFix(self); }

static rc_t RepairFix(Repair *self) {
    rc_t rc = 0;

    assert(self);

    if (! (self->mode & eFix))
        return rc;

    rc = RepairParseFix(self);
    if (rc != 0)
        return rc;

    if (self->name[0] == '\0')
        return rc; /* nothing to fix */

    if (self->inType == eAcc) {
        rc = RepairPrefetch(self);
        if (rc != 0)
            return rc;
    }

    switch (self->inType) { /* no breaks here */
        case eFile:
            self->inFile = self->in;
        case eAcc:
        case aAccAsDirSra:
        case aAccAsDirSralite:
            rc = RepairUnkar(self);
            if (rc != 0)
                return rc;
        default:
            break;
    }

    switch (self->inType) { /* no breaks here */
        case aAccAsDirSra:
        case aAccAsDirSralite:
        case eDir:
            if (self->inDir == NULL)
                self->inDir = self->in;
        default:
            break;
    }

    rc = RepairUnlock(self, self->inDir);
    if (rc != 0)
        return rc;

    rc = RepairCheckTable(self);

    while (rc == 0 && self->name[0] != '\0') {
        rc = RepairDoFix(self);
        if (rc == 0)
            rc = RepairNextFix(self);
    }

    if (rc == 0)
        rc = RepaitFinish(self);

    return rc;
}

const char UsageDefaultName [] = "meta-repair";

rc_t CC UsageSummary (const char *progname) {return 0;}

rc_t CC Usage (const Args *args){
    char const *progname = UsageDefaultName;
    char const *fullpath = UsageDefaultName;
    rc_t rc = 0;
    unsigned i = 0;

    rc = ArgsProgram(args, &fullpath, &progname);

    UsageSummary(progname);

    KOutMsg( "Options:\n" );
    for (i = 0; i < sizeof OPTIONS / sizeof OPTIONS[0]; ++i)
        HelpOptionLine(OPTIONS[i].aliases, OPTIONS[i].name,
                          USAGE_PARAMS[i], OPTIONS[i].help);
    XMLLogger_Usage();
    KOutMsg( "\n" );
    HelpOptionsStandard();


    KOutMsg( "\n\n" );
    KOutMsg( "Use examples:\n" );

    KOutMsg("\n");
    KOutMsg( "  To generate a fix file named 'fix.file' for an input archive "
             "'SRR1215779':\n"
             "     if exit code is 0 - fix is not needed;\n"
             "     if exit code is not 0 - fix is needed or failure.\n"
             "    If fix is needed "
                    "- returned rc is 'data unequal while validating data'.\n"
             "\n"
             "  $ meta-repair --mode check -i SRR1215779 > fix.file 2>&1\n\n");
    KOutMsg( "\n" );

    KOutMsg( "  To replace an archive named 'SRR1215779'"
             " with fix file named 'fix.file' as file 'SRR1215779.sra'"
             "\n\n"
             "  $ meta-repair --mode fix -i SRR1215779 -F fix.file "
                                             "-o SRR1215779.sra\n\n");
    KOutMsg( "\n" );

    KOutMsg( "  You can pipe input file as:"
             "\n\n"
             "  $ cat fix.file | meta-repair --mode fix -i SRR1215779 -F - "
                                                     "-o SRR1215779.sra\n\n");

    KOutMsg("\n");

    KOutMsg( "Use --info to report (field, old value, correct value) "
    "for all fields examined even if the old value is correct.\n\n");

    KOutMsg( "Binaries needed:\n"
    "- for check mode:\n"
    "  -  sra-stat\n"
    "- for fix mode:\n"
    "  -  kar\n"
    "  -  kdbmeta\n"
    "  -  prefetch\n"
    "  -  vdb-lock\n"
    "  -  vdb-unlock\n");

    HelpVersion(fullpath, KAppVersion());

    return rc;
}

MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    rc_t rc = 0, r2 = 0;
    Args *args=NULL;
    Repair pars;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    r2 = RepairInit(&pars, argc, argv, &args);
    if (rc == 0 && r2 != 0)
        rc = r2;
    if (rc == 0)
        rc = RepairCheckBins(&pars);
    if (rc == 0 && pars.mode & eFix)
        rc = RepairGetFix(&pars);
    if (rc == 0 && pars.mode & eCheck)
        rc = RepairCheck(&pars, pars.in, NULL);
    if (rc == 0 && pars.mode & eFix)
        rc = RepairFix(&pars);
    STSMSG(1, ("Finishing...\n"));
    if (pars.prefetched[0] != '\0') {
        STSMSG(2, ("Removing %s...\n", pars.prefetched));
        r2 = KDirectoryRemove(pars.dir, true, pars.prefetched);
        if (rc == 0 && r2 != 0)
            rc = r2;
    }
    if (rc == 0 && pars.unkared != NULL && pars.outDir == NULL) {
        char command[4123] = "";
        int const n = snprintf(command, sizeof(command), UNLOCK " %s", pars.unkared);
        assert(n < sizeof(command));
        STSMSG(2, ("Unlocking...\n"));
        rc = RepairExecute(&pars, command);

        if (rc == 0) {
            STSMSG(2, ("Removing %s...\n", pars.unkared));
            r2 = KDirectoryRemove(pars.dir, true, pars.unkared);
            if (rc == 0 && r2 != 0)
                rc = r2;
        }
    }
    if (pars.tmpCreated) {
        STSMSG(2, ("Removing %s...\n", pars.tmp));
        r2 = KDirectoryRemove(pars.dir, false, pars.tmp);
        if (rc == 0 && r2 != 0)
            rc = r2;
    }
    r2 = KDirectoryRelease(pars.dir);
    if (rc == 0 && r2 != 0)
        rc = r2;

    if (pars.mode & eFix) {
        if (pars.fix[0] == '\0')
            if (rc == 0)
                STSMSG(1, ("Succeed: fix was not needed.\n"));
            else
                STSMSG(1, ("Failed.\n"));
        else
            if (rc == 0)
                STSMSG(1, ("Succeed: fix was done.\n"));
            else
                STSMSG(1, ("Failed: fix was needed.\n"));
    } else if (pars.mode & eCheck) {
        if (pars.fix[0] != '\0')
            if (rc == 0)
                STSMSG(1, ("Succeed: fix is needed.\n"));
            else
                STSMSG(1, ("Failed: fix is needed.\n"));
        else
            if (rc == 0)
                STSMSG(1, ("Done: fix is not needed.\n"));
            else
                STSMSG(1, ("Failed.\n"));
    }

    XMLLogger_Release(pars.logger);
    r2 = ArgsWhack(args);
    if (rc == 0 && r2 != 0)
        rc = r2;

    if (rc == 0 && pars.fix[0] != '\0' && !(pars.mode & eFix))
        rc = RC(rcExe, rcData, rcValidating, rcData, rcUnequal);

    return VdbTerminate( rc );
}
