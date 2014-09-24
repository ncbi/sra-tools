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
#include <klib/rc.h>
#include <klib/out.h>
#include <klib/text.h>
#include <klib/log.h>
#include <klib/printf.h>

#include <kapp/main.h>
#include <kapp/args.h>

#include <kfs/file.h>
#include <kfs/directory.h>
#include <kfs/buffile.h>
#include <kfs/bzip.h>
#include <kfs/gzip.h>

#include <vdb/manager.h>

#include <align/manager.h>
#include <align/reference.h>

#include <sra/srapath.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

static bool string_ends_with( const char * s, const char * pattern )
{
    const char * found = strstr ( s, pattern );
    if ( found == NULL )
        return false;
    else
    {
        size_t len_string = string_size ( s );
        size_t len_pattern = string_size ( pattern );
        size_t found_at = ( found - s );
        return ( found_at == ( len_string - len_pattern ) );
    }
}


static char * fasta_ref = "--fastaref";

/* we have to test if argv[0] ends in 'fastq-dump' or 'fastq-dump.exe',
   and we have an argument called '--fasta' */
bool fasta_dump_requested( int argc, char* argv[] )
{
    if ( string_ends_with( argv[ 0 ], "fastq-dump" ) ||
         string_ends_with( argv[ 0 ], "fastq-dump.exe" ) )
    {
        int i;
        for ( i = 1; i < argc; ++i )
        {
            if ( strcmp( argv[ i ], fasta_ref ) == 0 )
                return true;
        }
    }

    return false;
}

#define OPTION_OUTF    "outfile"
#define ALIAS_OUTF     "o"

#define OPTION_WIDTH   "width"
#define ALIAS_WIDTH    "w"

#define OPTION_GZIP    "gzip"
#define ALIAS_GZIP     NULL

#define OPTION_BZIP    "bzip2"
#define ALIAS_BZIP     NULL

#define OPTION_REFNAME "refname"
#define ALIAS_REFNAME  "r"

#define OPTION_FROM    "from"
#define ALIAS_FROM     "N"

#define OPTION_LENGTH   "length"
#define ALIAS_LENGTH    "X"

#define OPTION_FAREF    "fastaref"
#define ALIAS_FAREF     NULL


OptDef MyOptions[] =
{
    /*name,           alias,         hfkt, usage-help,    maxcount, needs value, required */
    { OPTION_OUTF,    ALIAS_OUTF,    NULL, NULL,          1,        true,        false },
    { OPTION_WIDTH,   ALIAS_WIDTH,   NULL, NULL,          1,        true,        false },
    { OPTION_GZIP,    ALIAS_GZIP,    NULL, NULL,          1,        false,       false },
    { OPTION_BZIP,    ALIAS_BZIP,    NULL, NULL,          1,        false,       false },
    { OPTION_REFNAME, ALIAS_REFNAME, NULL, NULL,          0,        true,        false },
    { OPTION_FROM,    ALIAS_FROM,    NULL, NULL,          1,        true,        false },
    { OPTION_LENGTH,  ALIAS_LENGTH,  NULL, NULL,          1,        true,        false },
    { OPTION_FAREF,   ALIAS_FAREF,   NULL, NULL,          1,        false,       false }
};


/* =========================================================================================== */


/* GLOBAL VARIABLES */
struct {
    KWrtWriter org_writer;
    void* org_data;
    KFile* kfile;
    uint64_t pos;
} g_out_writer = { NULL };


/* =========================================================================================== */

static rc_t get_str_option( const Args *args, const char *name, const char ** res )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    *res = NULL;
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "ArgsOptionCount() failed" );
    }
    else
    {
        if ( count > 0 )
        {
            rc = ArgsOptionValue( args, name, 0, res );
            if ( rc != 0 )
            {
                LOGERR( klogInt, rc, "ArgsOptionValue() failed" );
            }
        }
    }
    return rc;
}


static rc_t get_bool_option( const Args *args, const char *name, bool *res, const bool def )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, name, &count );
    if ( rc == 0 && count > 0 )
    {
        *res = true;
    }
    else
    {
        *res = def;
    }
    return rc;
}


static rc_t get_int_option( const Args *args, const char *name, uint32_t *res, const uint32_t def )
{
    const char * s;
    rc_t rc = get_str_option( args, name, &s );
    *res = def;
    if ( rc == 0 && s != NULL )
    {
        *res = atoi( s );
    }
    return rc;
}


/* =========================================================================================== */


static rc_t CC BufferedWriter ( void* self, const char* buffer, size_t bufsize, size_t* num_writ )
{
    rc_t rc = 0;

    assert( buffer != NULL );
    assert( num_writ != NULL );

    do {
        rc = KFileWrite( g_out_writer.kfile, g_out_writer.pos, buffer, bufsize, num_writ );
        if ( rc == 0 )
        {
            buffer += *num_writ;
            bufsize -= *num_writ;
            g_out_writer.pos += *num_writ;
        }
    } while ( rc == 0 && bufsize > 0 );
    return rc;
}


static rc_t set_stdout_to( bool gzip, bool bzip2, const char * filename, size_t bufsize )
{
    rc_t rc = 0;
    if ( gzip && bzip2 )
    {
        rc = RC( rcApp, rcFile, rcConstructing, rcParam, rcAmbiguous );
    }
    else
    {
        KDirectory *dir;
        rc = KDirectoryNativeDir( &dir );
        if ( rc != 0 )
            LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
        else
        {
            KFile *of;
            rc = KDirectoryCreateFile ( dir, &of, false, 0664, kcmInit, "%s", filename );
            if ( rc == 0 )
            {
                KFile *buf;
                if ( gzip )
                {
                    KFile *gz;
                    rc = KFileMakeGzipForWrite( &gz, of );
                    if ( rc == 0 )
                    {
                        KFileRelease( of );
                        of = gz;
                    }
                }
                if ( bzip2 )
                {
                    KFile *bz;
                    rc = KFileMakeBzip2ForWrite( &bz, of );
                    if ( rc == 0 )
                    {
                        KFileRelease( of );
                        of = bz;
                    }
                }

                rc = KBufFileMakeWrite( &buf, of, false, bufsize );
                if ( rc == 0 )
                {
                    g_out_writer.kfile = buf;
                    g_out_writer.org_writer = KOutWriterGet();
                    g_out_writer.org_data = KOutDataGet();
                    rc = KOutHandlerSet( BufferedWriter, &g_out_writer );
                    if ( rc != 0 )
                        LOGERR( klogInt, rc, "KOutHandlerSet() failed" );
                }
                KFileRelease( of );
            }
            KDirectoryRelease( dir );
        }
    }
    return rc;
}


static void release_stdout_redirection( void )
{
    KFileRelease( g_out_writer.kfile );
    if( g_out_writer.org_writer != NULL )
    {
        KOutHandlerSet( g_out_writer.org_writer, g_out_writer.org_data );
    }
    g_out_writer.org_writer = NULL;
}


static rc_t CC write_to_FILE( void *f, const char *buffer, size_t bytes, size_t *num_writ )
{
    * num_writ = fwrite ( buffer, 1, bytes, f );
    if ( * num_writ != bytes )
        return RC( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
    return 0;
}


/* =========================================================================================== */

typedef struct fasta_options
{
    uint32_t width;
    bool gzip;
    bool bzip;
    const char * outfile;
    INSDC_coord_zero from;
    INSDC_coord_len length;
} fasta_options;


static rc_t get_fasta_options( Args * args, fasta_options *opts )
{
    rc_t rc = get_int_option( args, OPTION_WIDTH, &opts->width, 80 );

    if ( rc == 0 )
        rc = get_str_option( args, OPTION_OUTF, &opts->outfile );

    if ( rc == 0 )
        rc = get_bool_option( args, OPTION_GZIP, &opts->gzip, false );

    if ( rc == 0 )
        rc = get_bool_option( args, OPTION_BZIP, &opts->bzip, false );

    if ( rc == 0 )
    {
        uint32_t x;
        rc = get_int_option( args, OPTION_FROM, &x, 0 );
        if ( rc == 0 )
            opts->from = x;
    }

    if ( rc == 0 )
    {
        uint32_t x;
        rc = get_int_option( args, OPTION_LENGTH, &x, 0 );
        if ( rc == 0 )
            opts->length = x;
    }

    return rc;
}

/* =========================================================================================== */

#if TOOLS_USE_SRAPATH != 0
static bool is_this_a_filesystem_path( const char * path )
{
    bool res = false;
    size_t i, n = string_size ( path );
    for ( i = 0; i < n && !res; ++i )
    {
        char c = path[ i ];
        res = ( c == '.' || c == '/' || c == '\\' );
    }
    return res;
}
#endif

#if TOOLS_USE_SRAPATH != 0
static char *translate_accession( SRAPath *my_sra_path,
                           const char *accession,
                           const size_t bufsize )
{
    rc_t rc;
    char * res = calloc( 1, bufsize );
    if ( res == NULL ) return NULL;

    rc = SRAPathFind( my_sra_path, accession, res, bufsize );
    if ( GetRCState( rc ) == rcNotFound )
    {
        free( res );
        return NULL;
    }
    else if ( GetRCState( rc ) == rcInsufficient )
    {
        free( res );
        return translate_accession( my_sra_path, accession, bufsize * 2 );
    }
    else if ( rc != 0 )
    {
        free( res );
        return NULL;
    }
    return res;
}
#endif

#if TOOLS_USE_SRAPATH != 0
static rc_t resolve_accession( const KDirectory *my_dir, char ** path )
{
    SRAPath *my_sra_path;
    rc_t rc = 0;

    if ( strchr ( *path, '/' ) != NULL )
        return 0;

    rc = SRAPathMake( &my_sra_path, my_dir );
    if ( rc != 0 )
    {
        if ( GetRCState ( rc ) != rcNotFound || GetRCTarget ( rc ) != rcDylib )
        {
            if ( rc != 0 )
                LOGERR( klogInt, rc, "SRAPathMake() failed" );
        }
        else
            rc = 0;
    }
    else
    {
        if ( !SRAPathTest( my_sra_path, *path ) )
        {
            char *buf = translate_accession( my_sra_path, *path, 64 );
            if ( buf != NULL )
            {
                free( (char*)(*path) );
                *path = buf;
            }
        }
        SRAPathRelease( my_sra_path );
    }
    return rc;
}
#endif

/* =========================================================================================== */


typedef struct fasta_ctx
{
    KDirectory *dir;
    const VDBManager *vdb_mgr;
    const AlignMgr *almgr;
} fasta_ctx;


static rc_t dump_reference_loop( fasta_options *opts, const ReferenceObj* refobj )
{
    rc_t rc = 0;
    uint8_t * buffer = malloc( opts->width + 2 );
    if ( buffer == NULL )
    {
        rc = RC( rcExe, rcNoTarg, rcListing, rcMemory, rcExhausted );
        LOGERR( klogInt, rc, "allocating reference-buffer failed!" );
    }
    else
    {
        INSDC_coord_zero pos = opts->from;
        INSDC_coord_len w = opts->length;

        while ( w > 0 && rc == 0 )
        {
            INSDC_coord_len written;
            INSDC_coord_len to_read = ( w > opts->width ? opts->width : w );
            rc = ReferenceObj_Read( refobj, pos, to_read, buffer, &written );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "ReferenceObj_Read() failed" );
            else
            {
                OUTMSG(( "%.*s\n", written, buffer ));
                w -= written;
                pos += written;
            }
        }
        free( buffer );
    }
    return rc;
}


static rc_t dump_reference( fasta_options *opts, const ReferenceObj* refobj )
{
    const char * name;
    rc_t rc = ReferenceObj_Name( refobj, &name );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "ReferenceObj_Name() failed!" );
    else
    {
        INSDC_coord_len seq_len;
        rc = ReferenceObj_SeqLength( refobj, &seq_len );
        if ( rc != 0 )
            LOGERR( klogInt, rc, "ReferenceObj_SeqLength() failed!" );
        else
        {
            if ( opts->from == 0 )
            {
                if ( opts->length == 0 )
                    opts->length = seq_len;
            }
            else
            {
                if ( opts->length == 0 )
                    opts->length = ( seq_len - opts->from );
            }
            if ( opts->from >= seq_len )
            {
                rc = RC( rcExe, rcNoTarg, rcListing, rcParam, rcInvalid );
                LOGERR( klogInt, rc, "reference-start-point beyond reference-end!" );
            }
            else
            {
                if ( opts->from + opts->length > seq_len )
                    opts->length = ( seq_len - opts->from );
                OUTMSG(( ">%s|from %u|%u bases\n", name, opts->from, opts->from + opts->length -1 ));
                rc = dump_reference_loop( opts, refobj );
            }
        }
    }
    return 0;
}


static rc_t dump_all_reference( fasta_options *opts, const ReferenceList* reflist )
{
    uint32_t count;
    rc_t rc = ReferenceList_Count( reflist, &count );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "ReferenceList_Count() failed!" );
    else
    {
        uint32_t i;
        for ( i = 0; i < count && rc == 0; ++i )
        {
            const ReferenceObj* refobj;
            rc = ReferenceList_Get( reflist, &refobj, i );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "ReferenceList_Get() failed!" );
            else
                rc = dump_reference( opts, refobj );
        }
    }
    return rc;
}


static rc_t foreach_refname( Args * args, fasta_options *opts, fasta_ctx *ctx, const char * path )
{
    uint32_t count;
    rc_t rc = ArgsOptionCount( args, OPTION_REFNAME, &count );
    if ( rc != 0 )
    {
        LOGERR( klogInt, rc, "ArgsOptionCount( REFNAME ) failed" );
    }
    else
    {
        const ReferenceList* reflist;

        rc = ReferenceList_MakePath( &reflist, ctx->vdb_mgr, path,
                        ereferencelist_usePrimaryIds, 0, NULL, 0 );
        if ( rc != 0 )
            LOGERR( klogInt, rc, "ReferenceList_MakePath() failed!" );
        else
        {
            if ( count == 0 )
            {
                /* dump all references... */
                rc = dump_all_reference( opts, reflist );
            }
            else
            {
                uint32_t i;
                for ( i = 0; i < count && rc == 0; ++i )
                {
                    const char * refname;
                    rc = ArgsOptionValue( args, OPTION_REFNAME, 0, &refname );
                    if ( rc != 0 )
                    {
                        LOGERR( klogInt, rc, "ArgsOptionValue( REFNAME ) failed" );
                    }
                    else
                    {
                        const ReferenceObj* refobj;
                        rc = ReferenceList_Find( reflist, &refobj, refname, string_size( refname ) );
                        if ( rc != 0 )
                            LOGERR( klogInt, rc, "ReferenceList_Find() failed" );
                        else
                            rc = dump_reference( opts, refobj );
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t foreach_argument( Args * args, fasta_options *opts, fasta_ctx *ctx )
{
    uint32_t count;
    rc_t rc = ArgsParamCount( args, &count );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "ArgsParamCount() failed" );
    else
    {
        uint32_t idx;
        if ( count < 1 )
        {
            rc = RC( rcExe, rcNoTarg, rcListing, rcParam, rcEmpty );
            LOGERR( klogInt, rc, "no source-files given!" );
        }
        else
        {
            for ( idx = 0; idx < count && rc == 0; ++idx )
            {
                const char *param = NULL;
                rc = ArgsParamValue( args, idx, &param );
                if ( rc != 0 )
                    LOGERR( klogInt, rc, "ArgsParamvalue() failed" );
                else
                {
                    char *path = string_dup_measure ( param, NULL );
                    if ( path == NULL )
                    {
                        rc = RC( rcExe, rcNoTarg, rcListing, rcMemory, rcExhausted );
                        LOGERR( klogInt, rc, "allocating path to source-file failed!" );
                    }
                    else
                    {

#if TOOLS_USE_SRAPATH != 0
                        if ( !is_this_a_filesystem_path( path ) )
                        {
                            rc = resolve_accession( ctx->dir, &path );
                        }
#endif

                        if ( rc == 0 )
                        {
                            rc = foreach_refname( args, opts, ctx, path );
                        }
                        free( path );
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t fasta_main( Args * args, fasta_options *opts )
{
    rc_t rc;
    fasta_ctx ctx;

    memset( &ctx, 0, sizeof ctx );
    rc = KDirectoryNativeDir( &ctx.dir );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "KDirectoryNativeDir() failed" );
    else
    {
        rc = VDBManagerMakeRead ( &ctx.vdb_mgr, ctx.dir );
        if ( rc != 0 )
            LOGERR( klogInt, rc, "VDBManagerMakeRead() failed" );
        else
        {
            rc = AlignMgrMakeRead ( &ctx.almgr );
            if ( rc != 0 )
                LOGERR( klogInt, rc, "AlignMgrMake() failed" );
            else
            {
                rc = foreach_argument( args, opts, &ctx );
                AlignMgrRelease ( ctx.almgr );
            }
            VDBManagerRelease( ctx.vdb_mgr );
        }
        KDirectoryRelease( ctx.dir );
    }
    return rc;
}

/* =========================================================================================== */


rc_t fasta_dump( int argc, char* argv[] )
{
    rc_t rc = KOutHandlerSet( write_to_FILE, stdout );
    if ( rc != 0 )
        LOGERR( klogInt, rc, "KOutHandlerSet() failed" );
    else
    {
        Args * args;

        KLogHandlerSetStdErr();
        rc = ArgsMakeAndHandle( &args, argc, argv, 1,
            MyOptions, sizeof MyOptions / sizeof MyOptions [ 0 ] );
        if ( rc == 0 )
        {
            fasta_options opts;
            rc = get_fasta_options( args, &opts );
            if ( rc == 0 )
            {
                if ( opts.outfile != NULL )
                {
                    rc = set_stdout_to( opts.gzip,
                                        opts.bzip,
                                        opts.outfile,
                                        32 * 1024 );
                }

                if ( rc == 0 )
                {
                    /* ============================== */
                    rc = fasta_main( args, &opts );
                    /* ============================== */
                }

                if ( opts.outfile != NULL )
                    release_stdout_redirection();
            }
            ArgsWhack( args );
        }
    }
    return rc;
}


static const char * outf_usage[]     = { "output to stdout if omitted", NULL };
static const char * width_usage[]    = { "default = 80 bases", NULL };
static const char * gzip_usage[]     = { "for file and stdout", NULL };
static const char * bzip_usage[]     = { "for file and stdout", NULL };
static const char * refname_usage[]  = { "multiple ref's permitted", NULL };
static const char * from_usage[]     = { "zero-based", NULL };
static const char * length_usage[]   = { NULL };

rc_t CC fasta_dump_usage ( const Args * args )
{
    rc_t rc;
    const char * fullname = "fastq-dump";
    const char * progname = fullname;

    if ( args == NULL )
        rc = RC( rcApp, rcArgv, rcAccessing, rcSelf, rcNull );
    else
        rc = ArgsProgram( args, &fullname, &progname );

    KOutMsg( "\nUsage:\n"
              "  %s <path> [options]\n"
              "\n", progname );

    KOutMsg( "Options:\n" );

    HelpOptionLine( ALIAS_OUTF,    OPTION_OUTF,    "output-file", outf_usage );
    HelpOptionLine( ALIAS_WIDTH,   OPTION_WIDTH,   "bases per line", width_usage );
    HelpOptionLine( ALIAS_GZIP,    OPTION_GZIP,    "output gzipped", gzip_usage );
    HelpOptionLine( ALIAS_BZIP,    OPTION_BZIP,    "output bzipped", bzip_usage );
    HelpOptionLine( ALIAS_REFNAME, OPTION_REFNAME, "reference to dump", refname_usage );
    HelpOptionLine( ALIAS_FROM,    OPTION_FROM,    "start-offset", from_usage );
    HelpOptionLine( ALIAS_LENGTH,  OPTION_LENGTH,  "length of sequence", length_usage );

    HelpOptionsStandard ();
    HelpVersion ( fullname, KAppVersion() );
    return rc;
}
