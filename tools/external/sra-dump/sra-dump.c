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

#include <sra/sradb.h>
#include <sra/srapath.h>
#include <sra/types.h>
#include <vdb/manager.h>
#include <vdb/schema.h>
#include <vdb/table.h>
#include <kapp/main.h>
#include <kapp/args.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/status.h>
#include <klib/container.h>
#include <klib/rc.h>
#include <os-native.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

typedef struct sradump_parms sradump_parms;
struct sradump_parms
{
    const char *src_path;

    const char **columns;
    uint32_t column_cnt;

    spotid_t start, stop;
};

typedef struct column_map column_map;
struct column_map
{
    const char *name;
    void ( * dump ) ( const void *base, bitsz_t row_bits );
    const SRAColumn *col;
    VTypedecl td;
    uint32_t elem_size;
};

/* dump_row
 */
static
void dump_text ( const void *base, bitsz_t row_bits )
{
    const unsigned char *data = base;
    uint32_t i, count = row_bits >> 3;

    for ( i = 0; i < count; ++ i )
  {
        if ( isprint ( data [ i ] ) )
            OUTMSG(( "%c", data [ i ] ));
        else
            OUTMSG(( "\\x%2x", data [ i ] ));
    }
}

static
void dump_I8 ( const void *base, bitsz_t row_bits )
{
    const int8_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 3;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%d", sep, data [ i ] ));
}

static
void dump_U8 ( const void *base, bitsz_t row_bits )
{
    const uint8_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 3;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%u", sep, data [ i ] ));
}

static
void dump_I16 ( const void *base, bitsz_t row_bits )
{
    const int16_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 4;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%d", sep, data [ i ] ));
}

static
void dump_U16 ( const void *base, bitsz_t row_bits )
{
    const uint16_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 4;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%u", sep, data [ i ] ));
}

static
void dump_I32 ( const void *base, bitsz_t row_bits )
{
    const int32_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 5;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%d", sep, data [ i ] ));
}

static
void dump_U32 ( const void *base, bitsz_t row_bits )
{
    const uint32_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 5;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%u", sep, data [ i ] ));
}

static
void dump_I64 ( const void *base, bitsz_t row_bits )
{
    const int64_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 6;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%ld", sep, data [ i ] ));
}

static
void dump_U64 ( const void *base, bitsz_t row_bits )
{
    const uint64_t *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 6;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%lu", sep, data [ i ] ));
}

static
void dump_F32 ( const void *base, bitsz_t row_bits )
{
    const float *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 5;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%f", sep, data [ i ] ));
}

static
void dump_F64 ( const void *base, bitsz_t row_bits )
{
    const double *data = base;
    const char *sep = "";
    uint32_t i, count = row_bits >> 6;

    for ( i = 0; i < count; sep = ",", ++ i )
        OUTMSG(( "%s%f", sep, data [ i ] ));
}


#define CASE( x ) \
    case x: p = # x; break

static
void dump_platform ( const void *base, bitsz_t row_bits )
{
    const char *p = NULL;
    const uint8_t *data = base;
    if ( row_bits == 8 )
    {
        static const char *platform_symbolic_names[] = { INSDC_SRA_PLATFORM_SYMBOLS };
        if ( data[ 0 ] < sizeof ( platform_symbolic_names ) / sizeof( *platform_symbolic_names ) )
        {
            p = platform_symbolic_names[ data [ 0 ] ];
        }
        else
        {
            OUTMSG(( "%u", data [ 0 ] ));
            return;
        }
    }
    OUTMSG(( "%s", p ));
}

static
void dump_readtype ( const void *base, bitsz_t row_bits )
{
    const char *sep = "";
    const uint8_t *data = base;
    uint32_t i, count = row_bits >> 3;
    for ( i = 0; i < count; sep = ",", ++ i )
    {
        const char *p;
        switch ( data [ i ] )
        {
        CASE ( SRA_READ_TYPE_TECHNICAL );
        CASE ( SRA_READ_TYPE_BIOLOGICAL );
        CASE ( SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_FORWARD );
        CASE ( SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_FORWARD );
        CASE ( SRA_READ_TYPE_TECHNICAL|SRA_READ_TYPE_REVERSE );
        CASE ( SRA_READ_TYPE_BIOLOGICAL|SRA_READ_TYPE_REVERSE );
        default:
            OUTMSG(( "%s%u", sep, data [ 0 ] ));
            continue;
        }
        OUTMSG(( "%s%s", sep, p ));
    }
}

static
void dump_readfilt ( const void *base, bitsz_t row_bits )
{
    const char *sep = "";
    const uint8_t *data = base;
    uint32_t i, count = row_bits >> 3;
    for ( i = 0; i < count; sep = ",", ++ i )
    {
        const char *p;
        switch ( data [ i ] )
        {
        CASE ( SRA_READ_FILTER_PASS );
        CASE ( SRA_READ_FILTER_REJECT );
        CASE ( SRA_READ_FILTER_CRITERIA );
        CASE ( SRA_READ_FILTER_REDACTED );
        default:
            OUTMSG(( "%s%u", sep, data [ 0 ] ));
            continue;
        }
        OUTMSG(( "%s%s", sep, p ));
    }
}

#undef CASE


static
rc_t dump_row ( const sradump_parms *pb, const column_map *cm, uint32_t count, spotid_t row )
{
    rc_t rc;
    uint32_t i;

    for ( rc = 0, i = 0; rc == 0 && i < count; ++ i )
    {
        const void *base;
        bitsz_t boff, row_bits;

        /* read data */
        rc = SRAColumnRead ( cm [ i ] . col, row, & base, & boff, & row_bits );
        if ( rc != 0 )
        {
            PLOGERR ( klogInt,  (klogInt, rc, "failed to read column '$(name)' row $(row)",
                                 "name=%s,row=%u", cm [ i ] . name, row ));
            break;
        }

        /* cannot deal with non-byte-aligned data */
        if ( boff != 0 || ( row_bits & 7 ) != 0 )
        {
            rc = RC ( rcExe, rcColumn, rcReading, rcType, rcUnsupported );
            PLOGERR ( klogInt,  (klogInt, rc, "non-byte-aligned data in column '$(name)' row $(row)",
                                 "name=%s,row=%u", cm [ i ] . name, row ));
            break;
        }

        /* write cell header - TBD - improve column alignment for readability */
        OUTMSG(( "%u. %s: ", row, cm [ i ] . name ));

        /* write cell data */
        if ( cm [ i ] . td . dim == 1 )
            ( cm [ i ] . dump ) ( base, row_bits );
        else
        {
            const char *sep;
            const uint8_t *data = base;
            uint32_t elem_bits = cm [ i ] . elem_size;
            uint32_t j, row_len = ( uint32_t ) ( row_bits / elem_bits );

            for ( sep = "{", j = 0; j < row_len; sep = "},{", ++ j )
            {
                OUTMSG(( "%s", sep ));
                ( cm [ i ] . dump ) ( & data [ j * elem_bits >> 3 ], elem_bits );
            }

            OUTMSG(( "}" ));
        }

        OUTMSG(( "\n" ));
    }

    return rc;
}

static
rc_t dump_table ( const sradump_parms *pb, const column_map *cm, uint32_t count )
{
    rc_t rc;
    spotid_t row;

    for ( rc = 0, row = pb -> start; row <= pb -> stop; ++ row )
    {
        rc = Quitting();
        if ( rc != 0 )
            break;
        rc = dump_row ( pb, cm, count, row );
        if ( rc != 0 )
            break;
    }

    return rc;
}

static
const char *s_454_default [] =
{
    "(INSDC:SRA:platform_id)PLATFORM",
    "(U8)NREADS",
    "(INSDC:SRA:xread_type)READ_TYPE",
    "(NCBI:SRA:Segment)READ_SEG",
    "(NCBI:SRA:Segment)LABEL_SEG",
    "(ascii)LABEL",
    "(INSDC:SRA:read_filter)READ_FILTER",
    "(ascii)SPOT_GROUP",

    "(INSDC:dna:text)FLOW_CHARS",
    "(INSDC:dna:text)KEY_SEQUENCE",
    "(INSDC:dna:text)LINKER_SEQUENCE",
    "(INSDC:dna:text)READ",
    "(INSDC:quality:phred)QUALITY",
    "(INSDC:coord:one)CLIP_ADAPTER_LEFT",
    "(INSDC:coord:one)CLIP_ADAPTER_RIGHT",
    "(INSDC:coord:one)CLIP_QUALITY_LEFT",
    "(INSDC:coord:one)CLIP_QUALITY_RIGHT",
    "(NCBI:isamp1)SIGNAL",
    "(INSDC:position:one)POSITION"
};

static
const char *s_Illumina_default [] =
{
    "(INSDC:SRA:platform_id)PLATFORM"
    , "(U8)NREADS"
    , "(INSDC:SRA:xread_type)READ_TYPE"
    , "(NCBI:SRA:Segment)READ_SEG"
    , "(NCBI:SRA:Segment)LABEL_SEG"
    , "(ascii)LABEL"
    , "(INSDC:SRA:read_filter)READ_FILTER"
    , "(ascii)SPOT_GROUP"

    , "(INSDC:dna:text)READ"
    , "(NCBI:qual4)QUALITY:QUALITY"
    , "(INSDC:quality:phred)PHYS_QUALITY:QUALITY"
    , "(INSDC:quality:phred)QUALITY2"
#if ENABLE_INTENSITY_COPY
    , "(NCBI:fsamp4)INTENSITY"
#endif
};

static
const char *s_ABI_default [] =
{
    " (INSDC:SRA:platform_id)PLATFORM"
    , "(U8)NREADS"
    , "(INSDC:SRA:xread_type)READ_TYPE"
    , "(NCBI:SRA:Segment)READ_SEG"
    , "(NCBI:SRA:Segment)LABEL_SEG"
    , "(ascii)LABEL"
    , "(INSDC:SRA:read_filter)READ_FILTER"
    , "(ascii)SPOT_GROUP"

    , "(INSDC:dna:text)CS_KEY"
    , "(INSDC:color:text)CSREAD"
    , "(INSDC:quality:phred)QUALITY"
#if ENABLE_INTENSITY_COPY
    , "(NCBI:fsamp4)SIGNAL"
#endif
};

static
const char *s_fastq_default [] =
{
    "(INSDC:SRA:platform_id)PLATFORM",
    "(U8)NREADS",
    "(INSDC:SRA:xread_type)READ_TYPE",
    "(NCBI:SRA:Segment)READ_SEG",
    "(NCBI:SRA:Segment)LABEL_SEG",
    "(ascii)LABEL",
    "(INSDC:SRA:read_filter)READ_FILTER",
    "(ascii)SPOT_GROUP",

    "(INSDC:dna:text)READ",
    "(INSDC:quality:phred)QUALITY"
};

static
rc_t get_default_columns ( sradump_parms *pb, const SRATable *tbl )
{
    const SRAColumn *col;
    rc_t rc = SRATableOpenColumnRead ( tbl, & col, "PLATFORM", sra_platform_id_t );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to determine table platform" );
    else
    {
        const void *base;
        bitsz_t boff, row_bits;
        rc = SRAColumnRead ( col, 1, & base, & boff, & row_bits );
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to read table platform" );
        else if ( boff != 0 || ( row_bits & 7 ) != 0 )
        {
            rc = RC ( rcExe, rcColumn, rcReading, rcData, rcCorrupt );
            PLOGERR ( klogInt,  (klogInt, rc, "bit offset $(boff), row bits $(bits)",
                                 "boff=%u,bits=%u", ( uint32_t ) boff, ( uint32_t ) row_bits ));
        }
        else
        {
            const uint8_t *platform_id = base;
            switch ( platform_id [ 0 ] )
            {
            case SRA_PLATFORM_UNDEFINED:
                rc = RC ( rcExe, rcTable, rcReading, rcType, rcInvalid );
                LOGERR ( klogInt, rc, "table platform undefined" );
                break;
            case SRA_PLATFORM_454:
                pb -> columns = s_454_default;
                pb -> column_cnt = sizeof s_454_default / sizeof s_454_default [ 0 ];
                break;
            case SRA_PLATFORM_ILLUMINA:
                pb -> columns = s_Illumina_default;
                pb -> column_cnt = sizeof s_Illumina_default / sizeof s_Illumina_default [ 0 ];
                break;
            case SRA_PLATFORM_ABSOLID:
                pb -> columns = s_ABI_default;
                pb -> column_cnt = sizeof s_ABI_default / sizeof s_ABI_default [ 0 ];
                break;
            default:
                pb -> columns = s_fastq_default;
                pb -> column_cnt = sizeof s_fastq_default / sizeof s_fastq_default [ 0 ];
            }
        }

        SRAColumnRelease ( col );
    }

    return rc;
}


typedef struct basetype basetype;
struct basetype
{
    BSTNode n;
    void ( * dump ) ( const void *base, bitsz_t row_bits );
    uint32_t type_id;
};

static
void CC basetype_whack ( BSTNode *n, void *ignore )
{
    free ( n );
}

static
int64_t CC basetype_cmp ( const void *item, const BSTNode *n )
{
    const VTypedecl *td = item;
    const basetype *bt = ( const basetype* ) n;

    return (int64_t) td -> type_id - (int64_t) bt -> type_id;
}

static
int64_t CC basetype_sort ( const BSTNode *item, const BSTNode *n )
{
    const basetype *td = ( const basetype* ) item;
    const basetype *bt = ( const basetype* ) n;

    return (int64_t) td -> type_id - (int64_t) bt -> type_id;
}

static
rc_t build_basetypes ( BSTree *ttree, const VDatatypes *dt )
{
    struct
    {
        const char *name;
        void ( * dump ) ( const void *base, bitsz_t row_bits );
    }
    types [] =
    {
        { "I8", dump_I8 },
        { "U8", dump_U8 },
        { "I16", dump_I16 },
        { "U16", dump_U16 },
        { "I32", dump_I32 },
        { "U32", dump_U32 },
        { "I64", dump_I64 },
        { "U64", dump_U64 },
        { "F32", dump_F32 },
        { "F64", dump_F64 },
        { "utf8", dump_text },
        { "ascii", dump_text },

        /* special case types */
        { "INSDC:SRA:platform_id", dump_platform },
        { "NCBI:SRA:platform_id", dump_platform },
        { "INSDC:SRA:xread_type", dump_readtype },
        { "INSDC:SRA:read_type", dump_readtype },
        { "NCBI:SRA:read_type", dump_readtype },
        { "INSDC:SRA:read_filter", dump_readfilt },
        { "NCBI:SRA:read_filter", dump_readfilt }
    };

    rc_t rc;
    uint32_t i, count = sizeof types / sizeof types [ 0 ];

    BSTreeInit ( ttree );

    for ( rc = 0, i = 0; i < count; ++ i )
    {
        VTypedef def;
        basetype *bt = malloc ( sizeof * bt );
        if ( bt == NULL )
        {
            rc = RC ( rcExe, rcType, rcAllocating, rcMemory, rcExhausted );
            LOGERR ( klogSys, rc, "failed to populate basetypes table" );
            break;
        }
        rc = VDatatypesResolveTypename ( dt, & def, types [ i ] . name );
        if ( rc != 0 )
        {
            free ( bt );
            PLOGERR ( klogInt,  (klogInt, rc, "failed to resolve typename '$(typename)'",
                                 "typename=%s", types [ i ] . name ));
            break;
        }

        bt -> dump = types [ i ] . dump;
        bt -> type_id = def . type_id;

        BSTreeInsert ( ttree, & bt -> n, basetype_sort );
    }

    return rc;
}

static
rc_t open_column ( const VDatatypes *dt, const SRATable *tbl,
    const BSTree *ttree, column_map *cm, const char *colspec )
{
    rc_t rc;
    VTypedef def;

    int len;
    const char *iname;
    char inamebuff [ 128 ];

    /* convert column spec to typedecl and name */
    char tdbuffer [ 128 ];
    const char *typedecl = NULL;
    const char *end = strchr ( colspec, ')' );
    if ( end != NULL )
    {
        const char *begin = strchr ( colspec, '(' );
        if ( begin != NULL && begin < end )
        {
            /* the colspec is now the name */
            colspec = end;

            /* trim whitespace */
            while ( isspace ( ( ++ colspec ) [ 0 ] ) )
                ( void ) 0;
            while ( ++ begin < end && isspace ( begin [ 0 ] ) )
                ( void ) 0;
            while ( begin < end && isspace ( end [ -1 ] ) )
                -- end;

            /* create a copy to allow for a NUL byte */
            len = snprintf ( tdbuffer, sizeof tdbuffer,
                "%.*s", ( int ) ( end - begin ), begin );
            if ( len < 0 || len >= sizeof tdbuffer )
            {
                rc = RC ( rcExe, rcColumn, rcOpening, rcString, rcExcessive );
                return rc;
            }

            typedecl = tdbuffer;
        }
    }

    /* convert name to iname and extname */
    iname = colspec;
    colspec = strchr ( colspec, ':' );
    if ( colspec == NULL )
        colspec = iname;
    else
    {
        len = snprintf ( inamebuff, sizeof inamebuff, "%.*s",
                         ( int ) ( colspec - iname ), iname );
        if ( len < 0 || len >= sizeof inamebuff )
        {
            rc = RC ( rcExe, rcColumn, rcOpening, rcString, rcExcessive );
            return rc;
        }

        iname = inamebuff;
        ++ colspec;
    }

    /* record the name */
    cm -> name = colspec;

    /* open the column */
    rc = SRATableOpenColumnRead ( tbl, & cm -> col, iname, typedecl );
    if ( rc != 0 )
        return rc;

    /* read its datatype */
    rc = SRAColumnDatatype ( cm -> col, & cm -> td, & def );
    if ( rc == 0 )
    {
        const basetype *bt;

        /* get its element size */
        cm -> elem_size = VTypedefSizeof ( & def ) * cm -> td . dim;

        /* try to find the actual type */
        bt = ( const basetype* ) BSTreeFind ( ttree, & cm -> td, basetype_cmp );
        if ( bt == NULL )
        {
            /* find the intrinsic type
               NB - this code treats ascii as intrinsic */
            rc = VDatatypesToIntrinsic ( ( const VDatatypes*) SRATableGetSchema(tbl), & cm -> td );
            if ( rc == 0 )
                bt = ( const basetype* ) BSTreeFind ( ttree, & cm -> td, basetype_cmp );
        }

        if ( bt == NULL )
            rc = RC ( rcExe, rcColumn, rcOpening, rcType, rcUnsupported );
        else
        {
            cm -> dump = bt -> dump;
            return 0;
        }
    }

    SRAColumnRelease ( cm -> col );
    cm -> col = NULL;
    return rc;
}

static
rc_t sra_dump ( sradump_parms *pb, const VDatatypes *dt, const SRATable *tbl )
{
    BSTree ttree;
    rc_t rc = build_basetypes ( & ttree, dt );
    if ( rc == 0 )
    {
        if ( pb -> column_cnt == 0 )
            rc = get_default_columns ( pb, tbl );
        if ( rc == 0 )
        {
            uint32_t count = pb -> column_cnt;
            const char **columns = pb -> columns;
            column_map *cm = malloc ( sizeof * cm * count );
            if ( cm == NULL )
                rc = RC ( rcExe, rcTable, rcReading, rcMemory, rcExhausted );
            else
            {

                uint32_t i;
                uint32_t count2 = count;

                for ( i = count = 0; rc == 0 && i < count2; ++ i )
                {
                    rc = open_column ( dt,  tbl, & ttree, & cm [ count ], columns [ i ] );
                    if ( rc == 0 )
                        ++ count;
                    else switch ( GetRCState ( rc ) )
                    {
                    case rcNotFound:
                    case rcUnsupported:
                    case rcExcessive:
                        rc = 0;
                        break;
                    default:
                        break;
                    }
                }

                if ( rc == 0 )
                {
                    spotid_t max_spot;
                    rc = SRATableMaxSpotId ( tbl, & max_spot );
                    if ( rc != 0 )
                        LOGERR ( klogInt, rc, "failed to read max spot id" );
                    else
                    {
                        if ( pb -> start == 0 )
                            pb -> start = 1;
                        if ( pb -> stop == 0 || pb -> stop > max_spot )
                            pb -> stop = max_spot;

                        rc = dump_table ( pb, cm, count );
                    }
                }

                for ( i = 0; i < count; ++ i )
                    SRAColumnRelease ( cm [ i ] . col );

                free (cm);
            }
        }

        BSTreeWhack ( & ttree, basetype_whack, NULL );
    }
    return rc;
}

static
rc_t run ( sradump_parms *pb )
{
    const SRAMgr *mgr;
    rc_t rc = SRAMgrMakeRead ( & mgr );
    if ( rc != 0 )
        LOGERR ( klogInt, rc, "failed to open SRAMgr" );
    else
    {
        const VDatatypes *dt;
        rc = SRAMgrOpenDatatypesRead ( mgr, & dt );
        if ( rc != 0 )
            LOGERR ( klogInt, rc, "failed to open VDatatypes" );
        else
        {
            const SRATable *tbl;
            rc = SRAMgrOpenTableRead ( mgr, & tbl, "%s", pb -> src_path );
            if ( rc != 0 )
                PLOGERR ( klogInt,
                          ( klogInt, rc, "failed to open SRATable '$(spec)'", "spec=%s", pb -> src_path));
            else
            {
                rc = sra_dump ( pb, dt, tbl );
                SRATableRelease ( tbl );
            }

            VDatatypesRelease ( dt );
        }

        SRAMgrRelease ( mgr );
    }

    return rc;
}

/* Usage
 */
static
KLogLevel default_log_level;


#define OPTION_START "start"
#define OPTION_STOP  "stop"
#define ALIAS_START ""
#define ALIAS_STOP  ""

static const
char * start_usage[] =
{
    "beginning spot id (default 1)", NULL
};


static const
char * stop_usage[] =
{
    "ending spot id (default max)", NULL
};


static
OptDef Options[] =
{
    { OPTION_START, ALIAS_START, NULL, start_usage, 1, true, false},
    { OPTION_STOP,  ALIAS_STOP,  NULL, stop_usage,  1, true, false}
};


const char UsageDefaultName[] = "sra-dump";


rc_t CC UsageSummary (const char * progname)
{
    return KOutMsg ("\n"
                    "Usage:\n"
                    "  %s [ options ] table [ column-spec ... ]\n"
                    "\n"
                    "Summary:\n"
                    "  Dump all data in table for specified or all columns\n"
                    "\n", progname );
}

static
const char * name_param[] =
{
    "NAME", "simple column name", NULL
};


static
const char * typedecl_param[] =
{
    "(typedecl)NAME", "specifically typed column name", NULL
};

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

    OUTMSG (( "Options:\n" ));

    OUTMSG (("table:\n"
	     "  path to table or accession id within NCBI\n"
             "\n"
             "column-spec:\n"));

    HelpParamLine (name_param[0], name_param+1);
    HelpParamLine (typedecl_param[0], typedecl_param+1);

    OUTMSG (( "\nOptions:\n" ));

    HelpOptionLine (ALIAS_START, OPTION_START, "ID", start_usage);

    HelpOptionLine (ALIAS_STOP, OPTION_STOP, "ID", stop_usage);

    HelpOptionsStandard ();

    HelpVersion (fullpath, KAppVersion());

    return rc;
}


MAIN_DECL( argc, argv )
{
    if ( VdbInitialize( argc, argv, 0 ) )
        return VDB_INIT_FAILED;

    Args * args;
    rc_t rc;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    default_log_level = KLogLevelGet();

    rc = KStsHandlerSetStdErr();
    if (rc)
        return rc;

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options, sizeof Options / sizeof (OptDef));
    if (rc == 0)
    {
        do
        {
            uint32_t pcount;

            sradump_parms pb;

            memset (&pb, 0, sizeof pb);

            rc = ArgsOptionCount (args, OPTION_START, &pcount);
            if (rc)
                break;

            if (pcount == 1)
            {
                const char * pc;

                rc = ArgsOptionValue (args, OPTION_START, 0, (const void **)&pc);
                if (rc)
                    break;

                pb.start = AsciiToU32 (pc, NULL, NULL);
            }

            rc = ArgsOptionCount (args, OPTION_STOP, &pcount);
            if (rc)
                break;

            if (pcount == 1)
            {
                const char * pc;

                rc = ArgsOptionValue (args, OPTION_STOP, 0, (const void **)&pc);
                if (rc)
                    break;

                pb.stop = AsciiToU32 (pc, NULL, NULL);
            }

            rc = ArgsParamCount (args, &pcount);
            if (rc)
                break;

            if (pcount == 0) {
                OUTMSG (("missing source table\n"));
                rc = MiniUsage (args);
                ArgsWhack(args);
                exit(EXIT_FAILURE);
            }

            rc = ArgsParamValue (args, 0, (const void **)&pb.src_path);
            if (rc)
                break;

            pb.column_cnt = pcount - 1; /* pcount is used below */

            if (pb.column_cnt) {
                pb.columns = malloc (pb.column_cnt * sizeof (const char *));
                if (pb.columns == NULL)
                    rc = RC (rcExe, rcArgv, rcParsing, rcMemory, rcExhausted);
            }
            else
                pb.columns = NULL;

            {
                uint32_t ix;
                const char ** ppc;

                ppc = pb.columns;
                for (ix = 1; ix < pcount; ix++)
                {
                    rc = ArgsParamValue (args, ix, (const void **)ppc);
                    if (rc)
                        break;
                    ppc++;
                }

                if (rc == 0)
                {
                    rc = run (&pb);
                }

                if ( pcount != 1 )
                    /* brain damaged windows compiler warned about next line */
                    free ( (void*)pb.columns );
            }
        } while (0);

        ArgsWhack (args);
    }
    return VdbTerminate( rc );
}


