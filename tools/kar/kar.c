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
#define USE_SKEY_MD5_FIX 1

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/container.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/arc.h>
#include <kfs/tar.h>
#include <kfs/toc.h>
#include <kfs/sra.h>
#include <kfs/md5.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <sysalloc.h>

#include <kapp/main.h>
#include <kapp/args.h>


#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
/* #include <stdarg.h> */
#include <string.h>
#include <assert.h>
#include <time.h>


static
KDirectory * kdir;


#define OPTION_CREATE    "create"
#define OPTION_TEST      "test"
#define OPTION_LIST      "list"
#define OPTION_EXTRACT   "extract"
#define OPTION_FORCE     "force"
#define OPTION_LONGLIST  "long-list"
#define OPTION_DIRECTORY "directory"
#define OPTION_ALIGN     "align"
#define OPTION_MD5       "md5"

#define ALIAS_CREATE    "c"
#define ALIAS_TEST      "t"
#define ALIAS_LIST      ""
#define ALIAS_EXTRACT   "x"
#define ALIAS_FORCE     "f"
#define ALIAS_LONGLIST  "l"
#define ALIAS_DIRECTORY "d"
#define ALIAS_ALIGN     "a"

static const char * create_usage[] = { "Create a new archive.", NULL };
static const char * extract_usage[] = { "Extract the contents of an archive into a directory.", NULL };
static const char * test_usage[] = { "Check the structural validity of an archive",
                                     "Optionally listing its contents", NULL };
static const char * list_usage[] = { "Check the structural validity of an archive",
                                     "Optionally listing its contents", NULL };
static const char * directory_usage[] = 
{ "The next token on the command line is the",
  "name of the directory to extract to or create",
  "from", NULL };
static const char * force_usage[] =
{ "(no parameter) this will cause the extract or",
  "create to over-write existing files unless",
  "they are write-protected.  Without this",
  "option the program will fail if the archive",
  "already exists for a create or the target",
  "directory exists for an extract", NULL };
static const char * align_usage[] =
 { "Forces the alignment of files in create",
   "mode putting the first byte of included",
   "files at <alignment boundaries",
   "alignment: 1|2|4|8",
   "(default=4)", NULL };
static const char * longlist_usage[] =
{ "more information will be given on each file",
  "in test/list mode.", NULL };
static const char * md5_usage[] = { "create md5sum-compatible checksum file", NULL };


OptDef Options[] = 
{
    { OPTION_CREATE,    ALIAS_CREATE,    NULL, create_usage, 1, true,  false },
    { OPTION_TEST,      ALIAS_TEST,      NULL, test_usage, 1, true,  false },
    { OPTION_LIST,      ALIAS_LIST,      NULL, list_usage, 1, true,  false },
    { OPTION_EXTRACT,   ALIAS_EXTRACT,   NULL, extract_usage, 1, true,  false },
    { OPTION_FORCE,     ALIAS_FORCE,     NULL, force_usage, 0, false, false },
    { OPTION_LONGLIST,  ALIAS_LONGLIST,  NULL, longlist_usage, 0, false, false },
    { OPTION_DIRECTORY, ALIAS_DIRECTORY, NULL, directory_usage, 1, true,  false },
    { OPTION_ALIGN,     ALIAS_ALIGN,     NULL, align_usage, 1, true,  false },
    { OPTION_MD5,       NULL,            NULL, md5_usage, 0, false,  false }
};

const char UsageDefaultName[] = "kar";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [OPTIONS] -%s|--%s <Archive> -%s|--%s <Directory> [Filter ...]\n"
                    "  %s [OPTIONS] -%s|--%s <Archive> -%s|--%s <Directory>\n"
                    "  %s [OPTIONS] -%s|--%s|--%s <Archive>\n"
                    "\n"
                    "Summary:\n"
                    "  Create, extract from, or test an archive.\n"
                    "\n",
                    progname, ALIAS_CREATE, OPTION_CREATE, ALIAS_DIRECTORY, OPTION_DIRECTORY,
                    progname, ALIAS_EXTRACT, OPTION_EXTRACT, ALIAS_DIRECTORY, OPTION_DIRECTORY,
                    progname, ALIAS_TEST, OPTION_TEST, OPTION_LIST);
}

rc_t CC Usage (const Args * args)
{
    static const char archive[] = "archive";
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    OUTMSG (("Archive Command:\n"
	     "  All of these options require the next token on the command line to be\n"
	     "  the name of the archive\n\n"));

    KOutMsg ("Options:\n");

    HelpOptionLine (ALIAS_CREATE, OPTION_CREATE, archive, create_usage);
    HelpOptionLine (ALIAS_EXTRACT, OPTION_EXTRACT, archive, extract_usage);
    HelpOptionLine (ALIAS_TEST, OPTION_TEST, archive, test_usage);
    OUTMSG (("\n"
             "Archive:\n"
             "  Path to a file that will/does hold the archive of other files.\n"
             "  This can be a full or relative path.\n"
             "\n"
             "Directory:\n"
	     "  Required for create or extract command, ignored for test command.\n"
             "  This can be a full or relative path.\n"
             "\n"
             "Filters:\n"
	     "  When present these act as include filters.\n"
	     "  Any file name will be included in the extracted files, created archive\n"
	     "  or test operation listing\n"
	     "  Any directory will be included as well as its contents\n"
             "\n"
             "Options:\n"));
    HelpOptionLine (ALIAS_DIRECTORY, OPTION_DIRECTORY, "Directory", directory_usage);
    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);
    HelpOptionLine (ALIAS_ALIGN, OPTION_ALIGN, "alignment", align_usage);
    HelpOptionLine (ALIAS_LONGLIST, OPTION_LONGLIST, NULL, longlist_usage);
    HelpOptionLine (NULL, OPTION_MD5, NULL, md5_usage);

    HelpOptionsStandard ();

    OUTMSG (("\n"
             "Use examples:"
             "\n"
             "  To create an archive named 'example.sra' that contains the same\n"
             "  contents as a subdirectory 'example' of the current directory\n"
             "\n"
             "  $ %s --%s example.sra --%s example\n",
             progname, OPTION_CREATE, OPTION_DIRECTORY));

    OUTMSG (("\n"
             "  To replace an existing archive named 'example.sra' with another that contains\n"
             "  the same contents as a subdirectory 'example' of the current directory\n"
             "\n"
             "  $ %s -%s -%s example.sra -%s example\n",
             progname, ALIAS_FORCE, ALIAS_CREATE, ALIAS_DIRECTORY));

    OUTMSG (("\n"
             "  To examine in detail the contents of an archive named 'example.sra'\n"
             "\n"
             "  $ %s --%s --%s example.sra\n",
             progname, OPTION_LONGLIST, OPTION_TEST));

    OUTMSG (("\n"
             "  To extract the files from an archive named 'example.sra' into\n"
             "  a subdirectory 'example' of the current directory.\n"
             "  NOTE: all extracted files will be read only.\n"
             "\n"
             "  $ %s --%s example.sra --%s example\n",
             progname, OPTION_EXTRACT, OPTION_DIRECTORY));


    HelpVersion (fullpath, KAppVersion());

    return rc;
}


static
KDirectory * kdir;

static
bool long_list;
static
bool force;
static
bool md5sum;

static
KSRAFileAlignment alignment;


static BSTree pnames;
typedef struct pnamesNode
{
    BSTNode      dad;
    const char * name;
    size_t name_len;
} pnamesNode;

static rc_t pnamesNodeMake (pnamesNode ** pself, const char * path)
{
    size_t len;
    pnamesNode * self;

    len = strlen (path);
    self = malloc (sizeof (pnamesNode));
    if (self == NULL)
    {
        rc_t rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
        PLOGERR (klogErr, (klogErr, rc, "out of memory allocating node for $(P)", PLOG_S(P), path));
        return rc;
    }
    self->name = path;
    self->name_len = len;
    *pself = self;
    return 0;
}

static int64_t CC pnameFindCmp (const void * _item, const BSTNode *_n)
{
    const char * item = _item;
    const pnamesNode * n = (const pnamesNode *)_n;
    size_t ilen = strlen (item);
    size_t nlen = n->name_len;

    /*     printf("compare <%s(%u),%s(%u)>\n", item, ilen, n->name, nlen); */

    if (((ilen+1) == nlen) && (n->name[ilen] == '/'))
    {
        /* 	printf("strncmp ilen\n"); */
        return (strncmp (item, n->name, ilen));
    }
    if ((item[nlen] == '/') || (item[nlen-1] == '/'))
    {
        /* 	printf("strncmp nlen\n"); */
        return (strncmp (item, n->name, nlen));
    }
    /*     printf("strcmp\n"); */
    return (strcmp (item, n->name));
}

static int64_t CC pnamesInsertCmp (const BSTNode * _item, const BSTNode *_n)
{
    const pnamesNode * item = (const pnamesNode *)_item;
    const pnamesNode * n = (const pnamesNode *)_n;
    return strcmp (item->name, n->name);
}

static rc_t pnamesInsert (const char * path)
{
    pnamesNode * node;
    rc_t rc;

    rc = pnamesNodeMake (&node, path);
/*     if (rc != 0) */
/* 	    PLOGERR (klogDebug2, (klogDebug2, rc, "Unable to process filter parameter $(P)", */
/*                  PLOG_S(P), path)); */
    if (rc == 0)
    {
        rc = BSTreeInsert ( &pnames, &node->dad, pnamesInsertCmp );
/*         if (rc != 0) */
/*             PLOGERR (klogDebug2, (klogDebug2, rc, "Unable to store filter parameter $(P)", */
/*                      PLOG_S(P), path)); */
/*         else */
/*             PLOGMSG (klogDebug2, "Processed filter parameter $(P)", */
/*                      PLOG_S(P), path); */
    }
    return rc;
}
static bool pnamesUsePath (const char * path)
{
    bool ret;

    STSMSG (4, ("Use path called %s", path));

    STSMSG (4, ("Depth %u ", BSTreeDepth(&pnames, false) ));
    if (BSTreeDepth(&pnames, false) == 0)
    {
/*         PLOGMSG (klogDebug9, "pnamesUsePath use $(P) by default", PLOG_S(P), path); */
        ret = true;
    }
    else
    {
        ret = ( BSTreeFind (&pnames, path, pnameFindCmp ) != NULL);
/*         if (ret) */
/*             PLOGMSG (klogDebug9, "pnamesUsePath use $(P)", PLOG_S(P), path); */
/*         else */
/*             PLOGMSG (klogDebug9, "pnamesUsePath don't use $(P)", PLOG_S(P), path); */
    }
    STSMSG (4, ("Use? %u ", ret));
    return ret;
}

static
bool CC pnamesFilter ( const KDirectory * dir, const char * path, void * data )
{
    return pnamesUsePath ( path );
}

static KSRAFileAlignment get_alignment (const char * str)
{
    if (str == NULL)
        return sraAlign4Byte;
    else
    {
        KSRAFileAlignment a;
	uint32_t	val;
        if (sscanf (str, "%u", &val) == 1)
        {
	    a = val;
            if (a == (a & ~(a-1)))
                return a; /* if a is 0 its valid for the test but 0 is
                           * sraAlignInvalid so it gets the right result */
        }
    }
    return sraAlignInvalid;
}

static
int64_t CC sort_cmp (const void ** l, const void ** r, void * data)
{
/*  KDirectory * d; */
    uint64_t lz, rz;
    rc_t rc;

/*    d = data; */
/*     lz = l; */
/*     rz = r; */

    rc = KDirectoryFileSize (data, &lz, *l);
    if (rc == 0)
    {
        rc = KDirectoryFileSize (data, &rz, *r);
        if (rc == 0)
        {
            int64_t zdiff;

            zdiff = lz - rz;
            if (zdiff != 0)
                return (zdiff < 0) ? -1 : 1;
            else
            {
                size_t lsz = string_size (*l);
                size_t rsz = string_size (*r);

                return string_cmp (*l, lsz, *r, rsz, lsz+1);
            }
        }
    }
    return 0; /* dunno just leave it... */
}


static
rc_t CC sort_size_then_rel_path (const KDirectory * d, struct Vector * v)
{
    VectorReorder (v, sort_cmp, (void*)d);

    return 0;
}





static
rc_t open_dir_as_archive (const char * path, const KFile ** file)
{
    rc_t rc;
    KPathType kpt;
    const KDirectory * d;

    assert (path != NULL);
    assert (path[0] != '\0');
    assert (file != NULL);

    rc = 0;
    kpt = KDirectoryPathType (kdir, path);
    switch (kpt)
    {
    case kptFile:
        STSMSG (1, ("Opening as archive %s\n", path));

        rc = KDirectoryOpenSraArchiveRead (kdir, &d, false, path);
        if (rc != 0)
            rc = KDirectoryOpenTarArchiveRead (kdir, &d, false, path);
        if (rc != 0)
            rc = RC (rcExe, rcParam, rcOpening, rcDirectory, rcInvalid);
        break;
    default:
        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcInvalid);
        PLOGERR (klogFatal, (klogFatal, rc, "Parameter [$(P)] must be a directory", PLOG_S(P), path));
        return rc;
    case kptDir:
/*         KOutMsg ("%s: opening dir\n",__func__); */
        rc = KDirectoryVOpenDirRead (kdir, &d, false, path, NULL);
    }
    if (rc == 0)
    {
/*         KOutMsg ("%s: dir to archive\n",__func__); */
        rc = KDirectoryOpenTocFileRead (d, file, alignment, pnamesFilter, NULL, sort_size_then_rel_path );
        KDirectoryRelease (d);
    }
    return rc;
}
static
rc_t open_file_as_dir (const char * path, const KDirectory ** dir)
{
    rc_t rc;
    KPathType kpt;
    const KDirectory * d;

    rc = 0;
    kpt = KDirectoryPathType (kdir, path);
    switch (kpt & ~kptAlias)
    {
    case kptNotFound:
        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcNotFound);
        PLOGERR (klogFatal, (klogFatal, rc, "Archive [$(A)] must exist", PLOG_S(A), path));
        return rc;
    case kptDir:
    default:
        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcInvalid);
        PLOGERR (klogFatal, (klogFatal, rc, "Archive [$(A)] must be a file", PLOG_S(A), path));
        return rc;
    case kptFile:
        break;
    }
    if (rc == 0)
    {
        STSMSG (1, ("Opening %s\n", path));
        rc = KDirectoryOpenArcDirRead (kdir, &d, /*chroot*/false, path, 
                                       tocKFile, KArcParseSRA,
                                       /*filter*/pnamesFilter, /*filt.param*/NULL);
        *dir = (rc == 0) ? d : NULL;
    }
    return rc;
}
static
rc_t open_out_dir (const char * path, KDirectory ** dir)
{
    rc_t rc;
    KCreateMode mode;

    rc = 0;
    mode = force ? kcmParents|kcmInit : kcmParents|kcmCreate;
    STSMSG (1, ("Creating %s\n", path));
    rc = KDirectoryVCreateDir (kdir, 0777, mode, path, NULL);
    if (rc != 0)
    {
        PLOGERR (klogFatal, (klogFatal, rc, "failure to create or clear output directory $(D)", PLOG_S(D), path));
    }
    else
    {
        rc = KDirectoryVOpenDirUpdate (kdir, dir, false, path, NULL);
        if (rc != 0)
        {
            PLOGERR (klogFatal, (klogFatal, rc, "failure to open output directory $(D)", PLOG_S(D), path));
        }
    }
    return rc;
}
/* this could be modified to accept stdout */
static
rc_t open_out_file (const char * path, KFile ** fout)
{
    rc_t rc;
    KPathType kpt;
    KCreateMode mode;
    rc = 0;
    mode = kcmParents|kcmCreate;

    kpt = KDirectoryPathType (kdir, path);
    switch (kpt & ~kptAlias)
    {
    case kptDir:
        /* -----
         * this version will fail but perhaps with directory
         * take last facet <F> from input path and use that as a name
         * to be <F>.sra in this directory
         */
    default:
        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcInvalid);
        PLOGERR (klogFatal, (klogFatal, rc, "archive [$(A)] must be a file", PLOG_S(A), path));
        break;
    case kptFile:
        if (force)
            mode = kcmParents|kcmInit;
        else
        {
            rc = RC (rcExe, rcNoTarg, rcParsing, rcFile, rcExists);
            PLOGERR (klogFatal, (klogFatal, rc, "archive [$(A)] already exists", PLOG_S(A), path));
        }
        break;
    case kptNotFound:
        break;
    }
    if (rc == 0)
    {
        rc = KDirectoryCreateFile (kdir, fout, false, 0664,
                                   mode, "%s", path);
        if (rc)
            PLOGERR (klogFatal, (klogFatal, rc, "unable to create archive [$(A)]", PLOG_S(A), path));
        else if ( md5sum )
        {
            KFile *md5_f;

            /* create the *.md5 file to hold md5sum-compatible checksum */
            rc = KDirectoryCreateFile ( kdir, &md5_f, false, 0664, mode, "%s.md5", path );
            if ( rc )
                PLOGERR (klogFatal, (klogFatal, rc, "unable to create md5 file [$(A).md5]", PLOG_S(A), path));
            else
            {
                KMD5SumFmt *fmt;
                    
                /* create md5 formatter to write to md5_f */
                rc = KMD5SumFmtMakeUpdate ( &fmt, md5_f );
                if ( rc )
                    LOGERR (klogErr, rc, "failed to make KMD5SumFmt");
                else
                {
                    KMD5File *kmd5_f;

                    size_t size = string_size ( path );
                    const char *fname = string_rchr ( path, size, '/' );
                    if ( fname ++ == NULL )
                        fname = path;

                    /* KMD5SumFmtMakeUpdate() took over ownership of "md5_f" */
                    md5_f = NULL;

                    /* create a file that knows how to calculate md5 as data
                       are written-through to archive, and then write digest
                       result to fmt, using "fname" as description. */
                    rc = KMD5FileMakeWrite ( &kmd5_f, * fout, fmt, fname );
                    KMD5SumFmtRelease ( fmt );
                    if ( rc )
                        LOGERR (klogErr, rc, "failed to make KMD5File");
                    else
                    {
                        /* success */
                        *fout = KMD5FileToKFile ( kmd5_f );
                        return 0;
                    }
                }

                /* error cleanup */
                KFileRelease ( md5_f );
            }

            /* error cleanup */
            KFileRelease ( * fout );
            * fout = NULL;
        }
    }
    return rc;
}


static
void remove_out_file (const char * path)
{
    KDirectoryRemove (kdir, true, path);
}

static
rc_t copy_file (const KFile * fin, KFile *fout)
{
    rc_t rc;
    uint8_t	buff	[64 * 1024];
    size_t	num_read;
    uint64_t	inpos;
    uint64_t	outpos;

    assert (fin != NULL);
    assert (fout != NULL);

    inpos = 0;
    outpos = 0;

    do
    {
        rc = KFileRead (fin, inpos, buff, sizeof (buff), &num_read);
        if (rc != 0)
        {
            PLOGERR (klogErr, (klogErr, rc,
                     "Failed to read from directory structure in creating archive at $(P)",
                               PLOG_U64(P), inpos));
            break;
        }
        else if (num_read > 0)
        {
            size_t to_write;

            inpos += (uint64_t)num_read;

            STSMSG (2, ("Read %zu bytes to %lu", num_read, inpos));

/*             PLOGMSG (klogDebug10, "Read $(B) Bytes for $(T)", PLOG_2(PLOG_U64(B),PLOG_U64(T)), num_read, inpos); */
            to_write = num_read;
            while (to_write > 0)
            {
                size_t num_writ;
                rc = KFileWrite (fout, outpos, buff, num_read, &num_writ);
                if (rc != 0)
                {
                    PLOGERR (klogErr, (klogErr, rc,
                             "Failed to write to archive in creating archive at $(P)",
                                       PLOG_U64(P), outpos));
                    break;
                }
                outpos += num_writ;
/*                 PLOGMSG (klogDebug10, "Wrote $(B) Bytes for $(T)", PLOG_2(PLOG_U64(B),PLOG_U64(T)), num_writ, outpos); */
                to_write -= num_writ;
            }
        }
/*         else */
/*             PLOGMSG (klogDebug10, "Read $(B) Bytes for $(T)", PLOG_2(PLOG_U64(B),PLOG_U64(T)), num_read, inpos); */
        if (rc != 0)
            break;
    } while (num_read != 0);
    return rc;
}

#if USE_SKEY_MD5_FIX
static
rc_t copy_file_skey_md5_kludge (const KFile * fin, KFile *fout)
{
/* size of HEX digest plus spzce plus * */
#define READ_SIZE 34
    static const uint8_t skey[] = "skey\n";
    uint8_t buff [256];
    uint64_t tot_read, tot_writ;
    size_t num_read, num_writ;
    rc_t rc;

    assert (fin);
    assert (fout);

    for (tot_read = 0 ; tot_read < READ_SIZE; tot_read += num_read)
    {
        rc = KFileRead (fin, tot_read, buff, READ_SIZE - tot_read, &num_read);
        if (rc != 0)
        {
            PLOGERR (klogErr, (klogErr, rc,
                               "Failed to read from directory structure in creating archive at $(P)",
                               PLOG_U64(P), tot_read));
            break;
        }
        if (num_read == 0)
            break;
    }
    if (rc == 0)
    {
        if (tot_read == READ_SIZE)
        {
            memcpy (buff + READ_SIZE, skey, sizeof (skey));
            tot_read += sizeof (skey) - 1;
        }

        for (tot_writ = 0; tot_writ < tot_read; tot_writ += num_writ)
        {
            rc = KFileWrite (fout, tot_writ, buff + tot_writ, 
                             (uint32_t)(tot_read - tot_writ), &num_writ);
            if (rc != 0)
            {
                PLOGERR (klogErr, (klogErr, rc,
                                   "Failed to write to archive in creating archive at $(P)",
                                   PLOG_U64(P), num_writ));
                break;
            }
        }
    }
    return rc;
}
#endif


/* filter will let us add the add to and extract by name things to kar */
static
rc_t step_through_dir (const KDirectory * dir, const char * path,
                       bool ( CC * filter)(const KDirectory *, const char *, void *),
                       void * fdata,
                       rc_t ( CC * action)(const KDirectory *, const char *, void *),
                       void * adata)
{
    rc_t rc;
    KNamelist * names;

    STSMSG (4, ("step_through_dir %s\n", path));

    rc = KDirectoryList (dir, &names, NULL, NULL, path);
    if (rc == 0)
    {
        uint32_t limit;
        rc = KNamelistCount (names, &limit);
        if (rc == 0)
        {
            uint32_t idx;
            size_t pathlen;

            pathlen = strlen(path);
            for (idx = 0; idx < limit; idx ++)
            {
                const char * name;
                rc = KNamelistGet (names, idx, &name);
                if (rc == 0)
                {
                    char * new_path;
                    size_t namelen;
                    size_t new_pathlen;
                    size_t rsize;

                    namelen = strlen (name);
                    new_pathlen = pathlen + 1 + namelen;
                    rsize = new_pathlen + 1;
                    new_path = malloc (rsize);
                    if (new_path != NULL)
                    {
                        char * recur_path;
                        if (pathlen == 0)
                        {
                            memcpy (new_path, name, namelen);
                            new_path[namelen] = '\0';
                        }
                        else
                        {
                            memcpy (new_path, path, pathlen);
                            new_path[pathlen] = '/';
                            memcpy (&new_path[pathlen+1], name, namelen);
                            new_path[new_pathlen] = '\0';
                        }
                        recur_path = malloc (rsize);
                        if (recur_path != NULL)
                        {
                            rc = KDirectoryVResolvePath (dir, false, recur_path,
                                                         rsize, new_path, NULL);

                            if (rc == 0)
                            {
                                bool use_name;
                                if (filter != NULL)
                                {
                                    use_name = filter (dir, recur_path, fdata);
                                }
                                else
                                {
                                    use_name = true;
                                }
                                if (use_name)
                                {
                                    rc = action (dir, recur_path, adata);
                                    /* 				    if (rc == 0) */
                                    /* 				    { */
                                    /* 					enum KPathType type; */
                                    /* 					type = (enum KPathType)KDirectoryPathType (dir, recur_path); */
                                    /* 					if (type == kptDir) */
                                    /*                                             rc = step_through_dir (dir, recur_path, filter, fdata, action, adata); */
                                    /* 				    } */
                                }
                            }
                            free (recur_path);
                        }
                        free (new_path);
                    }
                }
                if (rc != 0)
                    break;
            }
        }
        KNamelistRelease (names);
    }
    return rc;
}
static
rc_t	derive_directory_name (char ** dirname, const char * arcname, const char * optname)
{
    rc_t rc;
    const char * srcname;
    size_t len;

    rc = 0;
    if (optname == NULL)
    {
        const char * dot;
        dot = strrchr (arcname, '.');
        if (dot == NULL)
        {
            rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcInvalid);
            PLOGERR (klogErr, (klogErr, rc, "Unable to derive directory name from $(D)", PLOG_S(D),
                               arcname));
            *dirname = NULL;
        }
        else 
        {
            len = dot - arcname;
            srcname = arcname;
        }
    }
    else
    {
        len = strlen (optname);
        srcname = optname;
    }
    if (rc == 0)
    {
        if ( len == 0 )
        {
            rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcInvalid);
            * dirname = NULL;
        }
        else
        {
            *dirname = malloc (len + 1);
            if (*dirname == NULL)
            {
                rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
                LOGERR (klogErr, rc, "Unable to allocate memory for directory name");
            }
            else
            {
                memcpy (*dirname, srcname, len);
                (*dirname)[len] = '\0';
            }
        }
    }
    return rc;
}

static
rc_t	run_kar_create(const char * archive, const char * directory)
{
    rc_t rc;
    const KFile * fin;
    KFile * fout;

    rc = open_out_file (archive, &fout);
    if (rc == 0)
    {
        char * directorystr;

        rc = derive_directory_name (&directorystr, archive, directory);

        if (rc != 0)
            LOGERR (klogErr, rc,"failed to derive directory name");
        else
        {
            assert (directorystr != NULL);
            assert (directorystr[0] != '\0');

            STSMSG (4, ("start creation of archive"));

            /* Jira ticket: SRA-1876
               Date: September 13, 2013
               raw usage of "directorystr" causes tests to fail within libs/kfs/arc.c */
            {
                char full [ 4096 ];
                rc = KDirectoryResolvePath ( kdir, true, full, sizeof full, "%s", directorystr );
                if ( rc == 0 )
                {
                    /* string should be non-empty based upon behavior of
                       "derive_directory_name" ( also fixed today ) */
                    assert ( full [ 0 ] != 0 );

                    /* eliminate double-slashes */
                    if ( full [ 1 ] != 0 )
                    {
                        uint32_t i, j;

                        /* ALLOW double slashes at the very start
                           set starting index to 2 for that reason */
                        for ( i = j = 2; full [ i ] != 0; ++ i )
                        {
                            if ( ( full [ j ] = full [ i ] ) != '/' || full [ j - 1 ] != '/' )
                                ++ j;
                        }

                        full [ j ] = 0;
                    }

                    rc = open_dir_as_archive ( full, & fin );
                }
            }
            if (rc != 0)
                PLOGERR (klogErr, (klogErr, rc,"failed to open directory '$(D)' as archive",
                                   PLOG_S(D),directorystr));
            else
            {
                assert (fin != NULL);
                assert (fout != NULL);

                STSMSG (4, ("start copy_file"));
                rc = copy_file (fin, fout);
                if (rc != 0)
                    LOGERR (klogErr, rc, "failed copy file in create");
                KFileRelease (fin);
            }
        }
        KFileRelease (fout);
        free (directorystr);

        if (rc)
        {
            remove_out_file (archive);
        }

    }
    return rc;
}
/* buffer must be at least 10 characters long */
void access_to_string (char * b, uint32_t a)
{
    b[0] = (a & 0400) ? 'r' : '-';
    b[1] = (a & 0200) ? 'w' : '-';
    b[2] = (a & 0100) ? 'x' : '-';
    b[3] = (a & 0040) ? 'r' : '-';
    b[4] = (a & 0020) ? 'w' : '-';
    b[5] = (a & 0010) ? 'x' : '-';
    b[6] = (a & 0004) ? 'r' : '-';
    b[7] = (a & 0002) ? 'w' : '-';
    b[8] = (a & 0001) ? 'x' : '-';
    b[9] = '\0';
}

typedef struct list_adata
{
    uint64_t max_size;
    uint64_t max_loc;
    bool (CC * filter)(const KDirectory *, const char *, void *);
    void * fdata;
    DLList list;
    bool has_zombies;
} list_adata;
typedef struct list_item
{
    DLNode         dad;
    KPathType      type;
    uint32_t       access;
    uint64_t       size;
    uint64_t       loc;
    KTime_t        mtime;
    char *         path;
    char *         link;

} list_item;

static
rc_t CC list_action (const KDirectory * dir, const char * path, void * _adata)
{
    rc_t           rc;
    list_adata *   data;
    list_item *    item;
    KPathType      type;
    uint32_t       access;
    uint64_t       size;
    uint64_t       loc;
    KTime_t        mtime;
    size_t         pathlen;
    size_t         linklen;
    char           link		[2 * 4096]; /* we'll truncate? */

    rc = 0;
    data = _adata;

    loc = size = 0;
    pathlen = strlen (path);
    type = KDirectoryPathType (dir, path);

    if (type & kptAlias)
    {
        rc = KDirectoryVResolveAlias (dir, false, link, sizeof (link),
                                      path, NULL);
        if (rc == 0)
            linklen = strlen (link);
    }
    else
    {
        linklen = 0;
        switch (type & ~kptAlias)
        {
        case kptNotFound:
            rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcNotFound);
            break;
        case kptBadPath:
            rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcInvalid);
            break;
        case kptZombieFile:
            if ( ! long_list )
                return 0;
            data->has_zombies = true;
        case kptFile:
            rc = KDirectoryFileSize (dir, &size, path);
            if (rc == 0)
            {
                if (size > data->max_size)
                    data->max_size = size;
                rc = KDirectoryFileLocator (dir, &loc, path);
                if ((rc == 0) && (loc > data->max_loc))
                    data->max_loc = loc;
            }
            break;
        case kptDir:
            break;
        case kptCharDev:
        case kptBlockDev:
        case kptFIFO:
            /* shouldn't get here */
            return 0;
        }
    }
    if (rc == 0)
    {
        rc = KDirectoryAccess (dir, &access, "%s", path);
        if (rc == 0)
        {
            rc = KDirectoryDate (dir, &mtime, "%s", path);

            if (rc == 0)
            {
                item = malloc (sizeof (*item) + pathlen + linklen + 2); /* usually one too many */
                if (item == NULL)
                {
                    rc = RC (rcExe, rcNoTarg, rcAllocating, rcMemory, rcExhausted);
                }
                else
                {
                    item->type = type;
                    item->access = access;
                    item->size = size;
                    item->loc = loc;
                    item->mtime = mtime;
                    item->path = (char *)(item+1);
                    strcpy (item->path, path);
                    if (type & kptAlias)
                    {
                        item->link = item->path + pathlen + 1;
                        strcpy (item->link, link);
                    }
                    else
                        item->link = NULL;
                    DLListPushHead (&data->list, &item->dad);

                    if (type == kptDir)
                        rc = step_through_dir (dir, path, data->filter, data->fdata, 
                                               list_action, data);


                }
            }
        }
    }
    return rc;
}
static
rc_t run_kar_test( const char * archive )
{
    rc_t rc;
    const KDirectory * din;

    rc = open_file_as_dir (archive, &din);
    if (rc == 0)
    {
        list_adata adata;

        adata.max_loc = adata.max_size = 0;
        adata.filter = pnamesFilter;
        adata.fdata = NULL;
        adata.has_zombies = false;
        DLListInit (&adata.list);
        /* find all directory entries */
        rc = step_through_dir ( din, ".", pnamesFilter, NULL, list_action, &adata );

        /* "sort" the file entries if requested */
        if (rc == 0)
        {
        }
        if (rc == 0)
        {
            const char * LineFormatFile = "%c%s %*lu %*lu %*s %s";
            const char * LineFormatEFile = "%c%s %*lu %*c %*s %s";
            const char * LineFormatDir = "%c%s %*c %*c %*s %s";
            const char * LineFormatTxt = "%c%s %-*s %-*s %-*s %s";
            /* 18446744073709551615 is max 64bit unsigned so 20 characters + 1 */
            char zwidth_buffer [32];
            size_t zwidth, lwidth;
            union u
            {
                DLNode * node;
                list_item * item;
            } ptr;

            rc = string_printf (zwidth_buffer, sizeof zwidth_buffer, & zwidth, "%lu", adata.max_size);
            rc = string_printf (zwidth_buffer, sizeof zwidth_buffer, & lwidth, "%lu", adata.max_size);
            if (long_list)
            { 
                /* need to use time_t not Ktime_t here since we are calling system functions */
                time_t t = time(NULL);
                struct tm * ts = localtime (&t);
                int dwidth = strftime (zwidth_buffer, sizeof (zwidth_buffer), "%Y-%m-%d %H:%M:%S", ts);

                KOutMsg (LineFormatTxt,
                         'T',"ypeAccess",
                         (uint32_t)zwidth, "Size",
                         (uint32_t)lwidth, "Offset",
                         (uint32_t)dwidth, "ModDateTime",
                         "Path Name\n");
            }
            for (ptr.node = DLListPopTail(&adata.list); 
                 ptr.node != NULL;
                 ptr.node = DLListPopTail(&adata.list))
            {
                if (long_list)
                {
                    struct tm * ts;
                    char t;
                    char a[10];
                    char d[20];
                    size_t dwidth;
                    time_t lt;

                    lt = ptr.item->mtime;
                    ts = localtime (&lt);
                    dwidth = strftime (d, sizeof (d), "%Y-%m-%d %H:%M:%S", ts);
                    access_to_string (a, ptr.item->access);
                    if (ptr.item->type & kptAlias)
                        t = 'l';
                    else if (ptr.item->type == kptDir)
                        t = 'd';
                    else
                        t = '-';
                    if ((ptr.item->type & ~kptAlias) == kptZombieFile)
                        KOutMsg(LineFormatFile,
                                t, "TRUNCATED",
                                (uint32_t)zwidth, ptr.item->size,
                                ( uint32_t )lwidth, ptr.item->loc,
                                ( uint32_t )dwidth, d, ptr.item->path);
                    else if ((ptr.item->type & ~kptAlias) == kptFile)
                    {
                        if (ptr.item->size == 0)
                            KOutMsg(LineFormatEFile,
                                    t, a,
                                    ( uint32_t )zwidth, ptr.item->size,
                                    ( uint32_t )lwidth, '-',
                                    ( uint32_t )dwidth, d, ptr.item->path);
                        else
                            KOutMsg(LineFormatFile,
                                    t, a,
                                    ( uint32_t )zwidth, ptr.item->size,
                                    ( uint32_t )lwidth, ptr.item->loc,
                                    ( uint32_t )dwidth, d, ptr.item->path);
                    }
                    else
                        KOutMsg(LineFormatDir,
                                t, a,
                                ( uint32_t )zwidth, '-',
                                ( uint32_t )lwidth, '-',
                                ( uint32_t )dwidth, d, ptr.item->path);
                    if (ptr.item->type & kptAlias)
                        KOutMsg (" -> %s\n", ptr.item->link);
                    else
                        KOutMsg ("\n");
                }
                else
                {
                    KOutMsg ("%s\n",ptr.item->path);
                }
            }
        }
        KDirectoryRelease (din);
    }
    return rc;
}

typedef struct extract_adata
{
    KDirectory * dir;
    bool ( CC * filter)(const KDirectory *, const char *, void *);
    void * fdata;
} extract_adata;

static
rc_t CC extract_action (const KDirectory * dir, const char * path, void * _adata)
{
    rc_t            rc;
    extract_adata * adata;
    KPathType     type;
    char            link	[2 * 4096]; /* we'll truncate? */
    uint32_t	    access;
    rc = 0;
    adata = _adata;

    STSMSG (1, ("extract_action: %s\n", path));

    type = KDirectoryPathType (dir, path);

    if (type & kptAlias)
    {
        rc = KDirectoryVResolveAlias (dir, false, link, sizeof (link),
                                      path, NULL);
        if (rc == 0)
        {
            rc = KDirectoryVAccess (dir, &access, path, NULL);
            if (rc == 0)
            {
                rc = KDirectoryCreateAlias (adata->dir, access, kcmCreate|kcmParents,
                                            link, path);
            }
        }
    }
    else
    {
        switch (type & ~kptAlias)
        {
        case kptNotFound:
            rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcNotFound);

            KOutMsg ("%s: %s type kptNotFouns %R\n", __func__, path, rc);

            break;
        case kptBadPath:
            rc = RC (rcExe, rcDirectory, rcAccessing, rcPath, rcInvalid);
            break;
        case kptFile:
            rc = KDirectoryVAccess (dir, &access, path, NULL);
            if (rc == 0)
            {
                const KFile * fin;
                KFile * fout;
                rc = KDirectoryVCreateFile (adata->dir, &fout, false, access,
                                            kcmCreate|kcmParents,
                                            path, NULL);
                if (rc == 0)
                {
                    rc = KDirectoryVOpenFileRead (dir, &fin, path, NULL);
                    if (rc == 0)
                    {
#if USE_SKEY_MD5_FIX
                        /* KLUDGE!!!! */
                        size_t pathz, skey_md5z;
                        static const char skey_md5[] = "skey.md5";

                        pathz = string_size (path);
                        skey_md5z = string_size(skey_md5);
                        if ( pathz >= skey_md5z && strcmp ( & path [ pathz - skey_md5z ], skey_md5 ) == 0 )
                            rc = copy_file_skey_md5_kludge (fin, fout);
                        else
#endif
                            rc = copy_file (fin, fout);
                        KFileRelease (fin);
                    }
                    KFileRelease (fout);
                }
            }
            break;
        case kptDir:
            rc = KDirectoryVAccess (dir, &access, path, NULL);
            if (rc == 0)
            {
                rc = KDirectoryCreateDir (adata->dir, 0700, 
                                          kcmCreate|kcmParents,
                                          path, NULL);
                if (rc == 0)
                {
                    rc = step_through_dir (dir, path, adata->filter, adata->fdata,
                                           extract_action, adata);
                    if (rc == 0)
                        rc = KDirectoryVSetAccess (adata->dir, false, access, 0777, path, NULL);
                }



            }
            break;
        case kptCharDev:
        case kptBlockDev:
        case kptFIFO:
            /* shouldn't get here */
            return 0;
        }
    }

    return rc;
}
static
rc_t	run_kar_extract (const char * archive, const char * directory)
{
    rc_t rc;
    const KDirectory * din;
    KDirectory * dout;

    STSMSG (1, ("run_kar_extract"));

    rc = open_file_as_dir (archive, &din);
    if (rc == 0 )
    {
        char * directorystr;
        rc = derive_directory_name (&directorystr, archive, directory);
        
        if (rc != 0)
        {
            ;
/*             PLOGERR (klogDebug1, (klogDebug1, rc, "failure to derive archive [$(A)/$(D)]", */
/*                      PLOG_2(PLOG_S(A),PLOG_S(D)), archive, directory)); */
        }
        else
        {
            rc = open_out_dir (directorystr, &dout);
            free (directorystr);
            if (rc != 0)
            {
                LOGERR (klogErr, rc, "failure to open output directory");
            }
            else
            {
                extract_adata adata;
                adata.dir = dout;
                adata.filter = pnamesFilter;
                adata.fdata = NULL;
                
                rc = step_through_dir (din, ".", pnamesFilter, NULL, extract_action, &adata);
                KDirectoryRelease (dout);
            }
        }
        KDirectoryRelease (din);
    }
    return rc;
}

rc_t CC NextLogLevelCommon ( const char * level_parameter );

typedef enum op_mode
{
    OM_NONE = 0,
    OM_CREATE,
    OM_TEST,
    OM_EXTRACT,
    OM_ERROR
} op_mode;

rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc == 0)
    {
        uint32_t pcount;
        const char * pc;
        const char * directory;
        const char * archive;

        do
        {
            op_mode mode;
            uint32_t ix;

            BSTreeInit (&pnames);

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;
            for (ix = 0; ix < pcount; ++ix)
            {
                rc = ArgsParamValue (args, ix, (const void **)&pc);
                if (rc)
                    break;
                rc = pnamesInsert (pc);
                if (rc)
                {
                    PLOGERR (klogInt, (klogInt, rc, "failure to process filter parameter [%s]", pc));
                    break;
                }
            }
            if (rc)
                break;

            rc = ArgsOptionCount (args, OPTION_ALIGN, &pcount);
            if (rc)
                break;
            if (pcount == 0)
                pc = NULL;
            else
            {
                rc = ArgsOptionValue (args, OPTION_ALIGN, 0, (const void **)&pc);
                if (rc)
                    break;
            }
            alignment = get_alignment (pc);
            if (alignment == sraAlignInvalid)
            {
                rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcInvalid);
                PLOGERR (klogFatal, (klogFatal, rc,
                         "Parameter for alignment [$(A)] is invalid: must be a power of two bytes",
                                     PLOG_S(A), pc));
                break;
            }

            rc = ArgsOptionCount (args, OPTION_LONGLIST, &pcount);
            if (rc)
                break;
            long_list = (pcount != 0);

            rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
            if (rc)
                break;
            force = (pcount != 0);

            rc = ArgsOptionCount (args, OPTION_MD5, &pcount);
            if (rc)
                break;
            md5sum = (pcount != 0);

            mode = OM_NONE;

            rc = ArgsOptionCount (args, OPTION_DIRECTORY, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_DIRECTORY, 0, (const void **)&directory);
                if (rc)
                    break;

            }
            else
                directory = NULL;

            rc = ArgsOptionCount (args, OPTION_CREATE, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                rc = ArgsOptionValue (args, OPTION_CREATE, 0, (const void **)&archive);

                if (rc)
                    break;
                mode = OM_CREATE;
            }

            rc = ArgsOptionCount (args, OPTION_TEST, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                if (mode == OM_NONE)
                {
                    mode = OM_TEST;
                    rc = ArgsOptionValue (args, OPTION_TEST, 0, (const void **)&archive);
                    if (rc)
                        break;
                }
                else
                    mode = OM_ERROR;
            }

            rc = ArgsOptionCount (args, OPTION_LIST, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                if (mode == OM_NONE)
                {
                    mode = OM_TEST;
                    rc = ArgsOptionValue (args, OPTION_LIST, 0, (const void **)&archive);
                    if (rc)
                        break;
                }
                else
                    mode = OM_ERROR;
            }

            rc = ArgsOptionCount (args, OPTION_EXTRACT, &pcount);
            if (rc)
                break;
            if (pcount)
            {
                if (mode == OM_NONE)
                {
                    mode = OM_EXTRACT;
                    rc = ArgsOptionValue (args, OPTION_EXTRACT, 0, (const void **)&archive);
                    if (rc)
                        break;
                }
                else
                    mode = OM_ERROR;
            }


            if (mode == OM_NONE)
            {
                MiniUsage(args);
                rc = RC(rcApp, rcArgv, rcParsing, rcParam, rcInsufficient);
                break;
            }


            /* -----
             * Mount the native filesystem as root
             */
            rc = KDirectoryNativeDir (&kdir);
            if (rc != 0)
            {
                LOGMSG (klogFatal, "Failed to open native KDirectory");
            }
            else
            {
                switch (mode)

                { 
                default: /* OM_NONE presumably */
/*                     assert (mode == OM_NONE); */
/*                     rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcEmpty); */
/*                     LOGERR (klogFatal, rc, "Must provide a single operating mode and archive: Create, Test (list) or eXtract"); */
                    pc = NULL;
                    break;

                case OM_ERROR:
                    rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcAmbiguous);
                    LOGERR (klogFatal, rc, "Must provide a single operating mode and archive: Create, Test (list) or eXtract");
                    break;
                case OM_CREATE:
                    STSMSG (2, ("Create Mode %s %s", archive, directory));
                    if (directory == NULL)
                    {
                        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcNull);
                        LOGERR (klogFatal, rc, "Must provide a directory for create mode");
                    }
                    else
                        rc = run_kar_create (archive, directory);
                    break;
                case OM_TEST:
                    STSMSG (2, ("Test Mode %s", archive));
                    rc = run_kar_test (archive);
                    break;
                case OM_EXTRACT:
                    STSMSG (2, ("Extract Mode %s %s", archive, directory));
                    if (directory == NULL)
                    {
                        rc = RC (rcExe, rcNoTarg, rcParsing, rcParam, rcNull);
                        LOGERR (klogFatal, rc, "Must provide a directory for extract mode");
                    }
                    else
                        rc = run_kar_extract (archive, directory);
                    break;
                }

                KDirectoryRelease (kdir);
            }

        } while (0);

        ArgsWhack (args);
    }
    if (rc)
        LOGERR (klogWarn, rc, "Failure exiting kar");
    else
        STSMSG (1, ("Success: Exiting kar\n"));
    return rc;
}

/* end of file */
