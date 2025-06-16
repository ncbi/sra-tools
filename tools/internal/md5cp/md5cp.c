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

#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/rc.h>

#include <klib/vector.h>
#include <klib/impl.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/md5.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MD5EXTENSION "md5"
#define DEFAULTMODE 0664
#define DEFAULT_DIR_MODE 0775

/*
Trying to mimic cp behavior, with a few differences,
mainly, that if -f not specified and target exists,
we do not clobber files, but instead print something about it on the
output.

cp -r dir1 dir2 makes dir2/dir1
cp -r dir1 <nonexistent-name> just "renames" directory dir1 to name
-r not specified and dir copied from, ignores that directory.

If multiple things are specified (more than 2)
then the last one should exist and be a directory.

-i should still be available, to ask what to do about things that would be clobbered.

Need to think what to do when inputs are relative and absolute

This needs to work with backslashes as well (i.e. independent of
target platform).

Need to worry about copying from a directory to itself
Definitely don't want to copy a file from itself to itself

Need also to worry about loops in references (when walking trees)

What to do about symbolic links

DONE: Need to make mode of file copied

Need to make return code of executable be useful to scripts

TESTS:

copy file to nonexistent name (should create, and .md5 as well)
do it again, make sure missing -f works (doesn't clobber).
do it again with -f to see it clobbers.

Make sure copying a directory to a file causes an error.

make a directory with 2 files, copy to new (non-existent) name
  without -r switch (should ignore directory)
add -r switch, see creates the new name dir
do it again, should create a subdirectory

Check that mode is preserved on copying directory to new (nonexistent) name
Check that mode is preserved on new files (created files)

On existing files, the mode will be preserved on the TARGET file,
unless -p is specified, in which case (like -f) the source will clobber
the target protections.

 */

int interacative = 0;
int force = 0;
int recurse = 0;
int test = 0;
int clobber_protections = 0;
int followlinks = 1;

rc_t CopyDirectoryToExistingDirectory( const KDirectory *top, const char *inname, KDirectory *targettop, const char *outname );
rc_t CopyFileToFile( const KDirectory *top, const char *inname, KDirectory *targettop, const char *outname );

#define BUFSIZE 8192

char buffer[BUFSIZE];

/*
 * out is a pre-allocated buffer.
 */
void JustTheName(const char *in, char *out)
{
  int len;
  int end;
  int begin;
  int i;

  len = strlen(in);
  end = len;
  end--;
  while (end > 0 && in[end] == '/') {
    end--;
  }
  begin = end;
  while (begin > 0 && in[begin-1] != '/') {
    begin--;
  }
  for (i=begin; i<=end; i++) {
    *out++ = in[i];
  }
  *out++ = '\0';
}

bool PathIsMD5File(const KDirectory *dir, const char *inname)
{
  int extlen;
  int inlen;

  inlen = strlen(inname);
  extlen = strlen(MD5EXTENSION);
  if (strlen(inname) > extlen+1 &&
      inname[inlen - extlen - 1] == '.' &&
      0 == strcmp(inname+inlen-extlen, MD5EXTENSION)) {
    return true;
  }
  return false;
}

bool CC PathIsFile( const KDirectory *dir, const char *name, void *data )
{
  uint32_t pathtype;
  pathtype = KDirectoryPathType( dir, "%s", name );
  return ((pathtype & ~kptAlias) == kptFile);
}


bool CC PathIsDir(const KDirectory *dir, const char *name, void *data)
{
  uint32_t pathtype;
  pathtype = KDirectoryPathType( dir, "%s", name );
  return ((pathtype & ~kptAlias) == kptDir);
}

rc_t CopyMode( const KDirectory *source, const char *sourcename,
	       KDirectory *target, char *targetname )
{
  /* Make sure they both exist and are the same type */
  uint32_t src_pathtype;
  uint32_t dest_pathtype;
  uint32_t mode;
  rc_t rc;

  src_pathtype = KDirectoryPathType( source, "%s", sourcename );
  dest_pathtype = KDirectoryPathType( target, "%s", targetname );
  if ((src_pathtype & ~kptAlias) != (dest_pathtype & ~kptAlias)) {
    return -1;
  }
  rc = KDirectoryAccess( source, &mode, "%s", sourcename );
  if (rc != 0)
    {
      LOGERR ( klogInt, rc, sourcename );
      return rc;
    }
  KDirectorySetAccess( target, false, mode, 0777, "%s", targetname );
  return 0;
}


rc_t CopyDirectoryFiles( const KDirectory *source, KDirectory *dest ) {
  rc_t rc;
  KNamelist *list;
  const char *name;
  int i;
  uint32_t count;
  char resolved[1024];

  rc = KDirectoryList( source, &list, PathIsFile, NULL, ".");
  if (rc != 0)
    {
      /* This doesn't do what I thought. */
      KDirectoryResolvePath( source, false, resolved, 1024, ".");
      LOGERR ( klogInt, rc, resolved );
      return rc;
    }
  KNamelistCount(list, &count);
  for (i=0; i<count; i++) {
    KNamelistGet(list, i, &name);
    if (test) {
      fprintf(stderr, "Will copy %s\n", name);
    } else {
      CopyFileToFile( source, name, dest, (char *)name );
    }
  }
  return 0;
}

rc_t CopyDirectoryDirectories( const KDirectory *source, KDirectory *dest ) {
  rc_t rc;
  KNamelist *list;
  const char *name;
  int i;
  uint32_t count;
  uint32_t mode;
  uint32_t pathtype;

  KDirectoryList( source, &list, PathIsDir, NULL, ".");
  KNamelistCount(list, &count);
  for (i=0; i<count; i++) {
    KNamelistGet(list, i, &name);
    /* fprintf(stderr, "Creating directory %s\n", name); */
    mode = DEFAULT_DIR_MODE;
    rc = KDirectoryAccess( source, &mode, "%s", name);
    if (rc != 0)
      {
	LOGERR ( klogInt, rc, name );
	return rc;
      }
    pathtype = KDirectoryPathType( dest, "%s", name );
    if ((pathtype & ~kptAlias) == kptNotFound) {
        rc = KDirectoryCreateDir( dest, mode, kcmOpen, "%s", name );
      if (rc != 0)
	{
	  LOGERR ( klogInt, rc, name );
	  return rc;
	}
    } else if ((pathtype & ~kptAlias) == kptDir) {
      if (clobber_protections) {
          KDirectorySetAccess( dest, false, mode, 0777, "%s", name);
      }
    }
    CopyDirectoryToExistingDirectory( source, name, dest, (char *)name);
  }
  return 0;
}




rc_t CopyFileToFile( const KDirectory *top, const char *inname, KDirectory *targettop, const char *outname )
{
  const KFile *in = NULL;
  KFile *out = NULL;
  KFile *md5file = NULL;
  KMD5File *md5out = NULL;
  KMD5SumFmt *md5sumfmt = NULL;
  char md5filename[1024];
  rc_t rc = 0;
  uint32_t mode = 0;
  uint32_t pathtype = 0;
  uint32_t failed = 0;
  int n;

  if (PathIsMD5File(top, inname)) {
    /* Skip it */
    return 0;
  }

  rc = KDirectoryOpenFileRead( top, &in, "%s", inname );
  if (rc != 0) {
    failed = rc;
    goto FAIL;
  }
  mode = DEFAULTMODE;
  rc = KDirectoryAccess( top, &mode, inname);
  if (rc != 0) {
    failed = rc;
    goto FAIL;
  }

  /*
   * Not sure here -- does kcmInit re-initialize the file mode as we specify?
   * Or does it preserve the existing mode (and do we want it to)?
   */
  if (clobber_protections) {
      pathtype = KDirectoryPathType( targettop, "%s", outname );
    if ((pathtype & ~kptAlias) == kptFile) {
        rc = KDirectorySetAccess( targettop, false, mode, 0777, "%s", outname);
      if (rc != 0) {
	failed = rc;
	goto FAIL;
      }
    }
  }

  rc = KDirectoryCreateFile( targettop, &out, false, mode, (force? kcmInit: kcmCreate), "%s", outname );
  if (rc != 0) {
    failed = rc;
    goto FAIL;
  }
  n = snprintf(md5filename, sizeof(md5filename), "%s.md5", outname);
  assert(n < sizeof(md5filename));
  rc = KDirectoryCreateFile( targettop, &md5file, false, DEFAULTMODE, (force? kcmInit: kcmCreate), "%s", md5filename);
  if (rc != 0) {
    failed = rc;
    goto FAIL;
  }

  rc = KMD5SumFmtMakeUpdate( &md5sumfmt, md5file);
  if (rc != 0) {
    failed = rc;
    goto FAIL;
  }

  rc = KMD5FileMakeWrite( &md5out, out, md5sumfmt, outname );
  if (rc != 0) {
    failed = rc;
    goto FAIL;
  }

  {
    uint64_t rpos = 0;
    uint64_t wpos = 0;

    size_t numread;

    while (true) {
      rc = KFileRead( in, rpos, buffer, BUFSIZE, &numread );
      /* fprintf(stderr, "Read %d bytes.\n", numread); */
      if (rc == 0 && numread == 0)
	break;
      if (rc != 0) {
	failed = rc;
	goto FAIL;
      }
      rpos += numread;

      {
	size_t numwritten = 0;
	int written = 0;
	while (written < numread) {
	  rc = KFileWrite( (KFile *)md5out, wpos, buffer+written, numread-written, &numwritten );
	  if (rc != 0) {
	    failed = rc;
	    break;
	  }
	  if (numwritten == 0) {
	    fprintf(stderr, "Didn't write anything.\n");
	    failed = -1;
	    goto FAIL;
	  }
	  wpos += numwritten;
	  written += numwritten;
	}
      }
    }
  }

  /* Success also, check the value of failed to see if failed */
 FAIL:

  if (NULL != md5out) {
    KFileRelease((KFile *)md5out);
    md5out = NULL;
  }

  /*KFileRelease(out); */
  if (NULL != md5sumfmt) {
    KMD5SumFmtRelease(md5sumfmt);
    md5sumfmt = NULL;
  }
  /*  KFileRelease(md5file); */
  if (NULL != in) {
    KFileRelease(in);
    in = NULL;
  }
  /* KDirectoryRelease(top); */

  if (failed) {
      KDirectoryRemove( targettop, false, "%s", md5filename );
      KDirectoryRemove( targettop, false, "%s", outname);
  }

  return failed;

}

/*
 * copies top/inname (a directory)
 * to targettop/outname, i.e. creates outname as a copy of that directory.
 */
rc_t CopyDirectoryToExistingDirectory( const KDirectory *top, const char *inname, KDirectory *targettop, const char *outname )
{
  rc_t rc;
  uint32_t mode;
  const KDirectory *source;
  KDirectory *dest;
  rc = KDirectoryOpenDirRead(top, &source, true, "%s", (const char *)inname);
  if (rc != 0)
    {
      LOGERR ( klogInt, rc, "can't open input directory" );
      return rc;
    }
  mode = DEFAULT_DIR_MODE;
  rc = KDirectoryAccess( top, &mode, "%s", inname);
  if (rc != 0)
    {
      LOGERR ( klogInt, rc, inname );
      return rc;
    }
  rc = KDirectoryCreateDir( targettop, mode, kcmOpen, "%s", outname );
  if (rc != 0)
    {
      LOGERR ( klogInt, rc, "can't create output directory" );
      return rc;
    }
  if (clobber_protections) {
      KDirectorySetAccess( targettop, false, mode, 0777, "%s", outname);
  }
  rc = KDirectoryOpenDirUpdate(targettop, &dest, true, "%s", outname);
  if (rc != 0)
    {
      LOGERR ( klogInt, rc, "can't open directory for write" );
      return rc;
    }
  CopyDirectoryFiles(source, dest);
  CopyDirectoryDirectories( source, dest );

  KDirectoryRelease( dest );
  KDirectoryRelease( source );
  return 0;
}

#define OPTION_FORCE    "force"
#define OPTION_RECURSE  "recursive"
#define OPTION_PRESERVE "preserve"
#define OPTION_TEST     "test"
#define ALIAS_FORCE     "f"
#define ALIAS_RECURSE   "r"
#define ALIAS_PRESERVE  "p"
#define ALIAS_TEST      "t"

static const char * force_usage[]    = { "overwrite existing columns", NULL };
static const char * recurse_usage[]  = { "Recurses over source directories",
                                         "(directories are ignored otherwise).", NULL };
static const char * preserve_usage[] = { "force replacement of existing modes on files", " and directories", NULL };
static const char * test_usage[]     = { "?", NULL };


OptDef Options[] =
{
    { OPTION_FORCE,    ALIAS_FORCE,    NULL, force_usage,    0, false, false },
    { OPTION_RECURSE,  ALIAS_RECURSE,  NULL, recurse_usage,  0, false, false },
    { OPTION_PRESERVE, ALIAS_PRESERVE, NULL, preserve_usage, 0, false, false },
    { OPTION_TEST,     ALIAS_TEST,     NULL, test_usage,     0, false, false }
};


const char UsageDefaultName[] = "md5cp";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s Options [file|directory ...] directory\n"
                    "\n"
                    "Summary:\n"
                    "  Copies files and/or directories, creating an md5sum checksum\n"
                    "  (named file.md5) for all copied files.\n",
                    progname);
}


rc_t CC Usage (const Args * args)
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    KOutMsg ("\n"
             "Option:\n");

    HelpOptionLine (ALIAS_FORCE, OPTION_FORCE, NULL, force_usage);
    HelpOptionLine (ALIAS_PRESERVE, OPTION_PRESERVE, NULL, preserve_usage);
    HelpOptionLine (ALIAS_RECURSE, OPTION_RECURSE, NULL, recurse_usage);
    HelpOptionLine (ALIAS_TEST, OPTION_TEST, NULL, test_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


rc_t run (Args * args)
{
    rc_t rc;

    do
    {
        const char * outname;
        const char * source;
        uint32_t pathtype;
        KDirectory *top;
        KDirectory *targettop;
        uint32_t pcount;
        uint32_t ix;
        char sourcename [1024];

        rc = KDirectoryNativeDir (&top);
        if (rc)
            break;
        rc = ArgsParamCount (args, &pcount);
        if (rc)
            break;

        if (pcount < 2)
        {
            MiniUsage (args);
            rc = RC (rcExe, rcArgv, rcParsing, rcParam, rcInsufficient);
            break;
        }

        rc = ArgsParamValue (args, 0, (const void **)&outname);
        if (rc)
            break;

        pathtype = KDirectoryPathType (top, "%s", outname);
        if ((pathtype & ~kptAlias) == kptDir)
        {
            /*
             * Copying things into an existing directory.
             */
            rc = KDirectoryOpenDirUpdate( top, &targettop, true, outname);
            if (rc)
            {
                LOGERR (klogFatal, rc, outname);
                break;
            }

            for (ix = 1; ix < pcount; ++ix)
            {

                rc = ArgsParamValue (args, ix, (const void **)&source);
                if (rc)
                    break;

                JustTheName (source, sourcename);
                pathtype = KDirectoryPathType (top, "%s", sourcename);
                if ((pathtype & ~kptAlias) == kptFile)
                {
                    CopyFileToFile (top, source, targettop, sourcename);
                }
                else if ((pathtype & ~kptAlias) == kptDir)
                {
                    if (!recurse)
                    {
                        STSMSG (0, ("Skipping directory %s\n", source));
                        continue;
                    }
                    CopyDirectoryToExistingDirectory (top, source, targettop, sourcename);
                }
            }
            if (rc)
                break;

            rc = KDirectoryRelease (targettop);
/* this looks wrong */
            if (rc)
                LOGERR (klogInt, rc, outname);
        }
        else if ((pathtype * ~kptAlias) == kptFile)
        {
            if (!force)
            {
                STSMSG (0, ("File exists -- %s\n", outname));
                break;
            }
            if (pcount > 2)
            {
                STSMSG (0, ("Target %s is a file. Too many parameters/\n", outname));
                break;
            }

            rc = ArgsParamValue (args, 1, (const void **)&source);
            if (rc)
                break;

            pathtype = KDirectoryPathType (top, "%s", source);

            if ((pathtype & ~kptAlias) == kptDir)
            {
                STSMSG (0, ("Cannot overwrite file with directory %s\n", source));
                break;
            }
            if ((pathtype & ~kptAlias) == kptFile)
            {
                CopyFileToFile (top, source, top, outname);
            }
        }
        else if ((pathtype & ~kptAlias) == kptNotFound)
        {
            if (pcount > 2)
            {
                STSMSG (0, ("Directory %s does not exist.\n", outname));
                break;
            }

            rc = ArgsParamValue (args, 1, (const void **)&source);
            if (rc)
                break;

            pathtype = KDirectoryPathType (top, "%s", source);
            if ((pathtype & ~kptAlias) == kptFile)
            {
                CopyFileToFile (top, source, top, outname);
            }
            else if ((pathtype & ~kptAlias) == kptDir)
            {
                if (!recurse)
                {
                    STSMSG (0, ("Skipping directory %s\n", source));
                    break;
                }
                CopyDirectoryToExistingDirectory (top, source, top, outname);
            }
        }

    } while (0);
    return rc;
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;
    rc_t rc;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    rc = ArgsMakeAndHandle (&args, argc, argv, 1,
                            Options, sizeof (Options) / sizeof (OptDef));
    if (rc == 0)
    {
        do
        {
            uint32_t pcount;

            rc = ArgsOptionCount (args, OPTION_FORCE, &pcount);
            if (rc)
                break;

            force = (pcount > 0);

            rc = ArgsOptionCount (args, OPTION_RECURSE, &pcount);
            if (rc)
                break;

            recurse = (pcount > 0);


            rc = ArgsOptionCount (args, OPTION_TEST, &pcount);
            if (rc)
                break;

            test = (pcount > 0);

            rc = ArgsOptionCount (args, OPTION_PRESERVE, &pcount);
            if (rc)
                break;

            clobber_protections = (pcount > 0);

            rc  = run (args);

        }while (0);

        ArgsWhack (args);
    }
    return VDB_TERMINATE( rc );
}

