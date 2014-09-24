#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/rc.h>

#include <klib/vector.h>
#include <kfs/directory.h>
#include <kfs/dyload.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


/* update libpath
 */
static
void testld_update_libpath ( KDyld *dl, const char *path )
{
    rc_t rc = 0;

    const char *end;
    for ( end = strchr ( path, ':' ); end != NULL; end = strchr ( path = end + 1, ':' ) )
    {
        if ( path < end )
        {
            rc = KDyldAddSearchPath ( dl, "%.*s",
                                      ( int ) ( end - path ), path );
            if ( rc != 0 )
            {
                PLOGERR ( klogWarn, ( klogWarn, rc, "failed to add library path '$(path)'",
                                      "path=%.*s", ( int ) ( end - path ), path ));
            }
        }
    }

    if ( path [ 0 ] != 0 )
    {
        rc = KDyldAddSearchPath ( dl, path );
        if ( rc != 0 )
        {
            PLOGERR ( klogWarn, ( klogWarn, rc, "failed to add library path '$(path)'", "path=s", path ));
        }
    }
}


/* load library
 */
static
void testld_load_library ( KDyld *dl, KDlset *libs, const char *libname )
{
    KDylib *lib;
    rc_t rc = KDyldLoadLib ( dl, & lib, libname );
    if ( rc != 0 )
    {
        PLOGERR ( klogErr, ( klogErr, rc, "failed to load library '$(name)'",
                             "name=%s", libname ));
    }
    else
    {
        printf ( "loaded library '%s'\n", libname);

        rc = KDlsetAddLib ( libs, lib );
        if ( rc != 0 )
        {
            PLOGERR ( klogInt, ( klogInt, rc, "failed to retain library '$(name)'",
                                 "name=%s", libname ));
        }

        KDylibRelease ( lib );
    }
}

/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
ver_t CC KAppVersion ( void )
{
    return 0;
}
    

/* KMain - EXTERN
 *  executable entrypoint "main" is implemented by
 *  an OS-specific wrapper that takes care of establishing
 *  signal handlers, logging, etc.
 *
 *  in turn, OS-specific "main" will invoke "KMain" as
 *  platform independent main entrypoint.
 *
 *  "argc" [ IN ] - the number of textual parameters in "argv"
 *  should never be < 0, but has been left as a signed int
 *  for reasons of tradition.
 *
 *  "argv" [ IN ] - array of NUL terminated strings expected
 *  to be in the shell-native character set: ASCII or UTF-8
 *  element 0 is expected to be executable identity or path.
 */

#define OPTION_LOAD "lib-path"
#define ALIAS_LOAD  "l"

static
const char * load_usage [] = 
	{ "Path(s) for loading dynamic libraries.", NULL };

static
OptDef MyOptions[] =
{
    { OPTION_LOAD, ALIAS_LOAD, NULL, load_usage, 0, true, false }
};


const char UsageDefaultName[] = "testld";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [Options]\n"
                    "\n"
                    "Summary:\n"
                    "  Test for dynamic loading.\n"
                    "\n", progname);
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

    OUTMSG (("Options\n"));

    HelpOptionLine (ALIAS_LOAD, OPTION_LOAD, "Path", load_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


rc_t CC KMain ( int argc, char *argv [] )
{
    Args * args;
    rc_t rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, MyOptions, sizeof MyOptions / sizeof (OptDef));
    if (rc)
        LOGERR (klogInt, rc, "failed to parse arguments");

    else
    {
        do
        {
            KDyld *dl;
            uint32_t num_libs;

            rc = ArgsParamCount (args, &num_libs);
            if (rc)
            {
                LOGERR ( klogInt, rc, "Failure to count parameters" );
                break;
            }

            if (num_libs == 0)
            {
                rc = MiniUsage(args);
                break;
            }

            /* create loader */
            rc = KDyldMake (&dl);
            if (rc)
            {
                LOGERR ( klogInt, rc, "failed to create dynamic loader" );
                break;
            }
            else
            {
                do
                {
                    KDlset *libs;
                    const char * path;
                    uint32_t ix;
                    uint32_t num_paths;

                    rc = ArgsOptionCount (args, OPTION_LOAD, &num_paths);
                    if (rc)
                    {
                        LOGERR (klogInt, rc, "failed to count paths");
                        break;
                    }

                    for (ix = 0; ix < num_paths; ix++)
                    {

                        rc = ArgsOptionValue (args, OPTION_LOAD, ix, &path);
                        if (rc)
                        {
                            LOGERR (klogInt, rc, "failed to access a path option");
                            break;
                        }
                        testld_update_libpath (dl, path);
                    }
                    if (rc)
                        break;

                    /* append contents of LD_LIBRARY_PATH */
                    path = getenv ( "LD_LIBRARY_PATH" );

                    if (path)
                        testld_update_libpath (dl, path);

                    /* create libset */
                    rc = KDyldMakeSet (dl, & libs);
                    if (rc)
                    {
                        LOGERR (klogInt, rc, "failed to create dl library set");
                        break;
                    }
                    else
                    {
                        /* test it */
                        for (ix = 0; ix < num_libs; ++ ix )
                        {
                            rc = ArgsParamValue (args, ix, &path);
                            if (rc)
                                break;

                            testld_load_library (dl, libs, path);
                        }

                        KDlsetRelease ( libs );
                    }
                } while (0);

                KDyldRelease ( dl );
            }

        } while (0);

        ArgsWhack (args);
    }
    return rc;
}
