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

#include <kapp/args.h>
#include <klib/checksum.h>
#include <klib/status.h>
#include <klib/out.h>
#include <klib/rc.h>
#include <kapp/main.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>

static
int crc32sum_calc ( FILE *in, uint32_t *crc32 )
{
    int status;
    char *buff = malloc ( 32 * 1024 );
    if ( buff == NULL )
        return errno;

    for ( status = 0, * crc32 = 0;; )
    {
        size_t num_read = fread ( buff, 1, 32 * 1024, in );
        if ( num_read == 0 )
        {
            if ( ! feof ( in ) )
                status = ferror ( in );
            break;
        }

        * crc32 = CRC32 ( * crc32, buff, num_read );
    }

    free ( buff );

    return status;
}

static
int crc32sum_check ( FILE *in, const char *fname )
{
    int cnt, mismatches;
    char line [ 5 * 1024 ];
    for ( cnt = mismatches = 0; fgets ( line, sizeof line, in ) != NULL; ++ cnt )
    {
        char *p;
        FILE *src;
        int status, bin = 0;
        uint32_t prior, crc32;

        if ( line [ 0 ] == 0 )
        {
            -- cnt;
            continue;
        }

        p = strrchr ( line, '\n' );
        if ( p != 0 )
            * p = 0;

        prior = strtoul ( line, & p, 16 );
        if ( ( p - line ) != 8 || p [ 0 ] != ' ' )
        {
            fprintf ( stderr, "badly formatted file '%s'\n", fname );
            return EINVAL;
        }

        if ( p [ 1 ] == '*' )
            bin = 1;
        else if ( p [ 1 ] != ' ' )
        {
            fprintf ( stderr, "badly formatted file '%s'\n", fname );
            return EINVAL;
        }
 
        src = fopen ( p += 2, bin ? "rb" : "r" );
        if ( src == NULL )
        {
            status = errno;
            fprintf ( stderr, "failed to open file '%s': %s\n", p, strerror ( status ) );
            return status;
        }

        status = crc32sum_calc ( src, & crc32 );

        fclose ( src );

        if ( status != 0 )
            fprintf ( stderr, "error processing file '%s': %s\n", p, strerror ( status ) );
        else
        {
            printf ( "%s: %s\n", p, ( crc32 == prior ) ? "OK" : "FAILED" );
            if ( crc32 != prior )
                ++ mismatches;
        }
    }

    if ( mismatches != 0 )
        fprintf ( stderr, "WARNING: %d of %d computed checksums did NOT match\n", mismatches, cnt );

    if ( ! feof ( in ) )
        return ferror ( in );
    return 0;
}

static
int crc32sum_gen ( FILE *in, const char *fname, int bin )
{
    uint32_t crc32;
    int status = crc32sum_calc ( in, & crc32 );
    if ( status != 0 )
        fprintf ( stderr, "error processing file '%s': %s\n", fname, strerror ( status ) );
    else
        printf ( "%08x %c%s\n", crc32, bin ? '*' : ' ', fname );
    return status;
}


#define OPTION_BINARY "binary"
#define OPTION_CHECK  "check"
#define ALIAS_BINARY  "b"
#define ALIAS_CHECK   "c"

static const char * binary_usage[] = { "open file in binary mode", NULL };
static const char * check_usage[]  = { "check CRC32 against given list", NULL };

OptDef Options[] =
{
    { OPTION_BINARY, ALIAS_BINARY, NULL, binary_usage, 0, false, false },
    { OPTION_CHECK,  ALIAS_CHECK,  NULL, check_usage,  0, false, false }
};



const char UsageDefaultName[] = "crc32sum";

rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg("\n"
                   "Usage:\n"
                   "  %s [Options] File [File ...]\n"
                   "\n"
                   "Summary:\n"
                   "  Generate or test crc32 file checks\n"
                   "\n", progname);
}


rc_t CC Usage ( const Args * args )
{
    const char * progname = UsageDefaultName;
    const char * fullpath = UsageDefaultName;
    rc_t rc;

    if (args == NULL)
        rc = RC (rcApp, rcArgv, rcAccessing, rcSelf, rcNull);
    else
        rc = ArgsProgram (args, &fullpath, &progname);

    UsageSummary (progname);

    KOutMsg ("Options\n");

    HelpOptionLine (ALIAS_BINARY, OPTION_BINARY, NULL, binary_usage);

    HelpOptionLine (ALIAS_CHECK, OPTION_CHECK, NULL, check_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


ver_t CC KAppVersion ( void )
{
    return 0;
}


rc_t CC KMain (int argc, char * argv [])
{
    Args *args;
    rc_t rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1,
                            Options, sizeof (Options) / sizeof (OptDef));
    if (rc == 0)
    {
        do
        {
            uint32_t pcount;
            int check;
            int bin;

            rc = ArgsOptionCount (args, OPTION_BINARY, &pcount);
            if (rc) break;

            bin = (pcount != 0);

            rc = ArgsOptionCount (args, OPTION_CHECK, &pcount);
            if (rc) break;

            check = (pcount != 0);

            rc = ArgsParamCount (args, &pcount);
            if (rc) break;

            if (pcount == 0)
            {
                MiniUsage(args);
            }
            else
            {
                uint32_t i;

                CRC32Init ();
           
                for ( i = 0; i < pcount; ++ i )
                {

                    int status;
                    const char *fname;
                    FILE *in;

                    rc = ArgsParamValue (args, i, &fname);

                    in = fopen ( fname, bin ? "rb" : "r" );

                    if ( in == NULL )
                    {
                        fprintf ( stderr, "failed to open file '%s'\n", fname );
                        return -1;
                    }

                    status = check ?
                        crc32sum_check ( in, fname ):
                        crc32sum_gen ( in, fname, bin );

                    fclose ( in );

                    if ( status != 0 )
                        return status;
                }
            }
        } while (0);
    }

    return 0;
}
