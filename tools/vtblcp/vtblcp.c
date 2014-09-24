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

#include "vtblcp.vers.h"
#include "vtblcp-priv.h"

#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <kapp/main.h>
#include <klib/container.h>
#include <klib/vector.h>
#include <klib/log.h>
#include <klib/rc.h>
#include <fmtdef.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* param block
 */
typedef struct vtblcp_parms vtblcp_parms;
struct vtblcp_parms
{
    /* manager params, e.g. global schema, library include paths */

    /* schema params, e.g. include paths, source file */
    const char *schema_src;

    /* source table params */
    const char *src_path;

    /* destination table params */
    const char *dst_type;
    const char *dst_path;

    /* column list */
    const char **columns;
    uint32_t column_cnt;

    /* options */
    bool redact_sensitive;
};

/* column map
 *  the write cursor determines which columns get copied
 *  the read cursor should have a similar structure
 *  each cursor will have its own column indices
 *  this structure allows a simple ordinal mapping of the pairs
 */
typedef struct vtblcp_column_map vtblcp_column_map;
struct vtblcp_column_map
{
    const void *redact_value;
    uint32_t wr, rd;
    bool sensitive;
    bool rd_filter;
    uint8_t align [ 2 ];
};


static
rc_t copy_row ( const vtblcp_parms *pb, VCursor *dcurs, const VCursor *scurs,
    const vtblcp_column_map *cm, uint32_t count, uint32_t rdfilt_idx, int64_t row )
{
    rc_t rc = VCursorOpenRow ( scurs );
    if ( rc != 0 )
        PLOGERR ( klogErr,  (klogErr, rc, "failed to open source row '$(row)'", "row=%" LD64, row ));
    else
    {
        rc_t rc2;
        rc = VCursorOpenRow ( dcurs );
        if ( rc != 0 )
            PLOGERR ( klogErr,  (klogErr, rc, "failed to open destination row '$(row)'", "row=%" LD64, row ));
        else
        {
            uint32_t i;
            const void *base;
            uint32_t elem_bits, boff, row_len;

            bool redact = false;
            if ( rdfilt_idx != 0 && pb -> redact_sensitive )
            {
                rc = VCursorCellData ( scurs, rdfilt_idx,
                    & elem_bits, & base, & boff, & row_len );
                if ( rc != 0 )
                {
                    PLOGERR ( klogErr,  (klogErr, rc, "failed to read cell data for read filter, row '$(row)'",
                                         "row=%" LD64, row ));
                }
                else
                {
                    const uint8_t *rd_filter = base;
                    for ( i = 0; i < row_len; ++ i )
                    {
                        if ( rd_filter [ i ] == 3 )
                        {
                            redact = pb -> redact_sensitive;
                            break;
                        }
                    }
                }
            }

            for ( i = 0; i < count && rc != 0; ++ i )
            {
                /* get column data */
                rc = VCursorCellData ( scurs, cm [ i ] . rd,
                    & elem_bits, & base, & boff, & row_len );
                if ( rc != 0 )
                {
                    PLOGERR ( klogErr,  (klogErr, rc, "failed to read cell data for column '$(idx)', row '$(row)'",
                                         "idx=%u,row=%" LD64, i, row ));
                    break;
                }

                if ( redact && cm [ i ] . sensitive )
                {
                    uint32_t j;

                    /* substitute base pointer with redact value */
                    base = cm [ i ] . redact_value;

                    /* redact destination */
                    for ( j = 0; j < row_len; ++ j )
                    {
                        rc = VCursorWrite ( dcurs, cm [ i ] . wr, elem_bits, base, 0, 1 );
                        if ( rc != 0 )
                        {
                            PLOGERR ( klogErr,  (klogErr, rc, "failed to redact cell data for column '$(idx)', row '$(row)'",
                                                 "idx=%u,row=%" LD64, i, row ));
                            break;
                        }
                    }
                }
                else
                {
                    /* write to destination */
                    rc = VCursorWrite ( dcurs, cm [ i ] . wr, elem_bits, base, boff, row_len );
                    if ( rc != 0 )
                    {
                        PLOGERR ( klogErr,  (klogErr, rc, "failed to write cell data for column '$(idx)', row '$(row)'",
                                             "idx=%u,row=%" LD64, i, row ));
                        break;
                    }
                }
            }

            /* commit row */
            if ( rc == 0 )
                rc = VCursorCommitRow ( dcurs );

            rc2 = VCursorCloseRow ( dcurs );
            if ( rc == 0 )
                rc = rc2;
        }

        rc2 = VCursorCloseRow ( scurs );
        if ( rc == 0 )
            rc = rc2;
    }

    return rc;
}


static
rc_t copy_table ( const vtblcp_parms *pb, VCursor *dcurs, const VCursor *scurs,
    const vtblcp_column_map *cm, uint32_t count, uint32_t rdfilt_idx )
{
    /* open source */
    rc_t rc = VCursorOpen ( scurs );
    if ( rc != 0 )
        LOGERR ( klogErr, rc, "failed to open source cursor" );
    else
    {
        /* get row range */
        int64_t row, last;
        uint64_t range_count;
        
        rc = VCursorIdRange ( scurs, 0, & row, & range_count );
        last = row + range_count;
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to determine row range for source cursor" );
        else
        {
            /* open desination cursor */
            rc = VCursorOpen ( dcurs );
            if ( rc != 0 )
                LOGERR ( klogErr, rc, "failed to open destination cursor" );
            else
            {
                /* focus destination on initial source row */
                rc = VCursorSetRowId ( dcurs, row );
                if ( rc != 0 )
                    PLOGERR ( klogErr,  (klogErr, rc, "failed to set destination cursor row to id '$(row)'", "row=%" LD64, row ));
                else
                {
                    /* copy each row */
                    for ( ; row <= last; ++ row )
                    {
                        rc = copy_row ( pb, dcurs, scurs, cm, count, rdfilt_idx, row );
                        if ( rc != 0 )
                            break;
                    }

                    /* commit changes */
                    if ( rc == 0 )
                        rc = VCursorCommit ( dcurs );
                }
            }
        }
    }

    return rc;
}

typedef struct stype_id stype_id;
struct stype_id
{
    BSTNode n;
    const void *redact_value;
    uint32_t type_id;
    uint32_t elem_size;
};


static
void CC stype_id_whack ( BSTNode *n, void *ignore )
{
    free ( n );
}

static
int CC stype_id_cmp ( const void *item, const BSTNode *n )
{
    const VTypedecl *a = item;
    const stype_id *b = ( const stype_id* ) n;

    if ( a -> type_id < b -> type_id )
        return -1;
    return a -> type_id > b -> type_id;
}

static
int CC stype_id_sort ( const BSTNode *item, const BSTNode *n )
{
    const stype_id *a = ( const stype_id* ) item;
    const stype_id *b = ( const stype_id* ) n;

    if ( a -> type_id < b -> type_id )
        return -1;
    return a -> type_id > b -> type_id;
}

static
rc_t mark_type_sensitivity ( const BSTree *stype_tbl, const VSchema *schema,
    const VTypedecl *td, vtblcp_column_map *cm )
{
    const stype_id *node;

    /* simple case - look for exact matches */
    if ( BSTreeFind ( stype_tbl, td, stype_id_cmp ) != NULL )
    {
        cm -> sensitive = true;
        return 0;
    }

    /* exhaustive case - perform one by one ancestry test */
    for ( node = ( const stype_id* ) BSTreeFirst ( stype_tbl );
          node != NULL;
          node = ( const stype_id* ) BSTNodeNext ( & node -> n ) )
    {
        cm -> redact_value = NULL;
        if ( td -> type_id > node -> type_id )
        {
            VTypedecl cast;

            /* test for our type being a subtype */
            if ( VTypedeclToType ( td, schema, node -> type_id, & cast, NULL ) )
            {
                cm -> redact_value = node -> redact_value;
                cm -> sensitive = true;
                return 0;
            }
        }
    }

    /* doesn't appear to be sensitive */
    cm -> sensitive = false;
    return 0;
}

static
rc_t populate_stype_tbl ( BSTree *stype_tbl, const VSchema *schema )
{
    rc_t rc;
    uint32_t i;
    static struct
    {
        const char *typename;
        const char *redact_value;
    } sensitive_types [] =
    {
        /* original SRA types */
        { "INSDC:fasta", "N" },
        { "INSDC:csfasta", "." },
        { "NCBI:2na", "\x00" },
        { "NCBI:2cs", "\x00" },
        { "NCBI:4na", "\xFF" },
        { "NCBI:qual1", "\x00" },
        { "NCBI:qual4", "\xFB\xFB\xFB\xFB" },
        { "NCBI:isamp1", "\x00\x00\x00" },
        { "NCBI:isamp4", "\x00\x00\x00" },
        { "NCBI:fsamp1", "\x00\x00\x00" },
        { "NCBI:fsamp4", "\x00\x00\x00" },
        { "INSDC:dna:text", "N" },
        { "INSDC:dna:bin", "\x04" },
        { "INSDC:dna:4na", "\xFF" },
        { "INSDC:dna:2na", "\x00" },
        { "INSDC:color:text", "." },
        { "INSDC:color:bin", "\x04" },
        { "INSDC:color:2cs", "\x00" },
        { "INSDC:quality:phred", "\x00" },
        { "INSDC:quality:log_odds", "\x00\x00\x00" }
        /* signal types TBD */
    };

    BSTreeInit ( stype_tbl );

    for ( rc = 0, i = 0; i < sizeof sensitive_types / sizeof sensitive_types [ 0 ]; ++ i )
    {
        VTypedecl td;
        const char *decl = sensitive_types [ i ] . typename;
        rc = VSchemaResolveTypedecl ( schema, & td, "%s", decl );
        if ( rc == 0 )
        {
            stype_id *n;
            BSTNode *exist;

            VTypedesc desc;
            rc = VSchemaDescribeTypedecl ( schema, & desc, & td );
            if ( rc != 0 )
            {
                PLOGERR ( klogInt,  (klogInt, rc, "failed to describe type '$(type)'", "type=%s", decl ));
                break;
            }

            n = malloc ( sizeof * n );
            if ( n == NULL )
            {
                rc = RC ( rcExe, rcNode, rcAllocating, rcMemory, rcExhausted );
                LOGERR ( klogInt, rc, "failed to record sensitive data type" );
                break;
            }

            n -> redact_value = sensitive_types [ i ] . redact_value;
            n -> type_id = td . type_id;
            n -> elem_size = VTypedescSizeof ( & desc );

            rc = BSTreeInsertUnique ( stype_tbl, & n -> n, & exist, stype_id_sort );
            if ( rc != 0 )
            {
                free ( n );
                if ( GetRCState ( rc ) != rcExists )
                {
                    LOGERR ( klogInt, rc, "failed to record sensitive data type" );
                    break;
                }
                rc = 0;
            }
        }
        else if ( GetRCState ( rc ) == rcNotFound )
        {
            rc = 0;
        }
        else
        {
            break;
        }
    }

    return rc;
}

static
rc_t populate_rdfilt_tbl ( BSTree *rftype_tbl, const VSchema *schema )
{
    rc_t rc;
    uint32_t i;
    const char *rftypes [] =
    {
        "NCBI:SRA:read_filter",
        "INSDC:SRA:read_filter"
    };

    BSTreeInit ( rftype_tbl );

    for ( rc = 0, i = 0; i < sizeof rftypes / sizeof rftypes [ 0 ]; ++ i )
    {
        VTypedecl td;
        const char *decl = rftypes [ i ];
        rc = VSchemaResolveTypedecl ( schema, & td, "%s", decl );
        if ( rc == 0 )
        {
            BSTNode *exist;

            stype_id *n = malloc ( sizeof * n );
            if ( n == NULL )
            {
                rc = RC ( rcExe, rcNode, rcAllocating, rcMemory, rcExhausted );
                LOGERR ( klogInt, rc, "failed to record read_filter data type" );
                break;
            }

            n -> redact_value = NULL;
            n -> type_id = td . type_id;
            n -> elem_size = 8;

            rc = BSTreeInsertUnique ( rftype_tbl, & n -> n, & exist, stype_id_sort );
            if ( rc != 0 )
            {
                free ( n );
                if ( GetRCState ( rc ) != rcExists )
                {
                    LOGERR ( klogInt, rc, "failed to record read_filter data type" );
                    break;
                }
                rc = 0;
            }
        }
        else if ( GetRCState ( rc ) == rcNotFound )
        {
            rc = 0;
        }
        else
        {
            break;
        }
    }

    return rc;
}


static
rc_t populate_cursors ( VTable *dtbl, VCursor *dcurs, const VCursor *scurs,
    vtblcp_column_map *cm, const Vector *v, uint32_t *rd_filt )
{
    uint32_t end = VectorLength ( v );
    uint32_t i = VectorStart ( v );

    BSTree stype_tbl, rftype_tbl;

    const VSchema *schema;
    rc_t rc = VTableOpenSchema ( dtbl, & schema );
    if ( rc != 0 )
    {
        LOGERR ( klogInt, rc, "failed to open destination table schema" );
        return rc;
    }

    /* populate sensitive type table */
    rc = populate_stype_tbl ( & stype_tbl, schema );
    if ( rc != 0 )
    {
        VSchemaRelease ( schema );
        return rc;
    }

    /* populate read filter type table */
    rc = populate_rdfilt_tbl ( & rftype_tbl, schema );
    if ( rc != 0 )
    {
        BSTreeWhack ( & stype_tbl, stype_id_whack, NULL );
        VSchemaRelease ( schema );
        return rc;
    }

    for ( end += i, rc = 0, * rd_filt = 0; i < end; ++ i )
    {
        VTypedecl td;
        char typedecl [ 128 ];

        const char *spec = ( const void* ) VectorGet ( v, i );

        /* request column in destination */
        rc = VCursorAddColumn ( dcurs, & cm [ i ] . wr, "%s", spec );
        if ( rc != 0 )
        {
            PLOGERR ( klogErr,  (klogErr, rc, "failed to add '$(spec)' to destination cursor", "spec=%s", spec ));
            break;
        }

        /* always retrieve data type */
        rc = VCursorDatatype ( dcurs, cm [ i ] . wr, & td, NULL );
        if ( rc != 0 )
        {
            PLOGERR ( klogInt,  (klogInt, rc, "failed to determine datatype of destination column '$(name)'", "name=%s", spec ));
            break;
        }

        /* mark column as sensitive or not */
        rc = mark_type_sensitivity ( & stype_tbl, schema, & td, & cm [ i ] );
        if ( rc != 0 )
            break;

        /* if spec is already typed, request it in source */
        if ( spec [ 0 ] == '(' )
        {
            rc = VCursorAddColumn ( scurs, & cm [ i ] . rd, "%s", spec );
            if ( rc != 0 )
            {
                PLOGERR ( klogErr,  (klogErr, rc, "failed to add '$(spec)' to source cursor", "spec=%s", spec ));
                break;
            }
        }
        else
        {
            rc = VTypedeclToText ( & td, schema, typedecl, sizeof typedecl );
            if ( rc != 0 )
            {
                PLOGERR ( klogInt,  (klogInt, rc, "failed to print datatype of destination column '$(name)'", "name=%s", spec ));
                break;
            }

            rc = VCursorAddColumn ( scurs, & cm [ i ] . rd, "(%s)%s", typedecl, spec );
            if ( rc != 0 )
            {
                PLOGERR ( klogErr,  (klogErr, rc, "failed to add '$(spec)' to source cursor", "spec=(%s)%s", typedecl, spec ));
                break;
            }
        }

        /* check if column is a read filter */
        cm [ i ] . rd_filter = false;
        if ( ! cm [ i ] . sensitive )
        {
            if ( BSTreeFind ( & rftype_tbl, & td, stype_id_cmp ) != NULL )
            {
                if ( * rd_filt != 0 )
                {
                    rc = RC ( rcExe, rcColumn, rcOpening, rcColumn, rcExists );
                    PLOGERR ( klogInt,  (klogInt, rc, "can't use column '$(name)' as read filter", "name=%s", spec ));
                    break;
                }

                * rd_filt = cm [ i ] . rd;
                cm [ i ] . rd_filter = true;
            }
        }
    }

    BSTreeWhack ( & rftype_tbl, stype_id_whack, NULL );
    BSTreeWhack ( & stype_tbl, stype_id_whack, NULL );
    VSchemaRelease ( schema );

    /* add read filter to input if not already there in some way */
    if ( * rd_filt == 0 )
    {
        rc = VCursorAddColumn ( scurs, rd_filt, "RD_FILTER" );
        if ( rc != 0 && GetRCState ( rc ) == rcNotFound )
            rc = 0;
    }

    return rc;
}


/* NOTES

   1. default schema from code
      a. discovered by VDBManager upon "Make" ? look for "libsraschema.so"
      b. discovered by client?
      c. explicitly added when making "empty" schema?

   2. need support for discovering all openable columns
      a. declaration in schema for creating a table from another,
         or actually from some set of base tables

   3. need support for indicating/discovering native columns
      a. this is related in some way to knowing whether the vcolumn
         represents a kcolumn, and so is not likely to be a manually
         indicated property of the vcolumn, especially with subclasses.

*/

/* vtblcp
 */
static
rc_t vtblcp ( const vtblcp_parms *pb, const VTable *stbl, VTable *dtbl, const Vector *v )
{
    const VCursor *scurs;
    rc_t rc = VTableCreateCursorRead ( stbl, & scurs );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to create empty destination cursor" );
    else
    {
        VCursor *dcurs;
        rc = VTableCreateCursorWrite ( dtbl, & dcurs, kcmInsert );
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to create empty destination cursor" );
        else
        {
            uint32_t count = VectorLength ( v );
            vtblcp_column_map *cm = malloc ( sizeof *cm * count );
            if ( cm == NULL )
            {
                rc = RC ( rcExe, rcVector, rcAllocating, rcMemory, rcExhausted );
                LOGERR ( klogInt, rc, "failed to create column index map" );
            }
            else
            {
                uint32_t rd_filt;
                rc = populate_cursors ( dtbl, dcurs, scurs, cm, v, & rd_filt );

                if ( rc == 0 )
                    rc = copy_table ( pb, dcurs, scurs, cm, count, rd_filt );

                free ( cm );
            }

            VCursorRelease ( dcurs );
        }
            
        VCursorRelease ( scurs );
    }

    return rc;
}


/* get_column_specs
 */
static
void CC free_column_spec ( void *elem, void *ignore )
{
    free ( elem );
}

static
rc_t get_column_specs ( const vtblcp_parms *pb, Vector *v, const VTable *stbl, VTable *dtbl )
{
    rc_t rc;
    
    /* always prepare the vector */
    VectorInit ( v, 0, pb -> column_cnt );

    /* unable at this moment to auto-determine column list */
    if ( pb -> column_cnt == 0 )
    {
        rc = RC ( rcExe, rcSchema, rcEvaluating, rcFunction, rcUnsupported );
        LOGERR ( klogInt, rc, "failed to determine column specs" );
    }
    else
    {
        uint32_t i;

        /* process command line arguments */
        for ( rc = 0, i = 0; i < pb -> column_cnt; ++ i )
        {
            const char *src = pb -> columns [ i ];

            char *dst = malloc ( strlen ( src ) + 2 );
            if ( dst == NULL )
            {
                rc = RC ( rcExe, rcString, rcAllocating, rcMemory, rcExhausted );
                break;
            }

            strcpy ( dst, src );

            rc = VectorAppend ( v, NULL, dst );
            if ( rc != 0 )
            {
                free ( dst );
                break;
            }
        }

        /* failure */
        if ( rc != 0 )
            VectorWhack ( v, free_column_spec, NULL );
    }

    return rc;
}


/* close_table
 */
static
rc_t close_dst_table ( const vtblcp_parms *pb, VTable *tbl, rc_t entry_rc )
{
    rc_t rc;

    /* perform "re-indexing" on columns
       this will go away soon once cursor sessions are supported */
    if ( entry_rc == 0 )
        entry_rc = VTableReindex ( tbl );

    /* close the table */
    rc = VTableRelease ( tbl );

    /* return most important return code */
    return entry_rc ? entry_rc : rc;
}

/* open_table
 */
static
rc_t open_src_table ( const vtblcp_parms *pb,
    const VDBManager *mgr, const VSchema *schema, const VTable **tblp )
{
    return VDBManagerOpenTableRead ( mgr, tblp, schema, "%s", pb -> src_path );
}

static
rc_t open_dst_table ( const vtblcp_parms *pb,
    VDBManager *mgr, const VSchema *schema, VTable **tblp )
{
    return VDBManagerCreateTable ( mgr, tblp, schema,
                                   pb -> dst_type, kcmInit, "%s", pb -> dst_path );
}


/* init_schema
 */
static
rc_t init_schema ( const vtblcp_parms *pb,
    const VDBManager *mgr, VSchema **schemap )
{
    VSchema *schema;
    rc_t rc = VDBManagerMakeSchema ( mgr, & schema );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to create empty schema" );
    else
    {
        /* parse schema file */
        rc = VSchemaParseFile ( schema, "%s", pb -> schema_src );
        if ( rc != 0 )
            PLOGERR ( klogErr,  (klogErr, rc, "failed to parse schema file '$(file)'", "file=%s", pb -> schema_src ));
        else
        {
            * schemap = schema;
            return 0;
        }

        VSchemaRelease ( schema );
    }

    return rc;
}


/* init_mgr
 */
static
rc_t init_mgr ( const vtblcp_parms *pb, VDBManager **mgrp )
{
    VDBManager *mgr;
    rc_t rc = VDBManagerMakeUpdate ( & mgr, NULL );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to open vdb library" );
    else
    {
        /* currently have no manager parameters */
        * mgrp = mgr;
    }

    return rc;
}


/* run
 */
static
rc_t run ( const vtblcp_parms *pb )
{
    VDBManager *mgr;
    rc_t rc = init_mgr ( pb, & mgr );
    if ( rc == 0 )
    {
        VSchema *schema;
        rc = init_schema ( pb, mgr, & schema );
        if ( rc == 0 )
        {
            const VTable *stbl;
            rc = open_src_table ( pb, mgr, schema, & stbl );
            if ( rc == 0 )
            {
                VTable *dtbl;
                rc = open_dst_table ( pb, mgr, schema, & dtbl );
                if ( rc == 0 )
                {
                    /* determine columns */
                    Vector v;
                    rc = get_column_specs ( pb, & v, stbl, dtbl );

                    /* perform the copy */
                    if ( rc == 0 )
                    {
                        rc = vtblcp ( pb, stbl, dtbl, & v );
                        VectorWhack ( & v, free_column_spec, NULL );
                    }

                    /* cleanup */
                    rc = close_dst_table ( pb, dtbl, rc );
                }

                VTableRelease ( stbl );
            }

            VSchemaRelease ( schema );
        }

        VDBManagerRelease ( mgr );
    }

    return rc;
}


/* Version
 */
ver_t CC KAppVersion ( void )
{
    return VTBLCP_VERS;
}

/* Usage
 */
static
void s_Usage ( const char *app_name )
{
    printf ( "\n"
             "Usage: %s [ options ] src-table dst-table [ column-spec ... ]\n"
             "\n%s%s"
             "    options:\n"
             "      -h                     give tool help\n"
             "      -K <path>              schema file for output\n"
             "      -T <table-name-expr>   fully qualified table name expression for output table\n"
             "\n"
             "    column-spec:\n"
             "      NAME                   simple column name\n"
             "      (typedecl)NAME         specifically typed column name\n"
             "\n",
             app_name,
             "    copy data from one table to another. default behavior is to use\n"
             "    schema from source table to create copy. if a new schema is to be\n"
             "    used, its identifier must be given with the '-T' option.\n"
             "\n"
             "    the '-K' option allows external schema definitions to be loaded.\n"
             "\n",
             "    the list of columns to copy may be given on the command line as\n"
             "    white-space separated arguments. the syntax used differs from VDB proper\n"
             "    in order to ease quoting.\n"
             "\n"
             "    if no columns are given the intent is to automatically derive them\n"
             "    from schema - NOT YET IMPLEMENTED.\n"
             "\n"
        );
}

static
void MiniUsage ( const char *app_name )
{
    printf ( "\n"
             "Usage: %s [ options ] src-table dst-table\n"
             "    run with option '-h' for help\n"
             , app_name
        );
}
    

/* KMain
 */
rc_t CC KMain ( int argc, char *argv [] )
{
    int i;
    rc_t rc;

    /* expect paths and schema types */
    vtblcp_parms pb;
    memset ( & pb, 0, sizeof pb );
    pb . columns = ( const char** ) & argv [ 1 ];

    /* parse arguments */
    for ( rc = 0, i = 1; i < argc; ++ i )
    {
        const char *arg = argv [ i ];
        if ( arg [ 0 ] != '-' )
        {
            if ( pb . src_path == NULL )
                pb . src_path = arg;
            else if ( pb . dst_path == NULL )
                pb . dst_path = NULL;
            else
            {
                /* capture column name/spec */
                pb . columns [ pb . column_cnt ++ ] = arg;
            }
        }
        else do switch ( ( ++ arg ) [ 0 ] )
        {
        case 'K':
            pb . schema_src = NextArg ( & arg, & i, argc, argv, NULL, NULL );
            break;
        case 'T':
            pb . dst_type = NextArg ( & arg, & i, argc, argv, NULL, NULL );
            break;
        case 'h':
        case '?':
            s_Usage ( argv [ 0 ] );
            return 0;
        default:
            fprintf ( stderr, "unrecognized switch: '%s'\n", argv [ i ] );
            MiniUsage ( argv [ 0 ] );
            return RC ( rcExe, rcArgv, rcReading, rcParam, rcInvalid );
        }
        while ( arg [ 1 ] != 0 );
    }

    /* check arguments */
    if ( pb . src_path == NULL )
    {
        fprintf ( stderr, "missing source table path\n" );
        MiniUsage ( argv [ 0 ] );
        return RC ( rcExe, rcArgv, rcReading, rcParam, rcNotFound );
    }
    if ( pb . dst_path == NULL )
    {
        fprintf ( stderr, "missing destination table path\n" );
        MiniUsage ( argv [ 0 ] );
        return RC ( rcExe, rcArgv, rcReading, rcParam, rcNotFound );
    }
    if ( pb . schema_src == NULL )
    {
        fprintf ( stderr, "missing schema source file\n" );
        MiniUsage ( argv [ 0 ] );
        return RC ( rcExe, rcArgv, rcReading, rcParam, rcNotFound );
    }
    if ( pb . dst_type == NULL )
    {
        fprintf ( stderr, "missing destination table type description\n" );
        MiniUsage ( argv [ 0 ] );
        return RC ( rcExe, rcArgv, rcReading, rcParam, rcNotFound );
    }

    /* run tool */
    return run ( & pb );
}
