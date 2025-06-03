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

#include "kar-args.h"

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/container.h>
#include <klib/sort.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/text.h>
#include <klib/printf.h>
#include <klib/time.h>
#include <sysalloc.h>
#include <kfs/directory.h>
#include <kfs/file.h>
#include <kfs/toc.h>
#include <kfs/sra.h>
#include <kfs/md5.h>

#include <kapp/main.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <endian.h>
#include <byteswap.h>


/*******************************************************************************
 * Globals + Forwards + Declarations + Definitions
 */

static uint64_t num_files;
static uint64_t max_size = 0;
static uint64_t max_offset = 0;
static uint32_t max_size_fw;
static uint32_t max_offset_fw;


typedef struct KARDir KARDir;

typedef struct KAREntry KAREntry;
struct KAREntry
{
    BSTNode dad;

    KTime_t mod_time;

    const char *name;
    KARDir *parentDir;

    uint32_t access_mode;

    uint8_t type;
};

struct KARDir
{
    KAREntry dad;

    BSTree contents;
};

typedef struct KARFile KARFile;
struct KARFile
{
    KAREntry dad;

    uint64_t byte_size;
    uint64_t byte_offset;
};

typedef KARFile **KARFilePtrArray;

typedef struct KARArchiveFile KARArchiveFile;
struct KARArchiveFile
{
    uint64_t starting_pos;
    uint64_t pos;
    KFile * archive;
};

typedef struct KARAlias KARAlias;
struct KARAlias
{
    KAREntry dad;

    KAREntry * resolved;
    const char *link;
};


enum
{
    ktocentrytype_unknown = -1,
    ktocentrytype_notfound,
    ktocentrytype_dir,
    ktocentrytype_file,
    ktocentrytype_chunked,
    ktocentrytype_softlink,
    ktocentrytype_hardlink,
    ktocentrytype_emptyfile,
    ktocentrytype_zombiefile
};

enum PrintMode
{
    pm_normal,
    pm_longlist
};

typedef struct KARPrintMode KARPrintMode;
struct KARPrintMode
{
    enum PrintMode pm;
    uint32_t indent;
};


static rc_t kar_scan_directory ( const KDirectory *dir, BSTree *tree, const char *path );
static size_t kar_entry_full_path ( const KAREntry * entry, const char * root_dir, char * buffer, size_t bsize );


/*******************************************************************************
 * Reporting functions
 */

static void kar_print ( BSTNode *node, void *data );

static
void printEntry ( const KAREntry *entry, KARPrintMode *kpm )
{
    char buffer [ 4096 ];
    size_t bsize = sizeof buffer;

    size_t num_writ = kar_entry_full_path ( entry, NULL, buffer, bsize );
    if ( num_writ >= bsize )
    {
        rc_t rc = RC (rcExe, rcPath, rcWriting, rcBuffer, rcInsufficient);
        pLogErr ( klogErr, rc, "Failed to write path for entry '$(name)'",
                  "name=%s", entry -> name );
        exit ( 3 );
    }

    switch ( kpm -> pm )
    {
    case pm_normal:
        KOutMsg ( "%s", buffer );
        break;
    case pm_longlist:
    {
        KTime tm;
        KTimeLocal ( &tm, entry -> mod_time );

        KOutMsg ( "%04u-%02u-%02u %02u:%02u:%02u %s",

                  tm . year, tm . month + 1, tm . day + 1,
                  tm . hour, tm . minute, tm . second, buffer);
        break;
    }
    default:
        break;
    }
}

static
uint32_t get_field_width ( uint64_t num )
{
    uint64_t count;
    if ( num == 0 )
        return 3;

    for ( count = 0; num != 0; ++ count )
        num /= 10;

    return count;
}

static
void printFile ( const KARFile *file, KARPrintMode *kpm )
{
    if ( kpm -> pm == pm_longlist )
    {
        if ( file -> byte_size == 0 )
            KOutMsg ( "%*c ", max_size_fw, '-' );
        else
            KOutMsg ( "%*lu ", max_size_fw, file -> byte_size );

        KOutMsg ( "%*lu ", max_offset_fw, file -> byte_offset );
    }

    printEntry ( ( KAREntry * ) file, kpm );
    KOutMsg ( "\n" );
}

static
void printDir ( const KARDir *dir, KARPrintMode *kpm )
{
    if ( kpm -> pm == pm_longlist )
    {
        KOutMsg ( "%*c ", max_size_fw, '-' );
        KOutMsg ( "%*c ", max_offset_fw, '-' );
    }

    printEntry ( & dir -> dad, kpm );
    KOutMsg ( "\n" );
    BSTreeForEach ( &dir -> contents, false, kar_print, kpm );
}

static
void printAlias ( const KARAlias *alias, KARPrintMode *kpm, uint8_t type )
{
    if ( kpm -> pm == pm_longlist )
    {
        KOutMsg ( "% *u ", max_size_fw, string_size ( alias -> link ) );
        KOutMsg ( "% *c ", max_offset_fw, '-' );
        printEntry ( ( KAREntry * ) alias, kpm );
        KOutMsg ( " -> %s", alias -> link );
    }
    else
        printEntry ( ( KAREntry * ) alias, kpm );

    KOutMsg ( "\n" );
}

/*
static
void printFile_tree ( const KARFile * file, uint32_t *indent )
{
    KOutMsg ( "%*s%s [ %lu, %lu ]\n", *indent, "", file -> dad . name, file -> byte_offset, file -> byte_size );
}

static
void printDir_tree ( const KARDir *dir, uint32_t *indent )
{
    KOutMsg ( "%*s%s\n", *indent, "", dir -> dad . name );
    *indent += 4;
    BSTreeForEach ( &dir -> contents, false, kar_print, indent );
    *indent -= 4;
}
*/
static
void print_mode ( uint32_t grp )
{
    char r = ( grp & 4 ) ? 'r' : '-';
    char w = ( grp & 2 ) ? 'w' : '-';
    char x = ( grp & 1 ) ? 'x' : '-';

    KOutMsg ( "%c%c%c", r, w, x );
}

static
void kar_print ( BSTNode *node, void *data )
{
    const KAREntry *entry = ( KAREntry * ) node;


    KARPrintMode *kpm = ( KARPrintMode * ) data;

    switch ( kpm -> pm )
    {
    case pm_normal:
    {
        switch ( entry -> type )
        {
        case kptFile:
            printFile ( ( KARFile * ) entry, kpm );
            break;
        case kptDir:
            printDir ( ( KARDir * ) entry, kpm );
            break;
        case kptAlias:
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            printAlias ( ( KARAlias * ) entry, kpm, entry -> type );
            break;
        default:
            break;
        }
        break;
    }
    case pm_longlist:
    {
        switch ( entry -> type )
        {
        case kptFile:
            KOutMsg ( "-" );
            print_mode ( ( entry -> access_mode >> 6 ) & 7 );
            print_mode ( ( entry -> access_mode >> 3 ) & 7 );
            print_mode ( entry -> access_mode & 7 );
            KOutMsg ( " " );
            printFile ( ( KARFile * ) entry, kpm );
            break;
        case kptDir:
            KOutMsg ( "d" );
            print_mode ( ( entry -> access_mode >> 6 ) & 7 );
            print_mode ( ( entry -> access_mode >> 3 ) & 7 );
            print_mode ( entry -> access_mode & 7 );
            KOutMsg ( " " );
            printDir ( ( KARDir * ) entry, kpm );
            break;
        case kptAlias:
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            KOutMsg ( "l" );
            print_mode ( ( entry -> access_mode >> 6 ) & 7 );
            print_mode ( ( entry -> access_mode >> 3 ) & 7 );
            print_mode ( entry -> access_mode & 7 );
            KOutMsg ( " " );
            printAlias ( ( KARAlias * ) entry, kpm, entry -> type );
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

}



/*******************************************************************************
 * Create
 */


/********** KAREntry */

static
void kar_entry_whack ( BSTNode *node, void *data )
{
    KAREntry *entry = ( KAREntry * ) node;

    if ( entry -> type == kptDir ) {
        BSTreeWhack (
                    & ( ( KARDir * ) entry ) -> contents,
                    kar_entry_whack,
                    NULL
                    );
    }

    /* do the cleanup */
    switch ( entry -> type )
    {
    case kptAlias:
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        free ( ( void * ) ( ( KARAlias * ) entry ) -> link );
        break;
    }

    free ( entry );
}

static
rc_t kar_entry_create ( KAREntry ** rtn, size_t entry_size,
    const KDirectory * dir, const char * name, uint32_t type )
{
    rc_t rc;

    size_t name_len = strlen ( name ) + 1;
    KAREntry * entry = calloc ( 1, entry_size + name_len );
    if ( entry == NULL )
    {
        rc = RC (rcExe, rcNode, rcAllocating, rcMemory, rcExhausted);
        pLogErr ( klogErr, rc, "Failed to allocated memory for entry '$(name)'",
                  "name=%s", name );
    }
    else
    {
        /* location for string copy */
        char * dst = & ( ( char * ) entry ) [ entry_size ];

        /* sanity check */
        assert ( entry_size >= sizeof * entry );

        /* populate the name by copying to the end of structure */
        memmove ( dst, name, name_len );
        entry -> name = dst;

        entry -> type = type;

        rc = KDirectoryAccess ( dir, & entry -> access_mode, "%s", entry -> name );
        if ( rc != 0 )
        {
            pLogErr ( klogErr, rc, "Failed to get access mode for entry '$(name)'",
                      "name=%s", entry -> name );
        }
        else
        {
            rc = KDirectoryDate ( dir, &entry -> mod_time, "%s", entry -> name );
            if ( rc != 0 )
            {
                pLogErr ( klogErr, rc, "Failed to get modification for entry '$(name)'",
                          "name=%s", entry -> name );
            }
            else
            {
                * rtn = entry;
                return 0;
            }
        }

        free ( entry );
    }

    * rtn = NULL;
    return rc;
}

static
rc_t kar_entry_inflate ( KAREntry **rtn, size_t entry_size, const char *name, size_t name_len,
                         uint64_t mod_time, uint32_t access_mode, uint8_t type )
{
    rc_t rc;
    KAREntry * entry;

    STATUS ( STAT_QA, "inflating entry for '%s' with name_len: %u + entry_size: %u",
             name, ( uint32_t ) name_len, ( uint32_t ) entry_size );
    entry = calloc ( 1, entry_size + name_len + 1 );
    if ( entry == NULL )
    {
        rc = RC (rcExe, rcNode, rcAllocating, rcMemory, rcExhausted);
        pLogErr ( klogErr, rc, "Failed to allocated memory for entry '$(name)'",
                  "name=%s", name );
    }
    else
    {
        /* location for string copy */
        char * dst = & ( ( char * ) entry ) [ entry_size ];

        /* sanity check */
        assert ( entry_size >= sizeof * entry );

        /* populate the name by copying to the end of structure */
        memmove ( dst, name, name_len + 1 );

        entry -> name = dst;
        entry -> mod_time = mod_time;
        entry -> access_mode = access_mode;
        entry -> type = type;

        *rtn = entry;
        STATUS ( STAT_QA, "finished inflating entry for '%s'", entry -> name );
        return 0;
    }

    free ( entry );

    * rtn = NULL;
    return rc;
}

static
void kar_entry_link_parent_dir ( BSTNode *node, void *data )
{
    KAREntry *entry = ( KAREntry * ) node ;

    entry -> parentDir = ( KARDir * ) data;

    if ( entry -> type == kptDir )
    {
        KARDir *dir = ( KARDir * ) entry;
        BSTreeForEach ( &dir -> contents, false, kar_entry_link_parent_dir, dir );
    }
}

static
void kar_entry_insert_file ( BSTNode *node, void *data )
{
    KAREntry *entry = ( KAREntry * ) node ;

    switch ( entry -> type )
    {
    case kptFile:
    {
        KARFile ** file_array = data;
        file_array [ num_files ++ ] = ( KARFile * ) entry;
        break;
    }
    case kptDir:
    {
        KARDir *dir = ( KARDir * ) entry;
        BSTreeForEach ( &dir -> contents, false, kar_entry_insert_file, data );
        break;
    }
    default:
        break;
    }
}

static
int64_t CC kar_entry_cmp ( const BSTNode *item, const BSTNode *n )
{
    /* TODO - ensure that this is consistent with the ordering in kfs/toc */
    const KAREntry *a = ( const KAREntry * ) item;
    const KAREntry *b = ( const KAREntry * ) n;

    return strcmp ( a -> name, b -> name );
}

static
int64_t CC kar_entry_sort_size ( const void *a, const void *b, void *ignore )
{
    /* Only KARFiles should make it here */
    const KARFile *f1 = * ( const KARFile ** ) a;
    const KARFile *f2 = * ( const KARFile ** ) b;

    return ( int64_t ) f1 -> byte_size - ( int64_t ) f2 -> byte_size;
}

/********** BSTree population */

static
rc_t kar_add_file ( const KDirectory *dir, const char *name, void *data )
{
    KARFile *file;
    rc_t rc = kar_entry_create ( ( KAREntry ** ) & file, sizeof * file, dir, name, kptFile );
    if ( rc == 0 )
    {
        rc = KDirectoryFileSize ( dir, &file -> byte_size, "%s", file -> dad . name );
        if ( rc != 0 )
        {
            pLogErr ( klogErr, rc, "Failed to get file size for file '$(name)'",
                      "name=%s", file -> dad . name );
        }
        else
        {
            rc = BSTreeInsert ( ( BSTree * ) data, &file -> dad . dad, kar_entry_cmp );
            if ( rc == 0 )
            {
                ++ num_files;
                return 0;
            }


            pLogErr ( klogErr, rc, "Failed to insert file '$(name)' into tree",
                      "name=%s", file -> dad . name );

        }

        free ( file );
    }

    return rc;
}


static
rc_t kar_add_dir ( const KDirectory *parent_dir, const char *name, void *data )
{
    KARDir * dir;
    rc_t rc = kar_entry_create ( ( KAREntry ** ) & dir, sizeof * dir, parent_dir, name, kptDir );
    if ( rc == 0 )
    {
        /* BSTree is already initialized, but it doesn't hurt to be thorough */
        BSTreeInit ( & dir -> contents );

        rc = BSTreeInsert ( ( BSTree* ) data, &dir -> dad . dad, kar_entry_cmp );
        if ( rc != 0 )
        {
            pLogErr ( klogErr, rc, "Failed to insert directory '$(name)' into tree",
                      "name=%s", dir -> dad . name );
        }
        else
        {
            /* the name passed to us was that of a child directory.
               we need to recursively process it - so open up the child directory. */
            const KDirectory * child_dir;
            rc = KDirectoryOpenDirRead ( parent_dir, & child_dir, false, "%s", name );
            if ( rc != 0 )
            {
                pLogErr ( klogErr, rc, "Failed to open directory '$(name)' for scanning",
                          "name=%s", dir -> dad . name );
            }
            else
            {
                /* recursively scan this directory to populate tree */
                rc = kar_scan_directory ( child_dir, & dir -> contents, "." );

                KDirectoryRelease ( child_dir );

                if ( rc == 0 )
                    return 0;

                pLogErr ( klogErr, rc, "Failed to scan directory '$(name)'",
                          "name=%s", dir -> dad . name );
            }
        }

        free ( dir );
    }

    return rc;
}


static
rc_t kar_add_alias ( const KDirectory *dir, const char *name, void *data, uint32_t type )
{
    KARAlias *alias;
    rc_t rc = kar_entry_create ( ( KAREntry ** ) & alias, sizeof * alias, dir, name, type );
    if ( rc == 0 )
    {
        char resolved [ 4096 ];
        rc = KDirectoryResolveAlias ( dir, false, resolved, sizeof resolved, "%s", name );
        if ( rc == 0 )
        {
            size_t rsize = string_size ( resolved );
            char * copy = malloc ( rsize + 1 );
            if ( copy == NULL )
            {
                rc = RC (rcExe, rcNode, rcAllocating, rcMemory, rcExhausted);
                pLogErr ( klogErr, rc, "Failed to allocated memory for entry '$(name)'",
                          "name=%s", name );
            }
            else
            {
                string_copy ( copy, rsize + 1, resolved, rsize );
                alias -> link = copy;

                rc = BSTreeInsert ( ( BSTree * ) data, &alias -> dad . dad, kar_entry_cmp );
                if ( rc == 0 )
                    return 0;


                pLogErr ( klogErr, rc, "Failed to insert file '$(name)' into tree",
                      "name=%s", alias -> dad . name );

            }
        }
    }
    return rc;
}

static
rc_t CC kar_populate_tree ( const KDirectory *dir, uint32_t type, const char *name, void *data )
{
    /* We have a KDirectory* to the directory being visited, plus an entry description,
       giving the type and name of the entry. In addition, we have the data pointer
       we sent in, which is either a BSTree* or a custom structure holding more data. */

    switch ( type )
    {
    case kptFile:
        return kar_add_file ( dir, name, data );

    case kptDir:
        return kar_add_dir ( dir, name, data );

    case kptAlias:
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        return kar_add_alias ( dir, name, data, type );

    default:
        LogMsg ( klogWarn, "Unsupported file type" );
    }

    return 0;
}

/********** File searching  */

static
rc_t kar_scan_directory ( const KDirectory *dir, BSTree *tree, const char *path )
{
    /* In this case, the directory itself is NOT added to the tree,
       but only its contents. Use a shallow (non-recursive) "Visit()"
       to list contents and process each in turn. */

    rc_t rc = KDirectoryVisit ( dir, false, kar_populate_tree, tree, "%s", path );
    if ( rc != 0 )
    {
        pLogErr ( klogErr, rc, "Failed to scan directory $(directory)",
                  "directory=%s", path );
    }

    return rc;
}

static
rc_t kar_scan_path ( const KDirectory *dir, BSTree *tree, const char *path )
{
    /* At this point, we have a directory and a path.
       if the path is a simple name, with no directory components before the leaf,
       then the leaf can be added as above. But for all leading directories in the
       path, we have to call ourselves recursively to discover the directory if it
       Exists, or create it if it doesn't, in order to build our way down to the leaf. */
    return -1;
}

/********** md5  */

static
rc_t kar_md5 ( KDirectory *wd, KFile **archive, const char *path, KCreateMode mode )
{
    rc_t rc = 0;
    KFile *md5_f;

    /* create the *.md5 file to hold md5sum-compatible checksum */
    rc = KDirectoryCreateFile ( wd, &md5_f, false, 0664, mode, "%s.md5", path );
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
            rc = KMD5FileMakeWrite ( &kmd5_f, * archive, fmt, fname );
            KMD5SumFmtRelease ( fmt );
            if ( rc )
                LOGERR (klogErr, rc, "failed to make KMD5File");
            else
            {
                /* success */
                *archive = KMD5FileToKFile ( kmd5_f );
                return 0;
            }
        }

        /* error cleanup */
        KFileRelease ( md5_f );
    }

    return rc;
}

/********** write to toc and archive  */

static
uint64_t align_offset ( uint64_t offset, uint64_t alignment )
{
    uint64_t mask = alignment - 1;
    return ( offset + mask ) & ~ mask;
}

static
void kar_write_header_v1 ( KARArchiveFile * af, uint64_t toc_size )
{
    rc_t rc;
    size_t num_writ, hdr_size;

    KSraHeader hdr;
    memmove ( hdr.ncbi, "NCBI", sizeof hdr.ncbi );
    memmove ( hdr.sra, ".sra", sizeof hdr.sra );

    hdr.byte_order = eSraByteOrderTag;

    hdr.version = 1;

    /* calculate header size based upon version */
    hdr_size = sizeof hdr - sizeof hdr . u + sizeof hdr . u . v1;

    /* TBD - don't use hard-coded alignment - get it from cmdline */
    hdr.u.v1.file_offset = align_offset ( hdr_size + toc_size, 4 );
    af -> starting_pos = hdr . u . v1 . file_offset;

    rc = KFileWriteAll ( af -> archive, af -> pos, &hdr, hdr_size, &num_writ );
    if ( rc != 0 || num_writ != hdr_size )
    {
        if ( rc == 0 )
            rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );

        LogErr ( klogInt, rc, "Failed to write header" );
        exit(5);
    }

    af -> pos += num_writ;
}


static
rc_t CC kar_write_archive ( void *param, const void *buffer,
    size_t bytes, size_t *num_writ )
{
    rc_t rc = 0;

    KARArchiveFile * self = param;

    rc = KFileWriteAll ( self -> archive, self -> pos, buffer, bytes, num_writ );
    self -> pos += * num_writ;

    return rc;
}

static
rc_t CC kar_persist ( void *param, const void *node,
    size_t *num_writ, PTWriteFunc write, void *write_param );

static
rc_t kar_persist_karentry ( const KAREntry * entry, int type_code,
    size_t * num_writ, PTWriteFunc write, void *write_param )
{
    rc_t rc = 0;
    size_t total_written, total_expected;

    uint16_t legacy_name_len;
    uint8_t legacy_type_code = ( uint8_t ) type_code;

    /* actual length of the string in bytes */
    size_t name_len = strlen ( entry -> name );

    STATUS ( STAT_QA, "%s: '%.*s'"
             , __func__
             , ( uint32_t ) name_len, entry -> name
        );

    if ( name_len > UINT16_MAX )
        return RC (rcExe, rcNode, rcWriting, rcPath, rcExcessive);

    legacy_name_len = ( uint16_t ) name_len;

    /* determine size */
    total_expected
        = sizeof legacy_name_len
        + name_len
        + sizeof entry -> mod_time
        + sizeof entry -> access_mode
        + sizeof legacy_type_code
        ;

    /* if just determining toc size - return */
    if ( write == NULL )
    {
        * num_writ = total_expected;

        return 0;
    }

    /* check */
    total_written = 0;
    rc = ( * write ) ( write_param, &legacy_name_len, sizeof legacy_name_len, num_writ );
    if ( rc == 0 )
    {
        total_written = * num_writ;
        rc = ( * write ) ( write_param, entry -> name, name_len, num_writ );
        if ( rc == 0 )
        {
            total_written += * num_writ;
            rc = ( * write ) ( write_param, &entry -> mod_time, sizeof entry -> mod_time, num_writ );
            if ( rc == 0 )
            {
                total_written += * num_writ;
                rc = ( * write ) ( write_param, &entry -> access_mode, sizeof entry -> access_mode, num_writ );
                if ( rc == 0 )
                {
                    total_written += * num_writ;
                    rc = ( * write ) ( write_param, &legacy_type_code, sizeof legacy_type_code, num_writ );
                    if ( rc == 0 )
                        total_written += * num_writ;
                }
            }
        }
    }

    if ( rc == 0 && total_written != total_expected )
        rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncorrect );

    * num_writ = total_written;

    return rc;
}

static
rc_t kar_persist_karfile ( const KARFile * entry, size_t *num_writ, PTWriteFunc write, void *write_param )
{
    size_t total_expected, total_written = 0;
    rc_t rc = kar_persist_karentry ( & entry -> dad,
                                     entry -> byte_size == 0 ? ktocentrytype_emptyfile : ktocentrytype_file,
                                     num_writ, write, write_param );
    if ( rc == 0 )
    {
        total_written = * num_writ;

        /* empty files are given a special type in the toc */
        if ( entry -> byte_size == 0 )
            total_expected = total_written;
        else
        {
            /* determine size */
            total_expected
                = total_written               /* from KAREntry       */
                + sizeof entry -> byte_offset  /* specific to KARFile */
                + sizeof entry -> byte_size
                ;

            /* if determining size of toc - return */
            if ( write == NULL )
            {
                * num_writ = total_expected;
                return 0;
            }

            /* actually write the toc file entry */
            rc = ( * write ) ( write_param, &entry -> byte_offset, sizeof entry -> byte_offset, num_writ );
            if ( rc == 0 )
            {
                total_written += * num_writ;
                rc = ( * write ) ( write_param, &entry -> byte_size, sizeof entry -> byte_size, num_writ );
                if ( rc == 0 )
                    total_written += *num_writ;
            }
        }
    }

    if ( rc == 0 && total_written != total_expected )
    {
        STATUS ( STAT_QA, "total_written ( %zu ) != total_expected ( %zu )", total_written, total_expected );
        rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncorrect );
    }

    *num_writ = total_written;

    return rc;
}


static
rc_t kar_persist_kardir ( const KARDir * entry, size_t *num_writ, PTWriteFunc write, void *write_param )
{
    rc_t rc = kar_persist_karentry ( & entry -> dad, ktocentrytype_dir, num_writ, write, write_param );
    if ( rc == 0 )
    {
        size_t entry_writ = * num_writ;
        rc = BSTreePersist ( &entry -> contents, num_writ, write, write_param, kar_persist, NULL );
        if ( rc == 0 )
            * num_writ += entry_writ;
    }

    return rc;
}

static
rc_t kar_persist_karalias ( const KARAlias *entry, size_t *num_writ, PTWriteFunc write, void *write_param )
{
    size_t total_expected, total_written = 0;
    rc_t rc = kar_persist_karentry ( & entry -> dad, ktocentrytype_softlink, num_writ, write, write_param );
    if ( rc == 0 )
    {
        size_t link_size = string_size ( entry -> link );
        uint16_t legacy_link_len = ( uint16_t ) link_size;

        if ( link_size > UINT16_MAX )
            return RC (rcExe, rcNode, rcWriting, rcPath, rcExcessive);

        total_written = * num_writ;

        /* determine size */
        total_expected
            = total_written               /* from KAREntry       */
            + sizeof legacy_link_len
            + link_size  /* specific to KARAlias */
            ;

        /* if determining size of toc - return */
        if ( write == NULL )
        {
            * num_writ = total_expected;
            return 0;
        }

        /* actually write the toc file entry */
        rc = ( * write ) ( write_param, &legacy_link_len, sizeof legacy_link_len, num_writ );
        if ( rc == 0 )
        {
            total_written += * num_writ;

            rc = ( * write ) ( write_param, entry -> link, link_size, num_writ );
            if ( rc == 0 )
                total_written += * num_writ;
        }
    }

    if ( rc == 0 && total_written != total_expected )
    {
        STATUS ( STAT_QA, "total_written ( %zu ) != total_expected ( %zu )", total_written, total_expected );
        rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncorrect );
    }

    *num_writ = total_written;

    return rc;
}

static
rc_t CC kar_persist ( void *param, const void *node,
    size_t *num_writ, PTWriteFunc write, void *write_param )
{
    rc_t rc = Quitting ();
    const KAREntry * entry = ( const KAREntry * ) node;

    if ( rc != 0 )
        return rc;

    STATUS ( STAT_QA, "%s called", __func__ );

    switch ( entry -> type )
    {
    case kptFile:
    {
        STATUS ( STAT_QA, "file entry" );
        rc = kar_persist_karfile ( ( const KARFile* ) entry, num_writ, write, write_param );
        return rc;
    }
    case kptDir:
    {
        STATUS ( STAT_QA, "directory entry" );
        rc = kar_persist_kardir ( ( const KARDir* ) entry, num_writ, write, write_param );
        break;
    }
    case kptAlias:
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        STATUS ( STAT_USR, "alias entry" );
        rc = kar_persist_karalias ( ( const KARAlias * ) entry, num_writ, write, write_param );
        break;
    default:
        STATUS ( 0, "unknown entry type: id %u", entry -> type );
        break;
    }

    return rc;
}

static
uint64_t kar_eval_toc_size ( const BSTree * tree )
{
    rc_t rc;
    size_t toc_size = 0;
    STATUS ( STAT_QA, "evaluating toc size" );
    rc = BSTreePersist ( tree, & toc_size, NULL, NULL, kar_persist, NULL );
    if ( rc != 0 )
    {
        LogErr ( klogInt, rc, "Failed to determine TOC size" );
        exit(5);
    }
    STATUS ( STAT_QA, "toc size reported as %lu bytes", toc_size );

    return toc_size;
}

static
void kar_write_toc ( KARArchiveFile * af, const BSTree * tree )
{
    rc_t rc;
    size_t toc_size = 0;

    STATUS ( STAT_QA, "writing toc" );
    rc = BSTreePersist ( tree, & toc_size, kar_write_archive, af, kar_persist, NULL );
    if ( rc != 0 )
    {
        LogErr ( klogInt, rc, "Failed to determine TOC size" );
        exit(5);
    }

    if ( af -> starting_pos > af -> pos ) {
            /* It would be better to use KFileSetSize,
             * however, md5 file can only shrunk files.
             */
        uint32_t BF = 0;
        rc = KFileWriteAll (
                            af -> archive,
                            af -> pos,
                            & BF,
                            af -> starting_pos - af -> pos,
                            NULL
                            );
        if ( rc != 0 ) {
            LogErr ( klogInt, rc, "Failed to write TOC" );
            exit(5);
        }

        af -> pos = af -> starting_pos;
    }

    STATUS ( STAT_QA, "toc written" );
}


static
rc_t kar_prepare_toc ( const BSTree *tree, KARFilePtrArray *file_array_ptr )
{
    rc_t rc = 0;

    /* create an array of KARFile* that will be sorted by size */
    KARFilePtrArray file_array = calloc ( num_files, sizeof * file_array );
    if ( file_array == NULL )
        rc = RC ( rcExe, rcBuffer, rcAllocating, rcMemory, rcExhausted );
    else
    {
        uint64_t i, offset;

        /* pass back output param */
        * file_array_ptr = file_array;

        num_files = 0; /* global */

        /* now fill the array with KARFile* by walking the tree again */
        BSTreeForEach ( tree, false, kar_entry_insert_file, file_array );

        /* now, sort the array based upon size - use <klib/sort.h> */
        ksort ( file_array, num_files, sizeof * file_array, kar_entry_sort_size, NULL );

        /* now you can assign offsets to the files in the array */
        for ( i = offset = 0; i < num_files; ++ i )
        {
            KARFile *f = file_array [ i ];
            f -> byte_offset = offset;

            offset += f -> byte_size;

            /* perform aligning to boundary */
            offset = align_offset ( offset, 4 );
        }
    }

    return rc;
}

static
size_t kar_entry_full_path ( const KAREntry * entry, const char * root_dir, char * buffer, size_t bsize )
{
    size_t offset = 0;
    if ( entry -> parentDir != NULL )
    {
        offset = kar_entry_full_path ( & entry -> parentDir -> dad, root_dir, buffer, bsize );
        if ( offset < bsize )
            buffer [ offset ++ ] = '/';
    }
    else if ( root_dir != NULL && root_dir [ 0 ] != 0 )
    {
        offset = string_copy_measure ( buffer, bsize, root_dir );
        if ( buffer [ offset - 1 ] != '/' && offset < bsize )
            buffer [ offset ++ ] = '/';
    }

    return string_copy_measure ( & buffer [ offset ], bsize - offset, entry -> name ) + offset;
}

static
void kar_write_file ( KARArchiveFile *af, const KDirectory *wd, const KARFile *file, const char * root_dir )
{
    rc_t rc;
    char *buffer;
    size_t num_read, align_size;
    uint64_t pos = 0;
    char align_buffer [ 4 ] = "0000";
    size_t bsize = 128 * 1024 * 1024;

    const KFile *f;

    char filename [ 4096 ];
    size_t path_size;

    if ( file -> byte_size == 0 )
        return;

    if ( bsize > file -> byte_size )
        bsize = file -> byte_size;

    STATUS ( STAT_QA, "writing file '%s'", file -> dad . name );

    path_size = kar_entry_full_path ( & file -> dad, root_dir, filename, sizeof filename );
    if ( path_size == sizeof filename )
    {
        /* path name was somehow too long */
        rc = RC ( rcExe, rcFile, rcWriting, rcMemory, rcExhausted );
        LogErr ( klogInt, rc, "File path was too long" );
        exit (5);
    }

    STATUS ( STAT_QA, "opening: full path is '%s'", filename );
    rc = KDirectoryOpenFileRead ( wd, &f, "%s", filename );
    if ( rc != 0 )
    {
        pLogErr ( klogInt, rc, "Failed to open file $(fname)", "fname=%s", file -> dad . name );
        exit (6);
    }

    STATUS ( STAT_QA, "allocating buffer of %,zu bytes", bsize );
    buffer = malloc ( bsize );
    if ( buffer == NULL )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcMemory, rcExhausted );
        pLogErr ( klogInt, rc, "Failed to open file $(fname)", "fname=%s", file -> dad . name );
        exit ( 7 );
    }

    /* establish current position */
    align_size = align_offset ( af -> pos, 4 ) - af -> pos;
    if ( align_size != 0  )
        rc = KFileWriteAll ( af -> archive, af -> pos, align_buffer, align_size, NULL );

    af -> pos = af -> starting_pos + file -> byte_offset;

    while ( rc == 0 && pos < file -> byte_size )
    {
        size_t num_writ, to_read = bsize;

        if ( pos + to_read > file -> byte_size )
            to_read = ( size_t ) ( file -> byte_size - pos );

        STATUS ( STAT_QA, "about to read at offset %lu from input file '%s'", pos, filename );
        rc = KFileReadAll ( f, pos, buffer, to_read, & num_read );
        if ( rc != 0 || num_read == 0 )
            break;

        STATUS ( STAT_QA, "about to write %zu bytes to archive", num_read );
        rc = KFileWriteAll ( af -> archive, af -> pos, buffer, num_read, & num_writ );
        if ( rc != 0 || num_writ != num_read )
        {
            /* error */
            break;
        }

        af -> pos += num_writ;
        pos += num_read;
    }

    STATUS ( STAT_PRG, "freeing memory" );
    free ( buffer );

    STATUS ( STAT_QA, "closing '%s'", filename );
    KFileRelease ( f );
}

static
rc_t kar_make ( const KDirectory * wd, KFile *archive, const BSTree *tree, const char * root_dir )
{
    rc_t rc = 0;

    KARFilePtrArray file_array;

    rc = kar_prepare_toc ( tree, &file_array );
    if ( rc == 0 )
    {
        uint64_t i, toc_size;
        KARArchiveFile af;
        /* evaluate toc size */
        toc_size = kar_eval_toc_size ( tree );

        af . starting_pos = 0;
        af . pos = 0;
        af . archive = archive;

        /*write header */
        kar_write_header_v1 ( & af, toc_size );

        /* write toc */
        kar_write_toc ( & af, tree );

        /* write each of the files in order */
        STATUS ( STAT_QA, "about to write %u files", num_files );
        for ( i = 0; i < num_files; ++ i )
        {
            STATUS ( STAT_QA, "writing file %u: '%s'", i, file_array [ i ] -> dad . name );
            kar_write_file ( & af, wd, file_array [ i ], root_dir );
        }

        free ( file_array );
    }

    return rc;
}


/********** main create execution  */


static
rc_t kar_create ( const Params *p )
{
    rc_t rc;

    KDirectory *wd;

    rc = KDirectoryNativeDir ( &wd );
    if ( rc != 0 )
        LogErr ( klogInt, rc, "Failed to create working directory" );
    else
    {
        KFile *archive;
        KCreateMode mode = ( p -> force ? kcmInit : kcmCreate ) | kcmParents;
        rc = KDirectoryCreateFile ( wd, &archive, false, 0666, mode,
                                    "%s", p -> archive_path );
        if ( rc != 0 )
        {
            pLogErr ( klogErr, rc, "Failed to create archive $(archive)",
                      "archive=%s", p -> archive_path );
        }
        else
        {
            if ( p -> md5sum )
                rc = kar_md5 ( wd, &archive, p -> archive_path, mode );

            if ( rc == 0 )
            {
                BSTree tree;
                BSTreeInit ( & tree );

                /* build contents by walking input directory if given,
                   and adding the individual members if given */
                if ( p -> dir_count != 0 )
                {
                    rc = kar_scan_directory ( wd, & tree, p -> directory_path );
                    if ( rc == 0 )
                    {
                        uint64_t i;
                        for ( i = 1; rc == 0 && i <= p -> mem_count; ++ i )
                            rc = kar_scan_path ( wd, & tree, p -> members [ i ] );

                        if ( rc == 0 )
                        {
                            BSTreeForEach ( &tree, false, kar_entry_link_parent_dir, NULL );

                            rc = kar_make ( wd, archive, &tree, p -> directory_path );
                            if ( rc != 0 )
                                LogErr ( klogInt, rc, "Failed to build archive" );
                        }
                    }
                }

                BSTreeWhack ( & tree, kar_entry_whack, NULL );
            }

            KFileRelease ( archive );
        }

        KDirectoryRelease ( wd );
    }

    return rc;
}

/*******************************************************************************
 * Test / Extract
 */

static
uint64_t kar_verify_header ( const KFile *archive, KSraHeader *hdr )
{
    rc_t rc;

    size_t num_read;


    STSMSG (1, ("Verifying header\n"));

    rc = KFileReadAll ( archive, 0, hdr, sizeof * hdr, &num_read );
    if ( rc != 0 )
    {
        LOGERR (klogErr, rc, "failed to access archive file");
        exit ( 1 );
    }

    if ( num_read < sizeof * hdr - sizeof hdr -> u )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcOffset, rcInvalid );
        LOGERR (klogErr, rc, "corrupt archive file - invalid header");
        exit ( 1 );
    }

    /* verify "ncbi" and "sra" members */
    if ( memcmp ( hdr -> ncbi, "NCBI", sizeof hdr -> ncbi ) != 0 ||
         memcmp ( hdr -> sra, ".sra", sizeof hdr -> sra ) != 0 )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcFormat, rcInvalid );
        LOGERR (klogErr, rc, "invalid file format");
        exit ( 1 );
    }

    /* test "byte_order".
       this is allowed to be either eSraByteOrderTag or eSraByteOrderReverse.
       anything else, is garbage */
    if ( hdr -> byte_order != eSraByteOrderTag && hdr -> byte_order != eSraByteOrderReverse )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcByteOrder, rcInvalid );
        LOGERR (klogErr, rc, "failed to access archive file - invalid byte order");
        exit ( 1 );
    }

    if ( hdr -> byte_order == eSraByteOrderReverse )
    {
        hdr -> version = bswap_32 ( hdr -> version );
    }

    /* test version */
    if ( hdr -> version == 0 )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcInterface, rcInvalid );
        LOGERR (klogErr, rc, "invalid version");
        exit ( 1 );
    }

    if ( hdr -> version > 1 )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcInterface, rcUnsupported );
        LOGERR (klogErr, rc, "version not supported");
        exit ( 1 );
    }

    /* test actual size against specific header version */
    if ( num_read < sizeof * hdr - sizeof hdr -> u + sizeof hdr -> u . v1 )
    {
        rc = RC ( rcExe, rcFile, rcValidating, rcOffset, rcIncorrect );
        LOGERR (klogErr, rc, "failed to read header");
        exit ( 1 );
    }

    return num_read;
}

static
size_t toc_data_copy ( void * dst, size_t dst_size, const uint8_t * toc_data, size_t toc_data_size, size_t offset )
{
    if ( offset + dst_size > toc_data_size )
    {
        rc_t rc = RC ( rcExe, rcFile, rcValidating, rcOffset, rcInvalid );
        LOGERR (klogErr, rc, "toc offset out of bounds");
        exit ( 3 );
    }

    memmove ( dst, & toc_data [ offset ], dst_size );
    return offset + dst_size;
}

static
int64_t kar_alias_find_link ( const void *item, const BSTNode *node )
{
    const char *link = ( const char * ) item;
    KAREntry *entry = ( KAREntry * ) node;

    uint64_t link_size = string_size ( link );
    uint64_t name_size = string_size ( entry -> name );

    return string_cmp ( item, link_size, entry -> name, name_size, link_size );
}


static
void kar_alias_link_type ( BSTNode *node, void *data )
{
    /* archive fake root directory node */
    const KARDir * root = ( const KARDir * ) data;

    const KARDir *dir;
    KAREntry *entry = ( KAREntry * ) node;

    if ( entry -> type == kptDir )
    {
        /* need to go recursive on contents */
        dir = ( const KARDir * ) entry;
        BSTreeForEach ( &dir -> contents, false, kar_alias_link_type, ( void * ) root );
    }
    else if ( entry -> type == kptAlias )
    {
        const KAREntry *e;

        size_t lsize;
        const char *link, *end;
        KARAlias *alias = ( KARAlias * ) entry;

        link = alias -> link;
        lsize = string_size ( link );
        end = link + lsize;

        /* if the link is an absolute path, it's outside of archive */
        if ( link [ 0 ] == '/' )
            return;

        /* establish root for search */
        dir = entry -> parentDir;
        if ( dir == NULL )
            dir = root;
        e = & dir -> dad;

        /* walk the path */
        while ( e != NULL && link < end )
        {
            /* get the segment */
            const char *seg = link;
            char *sep = string_chr ( link, lsize, '/' );
            if ( sep == NULL )
                link = end;
            else
            {
                *sep = 0;
                link = sep + 1;
            }

            /* if the segment is empty, then we saw '/'.
               if the segment is a single '.', then it means same thing */
            if ( seg [ 0 ] == 0  ||
                 ( seg [ 0 ] == '.' && seg [ 1 ] == 0 ) )
            {
                /* do nothing */
            }
            else if ( seg [ 0 ] == '.' && seg [ 1 ] == '.' && seg [ 2 ] == 0 )
            {
                /* move up to parent */
                if ( e == & root -> dad )
                    e = NULL;
                else
                {
                    e = & e -> parentDir -> dad;
                    if ( e == NULL )
                        e = & root -> dad;
                }
            }
            else
            {
                rc_t rc;

                while ( e != NULL && e -> type == kptAlias )
                {
                    assert ( ( ( KARAlias * ) e ) -> resolved == NULL );
                    kar_alias_link_type ( ( BSTNode * ) & e -> dad, ( void * ) root );
                    e = ( ( KARAlias * ) e ) -> resolved;
                }

                if ( e -> type == ( kptDir | kptAlias ) )
                {
                    assert ( ( ( KARAlias * ) e ) -> resolved != NULL );
                    e = ( ( KARAlias * ) e ) -> resolved;
                }

                /* move down */
                if ( e -> type == kptDir )
                {
                    dir = ( KARDir * ) e;
                    e = ( KAREntry * ) BSTreeFind ( & dir -> contents, seg, kar_alias_find_link );

                    while ( e != NULL && ( e -> type & kptAlias ) != 0 )
                    {
                        if ( ( ( const KARAlias * ) e ) -> resolved == NULL )
                            break;
                        e = ( ( const KARAlias * ) e ) -> resolved;
                    }
                }
                else
                {
                    e = NULL;
                    rc = RC ( rcExe, rcPath, rcValidating, rcPath, rcInvalid );
                    LOGERR (klogErr, rc, "unable to locate symlink reference");
                }
            }

            if ( sep != NULL )
                *sep = '/';
        }

        if ( e != NULL )
        {
            assert ( link == end );
            alias -> dad . type = e -> type | kptAlias;
            alias -> resolved = ( KAREntry * ) e;
        }
    }
}

static
void kar_inflate_toc ( PBSTNode *node, void *data )
{
    rc_t rc = 0;

    size_t offset = 0;
    const uint8_t * toc_data = node -> data . addr;
    char buffer [ 4096 ], * name = buffer;
    uint16_t name_len = 0;
    uint64_t mod_time = 0;
    uint32_t access_mode = 0;
    uint8_t type_code = 0;

    offset = toc_data_copy ( & name_len, sizeof name_len, toc_data, node -> data . size, offset );
    if ( name_len >= sizeof buffer )
    {
        name = malloc ( name_len + 1 );
        if ( name == NULL )
            exit ( 10 );
    }
    offset = toc_data_copy ( name, name_len, toc_data, node -> data . size, offset );
    name [ name_len ] = 0;
    STATUS ( STAT_QA, "inflating '%s'", name );
    offset = toc_data_copy ( & mod_time, sizeof mod_time, toc_data, node -> data . size, offset );
    offset = toc_data_copy ( & access_mode, sizeof access_mode, toc_data, node -> data . size, offset );
    offset = toc_data_copy ( & type_code, sizeof type_code, toc_data, node -> data . size, offset );

    switch ( type_code )
    {
    case ktocentrytype_file:
    {
        KARFile *file;

        rc = kar_entry_inflate ( ( KAREntry ** ) &file, sizeof *file, name, name_len,
             mod_time, access_mode, kptFile );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed inflate KARFile");
            exit ( 3 );
        }

        offset = toc_data_copy ( & file -> byte_offset, sizeof file -> byte_offset, toc_data, node -> data . size, offset );
        toc_data_copy ( & file -> byte_size, sizeof file -> byte_size, toc_data, node -> data . size, offset );

        if ( file -> byte_size > max_size )
            max_size = file -> byte_size;
        if ( file -> byte_offset > max_offset )
            max_offset = file -> byte_offset;

        STATUS ( STAT_QA, "inserting '%s'", file -> dad . name );
        rc = BSTreeInsert ( ( BSTree * ) data, &file -> dad . dad, kar_entry_cmp );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed insert KARFile into tree");
            exit ( 3 );
        }

        break;
    }
    case ktocentrytype_emptyfile:
    {
        KARFile *file;

        rc = kar_entry_inflate ( ( KAREntry ** ) &file, sizeof *file, name, name_len,
             mod_time, access_mode, kptFile );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed inflate KARFile");
            exit ( 3 );
        }

        file -> byte_offset = 0;
        file -> byte_size = 0;

        STATUS ( STAT_QA, "inserting '%s'", file -> dad . name );
        rc = BSTreeInsert ( ( BSTree * ) data, &file -> dad . dad, kar_entry_cmp );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed insert KARFile into tree");
            exit ( 3 );
        }

        break;
    }
    case ktocentrytype_dir:
    {
        KARDir *dir;
        PBSTree *ptree;

        rc = kar_entry_inflate ( ( KAREntry ** ) &dir, sizeof *dir, name, name_len, mod_time, access_mode, kptDir );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed inflate KARDir");
            exit ( 3 );
        }

        BSTreeInit ( &dir -> contents );


        rc = PBSTreeMake ( & ptree, & toc_data [ offset ], node -> data . size - offset, false );
        if ( rc != 0 )
            LOGERR (klogErr, rc, "failed make PBSTree");
        else
        {
            PBSTreeForEach ( ptree, false, kar_inflate_toc, &dir -> contents );

            PBSTreeWhack ( ptree );
        }

        rc = BSTreeInsert ( ( BSTree * ) data, &dir -> dad . dad, kar_entry_cmp );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed insert KARDir into tree");
            exit ( 3 );
        }

        break;
    }
    case ktocentrytype_softlink:
    {
        KARAlias *alias;

        rc = kar_entry_inflate ( ( KAREntry ** ) &alias, sizeof *alias, name, name_len, mod_time, access_mode, kptAlias );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed inflate KARAlias");
            exit ( 3 );
        }

        /* need to reuse name* for soft-link string */
        if ( name != buffer )
            free ( name );

        offset = toc_data_copy ( & name_len, sizeof name_len, toc_data, node -> data . size, offset );
        name = malloc ( name_len + 1 );
        if ( name == NULL )
            exit ( 10 );
        offset = toc_data_copy ( name, name_len, toc_data, node -> data . size, offset );
        name [ name_len ] = 0;

        alias -> link = name;
        name = buffer;

        rc = BSTreeInsert ( ( BSTree * ) data, &alias -> dad . dad, kar_entry_cmp );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed insert KARAlias into tree");
            exit ( 3 );
        }
        break;
    }
    default:
        STATUS ( 0, "unknown entry type: id %u", type_code );
        break;
    }

    if ( name != buffer )
        free ( name );
}

static
rc_t kar_extract_toc ( const KFile *archive, BSTree *tree, uint64_t *toc_pos, const size_t toc_size )
{
    rc_t rc = 0;

    char *buffer;
    buffer = malloc ( toc_size );
    if ( buffer == NULL )
    {
        rc = RC ( rcExe, rcFile, rcAllocating, rcMemory, rcExhausted );
        LOGERR (klogErr, rc, "failed allocate memory");
    }
    else
    {
        size_t num_read;

        rc = KFileReadAll ( archive, *toc_pos, buffer, toc_size, &num_read );
        if ( rc != 0 )
        {
            LOGERR (klogErr, rc, "failed to access archive file");
            exit ( 2 );
        }

        if ( num_read < toc_size )
        {
            rc = RC ( rcExe, rcFile, rcValidating, rcOffset, rcInsufficient );
            LOGERR (klogErr, rc, "failed to read header");
        }
        else
        {
            PBSTree *ptree;

            rc = PBSTreeMake ( &ptree, buffer, num_read, false );
            if ( rc != 0 )
                LOGERR (klogErr, rc, "failed make PBSTree");
            else
            {
                PBSTreeForEach ( ptree, false, kar_inflate_toc, tree );

                PBSTreeWhack ( ptree );
            }
        }

        free ( buffer );
    }

    return rc;
}

/****************************************************************
 * JOJOBA: Changes I made
 * We are splitting extracting archive to two phases :
 * 1) during first phase we will go through all items and will
 *    create directories, and will collect data about files to store
 * 2) We will sort array of files to store by offset in original
 *    archive file, and we will restore each file, so reading of
 *    archive will be not random, but linear.
 * 3) We setting attributes for all items extracted.
 ****************************************************************/
typedef struct stored_file stored_file;
struct stored_file
{
    const KARFile * file;

    KDirectory * cdir;      /*  Note, we add ref to it */
};  /* stored_file */

#define SF_SE(SF,SH)     SF -> file -> dad . SH
#define SF_SF(SF,SH)     SF -> file -> SH

static
rc_t stored_file_init ( stored_file * self,
                        const KARFile * file,
                        KDirectory * cdir
)
{
    rc_t rc = 0;

    if ( self == NULL ) {
        rc = RC ( rcExe, rcFile, rcConstructing, rcSelf, rcNull );
    }

    if ( file == NULL || cdir == NULL ) {
        rc = RC ( rcExe, rcFile, rcConstructing, rcParam, rcNull );
    }

    memset ( self, 0, sizeof ( stored_file ) );

    rc = KDirectoryAddRef ( cdir );
    if ( rc == 0 ) {
        self -> file = file;
        self -> cdir = cdir;
    }

    return rc;
}   /* stored_file_init () */

static
rc_t stored_file_whack ( stored_file * self )
{
    if ( self != NULL ) {
        if ( self -> cdir != NULL ) {
            KDirectoryRelease ( self -> cdir );
            self -> cdir = NULL;
        }

        memset ( self, 0, sizeof ( stored_file ) );
    }

    return 0;
}   /* stored_file_whack () */

typedef struct file_depot file_depot;
struct file_depot
{
    stored_file * depot;
    size_t qty;
    size_t capacity;
};  /* file_depot */

static
rc_t file_depot_realloc ( file_depot * self, size_t capacity )
{
    rc_t rc = 0;

    if ( self == NULL ) {
        return RC ( rcExe, rcFile, rcAllocating, rcSelf, rcNull );
    }

    if ( capacity == 0 ) {
        capacity = 256;
    }

    if ( self -> capacity < capacity ) {
        stored_file * new_dep = NULL;
        size_t cap_inc = 256;

        size_t new_cap =
                    ( ( self -> capacity / cap_inc ) + 1 ) * cap_inc;

        new_dep = calloc ( new_cap, sizeof ( stored_file ) );
        if ( new_dep == NULL ) {
            return RC ( rcExe, rcFile, rcAllocating, rcMemory, rcExhausted );
        }
        else {
            if ( self -> qty != 0 && self -> depot != NULL ) {
                memmove (
                        new_dep,
                        self -> depot,
                        sizeof ( stored_file ) * self -> qty
                        );
            }

            free ( self -> depot );

            self -> capacity = new_cap;
            self -> depot = new_dep;
        }
    }
    return rc;
}   /* file_depot_realloc () */

static
rc_t file_depot_dispose ( file_depot * self )
{
    if ( self != NULL ) {
        if ( self -> depot != NULL ) {
            for ( size_t llp = 0; llp < self -> qty; llp ++ ) {
                stored_file_whack ( self -> depot + llp );
            }
            free ( self -> depot );
        }

        memset ( self, 0, sizeof ( file_depot ) );

        free ( self );
    }
    return 0;
}   /* file_depot_dispose () */

static
rc_t file_depot_make ( file_depot ** depot, size_t initial_capacity )
{
    rc_t rc = 0;
    file_depot * ret_depot = NULL;

    if ( depot == NULL ) {
        rc = RC ( rcExe, rcFile, rcConstructing, rcParam, rcNull );
    }

    * depot = NULL;

    ret_depot = calloc ( 1, sizeof ( file_depot ) );
    if ( ret_depot == NULL ) {
        rc = RC ( rcExe, rcFile, rcConstructing, rcMemory, rcExhausted );
    }
    else {
        memset ( ret_depot, 0, sizeof ( file_depot ) );
        rc = file_depot_realloc ( ret_depot, initial_capacity );
        if ( rc == 0 ) {
            * depot = ret_depot;
        }
    }

    if ( rc != 0 ) {
        * depot = NULL;
        if ( ret_depot != NULL ) {
            file_depot_dispose ( ret_depot );
            ret_depot = NULL;
        }
    }

    return rc;
}   /* file_depot_make () */

static
rc_t file_depot_add (
                    file_depot * self,
                    const KARFile * file,
                    KDirectory * cdir
)
{
    rc_t rc = 0;

    if ( self == NULL ) {
        rc = RC ( rcExe, rcFile, rcAllocating, rcSelf, rcNull );
    }

    rc = file_depot_realloc ( self, self -> qty + 1 );
    if ( rc == 0 ) {
        rc = stored_file_init (
                                self -> depot + self -> qty,
                                file,
                                cdir
                                );
        if ( rc == 0 ) {
            self -> qty ++;
        }
    }

    return rc;
}   /* file_depot_add () */

/****************************************************************
 * JOJOBA
 ****************************************************************/

typedef struct extract_block extract_block;
struct extract_block
{
    uint64_t extract_pos;

    KDirectory *cdir;
    const KFile *archive;

    file_depot * depot;

    rc_t rc;

};

static bool CC kar_extract ( BSTNode *node, void *data );

static
rc_t extract_file ( const KARFile *src, const extract_block *eb )
{
        /*  We will extract files later, after sotrint
         */
    rc_t rc = file_depot_add ( eb -> depot, src, eb -> cdir );
    if ( rc != 0 ) {
        pLogErr (klogErr, rc, "something is wrong '$(fname)'", "fname=%s", src -> dad . name );
        exit ( 4 );
    }

    return 0;
}

static
rc_t store_extracted_file ( stored_file * sf, const extract_block * eb )
{
    KFile *dst;
    char *buffer;
    size_t num_writ = 0, num_read = 0, total = 0;
    size_t bsize = 256 * 1024 *1024;

    rc_t rc = KDirectoryCreateFile ( sf -> cdir, &dst, false, 0200,
                                 kcmCreate, "%s", SF_SE(sf,name) );
    if ( rc != 0 )
    {
        pLogErr (klogErr, rc, "failed extract to file '$(fname)'", "fname=%s", SF_SE(sf,name) );
        exit ( 4 );
    }


    buffer = malloc ( bsize );
    if ( buffer == NULL )
    {
        rc = RC ( rcExe, rcFile, rcAllocating, rcMemory, rcExhausted );
        pLogErr (klogErr, rc, "failed to allocate '$(mem)'", "mem=%zu", bsize );
        exit ( 4 );
    }

    for ( total = 0; total < SF_SF(sf,byte_size); total += num_read )
    {
        size_t to_read =  SF_SF(sf,byte_size) - total;
        if ( to_read > bsize )
            to_read = bsize;

        rc = KFileReadAll ( eb -> archive, SF_SF(sf,byte_offset) + eb -> extract_pos + total,
                            buffer, to_read, &num_read );
        if ( rc != 0 )
        {
            pLogErr (klogErr, rc, "failed to read from archive '$(fname)'", "fname=%s", SF_SE(sf,name) );
            exit ( 4 );
        }

        if ( num_read == 0 && to_read != 0 ) {
            /*  we reached end of file, and we still need more data
             */
            pLogErr (klogErr, rc, "end of file reached while reading from archive '$(fname)'", "fname=%s", SF_SE(sf,name) );
            exit ( 4 );
        }

        rc = KFileWriteAll ( dst, total, buffer, num_read, &num_writ );
        if ( rc != 0 )
        {
            pLogErr (klogErr, rc, "failed to write to file '$(fname)'", "fname=%s", SF_SE(sf,name) );
            exit ( 4 );
        }

        if ( num_writ < num_read )
        {
            rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );
            pLogErr (klogErr, rc, "failed to write to file '$(fname)'", "fname=%s", SF_SE(sf,name) );
            exit ( 4 );
        }
    }

    KFileRelease ( dst );

    free ( buffer );

    return rc;
}   /* store_extracted_file () */

int64_t CC
store_extracted_files_comparator (
                                    const void * l,
                                    const void * r,
                                    void * data
)
{
    stored_file * sl = ( stored_file * ) l;
    stored_file * sr = ( stored_file * ) r;

    return SF_SF(sl,byte_offset) - SF_SF(sr,byte_offset);
}   /* store_extracted_files_comparator () */

static
rc_t store_extracted_files ( const extract_block * eb )
{
    rc_t rc = 0;

    file_depot * fb = eb -> depot;

    ksort (
            fb -> depot,
            fb -> qty,
            sizeof ( stored_file ),
            store_extracted_files_comparator,
            NULL
            );

    for ( size_t llp = 0; llp < fb -> qty; llp ++ ) {
        stored_file * sf = fb -> depot + llp;
        rc = store_extracted_file ( sf, eb );
        if ( rc != 0 ) {
            pLogErr (klogErr, rc, "failed to store extracted files", "" );
            exit ( 4 );
        }
    }

    return rc;
}   /* store_extracted_files () */

static
rc_t extract_dir ( const KARDir *src, const extract_block *eb )
{
    rc_t rc;

    STATUS ( STAT_QA, "extracting dir: %s", src -> dad . name );
    rc = KDirectoryCreateDir ( eb -> cdir, 0700, kcmCreate, "%s", src -> dad . name );
    if ( rc == 0 )
    {
        extract_block c_eb = *eb;
        rc = KDirectoryOpenDirUpdate ( eb -> cdir, &c_eb . cdir, false, "%s", src -> dad . name );
        if ( rc == 0 )
        {
            BSTreeDoUntil ( &src -> contents, false, kar_extract, &c_eb );

            rc = c_eb . rc;

            KDirectoryRelease ( c_eb . cdir );
        }
    }
    return rc;
}

static
rc_t extract_alias ( const KARAlias *src, const extract_block *eb )
{
    return KDirectoryCreateAlias ( eb -> cdir, 0777, kcmCreate, src -> link, src -> dad . name );
}

static
bool CC kar_extract ( BSTNode *node, void *data )
{
    const KAREntry *entry = ( KAREntry * ) node;
    extract_block *eb = ( extract_block * ) data;
    eb -> rc = 0;
    STATUS ( STAT_QA, "Entry to extract: %s", entry -> name );

    switch ( entry -> type )
    {
    case kptFile:
        eb -> rc = extract_file ( ( const KARFile * ) entry, eb );
        break;
    case kptDir:
        eb -> rc = extract_dir ( ( const KARDir * ) entry, eb );
        break;
    case kptAlias:
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        eb -> rc = extract_alias ( ( const KARAlias * ) entry, eb );
        if ( eb -> rc != 0 )
            return true;
        else
            return false;
        /* TBD - need to mdify the timestamp of the symlink without dereferencing using lutimes - requires library code handling*/
        /* should not get down below to setaccess or setdate */
        break;
    default:
        break;
    }

/*
    if ( eb -> rc == 0 )
        eb -> rc = KDirectorySetAccess ( eb -> cdir, false, entry -> access_mode, 0777, "%s", entry -> name );
    if ( eb -> rc == 0 )
        eb -> rc = KDirectorySetDate ( eb -> cdir, false, entry -> mod_time, "%s", entry -> name );
*/

    if ( eb -> rc != 0 )
        return true;

    return false;
}

static bool CC kar_set_attributes ( BSTNode *node, void *data );

static
rc_t set_attributes_dir ( const KARDir *src, const extract_block *eb )
{
    rc_t rc;

    STATUS ( STAT_QA, "set attributes dir: %s", src -> dad . name );
    extract_block c_eb = *eb;
    rc = KDirectoryOpenDirUpdate ( eb -> cdir, &c_eb . cdir, false, "%s", src -> dad . name );
    if ( rc == 0 )
    {
        BSTreeDoUntil ( &src -> contents, false, kar_set_attributes, &c_eb );

        rc = c_eb . rc;

        KDirectoryRelease ( c_eb . cdir );
    }
    return rc;
}

static
bool CC kar_set_attributes ( BSTNode *node, void *data )
{
    const KAREntry *entry = ( KAREntry * ) node;
    extract_block *eb = ( extract_block * ) data;
    eb -> rc = 0;

    STATUS ( STAT_QA, "Entry to set attributes: %s", entry -> name );

    if ( entry -> type == kptDir ) {
        eb -> rc = set_attributes_dir ( ( const KARDir * ) entry, eb );
    }

    if ( eb -> rc == 0 )
        eb -> rc = KDirectorySetAccess ( eb -> cdir, false, entry -> access_mode, 0777, "%s", entry -> name );
    if ( eb -> rc == 0 )
        eb -> rc = KDirectorySetDate ( eb -> cdir, false, entry -> mod_time, "%s", entry -> name );

    if ( eb -> rc != 0 )
        return true;

    return false;
}   /* kar_set_attributes () */


rc_t kar_open_file_read (
                        struct KDirectory * Dir,
                        const struct KFile ** File,
                        const char * PathOrAccession
                        );

static
rc_t kar_test_extract ( const Params *p )
{
    rc_t rc;

    KDirectory *wd;

    STSMSG (1, ("Extracting kar\n"));

    rc = KDirectoryNativeDir ( &wd );
    if ( rc != 0 )
        LogErr ( klogInt, rc, "Failed to create working directory" );
    else
    {
        const KFile *archive;
/*  JOJOBA : temparary
        rc = KDirectoryOpenFileRead ( wd, &archive, p -> archive_path );
 */
        rc = kar_open_file_read ( wd, &archive, p -> archive_path );
        if ( rc != 0 )
            LogErr ( klogInt, rc, "Failed to open archive" );
        else
        {
            KARDir root;
            BSTree *tree;
            KSraHeader hdr;
            uint64_t toc_pos, toc_size, file_offset;

            toc_pos = kar_verify_header ( archive, &hdr );
            file_offset = hdr . u . v1 . file_offset;
            toc_size = file_offset - toc_pos;

            memset ( & root, 0, sizeof root );
            root . dad . type = kptDir;

            tree = & root . contents;
            BSTreeInit ( tree );

            STATUS ( STAT_QA, "extracting toc" );
            rc = kar_extract_toc ( archive, tree, &toc_pos, toc_size );
            if ( rc == 0 )
            {
                BSTreeForEach ( tree, false, kar_entry_link_parent_dir, NULL );

                /* find what the alias points to */
                BSTreeForEach ( tree, false, kar_alias_link_type, &root );

                /* Finish test */
                if ( p -> x_count == 0 )
                {
                    KARPrintMode kpm;
                    STATUS ( STAT_QA, "Test Mode" );

                    max_size_fw = get_field_width ( max_size );
                    max_offset_fw = get_field_width ( max_offset );

                    kpm . indent = 0;

                    if ( p -> long_list )
                    {
                        KOutMsg ( "TypeAccess Size Offset ModDateTime         Path Name\n" );
                        kpm . pm = pm_longlist;
                    }
                    else
                        kpm . pm = pm_normal;

                    BSTreeForEach ( tree, false, kar_print, &kpm );
                }
                else
                {
                    extract_block eb;
                    /* begin extracting */
                    STATUS ( STAT_QA, "Extract Mode" );
                    eb . archive = archive;
                    eb . extract_pos = file_offset;
                    eb . rc = 0;

                    rc = file_depot_make ( & eb . depot, 256 );
                    if ( rc == 0 )
                    {

                        STATUS ( STAT_QA, "creating directory from path: %s", p -> directory_path );
                        rc = KDirectoryCreateDir ( wd, 0777, kcmInit, "%s", p -> directory_path );
                        if ( rc == 0 )
                        {
                            STATUS ( STAT_QA, "opening directory"  );
                            rc = KDirectoryOpenDirUpdate ( wd, &eb . cdir, false, "%s", p -> directory_path );
                            if ( rc == 0 )
                            {
                                    /*  Extracting directories
                                     */
                                BSTreeDoUntil ( tree, false, kar_extract, &eb );
                                rc = eb . rc;

                                if ( rc == 0 ) {
                                        /*  Writing files
                                         */
                                    rc = store_extracted_files ( & eb );

                                    if ( rc == 0 ) {
                                            /*  Setting attributes
                                             */
                                        BSTreeDoUntil ( tree, false, kar_set_attributes, &eb );
                                        rc = eb . rc;
                                    }
                                }
                            }

                            KDirectoryRelease ( eb . cdir );
                        }

                        file_depot_dispose ( eb . depot );
                    }
                }
            }

            BSTreeWhack ( tree, kar_entry_whack, NULL );
            KFileRelease ( archive );
        }

        KDirectoryRelease ( wd );
    }

    return rc;
}

/*******************************************************************************
 * Startup
 */

static
rc_t run ( const Params *p )
{
    if ( p -> c_count != 0 )
        return kar_create ( p );

    if ( p -> x_count != 0 )
        return kar_test_extract ( p );

    assert ( p -> t_count != 0 );
    return kar_test_extract ( p );
}

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Params params;
    Args *args = NULL;

    rc_t rc = parse_params ( &params, &args, argc, argv );
    if ( rc == 0 )
    {
        rc = run ( &params );
    }
    ArgsWhack ( args );

    if ( rc == 0 )
        STSMSG (1, ("Success: Exiting kar\n"));

    return VDB_TERMINATE( rc );
}

