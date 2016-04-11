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
#include "kar.vers.h"
#include "kar-args.h"

#include <klib/rc.h>
#include <klib/namelist.h>
#include <klib/vector.h>
#include <klib/container.h>
#include <klib/sort.h>
#include <kfs/directory.h>
#include <kfs/file.h>
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


static rc_t kar_scan_directory ( const KDirectory *dir, BSTree *tree, const char *path );


/*******************************************************************************
 * Reporting functions
 */

static void kar_print ( BSTNode *node, void *data );

static
void printFile ( const KARFile * file, uint32_t *indent )
{
    KOutMsg ( "%*s%s [ %lu, %lu ]\n", *indent, "", file -> dad . name, file -> byte_offset, file -> byte_size );
}

static
void printDir ( const KARDir *dir, uint32_t *indent )
{
    KOutMsg ( "%*s%s\n", *indent, "", dir -> dad . name );
    *indent += 4;
    BSTreeForEach ( &dir -> contents, false, kar_print, indent );
    *indent -= 4;
}

static
void kar_print ( BSTNode *node, void *data )
{
    const KAREntry *entry = ( KAREntry * ) node;

    switch ( entry -> type )
    {
    case kptFile:
        printFile ( ( KARFile * ) entry, data );
        break;
    case kptDir:
        printDir ( ( KARDir * ) entry, data );
        break;
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        break;
    default:
        break;
    }
}

/*
  this would be a good place to test the contents of the BSTree
  write a function to walk the BSTree and print out what was found
BSTreeForEach ( &tree, false, kar_print, &indent );


*/

/*******************************************************************************
 * Create
 */


/********** KAREntry */

static 
void kar_entry_whack ( BSTNode *node, void *data )
{
    KAREntry *entry = ( KAREntry * ) node;

    /* do the cleanup */
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
        pLogErr ( klogErr, rc, "Failed to allocated memory for entry $(name)",
                  "name=%s", name );
    }
    else
    {
        /* location for string copy */
        char * dst = & ( ( char * ) entry ) [ entry_size ];

        /* sanity check */
        assert ( entry_size >= sizeof * entry );

        /* populate the name by copying to the end of structure */
        memcpy ( dst, name, name_len );
        entry -> name = dst;

        entry -> type = type;

        rc = KDirectoryAccess ( dir, & entry -> access_mode, "%s", entry -> name );
        if ( rc != 0 )
        {
            pLogErr ( klogErr, rc, "Failed to get access mode for entry $(name)",
                      "name=%s", entry -> name );
        }
        else
        {
            rc = KDirectoryDate ( dir, &entry -> mod_time, "%s", entry -> name );
            if ( rc != 0 )
            {
                pLogErr ( klogErr, rc, "Failed to get modification for entry $(name)",
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
rc_t kar_add_alias ( const KDirectory *dir, const char *name, void *data )
{
    /* need to call KDirectoryResolveAlias() to map "name" into a substitution path */
    return -1;
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

    case kptFile | kptAlias:
    case kptDir | kptAlias:
        return kar_add_alias ( dir, name, data );

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
    size_t num_writ;

    KSraHeader hdr;
    memcpy ( hdr.ncbi, "NCBI", sizeof hdr.ncbi );
    memcpy ( hdr.sra, ".sra", sizeof hdr.sra );

    hdr.byte_order = eSraByteOrderTag;

    hdr.version = 1;

    /* TBD - don't use hard-coded alignment - get it from cmdline */
    hdr.u.v1.file_offset = align_offset ( sizeof hdr + toc_size, 4 );
    af -> starting_pos = hdr . u . v1 . file_offset;

    rc = KFileWriteAll ( af -> archive, af -> pos, &hdr, sizeof hdr, &num_writ );
    if ( rc != 0 || num_writ != sizeof hdr )
    {
        if ( rc == 0 )
            rc = RC ( rcExe, rcFile, rcWriting, rcTransfer, rcIncomplete );

        LogErr ( klogInt, rc, "Failed to write header" );
        exit(5);
    }

    af -> pos += num_writ;
}

/********** write to toc and archive  */

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

    if ( write == NULL )
    {
        * num_writ = total_expected;

        /*KOutMsg ( "[ toc_entry_size %lu ] +", *num_writ );*/
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
    size_t total_expected, total_written;
    rc_t rc = kar_persist_karentry ( & entry -> dad, ktocentrytype_file, num_writ, write, write_param );
    if ( rc == 0 )
    {
        total_written = * num_writ;
        
        /* determine size */
        total_expected
            = total_written               /* from KAREntry       */
            + sizeof entry -> byte_offset  /* specific to KARFile */
            + sizeof entry -> byte_size
            ;

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
    case kptFile | kptAlias:
    case kptDir | kptAlias:
        STATUS ( STAT_USR, "an alias exists - not handled" );
        break;
    default:
        STATUS ( 0, "unknown entry type: id %u", entry -> type );
        break;
    }
    KOutMsg ( "\n" );

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
        uint32_t indent = 0;
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

        if ( KStsLevelGet () >= STAT_PRG )
        {
            BSTreeForEach ( tree, false, kar_print, &indent );

            for ( i = 0; i < num_files; ++ i )
                printFile ( file_array [ i ], & indent );
        }

    }
    
    return rc;
}

static
size_t kar_entry_full_path ( const KAREntry * entry, char * buffer, size_t bsize )
{
    size_t offset = 0;
    if ( entry -> parentDir != NULL )
    {
        offset = kar_entry_full_path ( & entry -> parentDir -> dad, buffer, bsize );
        if ( offset < bsize )
            buffer [ offset ++ ] = '/';
    }

    return string_copy_measure ( & buffer [ offset ], bsize - offset, entry -> name ) + offset;
}

static
void kar_write_file ( KARArchiveFile *af, const KDirectory *wd, const KARFile *file )
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

    path_size = kar_entry_full_path ( & file -> dad, filename, sizeof filename );
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
        pLogErr ( klogInt, rc, "Failed to open file %s", file -> dad . name );
        exit (6);
    }
    
    STATUS ( STAT_QA, "allocating buffer of %,zu bytes", bsize );
    buffer = malloc ( bsize );
    if ( buffer == NULL )
    {
        rc = RC ( rcExe, rcFile, rcWriting, rcMemory, rcExhausted );
        pLogErr ( klogInt, rc, "Failed to open file %s", file -> dad . name );
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
rc_t kar_make ( const KDirectory * wd, KFile *archive, const BSTree *tree )
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
        /*KOutMsg ( "toc_size=%lu\n\n", toc_size );*/

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
            kar_write_file ( & af, wd, file_array [ i ] );
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
                            
                            rc = kar_make ( wd, archive, &tree );
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
 * Extract
 */

static
rc_t kar_extract ( const Params *p )
{
    return -1;
}

/*******************************************************************************
 * Test
 */


static
rc_t kar_test ( const Params *p )
{
    return -1;
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
        return kar_extract ( p );

    assert ( p -> t_count != 0 );
    return kar_test ( p );
}

ver_t CC KAppVersion (void)
{
    return  KAR_VERS;
}
    
rc_t CC KMain ( int argc, char *argv [] )
{
    Params params;
    Args *args = NULL;

    rc_t rc = parse_params ( &params, args, argc, argv );
    if ( rc == 0 )
    {
        rc = run ( &params );

        ArgsWhack ( args );
    }

    if ( rc == 0 )
        STSMSG (1, ("Success: Exiting kar\n"));

    return rc;
}

