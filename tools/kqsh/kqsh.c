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

#include "kqsh.vers.h"
#include "kqsh-priv.h"
#include "kqsh-tok.h"

#include <klib/container.h>
#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/mmap.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/rc.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

bool read_only;
bool interactive;


/* exec_file
 *  execute a named file
 */
rc_t kqsh_exec_file ( KSymTable *tbl, const String *path )
{
    KDirectory *wd;
    rc_t rc = KDirectoryNativeDir ( & wd );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to open working directory" );
    else
    {
        const KFile *f;
        rc = KDirectoryOpenFileRead ( wd, & f, "%.*s"
            , ( int ) path -> size, path -> addr );
        if ( rc != 0 )
        {
            PLOGERR ( klogErr,  (klogErr, rc, "failed to open file '$(path)'", "path=%.*s"
                                 , ( int ) path -> size, path -> addr ));
        }
        else
        {
            const KMMap *mm;
            rc = KMMapMakeRead ( & mm, f );
            if ( rc != 0 )
            {
                PLOGERR ( klogErr,  (klogErr, rc, "failed to map file '$(path)'", "path=%.*s"
                                     , ( int ) path -> size, path -> addr ));
            }
            else
            {
                const void *addr;
                rc = KMMapAddrRead ( mm, & addr );
                if ( rc != 0 )
                    LOGERR ( klogInt, rc, "failed to obtain mmap addr" );
                else
                {
                    size_t size;
                    rc = KMMapSize ( mm, & size );
                    if ( rc != 0 )
                        LOGERR ( klogInt, rc, "failed to obtain mmap size" );
                    else
                    {
                        String text;
                        KTokenText tt;
                        KTokenSource src;

                        StringInit ( & text, addr, ( size_t ) size,
                            string_len ( addr, ( size_t ) size ) );
                        KTokenTextInit ( & tt, & text, path );
                        KTokenSourceInit ( & src, & tt );

                        rc = kqsh ( tbl, & src );
                    }
                }

                KMMapRelease ( mm );
            }

            KFileRelease ( f );
        }

        KDirectoryRelease ( wd );
    }

    return rc;
}

/* init_symtable
 *  initialize tool table
 */
static
rc_t kqsh_init_keywords ( KSymTable *tbl )
{
    int i;
    static struct
    {
        const char *keyword;
        int id;
    } kw [] =
    {
#define KEYWORD( word ) \
        { # word, kw_ ## word }
#define RESERVED( word ) \
        { # word, rsrv_ ## word }

        /* keywords */
        KEYWORD ( add ),
        KEYWORD ( alias ),
        KEYWORD ( alter ),
        KEYWORD ( as ),
        KEYWORD ( at ),
        KEYWORD ( close ),
        KEYWORD ( column ),
        KEYWORD ( columns ),
        KEYWORD ( commands ),
        KEYWORD ( compact ),
        KEYWORD ( constants ),
        KEYWORD ( create ),
        KEYWORD ( cursor ),
        KEYWORD ( database ),
        KEYWORD ( databases ),
        KEYWORD ( delete ),
        KEYWORD ( drop ),
        KEYWORD ( execute ),
        KEYWORD ( exit ),
        KEYWORD ( for ),
        KEYWORD ( formats ),
        KEYWORD ( functions ),
        KEYWORD ( help ),
        KEYWORD ( include ),
        KEYWORD ( initialize ),
        KEYWORD ( insert ),
        KEYWORD ( kdb ),
        KEYWORD ( library ),
        KEYWORD ( load ),
        KEYWORD ( manager ),
        KEYWORD ( metadata ),
        KEYWORD ( objects ),
        KEYWORD ( on ),
        KEYWORD ( open ),
        KEYWORD ( or ),
        KEYWORD ( path ),
        KEYWORD ( quit ),
        KEYWORD ( rename ),
        KEYWORD ( replace ),
        KEYWORD ( row ),
        KEYWORD ( schema ),
        KEYWORD ( show ),
        KEYWORD ( sra ),
        KEYWORD ( table ),
        KEYWORD ( tables ),
        KEYWORD ( text ),
        KEYWORD ( types ),
        KEYWORD ( typesets ),
        KEYWORD ( update ),
        KEYWORD ( use ),
        KEYWORD ( using ),
        KEYWORD ( version ),
        KEYWORD ( vdb ),
        KEYWORD ( with ),
        KEYWORD ( write ),

        /* reserved names */
        RESERVED ( kmgr ),
        RESERVED ( vmgr ),
        RESERVED ( sramgr ),
        RESERVED ( srapath ),

        /* abbreviations */
        KEYWORD ( col ),
        KEYWORD ( cols ),
        KEYWORD ( db ),
        KEYWORD ( dbs ),
        KEYWORD ( exec ),
        KEYWORD ( init ),
        KEYWORD ( lib ),
        KEYWORD ( mgr ),
        KEYWORD ( tbl )

#undef KEYWORD
#undef RESERVED
    };


    /* define keywords */
    for ( i = 0; i < sizeof kw / sizeof kw [ 0 ]; ++ i )
    {
        rc_t rc;

        String name;
        StringInitCString ( & name, kw [ i ] . keyword );
        rc = KSymTableCreateSymbol ( tbl, NULL, & name, kw [ i ] . id, NULL );
        if ( rc != 0 )
            return rc;
    }

    return 0;
}

static
rc_t kqsh_init_symtable ( KSymTable *tbl, BSTree *intrinsic )
{
    rc_t rc = KSymTablePushScope ( tbl, intrinsic );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to initialize symbol table" );
    else
    {
        rc = kqsh_init_keywords ( tbl );

        KSymTablePopScope ( tbl );
    }

    return rc;
}


/* whackobj
 *  whacks created/opened objects
 */
static
rc_t CC kqsh_read_stdin ( void *data, KTokenText *tt, size_t save )
{
    rc_t rc;
    size_t num_read;
    const KFile *self = ( const void* ) data;

    /* manage stdin for the process */
    static uint64_t pos;
    static char buff [ 4096 ];

    /* save any characters not yet consumed */
    if ( save != 0 )
    {
        assert ( save < sizeof buff );
        memmove ( buff, & tt -> str . addr [ tt -> str . size - save ], save );
    }

    /* read as many characters as are available */
    rc = KFileRead ( self, pos, & buff [ save ], sizeof buff - save, & num_read );
    if ( rc != 0 )
        LOGERR ( klogErr, rc, "failed to read stdin" );
    else
    {
        /* reset the buffer in "tt" */
        tt -> str . addr = buff;
        tt -> str . size = save + num_read;
        tt -> str . len = string_len ( buff, save + num_read );
        pos += num_read;
    }

    return rc;
}

/* init_console
 */
static
rc_t kqsh_init_console ( KTokenText *console )
{
    const KFile *std_in;
    rc_t rc = KFileMakeStdIn ( & std_in );
    if ( rc != 0 )
        LOGERR ( klogErr, rc, "failed to init stdin" );
    else
    {
        KTokenTextInitCString ( console, "", "stdin" );
        console -> read = kqsh_read_stdin;
        console -> data = ( void* ) std_in;

        if ( KFileType ( std_in ) == kfdCharDev )
            interactive = true;
    }
    return rc;
}


/* Version  EXTERN
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
rc_t CC KAppVersion ( void )
{
    return KQSH_VERS;
}
    

#define OPTION_UPDATE  "update"
#define OPTION_LIBPATH "lib-path"
#define ALIAS_UPDATE   "u"
#define ALIAS_LIBPATH  "l"

static const char * update_usage[] = { "use update managers", NULL };
static const char * libpath_usage[] = { "add to load library path", NULL };

OptDef MyOptions [] = 
{
    { OPTION_UPDATE,  ALIAS_UPDATE,  NULL, update_usage,  0, false, false },
    { OPTION_LIBPATH, ALIAS_LIBPATH, NULL, libpath_usage, 0, true, false }
};

const char UsageDefaultName[] = "kqsh";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [ options ] [ file ... ]\n"
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
    if (rc)
        progname = fullpath = UsageDefaultName;

    UsageSummary (progname);

    KOutMsg ("Options:\n");

    HelpOptionLine (ALIAS_UPDATE, OPTION_UPDATE, NULL, update_usage);

    HelpOptionLine (ALIAS_LIBPATH, OPTION_LIBPATH, "path", libpath_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
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
static
rc_t kqsh_main ( int argc, char *argv [] )
{
    rc_t rc;
    Args *args;

    rc = ArgsMakeAndHandle ( & args, argc, argv, 1, MyOptions,
        sizeof MyOptions / sizeof MyOptions [ 0 ] );
    if ( rc == 0 ) do 
    {
        uint32_t ix;
        uint32_t pcount;
        const char * pc;

        /* did anyone ask to open update managers? */
        rc = ArgsOptionCount ( args, OPTION_UPDATE, & pcount );
        if ( rc != 0 )
        {
            PLOGERR ( klogInt, ( klogInt, rc, "failed to retrieve '$(option)' option count", "option=" OPTION_UPDATE ) );
            break;
        }

        /* record zero count as read-only */
        read_only = ( pcount == 0 );

        /* did anyone ask to add a custom library search path? */
        rc = ArgsOptionCount ( args, OPTION_LIBPATH, & pcount );
        if ( rc != 0 )
        {
            PLOGERR ( klogInt, ( klogInt, rc, "failed to retrieve '$(option)' option count", "option=" OPTION_LIBPATH ) );
            break;
        }

        /* add each path in order */
        for ( ix = 0; ix < pcount; ++ ix )
        {
            rc = ArgsOptionValue ( args, OPTION_LIBPATH, ix, & pc );
            if ( rc != 0 )
            {
                PLOGERR ( klogInt, ( klogInt, rc, "failed to retrieve '$(option)' option value [ $(idx) ]", "option=" OPTION_LIBPATH ",idx=%u", ix ) );
                break;
            }

            rc = kqsh_update_libpath ( pc );
            if ( rc != 0 )
                break;
        }

        /* add system library path */
        if ( rc == 0 )
            rc = kqsh_system_libpath ();
        if ( rc == 0 )
        {
            KSymTable tbl;

            BSTree intrinsic;
            BSTreeInit ( & intrinsic );

            rc = KSymTableInit ( & tbl, & intrinsic );
            if ( rc != 0 )
                LOGERR ( klogInt, rc, "failed to initialize symbol table" );
            else
            {
                rc = kqsh_init_symtable ( & tbl, & intrinsic );
                while ( rc == 0 )
                {
                    rc = ArgsParamCount ( args, & pcount );
                    if ( rc != 0 )
                    {
                        LOGERR ( klogInt, rc, "failed to retrieve parameter count" );
                        break;
                    }

                    if ( pcount == 0 )
                    {
                        KTokenText console;
                        
                        rc = kqsh_init_console ( & console );
                        if ( rc == 0 )
                        {
                            KTokenSource src;
                            
                            KTokenSourceInit ( & src, & console );
                                
                            if ( interactive )
                            {
                                uint32_t vers = KAppVersion ();
                                
                                kqsh_printf ( "\nkqsh version %u.%u"
                                              , ( vers >> 24 )
                                              , ( vers >> 16 ) & 0xFF );
                                if ( ( vers & 0xFFFF ) != 0 )
                                    kqsh_printf ( ".%u", vers & 0xFFFF );
                            }
                            
                            rc = kqsh ( & tbl, & src );
                            KFileRelease ( console . data );
                        }
                    }
            
                    else for ( ix = 1; ix <= pcount; ++ ix )
                    {
                        const char *file = argv [ ix ];

                        String path;
                        StringInitCString ( & path, file );

                        rc = kqsh_exec_file ( & tbl, & path );
                        if ( rc != 0 )
                            break;
                    }

                    KSymTableWhack ( & tbl );
                    break; /* never really loop */
                }

                BSTreeWhack ( & intrinsic, KSymbolWhack, NULL );
            }
        }

    } while ( 0 );

    return rc;
}

rc_t CC KMain ( int argc, char *argv [] )
{
    rc_t rc;

    KLogHandlerSetStdErr ();
    KLogLibHandlerSetStdErr ();

    rc = kqsh_init_libpath ();
    if ( rc == 0 )
    {
        read_only = true;
        rc = kqsh_main ( argc, argv );
        kqsh_whack_libpath ();
    }
    return rc;
}
