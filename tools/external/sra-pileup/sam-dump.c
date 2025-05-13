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

#ifndef _h_klib_report_
#include <klib/report.h>
#endif

#ifndef _h_klib_log_
#include <klib/log.h>
#endif

#ifndef _h_klib_status_
#include <klib/status.h>
#endif

#ifndef _h_klib_out_
#include <klib/out.h>
#endif

#ifndef _h_klib_vector_
#include <klib/vector.h>
#endif

#ifndef _h_klib_printf_
#include <klib/printf.h>
#endif

#ifndef _h_vfs_manager_
#include <vfs/manager.h>
#endif

#ifndef _h_vfs_path_priv_
#include <vfs/path-priv.h>    /* VPathOption() */
#endif

#ifndef _h_kfs_file_
#include <kfs/file.h>
#endif

#ifndef _h_kfs_buffile_
#include <kfs/buffile.h>
#endif

#ifndef _h_kfs_gzip_
#include <kfs/gzip.h>
#endif

#ifndef _h_kfs_bzip_
#include <kfs/bzip.h>
#endif

#ifndef _h_kdb_meta_
#include <kdb/meta.h>
#endif

#ifndef _h_kdb_namelist_
#include <kdb/namelist.h>     /* KMDataNodeListChild() */
#endif

#ifndef _h_kapp_main_
#include <kapp/main.h>
#endif

#ifndef _h_insdc_sra_
#include <insdc/sra.h>
#endif

#ifndef _h_vdb_report_
#include <vdb/report.h>
#endif

#ifndef _h_vdb_cursor_
#include <vdb/cursor.h>
#endif

#ifndef _h_vdb_vdb_priv_
#include <vdb/vdb-priv.h>
#endif

#ifndef _h_vdb_schema_
#include <vdb/schema.h>
#endif

#ifndef _h_vdb_dependencies_
#include <vdb/dependencies.h>     /* UIError() / UIDatabaseLOGError() / UITableLOGError() */
#endif

#ifndef _h_sra_sraschema_
#include <sra/sraschema.h>
#endif

#ifndef _h_align_dna_reverse_cmpl_
#include <align/dna-reverse-cmpl.h>
#endif

#ifndef _h_align_reader_reference_
#include <align/reference.h>
#endif

#ifndef _h_align_quality_quantizer_
#include <align/quality-quantizer.h>
#endif

#ifndef _tools_sam_dump_debug_h_
#include "debug.h"
#endif

#include <stdio.h>      /* snprintf() */
#include <limits.h>     /* UINT_MAX */
#include <strtol.h>     /* strtou32() */
#include <ctype.h>      /* tolower() / toupper() / isdigit() / isalpha() */

#ifndef WINDOWS
#include <strings.h> /* strcasecmp */
#endif

#if _ARCH_BITS == 64
#define USE_MATE_CACHE 1
#define CURSOR_CACHE (4 * 1024 * 1024 * 1024UL)
#else
#define USE_MATE_CACHE 0
#define CURSOR_CACHE (256 * 1024 * 1024)
#endif

rc_t cg_canonical_print_cigar( const char * cigar, size_t cigar_len);

typedef struct TAlignedRegion_struct {
    char name[1024];
    struct {
        INSDC_coord_zero from;
        INSDC_coord_zero to;
    } r[10240];
    int rq;
    INSDC_coord_zero max_to;
    INSDC_coord_zero min_from;
    int printed;
} TAlignedRegion;

typedef struct TMatepairDistance_struct {
    uint32_t from;
    uint32_t to;
} TMatepairDistance;

struct params_s {
    /* which outputs are on */
    bool primaries;
    bool secondaries;
    bool unaligned;
    bool cg_evidence;
    bool cg_ev_dnb;
    bool cg_sam;

    bool use_seqid;
    bool long_cigar;
    bool reheader;
    bool noheader;
    bool hide_identical;
    bool fasta;
    bool fastq;
    bool spot_group_in_name;
    bool cg_friendly_names;
    bool reverse_unaligned;
    bool unaligned_spots;

    bool output_gzip;
    bool output_bz2;

    bool xi;
    int cg_style; /* 0: raw; 1: with B's; 2: without B's, fixed up SEQ/QUAL; */
    char const *name_prefix;
    /* region filter data */
    TAlignedRegion* region;
    uint32_t region_qty;
    /* distance filter data */
    bool mp_dist_unknown;
    TMatepairDistance* mp_dist;
    uint32_t mp_dist_qty;
    uint32_t test_rows;

    /* mate info cache */
    int64_t mate_row_gap_cachable;

    char const **comments;

    bool quantizeQual;
    uint8_t qualQuant[256];
    uint8_t qualQuantSingle; /*** the quality is quantized - single value, no need to retrieve **/
};

struct params_s const *param;
ReferenceList const *gRefList;

typedef union UData_union {
    void const *v;
    uint32_t const *u32;
    int32_t const *i32;
    int64_t const *i64;
    uint64_t const *u64;
    uint8_t const *u8;
    char const *str;
    bool const *tf;
    INSDC_coord_one* coord1;
    INSDC_coord_zero* coord0;
    INSDC_coord_len* coord_len;
    INSDC_coord_val* coord_val;
    INSDC_SRA_xread_type* read_type;
    INSDC_SRA_read_filter* read_filter;
} UData;

typedef struct SCol_struct {
    char const *name;
    uint32_t idx;
    UData base;
    uint32_t len;
    bool optional;
} SCol;

typedef struct STable_struct {
    char const *name;
    VTable const *vtbl;
} STable;

typedef struct SCursCache_struct {
    KVector* cache;
    KVector* cache_unaligned_mate; /* keeps unaligned-mate for a half-aligned spots */
    uint32_t sam_flags;
    INSDC_coord_zero pnext;
    int32_t tlen;
    ReferenceObj const *ref;
    /* cache stats */
    uint64_t projected;
    uint64_t added;
    uint64_t hit;
    uint64_t bad;
} SCursCache;

typedef struct SCurs_struct {
    STable const *tbl;
    VCursor const *vcurs;
    SCursCache* cache;
    SCursCache cache_local;
    uint64_t col_reads_qty;
} SCurs;

enum eDSTableType {
    edstt_NotSpecified,
    edstt_Reference,
    edstt_Sequence,
    edstt_PrimaryAlignment,
    edstt_SecondaryAlignment,
    edstt_EvidenceAlignment,
    edstt_EvidenceInterval
};

typedef struct DataSource_s {
    STable tbl;
    SCurs curs;
    SCol *cols;
    enum eDSTableType type;
} DataSource;

#define DATASOURCE_INIT(O, NAME) do { memset(&O, 0, sizeof(O)); O.tbl.name = NAME; O.curs.tbl = &O.tbl; } while(0)

typedef struct SAM_dump_ctx_s {
    VDatabase const *db;
    char const *fullPath;
    char const *accession;
    char const *readGroup;

    DataSource ref;
    DataSource pri;
    DataSource sec;
    DataSource evi;
    DataSource eva;
    DataSource seq;
} SAM_dump_ctx_t;

enum ealg_col {
    alg_SEQ_SPOT_ID = 0,
    alg_SEQ_NAME,
    alg_MAPQ,
    alg_CIGAR,
    alg_READ,
    alg_READ_START,
    alg_READ_LEN,
    alg_CIGAR_LEN,
    alg_SAM_QUALITY,
    alg_SPOT_GROUP,
    alg_SEQ_SPOT_GROUP,
    alg_SEQ_READ_ID,
    alg_REVERSED,
    alg_ALIGNMENT_COUNT,
    alg_EDIT_DISTANCE,
    alg_READ_FILTER,
    alg_MATE_ALIGN_ID,
    alg_MATE_REF_NAME,
    alg_SAM_FLAGS,
    alg_REF_START,
    alg_MATE_REF_POS,
    alg_ALIGN_GROUP,
    alg_EVIDENCE_ALIGNMENT_IDS,
    alg_REF_PLOIDY,
    alg_REF_ID,
    alg_MATE_REF_ID,
    alg_HAS_MISMATCH,
    alg_REGION_FILTER,
    alg_REF_NAME = alg_REGION_FILTER,
    alg_REF_SEQ_ID,
    alg_REF_POS,
    alg_REF_LEN,
    alg_DISTANCE_FILTER,
    alg_TEMPLATE_LEN = alg_DISTANCE_FILTER,
    alg_CG_TAGS_STR
};


SCol const g_alg_col_tmpl[] = {
    { "SEQ_SPOT_ID", 0, {NULL}, 0, true },
    { "SEQ_NAME", 0, {NULL}, 0, false },
    { "MAPQ", 0, {NULL}, 0, false },
    { "?CIGAR column name?", 0, {NULL}, 0, false },
    { "?READ column name?", 0, {NULL}, 0, false },
    { "READ_START", 0, {NULL}, 0, false }, /* READ_START */
    { "READ_LEN", 0, {NULL}, 0, false }, /* READ_LEN */
    { "?CIGAR_LEN column name?", 0, {NULL}, 0, true }, /* CIGAR_LEN */
    { "SAM_QUALITY", 0, {NULL}, 0, false },
    { "SPOT_GROUP", 0, {NULL}, 0, true },
    { "SEQ_SPOT_GROUP", 0, {NULL}, 0, true },
    { "SEQ_READ_ID", 0, {NULL}, 0, true },
    { "REF_ORIENTATION", 0, {NULL}, 0, true },
    { "ALIGNMENT_COUNT", 0, {NULL}, 0, true },
    { "EDIT_DISTANCE", 0, {NULL}, 0, false },
    { "", 0, {NULL}, 0, false },
    /* start cols used as standalone in DumpUnaligned */
    /* MATE_ALIGN_ID col must preceeed
       MATE_REF_NAME, MATE_REF_POS, SAM_FLAGS, TEMPLATE_LEN for cache to work */
    { "MATE_ALIGN_ID", 0, {NULL}, 0, false },
    { "?MATE_REF_NAME column name?", 0, {NULL}, 0, false },
    { "SAM_FLAGS", 0, {NULL}, 0, false },
    { "REF_START", 0, {NULL}, 0, false },  /* priming cursor cache */
    { "MATE_REF_POS", 0, {NULL}, 0, false },
    { "ALIGN_GROUP", 0, {NULL}, 0, true },
    { "", 0, {NULL}, 0, true }, /* EVIDENCE_ALIGNMENT_IDS */
    { "", 0, {NULL}, 0, true }, /* REF_PLOIDY */
    { "REF_ID", 0, {NULL}, 0, true }, /* REF_ID */
    { "MATE_REF_ID", 0, {NULL}, 0, true }, /* priming cursor cache */
    { "(bool)HAS_MISMATCH", 0, {NULL}, 0, true },
    /* these are read before any other for filtering so they must be last */
    { "REF_NAME", 0, {NULL}, 0, false },
    { "REF_SEQ_ID", 0, {NULL}, 0, false },
    { "REF_POS", 0, {NULL}, 0, false },
    /* end cols used as standalone in DumpUnaligned */
    { "REF_LEN", 0, {NULL}, 0, false },
    { "TEMPLATE_LEN", 0, {NULL}, 0, false },
    { NULL, 0, {NULL}, 0, false }, /* alg_CG_TAGS_STR */
    { NULL, 0, {NULL}, 0, false }
};

enum eseq_col {
    seq_READ = 0,
    seq_QUALITY,
    seq_SPOT_GROUP,
    seq_READ_START,
    seq_READ_LEN,
    seq_READ_TYPE,
    seq_READ_FILTER,
    seq_NAME,
    seq_PRIMARY_ALIGNMENT_ID
};

static SCol const gSeqCol[] = {
    { "READ", 0, {NULL}, 0, false },
    { "(INSDC:quality:text:phred_33)QUALITY", 0, {NULL}, 0, false },
    { "SPOT_GROUP", 0, {NULL}, 0, true },
    { "READ_START", 0, {NULL}, 0, true },
    { "READ_LEN", 0, {NULL}, 0, true },
    { "READ_TYPE", 0, {NULL}, 0, true },
    { "READ_FILTER", 0, {NULL}, 0, true },
    { "NAME", 0, {NULL}, 0, true },
    /* must be last in list to avoid reading all columns */
    { "PRIMARY_ALIGNMENT_ID", 0, {NULL}, 0, true },
    { NULL, 0, {NULL}, 0, false }
};

static rc_t RefSeqPrint( void ) {
    rc_t rc = 0;
    uint32_t i, count = 0;

    rc = ReferenceList_Count( gRefList, &count );
    for( i = 0; rc == 0 && i < count; i++ ) {
        ReferenceObj const *obj;
        rc = ReferenceList_Get( gRefList, &obj, i );
        if ( rc == 0 ) {
            char const *seqid = NULL;
            rc = ReferenceObj_SeqId( obj, &seqid );
            if ( rc == 0 ) {
                char const *name = NULL;
                rc = ReferenceObj_Name( obj, &name );
                if ( rc == 0 ) {
                    INSDC_coord_len len;
                    rc = ReferenceObj_SeqLength( obj, &len );
                    if ( rc == 0 ) {
                        char const *nm;
                        if ( param->use_seqid && seqid != NULL && seqid[ 0 ] != '\0' ) {
                            nm = seqid;
                        } else {
                            nm = name;
                        }
                        KOutMsg( "@SQ\tSN:%s", nm );
                        if ( nm != seqid && seqid != NULL && seqid[ 0 ] != '\0' && strcmp( nm, seqid ) != 0 ) {
                            KOutMsg( "\tAS:%s", seqid );
                        }
                        KOutMsg( "\tLN:%u\n", len );
                    }
                }
            }
            ReferenceObj_Release( obj );
        }
    }
    return rc;
}

#if USE_MATE_CACHE
static rc_t Cache_Init( SCursCache* c ) {
    if ( c != NULL ) {
        rc_t rc;
        memset( c, 0, sizeof( *c ) );
        rc = KVectorMake( &c->cache );
        if ( rc == 0 ) {
            rc = KVectorMake( &c->cache_unaligned_mate );
        }
    }
    return 0;
}

static void Cache_Close( char const *name, SCursCache* c ) {
    if ( c != NULL ) {
        KVectorRelease( c->cache );
        KVectorRelease( c->cache_unaligned_mate );
        if ( c->added > 0 ) {
            SAM_DUMP_DBG( 2, ( "%s cache stats: projected %lu added of those %lu; "
                               "hits %lu of those broken %lu;\n",
                               name, c->projected, c->added, c->hit, c->bad ) );
        }
    }
    memset( c, 0, sizeof( *c ) );
}

static rc_t Cache_Add( uint64_t key, SCurs const *curs, SCol const *cols ) {
    /* compact values for mate record to cache as:
        pos_delta - 32bit, ref_proj - 11bit, flags - 11bit, rnext_idx - 10bit = 64bit
    */
    rc_t rc = 0;
    ReferenceObj const *r = NULL;
    uint32_t rid = 0;
    uint64_t val = 0;
    int64_t mate_id = cols[ alg_MATE_ALIGN_ID ].len > 0 ? cols[ alg_MATE_ALIGN_ID ].base.i64[ 0 ] : 0;

    rc = ReferenceList_Find( gRefList, &r, cols[ alg_REF_NAME ].base.str, cols[ alg_REF_NAME ].len );
    if ( rc == 0 ) {
        rc = ReferenceObj_Idx( r, &rid );
    }
#if _DEBUGGING
    {
        char const *rname = NULL;
        curs->cache->projected++;
        ReferenceObj_Name( r, &rname );
        SAM_DUMP_DBG( 10, ( "to cache row %li for mate %li: %u,%s[%hu],%u,%u,%i",
            key, mate_id, cols[ alg_SAM_FLAGS ].base.u32[ 0 ], rname, rid,
            cols[ alg_REF_POS ].len ? cols[ alg_REF_POS ].base.coord0[ 0 ] : 0,
            cols[ alg_MATE_REF_POS ].len ? cols[ alg_MATE_REF_POS ].base.coord0[ 0 ] : 0,
            cols[ alg_TEMPLATE_LEN ].len > 0 ? cols[ alg_TEMPLATE_LEN ].base.i32[ 0 ] : 0));
    }
#endif
    if ( rc == 0 && !( rid & 0xFC00 ) ) {
        int64_t pos_delta64;
        int32_t pos_delta32;

        if ( mate_id != 0 ) {
            ReferenceObj const *rm = NULL;
            uint32_t rm_id;

            rc = ReferenceList_Find( gRefList, &rm, cols[ alg_MATE_REF_NAME ].base.str, cols[ alg_MATE_REF_NAME ].len );
            if ( rc == 0 ) {
                rc = ReferenceObj_Idx( rm, &rm_id );
            }
            assert( rm != NULL );
            if ( rc == 0 && rid != rm_id ) {
                char const *rm_name = NULL;
                ReferenceObj_Name( rm, &rm_name );
                mate_id = 0;
                SAM_DUMP_DBG( 10, ( " mate ref differ: %s[%hu]!", rm_name, rm_id ) );
            }
            ReferenceObj_Release( rm );
        }

        if ( mate_id != 0 ) {
            pos_delta64 = cols[ alg_MATE_REF_POS ].base.coord0[ 0 ] - cols[ alg_REF_POS ].base.coord0[ 0 ];
        } else {
            pos_delta64 = cols[ alg_REF_POS ].base.coord0[ 0 ];
        }

        pos_delta32 = pos_delta64;
        if ( pos_delta64 == pos_delta32 ) {
            int64_t ref_proj;
            if ( mate_id == 0 ) {
                ref_proj = 0; /* indicates full value in delta */
            } else if ( cols[ alg_TEMPLATE_LEN ].base.i32[ 0 ] < 0 ) {
                assert( pos_delta32 <= 0 );
                ref_proj = pos_delta32 - cols[ alg_TEMPLATE_LEN ].base.i32[ 0 ];
            } else {
                assert( pos_delta32 >= 0 );
                ref_proj = cols[ alg_TEMPLATE_LEN ].base.i32[ 0 ] - pos_delta32;
            }

            if ( !( ref_proj & 0xFFFFF800 ) ) {
                val = ( pos_delta64 << 32 ) | ( ref_proj << 21 ) | ( cols[ alg_SAM_FLAGS ].base.u32[ 0 ] << 10 ) | rid;
                rc = KVectorSetU64( curs->cache->cache, key, val );
            }
        }
    }
    ReferenceObj_Release( r );

#if _DEBUGGING
    if ( val == 0 ) {
        SAM_DUMP_DBG( 10, ( " --> out of range\n" ) );
    } else {
        SAM_DUMP_DBG( 10, ( " --> %016lX\n", val ) );
        curs->cache->added++;
    }
#endif
    return rc;
}

static rc_t Cache_Get( SCurs const *curs, uint64_t key, uint64_t* val ) {
    rc_t rc = KVectorGetU64( curs->cache->cache, key, val );
    if ( rc == 0 ) {
        uint32_t id = ( *val & 0x3FF );
#if _DEBUGGING
        curs->cache->hit++;
#endif
        KVectorUnset( curs->cache->cache, key );
        rc = ReferenceList_Get( gRefList, &curs->cache->ref, id );
        if ( rc != 0 ) {
            *val = 0;
            curs->cache->ref = NULL;
            rc = RC( rcExe, rcNoTarg, rcSearching, rcItem, rcNotFound );
#if _DEBUGGING
            curs->cache->bad++;
#endif
        } else {
            SAM_DUMP_DBG( 10, ( "from cache row %li %016lX", key, *val ) );
        }
    }
    return rc;
}

static void Cache_Unpack( uint64_t val, int64_t mate_id, SCurs const *curs, SCol* cols ) {
    int32_t pos_delta = ( val & 0xFFFFFFFF00000000 ) >> 32;
    uint32_t ref_proj = ( val & 0x00000000FFE00000 ) >> 21;
    uint32_t flags = ( val & 0x00000000001FFC00 ) >> 10;

    if ( mate_id != 0 ) {
        /* adjust flags for mate record */
        curs->cache->sam_flags = ( flags & 0x1 ) |
                                 ( flags & 0x2 ) |
                                 ( ( flags & 0x8 ) >> 1 ) |
                                 ( ( flags & 0x4 ) << 1 ) |
                                 ( ( flags & 0x20 ) >> 1 ) |
                                 ( ( flags & 0x10 ) << 1 ) |
                                 ( ( flags & 0x40 ) ? 0x80 : 0x40 ) |
                                 ( flags & 0x700 );
    } else {
        /* preserve flags as if original records is restored */
        curs->cache->sam_flags = flags;
    }
    cols[ alg_SAM_FLAGS ].base.u32 = &curs->cache->sam_flags;
    cols[ alg_SAM_FLAGS ].len = sizeof( curs->cache->sam_flags );

    if ( param->use_seqid ) {
        ReferenceObj_SeqId( curs->cache->ref, &cols[ alg_MATE_REF_NAME ].base.str );
    } else {
        ReferenceObj_Name( curs->cache->ref, &cols[ alg_MATE_REF_NAME ].base.str );
    }

    cols[ alg_MATE_REF_NAME ].len = string_size( cols[ alg_MATE_REF_NAME ].base.str );

    if ( ref_proj == 0 ) {
        curs->cache->pnext = pos_delta;
        curs->cache->tlen = 0;
    } else if ( pos_delta > 0 ) {
        curs->cache->pnext = ( cols[ alg_REF_POS ].len > 0 ? cols[ alg_REF_POS ].base.coord0[ 0 ] : 0 ) - pos_delta;
        curs->cache->tlen = - ( ref_proj + pos_delta );
    } else {
        curs->cache->pnext = ( cols[ alg_REF_POS ].len > 0 ? cols[ alg_REF_POS ].base.coord0[ 0 ] : 0 ) - pos_delta;
        curs->cache->tlen = ref_proj - pos_delta;
    }

    cols[ alg_MATE_REF_POS ].base.coord0 = &curs->cache->pnext;
    cols[ alg_MATE_REF_POS ].len = sizeof( curs->cache->pnext );
    cols[ alg_TEMPLATE_LEN ].base.i32 = &curs->cache->tlen;
    cols[ alg_TEMPLATE_LEN ].len = sizeof( curs->cache->tlen );

    {
        uint32_t id;
        ReferenceObj_Idx( curs->cache->ref, &id );
        SAM_DUMP_DBG( 10, ( " --> mate %li: %u,%s[%hu],%u,%i\n",
            mate_id, curs->cache->sam_flags, cols[ alg_MATE_REF_NAME ].base.str,
            id, curs->cache->pnext, curs->cache->tlen ) );
    }
}
#endif /* USE_MATE_CACHE */

#if 0
static rc_t OpenVTable( VDatabase const *db, STable* tbl, char const *name, bool optional ) {
    rc_t rc = VDatabaseOpenTableRead( db, &tbl->vtbl, "%s", name );
    if ( GetRCState( rc ) == rcNotFound && optional ) {
        rc = 0;
        tbl->vtbl = NULL;
    }
    tbl->name = name;
    return rc;
}
#endif

static rc_t Cursor_Open( STable const *const tbl, SCurs *const curs, SCol cols[], SCursCache* cache ) {
    rc_t rc = 0;
    unsigned i;

    curs->vcurs = NULL;
    if ( tbl == NULL || tbl->vtbl == NULL )
        return 0;

    rc = VTableCreateCachedCursorRead( tbl->vtbl, &curs->vcurs, CURSOR_CACHE );
    if ( rc != 0 )
        return rc;

    rc = VCursorPermitPostOpenAdd( curs->vcurs );
    if ( rc != 0 )
        return rc;

    if ( rc == 0 ) {
        rc = VCursorOpen( curs->vcurs );
        if ( rc == 0 ) {
#if USE_MATE_CACHE
            if ( cache != NULL ) {
                curs->cache = cache;
            } else {
                curs->cache = &curs->cache_local;
                rc = Cache_Init( &curs->cache_local );
            }
#endif /* USE_MATE_CACHE */
            curs->tbl = tbl;
        }
    }

    for ( i = 0; cols[ i ].name != NULL; ++i ) {
        if ( cols[ i ].name[ 0 ] == 0 )
            continue;
        rc = VCursorAddColumn( curs->vcurs, &cols[ i ].idx, "%s", cols[ i ].name );
        if ( GetRCObject( rc ) == ( enum RCObject ) rcColumn ) {
            switch ( GetRCState( rc ) ) {
            case rcNotFound:
            case rcUndefined:
                if ( !cols[ i ].optional )
                    break;
            case rcExists:
                rc = 0;
            default:
                break;
            }
        }
        if ( rc != 0 ) {
            (void)PLOGERR( klogErr, ( klogErr, rc, "table $(t) column $(c)", "t=%s,c=%s", tbl->name, cols[ i ].name ) );
            break;
        }
    }

    if ( rc != 0 ) {
        VCursorRelease( curs->vcurs );
        curs->vcurs = NULL;
        if ( rc != KLogLastErrorCode() ) {
            (void)PLOGERR( klogErr, ( klogErr, rc, "table $(t)", "t=%s", tbl->name ) );
        }
    } else {
        SAM_DUMP_DBG( 2, ( "%s: table %s\n", __func__, curs->tbl->name ) );
    }
    return rc;
}

static void Cursor_Close( SCurs* curs ) {
    if ( curs != NULL && curs->vcurs != NULL )
    {
        SAM_DUMP_DBG( 2, ( "%s: table %s, columns rows read %lu\n", __func__, curs->tbl->name, curs->col_reads_qty ) );
        VCursorRelease( curs->vcurs );
#if USE_MATE_CACHE
        if ( curs->cache == &curs->cache_local ) {
            Cache_Close( curs->tbl->name, curs->cache );
        }
#endif /* USE_MATE_CACHE */
        memset( curs, 0, sizeof( *curs ) );
    }
}

static rc_t Cursor_Read( DataSource *ds, int64_t row_id, int firstCol, unsigned nCols ) {
    rc_t rc = 0;

    if ( ds->curs.vcurs == NULL ) {
        rc = Cursor_Open( &ds->tbl, &ds->curs, ds->cols, ds->curs.cache );
        if ( rc != 0 )
            return rc;
    }
    if ( 1 ) {
        SCol *const col = &ds->cols[ firstCol ];
        unsigned i;

        for ( i = 0; i < nCols && col[ i ].name; ++i ) {
            uint32_t const idx = col[ i ].idx;

            if ( idx != 0 ) {
                uint32_t elem_bits;
                uint32_t bit_offset;
                uint32_t elem_count;
                void const *base;

                rc = VCursorCellDataDirect( ds->curs.vcurs, row_id, idx, &elem_bits, &base, &bit_offset, &elem_count );
                if ( rc != 0 ) {
                    (void)PLOGERR( klogWarn, ( klogWarn, rc, "reading $(t) row $(r), column $(c)", "t=%s,r=%li,c=%s",
                                               ds->tbl.name, row_id, col[ i ].name ) );
                    return rc;
                }

                assert( bit_offset == 0 );
                assert( elem_bits % 8 == 0 );

                col[ i ].base.v = base;
                col[ i ].len = elem_count;
            }
        }
    }
    return rc;
}

struct {
    KWrtWriter writer;
    void* data;
    KFile* kfile;
    uint64_t pos;
} g_out_writer = {NULL};

static rc_t CC BufferedWriter( void *const self, char const buffer[], size_t const bufsize, size_t *const pnum_writ ) {
    rc_t rc = 0;
    size_t written = 0;

    assert( buffer != NULL );

    while ( written < bufsize ) {
        size_t n;

        rc = KFileWrite( g_out_writer.kfile, g_out_writer.pos + written, &buffer[ written ], bufsize - written, &n );
        if ( rc != 0 )
            break;
        written += n;
    }
    g_out_writer.pos += written;
    if ( pnum_writ != NULL ) {
        *pnum_writ = written;
    }
    return rc;
}

static rc_t BufferedWriterMake( bool gzip, bool bzip2 ) {
    rc_t rc = 0;

    if ( gzip && bzip2 ) {
        rc = RC( rcApp, rcFile, rcConstructing, rcParam, rcAmbiguous );
    } else if ( g_out_writer.writer != NULL ) {
        rc = RC( rcApp, rcFile, rcConstructing, rcParam, rcAmbiguous );
    } else {
        rc = KFileMakeStdOut( &g_out_writer.kfile );
        if ( rc == 0 ) {
            g_out_writer.pos = 0;

            if ( gzip ) {
                KFile* gz;
                rc = KFileMakeGzipForWrite( &gz, g_out_writer.kfile );
                if ( rc == 0 ) {
                    KFileRelease( g_out_writer.kfile );
                    g_out_writer.kfile = gz;
                }
            } else if ( bzip2 ) {
                KFile* bz;
                rc = KFileMakeBzip2ForWrite( &bz, g_out_writer.kfile );
                if ( rc == 0 ) {
                    KFileRelease( g_out_writer.kfile );
                    g_out_writer.kfile = bz;
                }
            }

            if ( rc == 0 ) {
                KFile* buf;
                rc = KBufFileMakeWrite( &buf, g_out_writer.kfile, false, 128 * 1024 );
                if ( rc == 0 ) {
                    KFileRelease( g_out_writer.kfile );
                    g_out_writer.kfile = buf;
                    g_out_writer.writer = KOutWriterGet();
                    g_out_writer.data = KOutDataGet();
                    rc = KOutHandlerSet( BufferedWriter, &g_out_writer );
                }
            }
        }
    }
    return rc;
}

static void BufferedWriterRelease( bool flush ) {
    if ( flush ) {
        /* avoid flushing buffered data after failure */
        KFileRelease( g_out_writer.kfile );
    }
    if ( g_out_writer.writer != NULL ) {
        KOutHandlerSet( g_out_writer.writer, g_out_writer.data );
    }
    g_out_writer.writer = NULL;
}

typedef struct ReadGroup {
    BSTNode node;
    char name[ 1024 ];
} ReadGroup;

static int64_t CC ReadGroup_sort( BSTNode const *item, BSTNode const *node ) {
    return strcmp( ( ( ReadGroup const * )item )->name, ( ( ReadGroup const * ) node )->name );
}

static rc_t ReadGroup_print( char const *nm ) {
    rc_t rc = 0;
#if WINDOWS
    if ( nm[ 0 ] != '\0' && _stricmp( nm, "default" ) ) {
#else
    if ( nm[ 0 ] != '\0' && strcasecmp( nm, "default" ) ) {
#endif
        rc = KOutMsg( "@RG\tID:%s\n", nm );
    }
    return rc;
}

static void CC ReadGroup_dump( BSTNode *n, void *data ) {
    ReadGroup const *g = ( ReadGroup* )n;
    ReadGroup_print( g->name );
}

static rc_t CC DumpReadGroupsScan( STable const *tbl ) {
    SCol cols[] = {
        { "SPOT_GROUP", 0, {NULL}, 0, false },
        { NULL, 0, {NULL}, 0, false }
    };
    rc_t rc = 0;
    BSTree tree;
    DataSource ds;

    memset( &ds, 0, sizeof( ds ) );
    ds.cols = cols;

    BSTreeInit( &tree );
    rc = Cursor_Open( tbl, &ds.curs, ds.cols, NULL );
    if ( rc == 0 ) {
        int64_t start;
        uint64_t count;

        rc = VCursorIdRange( ds.curs.vcurs, 0, &start, &count );
        if ( rc == 0 ) {
            ReadGroup* node = NULL;
            uint32_t last_len = 0;

            while ( count > 0 && rc == 0 ) {
                rc = Cursor_Read( &ds, start, 0, ~(unsigned)0 );
                if ( rc == 0 && cols[ 0 ].len != 0 ) {
                    if ( node == NULL ||
                         last_len != cols[ 0 ].len ||
                         strncmp( cols[ 0 ].base.str, node->name, cols[ 0 ].len ) != 0 ) {
                        node = malloc( sizeof( *node ) );
                        if ( node == NULL ) {
                            rc = RC( rcExe, rcNode, rcConstructing, rcMemory, rcExhausted );
                        } else if ( cols[ 0 ].len > sizeof( node->name ) ) {
                            rc = RC( rcExe, rcString, rcCopying, rcBuffer, rcInsufficient );
                        } else {
                            last_len = cols[ 0 ].len;
                            string_copy( node->name, ( sizeof node->name ) - 1, cols[ 0 ].base.str, last_len );
                            node->name[ last_len ] = '\0';
                            rc = BSTreeInsertUnique( &tree, &node->node, NULL, ReadGroup_sort );
                            if ( GetRCState( rc ) == rcExists ) {
                                free( node );
                                rc = 0;
                            }
                        }
                    }
                } else if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcRow ) {
                    rc = 0;
                }
                start++;
                count--;
            }
        }
        Cursor_Close( &ds.curs );
    }

    if ( rc == 0 ) {
        BSTreeForEach( &tree, false, ReadGroup_dump, NULL );
    }  else if ( rc != KLogLastErrorCode() ) {
        (void)PLOGERR( klogErr, ( klogErr, rc, "$(f)", "f=%s", __func__ ) );
    }
    BSTreeWhack( &tree, NULL, NULL );
    return rc;
}

rc_t CC DumpReadGroups( STable const *tbl ) {
    rc_t rc = 0;
    KMetadata const *m;

    /* try getting list from stats meta */
    rc = VTableOpenMetadataRead( tbl->vtbl, &m );
    if ( rc == 0 ) {
        KMDataNode const *n;
        rc = KMetadataOpenNodeRead( m, &n, "/STATS/SPOT_GROUP" );
        if ( rc == 0 ) {
            KNamelist* names;
            rc = KMDataNodeListChild( n, &names );
            if ( rc == 0 ) {
                uint32_t i, q;
                rc = KNamelistCount( names, &q );
                if ( rc == 0 ) {
                    for ( i = 0; rc == 0 && i < q; i++ ) {
                        char const *nm;
                        rc = KNamelistGet( names, i, &nm );
                        if ( rc == 0 ) {
                            rc = ReadGroup_print( nm );
                        }
                    }
                }
                KNamelistRelease( names );
            }
            KMDataNodeRelease( n );
        }
        KMetadataRelease( m );
    }

    if ( GetRCState( rc ) == rcNotFound ) {
        rc = DumpReadGroupsScan( tbl );
    } else if ( rc != 0 && rc != KLogLastErrorCode() ) {
        (void)PLOGERR( klogErr, ( klogErr, rc, "$(f)", "f=%s", __func__ ) );
    }
    return rc;
}

static rc_t Cursor_ReadAlign( SCurs const *curs, int64_t row_id, SCol* cols, uint32_t idx ) {
    rc_t rc = 0;
    SCol* c = NULL;
    SCol* mate_id = NULL;
#if USE_MATE_CACHE
    uint64_t cache_val = 0;
    bool cache_miss = false;
#endif /* USE_MATE_CACHE */


    for( ; rc == 0 && cols[ idx ].name != NULL; idx++ ) {
        c = &cols[ idx ];
        if ( c->idx != 0 ) {
#if USE_MATE_CACHE
            if ( mate_id != NULL && curs->cache != NULL && !cache_miss &&
                ( idx == alg_SAM_FLAGS || idx == alg_MATE_REF_NAME || idx == alg_MATE_REF_POS || idx == alg_TEMPLATE_LEN ) &&
                mate_id->idx && mate_id->len > 0 && mate_id->base.i64[ 0 ] > 0 ) {
                if ( cache_val != 0 ) {
                    continue;
                }
                rc = Cache_Get( curs, mate_id->base.u64[ 0 ], &cache_val );
                if ( rc == 0 ) {
                    continue;
                } else if ( !( GetRCObject( rc ) == rcItem && GetRCState( rc ) == rcNotFound ) ) {
                    break;
                } else {
                    /* avoid multiple lookups in cache */
                    cache_miss = true;
                }
            }
#endif /* USE_MATE_CACHE */
            rc = VCursorCellDataDirect( curs->vcurs, row_id, c->idx, NULL, &c->base.v, NULL, &c->len );
            if ( rc == 0 ) {
                if ( idx == alg_SEQ_SPOT_ID && ( c->len  ==0 || c->base.i64[ 0 ] == 0 ) ) {
                    return RC( rcExe, rcColumn, rcReading, rcRow, rcNotFound );
                }
                if ( idx == alg_MATE_ALIGN_ID ) {
                    mate_id = &cols[ alg_MATE_ALIGN_ID ];
                }
#if _DEBUGGING
                ( ( SCurs* )curs )->col_reads_qty++;
#endif
            }
        } else {
            static INSDC_coord_zero readStart;
            static INSDC_coord_len  readLen;
            static INSDC_coord_len  cigarLen;

            switch ( (int)idx ) {
            case alg_READ_START:
                readStart = 0;
                c->base.coord0 = &readStart;
                c->len = 1;
                break;
            case alg_READ_LEN:
                readLen = cols[ alg_READ ].len;
                c->base.coord_len = &readLen;
                c->len = 1;
                break;
            case alg_CIGAR_LEN:
                cigarLen = cols[ alg_CIGAR ].len;
                c->base.coord_len = &cigarLen;
                c->len = 1;
                break;
            }
        }
    }

    if ( rc != 0 ) {
        (void)PLOGERR( klogWarn, ( klogWarn, rc, "reading $(t) row $(r), column $(c)", "t=%s,r=%li,c=%s",
            curs->tbl->name, row_id, c ? c->name : "<none>" ) );
#if USE_MATE_CACHE
    } else if ( curs->cache == NULL ) {
    } else if ( cache_val == 0 ) {
        /* this row is not from cache */
        int64_t mate_align_id = ( mate_id != NULL && mate_id->len > 0 ) ? mate_id->base.i64[ 0 ] : 0;
        if ( cols[ 0 ].idx != 0 && /* we have full cursor which means we are reading alignment table */
            /* no mate -> unaligned (not secondary!) */
            ( ( mate_align_id == 0 && param->unaligned && curs->cache != &curs->cache_local ) ||
            /* 2nd mate with higher rowid and more than set gap rows away */
              ( mate_align_id != 0 && mate_align_id > row_id && ( mate_align_id - row_id) > param->mate_row_gap_cachable ) ) )
        {
          rc = Cache_Add( row_id, curs, cols );
        }
        if ( param->unaligned == true && mate_align_id == 0 ) {
            rc = KVectorSetBool( curs->cache->cache_unaligned_mate, cols[alg_SEQ_SPOT_ID].base.i64[0], true );
        }
    } else {
        Cache_Unpack( cache_val, row_id, curs, cols );
#endif /* USE_MATE_CACHE */
    }
    return rc;
}

static rc_t DumpName( char const *name, size_t name_len,
                      const char spot_group_sep, char const *spot_group,
                      size_t spot_group_len, int64_t spot_id ) {
    rc_t rc = 0;
    if ( param->cg_friendly_names ) {
        rc = KOutMsg( "%.*s-1:%lu", spot_group_len, spot_group, spot_id );
    } else {
        if ( param->name_prefix != NULL ) {
            rc = KOutMsg( "%s.", param->name_prefix );
        }
        rc = BufferedWriter( NULL, name, name_len, NULL );
        if ( rc == 0 && param->spot_group_in_name && spot_group_len > 0 ) {
            rc = BufferedWriter( NULL, &spot_group_sep, 1, NULL );
            if ( rc == 0 ) {
                rc = BufferedWriter( NULL, spot_group, spot_group_len, NULL );
            }
        }
    }
    return rc;
}

static rc_t DumpQuality( char const quality[], unsigned const count, bool const reverse, bool const quantize ) {
    rc_t rc = 0;
    if ( quality == NULL ) {
        unsigned i;

        for ( i = 0; rc == 0 && i < count; ++i ) {
            char const newValue = (param->qualQuantSingle?param->qualQuantSingle:30) + 33;
            rc = BufferedWriter( NULL, &newValue, 1, NULL );
        }
    } else if ( reverse || quantize ) {
        unsigned i;

        for ( i = 0; rc == 0 && i < count; ++i ) {
            char const qual = quality[ reverse ? ( count - i - 1 ) : i ];
            char const newValue = quantize ? param->qualQuant[ qual - 33 ] + 33 : qual;

            rc = BufferedWriter( NULL, &newValue, 1, NULL );
        }
    } else {
        rc = BufferedWriter( NULL, quality, count, NULL );
    }
    return rc;
}

static rc_t DumpUnalignedFastX( const SCol cols[], uint32_t read_id, INSDC_coord_zero readStart, INSDC_coord_len readLen, int64_t row_id ) {
    /* fast[AQ] represnted in SAM fields:
       [@|>]QNAME unaligned
       SEQ
       +
       QUAL
    */
    rc_t rc = BufferedWriter( NULL, param->fastq ? "@" : ">", 1, NULL );

    /* QNAME: [PFX.]SEQUENCE:NAME[#SPOT_GROUP] */
    if ( rc == 0 ) {
        rc = DumpName( cols[ seq_NAME ].base.str, cols[ seq_NAME ].len, '#',
                       cols[ seq_SPOT_GROUP ].base.str, cols[ seq_SPOT_GROUP ].len, row_id );
    }
    if ( rc == 0 && read_id > 0 ) {
        rc = KOutMsg( "/%u", read_id );
    }
    if ( rc == 0 ) {
        rc = BufferedWriter( NULL, " unaligned\n", 11, NULL );
    }
    /* SEQ: SEQUENCE.READ */
    if ( rc == 0 ) {
        rc = BufferedWriter( NULL, &cols[ seq_READ ].base.str[readStart], readLen, NULL );
    }
    if ( rc == 0 && param->fastq ) {
        /* QUAL: SEQUENCE.QUALITY */
        rc = BufferedWriter( NULL, "\n+\n", 3, NULL );
        if ( rc == 0 ) {
            rc = DumpQuality( &cols[ seq_QUALITY ].base.str[ readStart ], readLen, false, param->quantizeQual );
        }
    }
    if ( rc == 0 ) {
        rc = BufferedWriter( NULL, "\n", 1, NULL );
    }
    return rc;
}

static rc_t DumpAlignedFastX( const SCol cols[], int64_t const alignId, uint32_t read_id, bool primary, bool secondary ) {
    rc_t rc = 0;
    size_t nm;
    unsigned readId;
    unsigned const nreads = cols[ alg_READ_LEN ].len;

    for ( readId = 0; readId < nreads && rc == 0 ; ++readId ){
        char const *qname = cols[ alg_SEQ_NAME ].base.str;
        size_t qname_len = cols[ alg_SEQ_NAME ].len;
        char synth_qname[ 40 ];
        int64_t const spot_id = cols[alg_SEQ_SPOT_ID].len > 0 ? cols[alg_SEQ_SPOT_ID].base.i64[0] : 0;
        char const *const read = cols[ alg_READ ].base.str + cols[ alg_READ_START ].base.coord0[ readId ];
        char const *const qual = cols[ alg_SAM_QUALITY ].base.v
                               ? cols[ alg_SAM_QUALITY ].base.str + cols[ alg_READ_START ].base.coord0[ readId ]
                               : NULL;
        unsigned const readlen = cols[ alg_READ_LEN ].base.coord_len[ readId ];

        /* fast[AQ] represnted in SAM fields:
           [@|>]QNAME primary|secondary ref=RNAME pos=POS mapq=MAPQ
           SEQ
           +
           QUAL
        */
        rc = BufferedWriter( NULL, param->fastq ? "@" : ">", 1, NULL );
        /* QNAME: [PFX.]SEQ_NAME[#SPOT_GROUP] */
        if ( qname_len == 0 || qname == NULL ) {
            string_printf( synth_qname, sizeof( synth_qname ), &qname_len, "%li.%u", alignId, readId + 1 );
            qname = synth_qname;
        }
        nm = cols[ alg_SPOT_GROUP ].len ? alg_SPOT_GROUP : alg_SEQ_SPOT_GROUP;
        if ( rc == 0 ) {
            rc = DumpName( qname, qname_len, '.', cols[ nm ].base.str, cols[ nm ].len, spot_id);
        }
        if ( rc == 0 && read_id > 0 ) {
            rc = KOutMsg( "/%u", read_id );
        }
        if ( rc == 0 ) {
            if ( primary ) {
                rc = BufferedWriter( NULL, " primary", 8, NULL );
            } else if ( secondary ) {
                rc = BufferedWriter( NULL, " secondary", 10, NULL );
            }
        }

        /* RNAME: REF_NAME or REF_SEQ_ID */
        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, " ref=", 5, NULL );
        }
        if ( rc == 0 ) {
            if ( param->use_seqid ) {
                rc = BufferedWriter( NULL, cols[ alg_REF_SEQ_ID ].base.str, cols[ alg_REF_SEQ_ID ].len, NULL );
            } else {
                rc = BufferedWriter( NULL, cols[ alg_REF_NAME ].base.str, cols[ alg_REF_NAME ].len, NULL );
            }
        }

        /* POS: REF_POS, MAPQ: MAPQ */
        if ( rc == 0 ) {
            rc = KOutMsg( " pos=%u mapq=%i\n", cols[ alg_REF_POS ].base.coord0[ 0 ] + 1, cols[ alg_MAPQ ].base.i32[ 0 ] );
        }
        /* SEQ: READ */
        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, read, readlen, NULL );
        }
        if ( rc == 0 && param->fastq ) {
            /* QUAL: SAM_QUALITY */
            rc = BufferedWriter( NULL, "\n+\n", 3, NULL );
            if ( rc == 0 ) {
                rc = DumpQuality( qual, readlen, false, param->quantizeQual );
            }
        }
        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, "\n", 1, NULL );
        }
    }
    return rc;
}


static rc_t DumpUnalignedSAM( const SCol cols[], uint32_t flags, INSDC_coord_zero readStart, INSDC_coord_len readLen,
                       char const *rnext, uint32_t rnext_len, INSDC_coord_zero pnext, char const readGroup[], int64_t row_id ) {
    unsigned i;

    /* QNAME: [PFX.]NAME[.SPOT_GROUP] */
    rc_t rc = DumpName( cols[ seq_NAME ].base.str, cols[ seq_NAME ].len, '.',
              cols[ seq_SPOT_GROUP ].base.str, cols[ seq_SPOT_GROUP ].len, row_id );

    /* all these fields are const text for now */
    if ( rc == 0 ) {
        rc = KOutMsg( "\t%u\t*\t0\t0\t*\t%.*s\t%u\t0\t",
             flags, rnext_len ? rnext_len : 1, rnext_len ? rnext : "*", pnext );
    }
    /* SEQ: SEQUENCE.READ */
    if ( flags & 0x10 ) {
        for( i = 0; i < readLen && rc == 0; i++ ) {
            char base;

            DNAReverseCompliment( &cols[ seq_READ ].base.str[ readStart + readLen - 1 - i ], &base, 1 );
            rc = BufferedWriter( NULL, &base, 1, NULL );
        }
    } else {
        rc = BufferedWriter( NULL, &cols[ seq_READ ].base.str[ readStart ], readLen, NULL );
    }

    if ( rc == 0 ) {
        rc = BufferedWriter( NULL, "\t", 1, NULL );
    }
    /* QUAL: SEQUENCE.QUALITY */
    if ( rc == 0 ) {
        rc = DumpQuality( &cols[ seq_QUALITY ].base.str[ readStart ], readLen, flags & 0x10, param->quantizeQual );
    }
    /* optional fields: */
    if ( rc == 0 ) {
        if ( readGroup ) {
            rc = BufferedWriter( NULL, "\tRG:Z:", 6, NULL );
            if ( rc == 0 ) {
                rc = BufferedWriter( NULL, readGroup, string_size( readGroup ), NULL );
            }
        } else if ( cols[ seq_SPOT_GROUP ].len > 0 ) {
            /* read group */
            rc = BufferedWriter( NULL, "\tRG:Z:", 6, NULL );
            if ( rc == 0 ) {
                rc = BufferedWriter( NULL, cols[ seq_SPOT_GROUP ].base.str, cols[ seq_SPOT_GROUP ].len, NULL );
            }
        }
    }
    if ( rc == 0 ) {
        rc = BufferedWriter( NULL, "\n", 1, NULL );
    }
    return rc;
}


static rc_t DumpAlignedSAM( SAM_dump_ctx_t *const ctx,
                     DataSource const *ds,
                     int64_t alignId,
                     char const readGroup[],
                     int type ) {
    rc_t rc = 0;
    unsigned const nreads = ds->cols[ alg_READ_LEN ].len;
    SCol const *const cols = ds->cols;
    int64_t const spot_id = cols[alg_SEQ_SPOT_ID].len > 0 ? cols[alg_SEQ_SPOT_ID].base.i64[0] : 0;
    INSDC_coord_one const read_id = cols[alg_SEQ_READ_ID].len > 0 ? cols[alg_SEQ_READ_ID].base.coord1[0] : 0;
    INSDC_SRA_read_filter const *align_filter = cols[alg_READ_FILTER].len == nreads ? cols[alg_READ_FILTER].base.read_filter : NULL;
    INSDC_SRA_read_filter seq_filter = 0;
    unsigned readId;
    unsigned cigOffset = 0;

    if ( align_filter == NULL && spot_id && read_id && ctx->seq.cols ) {
        rc_t rc;

        rc = Cursor_Read(&ctx->seq, spot_id, seq_READ_FILTER, 1);
        if ( rc == 0 && ctx->seq.cols[seq_READ_FILTER].len >= read_id ) {
            seq_filter = ctx->seq.cols[seq_READ_FILTER].base.read_filter[read_id - 1];
        }
    }

    for ( readId = 0; readId < nreads && rc == 0 ; ++readId ) {
        char const *qname = cols[ alg_SEQ_NAME ].base.str;
        size_t qname_len = cols[ alg_SEQ_NAME ].len;
        char const *const read = cols[ alg_READ ].base.str + cols[ alg_READ_START ].base.coord0[ readId ];
        char const *const qual = cols[ alg_SAM_QUALITY ].base.v
                               ? cols[ alg_SAM_QUALITY ].base.str + cols[ alg_READ_START ].base.coord0[ readId ]
                               : NULL;
        unsigned const readlen = nreads > 1 ? cols[ alg_READ_LEN ].base.coord_len[ readId ] : cols[ alg_READ ].len;
        unsigned const sflags = cols[ alg_SAM_FLAGS ].base.v ? cols[ alg_SAM_FLAGS ].base.u32[ readId ] : 0;
        INSDC_SRA_read_filter const filt = align_filter ? align_filter[readId] : seq_filter;
        unsigned const flags = (sflags & ~((unsigned)0x200)) | ((filt == SRA_READ_FILTER_REJECT) ? 0x200 : 0);
        char const *const cigar = cols[ alg_CIGAR ].base.str + cigOffset;
        unsigned const cigLen = nreads > 1 ? cols[ alg_CIGAR_LEN ].base.coord_len[ readId ] : cols[ alg_CIGAR ].len;
        size_t nm;
        char synth_qname[1024];

        cigOffset += cigLen;
        if ( qname_len == 0 || qname == NULL ) {
            string_printf( synth_qname, sizeof( synth_qname ), &qname_len, "ALLELE_%li.%u", alignId, readId + 1 );
            qname = synth_qname;
        } else if (ds->type == edstt_EvidenceAlignment) {
            string_printf( synth_qname, sizeof( synth_qname ), &qname_len, "%u/ALLELE_%li.%u", spot_id, cols[ alg_REF_ID ].base.i64[ readId ], cols[ alg_REF_PLOIDY ].base.u32[ readId ] );
            qname = synth_qname;
        }
        nm = cols[ alg_SPOT_GROUP ].len ? alg_SPOT_GROUP : alg_SEQ_SPOT_GROUP;
        rc = DumpName( qname, qname_len, '.', cols[ nm ].base.str, cols[ nm ].len, spot_id );

        /* FLAG: SAM_FLAGS */
        if ( rc == 0 ) {
            if ( ds->type == edstt_EvidenceAlignment ) {
                bool const cmpl = cols[alg_REVERSED].base.v && readId < cols[alg_REVERSED].len ? cols[alg_REVERSED].base.tf[readId] : false;
                rc = KOutMsg( "\t%u\t", 1 | (cmpl ? 0x10 : 0) | (read_id == 1 ? 0x40 : 0x80) );
            } else if ( !param->unaligned      /** not going to dump unaligned **/
                 && ( flags & 0x1 )     /** but we have sequenced multiple fragments **/
                 && ( flags & 0x8 ) ) {   /** and not all of them align **/
                /*** remove flags talking about multiple reads **/
                /* turn off 0x001 0x008 0x040 0x080 */
                rc = KOutMsg( "\t%u\t", flags & ~0xC9 );
            } else {
                rc = KOutMsg( "\t%u\t", flags );
            }
        }

        if ( rc == 0 ) {
            if ( ds->type == edstt_EvidenceAlignment && type == 0 ) {
                rc = KOutMsg( "ALLELE_%li.%u", cols[ alg_REF_ID ].base.i64[ readId ], cols[ alg_REF_PLOIDY ].base.u32[ readId ] );
            } else {
                /* RNAME: REF_NAME or REF_SEQ_ID */
                if ( param->use_seqid ) {
                    rc = BufferedWriter( NULL, cols[ alg_REF_SEQ_ID ].base.str, cols[ alg_REF_SEQ_ID ].len, NULL );
                } else {
                    rc = BufferedWriter( NULL, cols[ alg_REF_NAME ].base.str, cols[ alg_REF_NAME ].len, NULL );
                }
            }
        }

        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, "\t", 1, NULL );
        }

        /* POS: REF_POS */
        if ( rc == 0 ) {
            rc = KOutMsg( "%i\t", cols[ alg_REF_POS ].base.coord0[ 0 ] + 1 );
        }

        /* MAPQ: MAPQ */
        if ( rc == 0 ) {
            rc = KOutMsg( "%i\t", cols[ alg_MAPQ ].base.i32[ 0 ] );
        }

        /* CIGAR: CIGAR_* */
        if ( ds->type == edstt_EvidenceInterval ) {
            unsigned i;

            for ( i = 0; i != cigLen && rc == 0; ++i ) {
                char ch = cigar[i];
                if ( ch == 'S' ) ch = 'I';
                rc = BufferedWriter( NULL, &ch, 1, NULL );
            }
        } else if ( ds->type == edstt_EvidenceAlignment ) {
            rc = cg_canonical_print_cigar(cigar,cigLen);
        } else {
            if ( rc == 0 ) {
                rc = BufferedWriter( NULL, cigar, cigLen, NULL );
            }
        }

        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, "\t", 1, NULL );
        }

        /* RNEXT: MATE_REF_NAME or '*' */
        /* PNEXT: MATE_REF_POS or 0 */
        if ( rc == 0 ) {
            if ( cols[ alg_MATE_REF_NAME ].len ) {
                if ( cols[ alg_MATE_REF_NAME ].len == cols[ alg_REF_NAME ].len &&
                    memcmp( cols[ alg_MATE_REF_NAME ].base.str, cols[ alg_REF_NAME ].base.str, cols[ alg_MATE_REF_NAME ].len ) == 0 ) {
                    rc = BufferedWriter( NULL, "=\t", 2, NULL );
                } else {
                    rc = BufferedWriter( NULL, cols[ alg_MATE_REF_NAME ].base.str, cols[ alg_MATE_REF_NAME ].len, NULL );
                    if ( rc == 0 ) {
                        rc = BufferedWriter( NULL, "\t", 1, NULL );
                    }
                }
                if ( rc == 0 ) {
                    rc = KOutMsg( "%u\t", cols[ alg_MATE_REF_POS ].base.coord0[ 0 ] + 1 );
                }
            } else {
                rc = BufferedWriter( NULL, "*\t0\t", 4, NULL );
            }
        }

        /* TLEN: TEMPLATE_LEN */
        if ( rc == 0 ) {
            rc = KOutMsg( "%i\t", cols[ alg_TEMPLATE_LEN ].base.v ? cols[ alg_TEMPLATE_LEN ].base.i32[ 0 ] : 0 );
        }

        /* SEQ: READ */
        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, read, readlen, NULL );
        }

        if ( rc == 0 ) {
            rc = BufferedWriter( NULL, "\t", 1, NULL );
        }

        /* QUAL: SAM_QUALITY */
        if ( rc == 0 ) {
            rc = DumpQuality( qual, readlen, false, param->quantizeQual );
        }

        /* optional fields: */
        if ( rc == 0 && ds->type == edstt_EvidenceInterval ) {
            rc = KOutMsg( "\tRG:Z:ALLELE_%u", readId + 1 );
        }

        if ( rc == 0 ) {
            if ( readGroup ) {
                rc = BufferedWriter( NULL, "\tRG:Z:", 6, NULL );
                if ( rc == 0 ) {
                    rc = BufferedWriter( NULL, readGroup, string_size( readGroup ), NULL );
                }
            } else if ( cols[ alg_SPOT_GROUP ].len > 0 ) {
                /* read group */
                rc = BufferedWriter( NULL, "\tRG:Z:", 6, NULL );
                if ( rc == 0 ) {
                    rc = BufferedWriter( NULL, cols[ alg_SPOT_GROUP ].base.str, cols[ alg_SPOT_GROUP ].len, NULL );
                }
            } else if ( cols[ alg_SEQ_SPOT_GROUP ].len > 0 ) {
                /* backward compatibility */
                rc = BufferedWriter( NULL, "\tRG:Z:", 6, NULL );
                if ( rc == 0 ) {
                    rc = BufferedWriter( NULL, cols[ alg_SEQ_SPOT_GROUP ].base.str, cols[ alg_SEQ_SPOT_GROUP ].len, NULL );
                }
            }
        }

        if ( rc == 0 && param->cg_style > 0 && cols[ alg_CG_TAGS_STR ].len > 0 ) {
            rc = BufferedWriter( NULL, cols[ alg_CG_TAGS_STR ].base.str, cols[ alg_CG_TAGS_STR ].len, NULL );
        }

        if ( rc == 0 ) {
            if ( param->cg_style > 0 && cols[ alg_ALIGN_GROUP ].len > 0 ) {
                char const *ZI = cols[ alg_ALIGN_GROUP ].base.str;
                unsigned i;

                for ( i = 0; rc == 0 && i < cols[ alg_ALIGN_GROUP ].len - 1; ++i ) {
                    if ( ZI[ i ] == '_' ) {
                        rc = KOutMsg( "\tZI:i:%.*s\tZA:i:%.1s", i, ZI, ZI + i + 1 );
                        break;
                    }
                }
            } else if ( ds->type == edstt_EvidenceAlignment && type == 1 ) {
                rc = KOutMsg( "\tZI:i:%li\tZA:i:%u", cols[ alg_REF_ID ].base.i64[ readId ], cols[ alg_REF_PLOIDY ].base.u32[ readId ] );
            }
        }

        /* align id */
        if ( rc == 0 && param->xi ) {
            rc = KOutMsg( "\tXI:i:%li", alignId );
        }

        /* hit count */
        if ( rc == 0 && cols[alg_ALIGNMENT_COUNT].len ) {
            rc = KOutMsg( "\tNH:i:%i", (int)cols[ alg_ALIGNMENT_COUNT ].base.u8[ readId ] );
        }

        /* edit distance */
        if ( rc == 0 && cols[ alg_EDIT_DISTANCE ].len ) {
            rc = KOutMsg( "\tNM:i:%i", cols[ alg_EDIT_DISTANCE ].base.i32[ readId ] );
        }

        if ( rc == 0 ) {
            rc = KOutMsg( "\n" );
        }
    }
    return rc;
}


static rc_t DumpUnalignedReads( SAM_dump_ctx_t *const ctx, SCol const calg_col[], int64_t row_id, uint64_t* rcount ) {
    rc_t rc = 0;
    uint32_t i, nreads = 0;

    if ( calg_col != NULL ) {
        if ( calg_col[ alg_SAM_FLAGS ].base.u32[ 0 ] & 0x02 ) {
            /* skip all aligned rows by flag */
            return rc;
        }
        /* get primary alignments only */
        rc = Cursor_Read( &ctx->seq, calg_col[ alg_SEQ_SPOT_ID ].base.i64[ 0 ], seq_PRIMARY_ALIGNMENT_ID, ~(unsigned)0 );
        if ( rc == 0 ) {
            for ( i = 0; i < ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].len; i++ ) {
                if ( ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[ i ] != 0 ) {
                    if ( ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[ i ] < row_id ) {
                        /* unaligned were printed with 1st aligment */
                        return rc;
                    }
                } else {
                    nreads++;
                }
            }
            if ( nreads == ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].len ) {
                /* skip all aligned rows by actual data, if flag above is not set properly */
                return rc;
            }
            row_id = calg_col[ alg_SEQ_SPOT_ID ].base.i64[ 0 ];
        }
    }
    if ( rc == 0 ) {
        rc = Cursor_Read( &ctx->seq, row_id, 0, ~(unsigned)0 );
        if ( rc == 0 ) {
            unsigned non_empty_reads = 0;

            nreads = ctx->seq.cols[ seq_READ_LEN ].idx != 0 ? ctx->seq.cols[ seq_READ_LEN ].len : 1;

            for( i = 0; i < nreads; i++ ) {
                unsigned const readLen = ctx->seq.cols[ seq_READ_LEN ].idx
                                       ? ctx->seq.cols[ seq_READ_LEN ].base.coord_len[ i ]
                                       : 0;

                if ( readLen ) {
                    ++non_empty_reads;
                }
            }
            for ( i = 0; i < nreads; i++ ) {
                INSDC_coord_zero readStart;
                INSDC_coord_len readLen;

                if ( ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].idx != 0 &&
                     ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[ i ] != 0 ) {
                    continue;
                }

                if ( ctx->seq.cols[ seq_READ_TYPE ].idx != 0 &&
                     !( ctx->seq.cols[ seq_READ_TYPE ].base.read_type[ i ] & SRA_READ_TYPE_BIOLOGICAL ) ) {
                    continue;
                }

                readLen = ctx->seq.cols[ seq_READ_LEN ].idx ?
                            ctx->seq.cols[ seq_READ_LEN ].base.coord_len[ i ] :
                            ctx->seq.cols[ seq_READ ].len;
                if ( readLen == 0 ) {
                    continue;
                }

                readStart = ctx->seq.cols[ seq_READ_START ].idx ?
                                ctx->seq.cols[ seq_READ_START ].base.coord0[ i ] :
                                0;
                if ( param->fasta || param->fastq ) {
                    rc = DumpUnalignedFastX( ctx->seq.cols, nreads > 1 ? i + 1 : 0, readStart, readLen, row_id );
                } else {
                    uint32_t cflags = 0x4;
                    if ( param->reverse_unaligned ) {
                        if ( ctx->seq.cols[ seq_READ_TYPE ].base.read_type[ i ] & SRA_READ_TYPE_REVERSE ) {
                            cflags |= 0x10;
                        }
                        if ( ctx->seq.cols[ seq_READ_TYPE ].base.read_type[ i == nreads - 1 ? 0 : ( i + 1 ) ] & SRA_READ_TYPE_REVERSE ) {
                            cflags |= 0x20;
                        }
                    }
                    if ( ctx->seq.cols[ seq_READ_FILTER ].idx != 0 ) {
                        if ( ctx->seq.cols[ seq_READ_FILTER ].base.read_filter[ i ] == SRA_READ_FILTER_REJECT ) {
                            cflags |= 0x200;
                        } else if ( ctx->seq.cols[ seq_READ_FILTER ].base.read_filter[ i ] == SRA_READ_FILTER_CRITERIA ) {
                            cflags |= 0x400;
                        }
                    }
                    if ( calg_col == NULL ) {
                        rc = DumpUnalignedSAM( ctx->seq.cols, cflags |
                                          ( non_empty_reads > 1 ? ( 0x1 | 0x8 | ( i == 0 ? 0x40 : 0x00 ) | ( i == nreads - 1 ? 0x80 : 0x00 ) ) : 0x00 ),
                                          readStart, readLen, NULL, 0, 0, ctx->readGroup, row_id );
                    } else {
                        int c = param->use_seqid ? alg_REF_SEQ_ID : alg_REF_NAME;
                        uint16_t flags = cflags | 0x1 |
                                         ( ( calg_col[ alg_SAM_FLAGS ].base.u32[ 0 ] & 0x10 ) << 1 ) |
                                         ( ( calg_col[ alg_SAM_FLAGS ].base.u32[ 0 ] & 0x40 ) ? 0x80 : 0x40 );
                        rc = DumpUnalignedSAM( ctx->seq.cols, flags, readStart, readLen,
                                          calg_col[ c ].base.str, calg_col[ c ].len,
                                          calg_col[ alg_REF_POS ].base.coord0[ 0 ] + 1, ctx->readGroup, row_id );
                    }
                }
                *rcount = *rcount + 1;
            }
        }
    }
    return rc;
}

static bool AlignRegionFilter( SCol const *cols ) {
    if ( param->region_qty == 0 ) {
        return true;
    }
    if ( cols[ alg_REF_NAME ].len != 0 || cols[ alg_REF_SEQ_ID ].len != 0 ) {
        unsigned i;

        assert( cols[ alg_REF_POS ].len == cols[ alg_REF_LEN ].len );

        for ( i = 0; i < param->region_qty; i++ ) {
            unsigned j;

            for ( j = 0; j < cols[ alg_REF_POS ].len; j++ ) {
                unsigned const refStart_zero = cols[ alg_REF_POS ].base.coord0[ j ];
                unsigned const refEnd_exclusive = refStart_zero + cols[ alg_REF_LEN ].base.coord_len[ j ];
                unsigned k;

                for ( k = 0; k < param->region[ i ].rq; k++ ) {
                    unsigned const from_zero = param->region[ i ].r[ k ].from;
                    unsigned const to_inclusive = param->region[ i ].r[ k ].to;

                    if ( from_zero < refEnd_exclusive && refStart_zero <= to_inclusive ) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool AlignDistanceFilter( SCol const *cols ) {
    if ( param->mp_dist_qty != 0 || param->mp_dist_unknown ) {
        if ( cols[ alg_TEMPLATE_LEN ].len == 0 && param->mp_dist_unknown ) {
            return true;
        } else {
            uint32_t i, j;
            for( i = 0; i < param->mp_dist_qty; i++ ) {
                for( j = 0; j < cols[ alg_TEMPLATE_LEN ].len; j++ ) {
                    if ( ( cols[ alg_TEMPLATE_LEN ].base.i32[ j ] == 0 && param->mp_dist_unknown ) ||
                         ( param->mp_dist[ i ].from <= cols[ alg_TEMPLATE_LEN ].base.i32[ j ] &&
                         cols[ alg_TEMPLATE_LEN ].base.i32[ j ] <= param->mp_dist[ i ].to ) ) {
                        return true;
                    }
                }
            }
        }
        return false;
    }
    return true;
}

typedef struct cgOp_s {
    uint16_t length;
    uint8_t type; /* 0: match, 1: insert, 2: delete */
    char code;
} cgOp;

static void print_CG_cigar( int line, cgOp const op[], unsigned const ops, unsigned const gap[ 3 ] ) {
#if _DEBUGGING
    unsigned i;

    SAM_DUMP_DBG( 3, ( "%5i: ", line ) );
    for ( i = 0; i < ops; ++i ) {
        if ( gap && ( i == gap[ 0 ] || i == gap[ 1 ] || i == gap[ 2 ] ) ) {
            SAM_DUMP_DBG( 3, ( "%u%c", op[ i ].length, tolower( op[ i ].code ) ) );
        } else {
            SAM_DUMP_DBG( 3, ( "%u%c", op[ i ].length, toupper( op[ i ].code ) ) );
        }
    }
    SAM_DUMP_DBG( 3, ( "\n" ) );
#endif
}

/* gap contains the indices of the wobbles in op
 * gap[0] is between read 1 and 2; it is the 'B'
 * gap[1] is between read 2 and 3; it is an 'N'
 * gap[2] is between read 3 and 4; it is an 'N'
 */

static rc_t CIGAR_to_CG_Ops( cgOp op[], unsigned const maxOps,
                             unsigned *const opCnt,
                             unsigned gap[ 3 ],
                             char const cigar[], unsigned const ciglen,
                             unsigned *const S_adjust,
                             unsigned *const CG_adjust,
                             unsigned const read,
                             bool const reversed,
                             bool * has_ref_offset_type ) {
    unsigned i;
    unsigned ops = 0;
    unsigned gapno;

    *opCnt = 0;
    *S_adjust = 0;
    for ( i = 0; i < ciglen; ++ops ) {
        char opChar=0;
        int opLen=0;
        int n;

        for( n = 0; n + i < ciglen && isdigit( cigar[ n + i ] ); n++ ) {
            opLen = opLen * 10 + cigar[n+i] - '0';
        }

        if ( n + i < ciglen ) {
            opChar = cigar[n+i];
            n++;
        }
        if ( ops + 1 >= maxOps ) {
            return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
        }

        i += n;

        op[ ops ].length = opLen;
        op[ ops ].code = opChar;
        switch ( opChar ) {
            case 'M':
            case '=':
            case 'X':
                op[ ops ].type = 0;
                break;
            case 'S':
                *S_adjust += opLen;
            case 'I':
                op[ ops ].type = 1;
                op[ ops ].code = 'I';
                break;
            case 'N':
                *has_ref_offset_type=true;
            case 'D':
                op[ ops ].type = 2;
                break;
            default:
                return RC( rcExe, rcData, rcReading, rcConstraint, rcViolated );
        }
    }

    *opCnt = ops;
    gap[ 0 ] = gap[ 1 ] = gap[ 2 ] = ops;
    print_CG_cigar( __LINE__, op, ops, NULL );
    if ( ops < 3 ) {
        return RC( rcExe, rcData, rcReading, rcFormat, rcNotFound ); /* CG pattern not found */
    }

    {
        unsigned fwd = 0; /* 5 10 10 10 */
        unsigned rev = 0; /* 10 10 10 5 */
        unsigned acc; /* accumulated length */

        if ( ( read == 1 && !reversed ) || ( read == 2 && reversed ) ) {
            for ( i = 0, acc = 0; i < ops  && acc <=5; ++i ) {
                if ( op[ i ].type != 2 ) {
                    acc += op[ i ].length;
                    if ( acc == 5 && op[ i + 1 ].type == 1 ) {
                        fwd = i + 1;
                        break;
                    } else if ( acc > 5 ) {
                        unsigned const right = acc - 5;
                        unsigned const left = op[ i ].length - right;

                        memmove( &op[ i + 2 ], &op[ i ], ( ops - i ) * sizeof( op[ 0 ] ) );
                        ops += 2;
                        op[ i ].length = left;
                        op[ i + 2 ].length = right;

                        op[ i + 1 ].type = 1;
                        op[ i + 1 ].code = 'B';
                        op[ i + 1 ].length = 0;

                        fwd = i + 1;
                        break;
                    }
                }
            }
        } else if ( ( read == 2 && !reversed ) || ( read == 1 && reversed ) ) {
            for ( i = ops, acc = 0; i > 0 && acc <= 5; ) {
                --i;
                if ( op[ i ].type != 2 ) {
                    acc += op[ i ].length;
                    if ( acc == 5 && op[ i ].type == 1 ) {
                        rev = i;
                        break;
                    }
                    else if ( acc > 5 ) {
                        unsigned const left = acc - 5;
                        unsigned const right = op[ i ].length - left;

                        memmove( &op[ i + 2 ], &op[ i ], ( ops - i ) * sizeof( op[ 0 ] ) );
                        ops += 2;
                        op[ i ].length = left;
                        op[ i + 2 ].length = right;

                        op[ i + 1 ].type = 1;
                        op[ i + 1 ].code = 'B';
                        op[ i + 1 ].length = 0;

                        rev = i + 1;
                        break;
                    }
                }
            }
        } else {
            /* fprintf(stderr, "guessing layout\n"); */
            for ( i = 0, acc = 0; i < ops  && acc <= 5; ++i ) {
                if ( op[ i ].type != 2 ) {
                    acc += op[ i ].length;
                    if ( acc == 5 && op[ i + 1 ].type == 1 ) {
                        fwd = i + 1;
                    }
                }
            }
            for ( i = ops, acc = 0; i > 0 && acc <= 5; ) {
                --i;
                if ( op[ i ].type != 2 ) {
                    acc += op[ i ].length;
                    if ( acc == 5 && op[i].type == 1 ) {
                        rev = i;
                    }
                }
            }
            if ( ( fwd == 0 && rev == 0 ) || ( fwd != 0 && rev != 0 ) ) {
                if ( !( *has_ref_offset_type ) ) for ( i = 0; i < ops; ++i ) {
                    if ( op[ i ].type == 2 ) {
                        op[ i ].code = 'N';
                        *CG_adjust += op[ i ].length;
                    }
                }
                return RC( rcExe, rcData, rcReading, rcFormat, rcNotFound ); /* CG pattern not found */
            }
        }

        if ( fwd && op[ fwd ].type == 1 ) {
            for ( i = ops, acc = 0; i > fwd + 1; ) {
                --i;
                if ( op[ i ].type != 2 ) {
                    acc += op[ i ].length;
                    if ( acc >= 10 ) {
                        if ( acc > 10 ) {
                            unsigned const r = 10 + op[ i ].length - acc;
                            unsigned const l = op[ i ].length - r;

                            if ( ops + 2 >= maxOps ) {
                                return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                            }
                            memmove( &op[ i + 2 ], &op[ i ], ( ops - i ) * sizeof( op[ 0 ] ) );
                            ops += 2;
                            op[ i + 2 ].length = r;
                            op[ i ].length = l;

                            op[ i + 1 ].length = 0;
                            op[ i + 1 ].type = 2;
                            op[ i + 1 ].code = 'N';
                            i += 2;
                        } else if ( i - 1 > fwd ) {
                            if ( op[ i - 1 ].type == 2 ) {
                                 op[ i - 1 ].code = 'N';
                            } else {
                                if ( ops + 1 >= maxOps ) {
                                    return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                                }
                                memmove( &op[ i + 1 ], &op[ i ], ( ops - i ) * sizeof( op[ 0 ] ) );
                                ops += 1;
                                op[ i ].length = 0;
                                op[ i ].type = 2;
                                op[ i ].code = 'N';
                                i += 1;
                            }
                        }
                        acc = 0;
                    }
                }
            }
            /** change I to B+M **/
            op[ fwd ].code = 'B';
            memmove( &op[ fwd + 1 ], &op[ fwd ], ( ops - fwd ) * sizeof( op[ 0 ] ) );
            ops += 1;
            op[ fwd + 1 ].code = 'M';
            *opCnt = ops;
            /** set the gaps now **/
            for ( gapno = 3, i = ops; gapno > 1 && i > 0; ) {
                --i;
                if ( op[ i ].code == 'N' ) {
                    gap[ --gapno ] = i;
                }
            }
            gap[ 0 ] = fwd;
            print_CG_cigar( __LINE__, op, ops, gap );
            return 0;
        }

        if ( rev && op[ rev ].type == 1 ) {
            for ( acc = i = 0; i < rev; ++i ) {
                if ( op[ i ].type != 2 ) {
                    acc += op[ i ].length;
                    if ( acc >= 10 ) {
                        if ( acc > 10 ) {
                            unsigned const l = 10 + op[ i ].length - acc;
                            unsigned const r = op[ i ].length - l;

                            if ( ops + 2 >= maxOps ) {
                                return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                            }
                            memmove( &op[ i + 2 ], &op[ i ], ( ops - i ) * sizeof( op[ 0 ] ) );
                            ops += 2;
                            op[ i + 2 ].length = r;
                            op[ i ].length = l;

                            op[ i + 1 ].length = 0;
                            op[ i + 1 ].type = 2;
                            op[ i + 1 ].code = 'N';
                            rev += 2;
                            i += 2;
                        } else if ( i + 1 < rev ) {
                            if ( op[ i + 1 ].type == 2 ) {
                                 op[ i + 1 ].code = 'N';
                            } else {
                                if ( ops + 1 >= maxOps ) {
                                    return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                                }
                                memmove( &op[ i + 1 ], &op[ i ], ( ops - i ) * sizeof( op[ 0 ] ) );
                                ops += 1;
                                op[ i + 1 ].length = 0;
                                op[ i + 1 ].type = 2;
                                op[ i + 1 ].code = 'N';
                                rev += 1;
                                i += 1;
                            }
                        }
                        acc = 0;
                    }
                }
            }
            for ( gapno = 3, i = 0; gapno > 1 && i < ops; ++i ) {
                if ( op[ i ].code == 'N' )
                    gap[ --gapno ] = i;
            }
            gap[ 0 ] = rev;
            op[ rev ].code = 'B';
            memmove( &op[ rev + 1 ], &op[ rev ], ( ops - rev ) * sizeof( op[ 0 ] ) );
            ops += 1;
            op[ rev + 1 ].code = 'M';
            *opCnt = ops;
            print_CG_cigar( __LINE__, op, ops, gap );
            return 0;
        }
    }
    return RC( rcExe, rcData, rcReading, rcFormat, rcNotFound ); /* CG pattern not found */
}

static rc_t GenerateCGData( SCol cols[], unsigned style )
{
    rc_t rc = 0;

    memset( &cols[ alg_CG_TAGS_STR], 0, sizeof( cols[ alg_CG_TAGS_STR ] ) );

    if ( cols[ alg_READ ].len == 35 && cols[ alg_SAM_QUALITY ].len == 35 ) {
        static char newCIGAR[ 35 * 11 ];
        static int32_t newEditDistance;
        unsigned gap[ 3 ] = { 0, 0, 0 };
        cgOp cigOp[ 35 ];
        unsigned opCnt;
        unsigned i;
        unsigned j;
        size_t sz;
        unsigned S_adjust = 0;
        unsigned CG_adjust = 0;
        unsigned const read = cols[ alg_SEQ_READ_ID ].len && cols[ alg_REVERSED ].len ? cols[ alg_SEQ_READ_ID ].base.coord1[ 0 ] : 0;
        bool const reversed = cols[ alg_REVERSED ].len ? cols[ alg_REVERSED ].base.u8[ 0 ] : false;
        bool has_ref_offset_type = false;

        rc = CIGAR_to_CG_Ops( cigOp, sizeof( cigOp ) / sizeof( cigOp[ 0 ] ), &opCnt, gap,
                              cols[ alg_CIGAR ].base.str, cols[ alg_CIGAR ].len,
                              &S_adjust, &CG_adjust, read, reversed, &has_ref_offset_type );
        if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcFormat ) {
            rc = 0;
            if ( style == 1 )
                goto CLEAN_CIGAR;
            else
                goto PRINT_CIGAR;
        }
        if ( rc != 0 ) {
            return 0;
        }

        if ( !has_ref_offset_type && CG_adjust == 0 ) {
            CG_adjust = ( gap[ 0 ] < opCnt ? cigOp[ gap[ 0 ] ].length : 0 )
                      + ( gap[ 1 ] < opCnt ? cigOp[ gap[ 1 ] ].length : 0 )
                      + ( gap[ 2 ] < opCnt ? cigOp[ gap[ 2 ] ].length : 0 );
        }

        print_CG_cigar( __LINE__, cigOp, opCnt, NULL );
        if ( style == 1 ) {
            static char newSeq[ 35 ];
            static char newQual[ 35 ];
            static char tags[ 35*22 + 1 ];
            unsigned const B_len = cigOp[ gap[ 0 ] ].length;
            unsigned const B_at = gap[ 0 ] < gap[ 2 ] ? 5 : 30;

            if ( 0 < B_len && B_len < 5 ) {
                memmove( newSeq, cols[ alg_READ ].base.v, 35 );
                memmove( newQual, cols[ alg_SAM_QUALITY ].base.v, 35 );

                cols[ alg_CG_TAGS_STR ].base.v = tags;

                cols[ alg_READ ].base.v = newSeq;
                cols[ alg_READ ].len = 35 - B_len;

                cols[ alg_SAM_QUALITY ].base.v = newQual;
                cols[ alg_SAM_QUALITY ].len = 35 - B_len;

                /* nBnM -> nB0M */
                cigOp[ gap[ 0 ] + 1 ].length -= B_len;
                if ( gap[ 0 ] < gap[ 2 ] ) {
                    rc = string_printf( tags, sizeof( tags ), &sz, "\tGC:Z:%uS%uG%uS\tGS:Z:%.*s\tGQ:Z:%.*s",
                        5 - B_len, B_len, 30 - B_len, 2 * B_len, &newSeq[ 5 - B_len ], 2 * B_len, &newQual[ 5 - B_len ] );
                    if ( rc == 0 ) {
                        cols[ alg_CG_TAGS_STR ].len = sz;
                    }
                    memmove( &cigOp[ gap[ 0 ] ],
                             &cigOp[ gap[ 0 ] + 1 ],
                             ( opCnt - ( gap[ 0 ] + 1 ) ) * sizeof( cigOp[ 0 ] ) );
                    --opCnt;
                } else {
                    rc = string_printf( tags, sizeof( tags ), &sz, "\tGC:Z:%uS%uG%uS\tGS:Z:%.*s\tGQ:Z:%.*s",
                        30 - B_len, B_len, 5 - B_len, 2 * B_len, &newSeq[ 30 - B_len ], 2 * B_len, &newQual[ 30 - B_len ] );
                    if ( rc == 0 ) {
                        cols[ alg_CG_TAGS_STR ].len = sz;
                    }
                    memmove( &cigOp[ gap[ 0 ] ],
                             &cigOp[ gap[ 0 ] + 1 ],
                             ( opCnt - ( gap[ 0 ] + 1 ) ) * sizeof( cigOp[ 0 ] ) );
                    --opCnt;
                }
                if ( rc == 0 ) {
                    for ( i = B_at; i < B_at + B_len; ++i ) {
                        int const Lq = newQual[ i - B_len ];
                        int const Rq = newQual[ i ];

                        if ( Lq <= Rq ) {
                            newSeq[ i - B_len ] = newSeq[ i ];
                            newQual[ i - B_len ] = Rq;
                        } else {
                            newSeq[ i ] = newSeq[ i - B_len ];
                            newQual[ i ] = Lq;
                        }
                    }
                    memmove( &newSeq [ B_at ], &newSeq [ B_at + B_len ], 35 - B_at - B_len );
                    memmove( &newQual[ B_at ], &newQual[ B_at + B_len ], 35 - B_at - B_len );
                }
            } else {
                int len = cigOp[ gap[ 0 ] ].length;

                cigOp[ gap[ 0 ] ].code = 'I';
                for ( i = gap[0] + 1; i < opCnt && len > 0; ++i ) {
                    if ( cigOp[ i ].length <= len ) {
                        len -= cigOp[i].length;
                        cigOp[i].length = 0;
                    } else {
                        cigOp[i].length -= len;
                        len = 0;
                    }
                }
                if ( !has_ref_offset_type ) {
                    CG_adjust -= cigOp[ gap[ 0 ] ].length;
                }
            }
        }

        if ( rc == 0 ) {
        PRINT_CIGAR:
        CLEAN_CIGAR:
            print_CG_cigar( __LINE__, cigOp, opCnt, NULL );
            /* remove zero length ops */
            for ( j = i = 0; i < opCnt; ) {
                if ( cigOp[ j ].length == 0 ) {
                    ++j;
                    --opCnt;
                    continue;
                }
                cigOp[ i++ ] = cigOp[ j++ ];
            }
            print_CG_cigar( __LINE__, cigOp, opCnt, NULL );
            if ( cols[ alg_EDIT_DISTANCE ].len ) {
                int const edit_distance = cols[ alg_EDIT_DISTANCE ].base.i32[ 0 ];
                int const adjusted = edit_distance + S_adjust - CG_adjust;

                newEditDistance = adjusted > 0 ? adjusted : 0;
                SAM_DUMP_DBG( 4, ( "NM: before: %u, after: %u(+%u-%u)\n", edit_distance, newEditDistance, S_adjust, CG_adjust ) );
                cols[ alg_EDIT_DISTANCE ].base.v = &newEditDistance;
                cols[ alg_EDIT_DISTANCE ].len = 1;
            }
            /* merge adjacent ops */
            for ( i = opCnt; i > 1; ) {
                --i;
                if ( cigOp[ i - 1 ].code == cigOp[ i ].code ) {
                    cigOp[ i - 1 ].length += cigOp[ i ].length;
                    memmove( &cigOp[ i ], &cigOp[ i + 1 ], ( opCnt - 1 - i ) * sizeof( cigOp[ 0 ] ) );
                    --opCnt;
                }
            }
            print_CG_cigar( __LINE__, cigOp, opCnt, NULL );
            for ( i = j = 0; i < opCnt && rc == 0; ++i ) {
                rc = string_printf( &newCIGAR[ j ], sizeof( newCIGAR ) - j, &sz, "%u%c", cigOp[ i ].length, cigOp[ i ].code);
                j += sz;
            }
            cols[ alg_CIGAR ].base.v = newCIGAR;
            cols[ alg_CIGAR ].len = j;
        }
    }
    return rc;
}

static bool DumpAlignedRow( SAM_dump_ctx_t *const ctx, DataSource *const ds,
                    int64_t row,
                    bool const primary,
                    int const cg_style,
                    rc_t *prc ) {
    rc_t rc = 0;

    /* if SEQ_SPOT_ID is open then skip rows with no SEQ_SPOT_ID */
    if ( ds->cols[ alg_SEQ_SPOT_ID ].idx ) {
        rc = Cursor_Read( ds, row, alg_SEQ_SPOT_ID, 1 );
        if ( rc != 0 ) {
            if ( !( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound ) ) {
                *prc = rc;
            }
            return false;
        }
        if ( ds->cols[ alg_SEQ_SPOT_ID ].len == 0 ||
             ds->cols[ alg_SEQ_SPOT_ID ].base.i64[ 0 ] == 0 ) {
            return false;
        }
    }

    /* skip rows that fail mate distance filter */
    if ( param->mp_dist_qty != 0 || param->mp_dist_unknown ) {
        rc = Cursor_Read( ds, row, alg_TEMPLATE_LEN, 1 );
        if ( rc != 0 ) {
            if ( !( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound ) ) {
                *prc = rc;
            }
            return false;
        }
        if ( !AlignDistanceFilter( ds->cols ) ) {
            return false;
        }
    }

    rc = Cursor_ReadAlign( &ds->curs, row, ds->cols, 0 );
    if ( rc != 0 ) {
        if ( !( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound ) ) {
            *prc = rc;
        }
        return false;
    }

    if ( param->fasta || param->fastq ) {
        unsigned const read_id = ds->cols[ alg_SEQ_READ_ID ].base.v ? ds->cols[ alg_SEQ_READ_ID ].base.coord1[ 0 ] : 0;

        rc = DumpAlignedFastX( ctx->pri.cols, row, read_id, primary, false );
    } else {
        if ( cg_style != 0 ) {
            rc = GenerateCGData( ds->cols, cg_style );
            if ( rc != 0 ) {
                *prc = rc;
                return false;
            }
        }
        rc = DumpAlignedSAM(ctx, ds, row, ctx->readGroup, 0 );
    }
    *prc = rc;
    return true;
}

static rc_t DumpAlignedRowList( SAM_dump_ctx_t *const ctx, DataSource *const ds, SCol const *const ids,
                        int64_t *rcount,
                        bool const primary,
                        int const cg_style,
                        bool const noFilter ) {
    rc_t rc;
    unsigned i;
    unsigned const n = ids->len;

    for ( i = 0; ( rc = Quitting() ) == 0 && i < n; ++i ) {
        int64_t const row = ids->base.i64[ i ];

        if ( ! noFilter ) {
            rc = Cursor_Read( ds, row, alg_REGION_FILTER, ~(unsigned)0 );
        }
        if ( rc == 0 ) {
            if ( noFilter || AlignRegionFilter( ds -> cols ) ) {
                if ( DumpAlignedRow( ctx, ds, row, primary, cg_style, &rc ) ) {
                    ++*rcount;
                }
                if ( rc != 0 ) {
                    break;
                }
            }
        }
    }
    return rc;
}

static rc_t DumpAlignedRowListIndirect( SAM_dump_ctx_t *const ctx,
                                DataSource *const indirectSource,
                                DataSource *const directSource,
                                SCol const *const directIDs,
                                int64_t *rcount,
                                bool const primary,
                                int const cg_style ) {
    unsigned i;

    for ( i = 0; i < directIDs->len; ++i ) {
        int64_t const row = directIDs->base.i64[ i ];
        rc_t rc = Cursor_Read( directSource, row, alg_REGION_FILTER, ~(unsigned)0 );

        if ( rc != 0 ) {
            return rc;
        }

        if ( !AlignRegionFilter( directSource->cols ) ) {
            continue;
        }

        rc = Cursor_Read( directSource, row, alg_EVIDENCE_ALIGNMENT_IDS, 1 );
        if ( rc != 0 ) {
            return rc;
        }

        rc = DumpAlignedRowList( ctx, indirectSource, &directSource->cols[ alg_EVIDENCE_ALIGNMENT_IDS ],
                                 rcount, primary, cg_style, true );
        if ( rc != 0 ) {
            return rc;
        }
    }
    return 0;
}

enum e_tables {
    primary_alignment,
    secondary_alignment,
    evidence_interval,
    evidence_alignment
};

enum e_IDS_opts {
    primary_IDS             = ( 1 << primary_alignment ),
    secondary_IDS           = ( 1 << secondary_alignment ),
    evidence_interval_IDS   = ( 1 << evidence_interval ),
    evidence_alignment_IDS  = ( 1 << evidence_alignment )
};

static rc_t DumpAlignedRowList_cb( SAM_dump_ctx_t *const ctx, TAlignedRegion const *const rgn,
                                   int options, int which, int64_t *rcount, SCol const *const IDS ) {
    /*SAM_DUMP_DBG(2, ("row %s index range is [%lu:%lu] pos %lu\n",
        param->region[r].name, start, start + count - 1, cur_pos));*/
    switch ( which ) {
        case primary_IDS:
            return DumpAlignedRowList( ctx, &ctx->pri, IDS, rcount, true, param->cg_style, false );

        case secondary_IDS:
            return DumpAlignedRowList( ctx, &ctx->sec, IDS, rcount, false, param->cg_style, false );

        case evidence_interval_IDS:
            {
                rc_t rc = 0;

                if ( ( options & evidence_interval_IDS ) != 0 ) {
                    rc = DumpAlignedRowList( ctx, &ctx->evi, IDS, rcount, true, 0, false );
                }

                if ( rc == 0 && ( options & evidence_alignment_IDS ) != 0 ) {
                    rc = DumpAlignedRowListIndirect( ctx, &ctx->eva, &ctx->evi, IDS, rcount, true, param->cg_style );
                }
                return rc;
            }
    }
    return RC( rcExe, rcTable, rcReading, rcParam, rcUnexpected );
}

typedef struct CigOps {
    char op;
    int8_t	 ref_sign; /*** 0;+1;-1; ref_offset = ref_sign * offset ***/
    int8_t	 seq_sign; /*** 0;+1;-1; seq_offset = seq_sign * offset ***/
    uint32_t oplen;
} CigOps;

static void SetCigOp( CigOps *dst, char op, uint32_t oplen ) {
    dst->op    = op;
    dst->oplen = oplen;
    switch( op ) { /*MX= DN B IS PH*/
        case 'M':
        case 'X':
        case '=':
            dst->ref_sign=+1;
            dst->seq_sign=+1;
            break;
        case 'D':
        case 'N':
            dst->ref_sign=+1;
            dst->seq_sign= 0;
            break;
        case 'B':
            dst->ref_sign=-1;
            dst->seq_sign= 0;
            break;
        case 'S':
        case 'I':
            dst->ref_sign= 0;
            dst->seq_sign=+1;
            break;
        case 'P':
        case 'H':
        case 0: /** terminating op **/
            dst->ref_sign= 0;
            dst->seq_sign= 0;
            break;
        default:
            assert( 0 );
            break;
    }
}

static unsigned ExplodeCIGAR( CigOps dst[], unsigned len, char const cigar[], unsigned ciglen ) {
    unsigned i;
    unsigned j;
    int opLen;

    for ( i = j = opLen=0; i < ciglen; i++ ) {
        if ( isdigit( cigar[ i ] ) ) {
            opLen = opLen * 10 + cigar[i] - '0';
        } else {
            assert( isalpha( cigar[ i ] ) );
            SetCigOp( dst + j, cigar[ i ], opLen );
            opLen=0;
            j++;
        }
    }
    SetCigOp( dst + j, 0, 0 );
    j++;
    return j;
}

#define CG_CIGAR_STRING_MAX (35 * 11 + 1)

#if 0
static unsigned FormatCIGAR( char dst[], unsigned cp, unsigned oplen, int opcode ) {
    size_t sz;
    string_printf( dst + cp, CG_CIGAR_STRING_MAX - cp, &sz, "%u%c", oplen, opcode );
    return sz;
}

static unsigned DeletedCIGAR( char dst[], unsigned const cp, int opcode, unsigned D,
                              uint32_t const Op[], int ri, unsigned len ) {
    unsigned i;
    unsigned curPos = 0;
    int curOp = opcode;
    unsigned oplen = 0;

    for ( i = 0; i < D && ri < len; ++i, ++ri ) {
        uint32_t const op = Op[ ri ];
        int const d = op >> 2;
        int const m = op & 1;
        int const nxtOp = m ? opcode : 'P';

        if ( d != 0 ) {
            if ( oplen != 0 ) {
                curPos += FormatCIGAR( dst, curPos + cp, oplen, curOp );
            }
            curPos += FormatCIGAR( dst, cp, d, 'D' );
            oplen = 0;
        }
        if ( oplen == 0 || nxtOp == curOp ) {
            ++oplen;
        } else {
            curPos += FormatCIGAR( dst, curPos + cp, oplen, curOp );
            oplen = 1;
        }
        curOp = nxtOp;
    }

    if ( oplen != 0 && curOp != opcode ) {
        curPos += FormatCIGAR( dst, curPos + cp, oplen, curOp );
        oplen = 0;
    }
    if ( i < D ) {
        oplen += D - i;
    }
    if ( oplen ) {
        curPos += FormatCIGAR( dst, curPos + cp, oplen, opcode );
    }
    return curPos;
}
#endif

static char merge_M_type_ops( char a, char b ) {
    /*MX=*/
    char c = 0;
    switch( b ) {
        case 'X':
            switch( a ) {
                case '=' :
                    c = 'X';
                    break;
                case 'X':
                    c = 'M';
                    break; /**we don't know - 2X may create '=' **/
                case 'M':
                    c = 'M';
                    break;
            }
            break;
        case 'M':
            c = 'M';
            break;
        case '=':
            c = a;
            break;
    }
    assert( c != 0) ;
    return c;
}

static unsigned formatCIGAR(char dst[], char const *const endp, unsigned oplen, char opcode)
{
    unsigned n;
    char buf[16];
    char *cp = buf + 16;
    *--cp = '\0';
    *--cp = opcode;
    do {
        *--cp = '0' + oplen % 10;
        oplen /= 10;
    } while (oplen);
    for (n = 0; *cp; ++n) {
        assert(dst + n < endp);
        dst[n] = *cp++;
    }
    return n;
}

static unsigned CombineCIGAR( char dst[], char const *const endp, CigOps const seqOp[], unsigned seq_len,
                              int refPos, CigOps const refOp[], unsigned ref_len ) {
    bool     done=false;
    unsigned ciglen=0,last_ciglen=0;
    char     last_cig_op = 0;
    uint32_t last_cig_oplen=0;
    int	     si=0,ri=0;
    CigOps   seq_cop={0,0,0,0},ref_cop={0,0,0,0};
    int	     seq_pos=0; /** seq_pos is tracked roughly - with every extraction from seqOp **/
    int      ref_pos=0; /** ref_pos is tracked precisely - with every delta and consumption in cigar **/
    int	     delta=refPos; /*** delta in relative positions of seq and ref **/
			   /*** when delta < 0 - rewind or extend the reference ***/
			   /*** wher delta > 0 - skip reference  ***/
#define MACRO_BUILD_CIGAR(OP,OPLEN) \
	if( last_cig_oplen > 0 && last_cig_op == OP){							\
                last_cig_oplen += OPLEN;								\
                ciglen = last_ciglen + formatCIGAR(dst+last_ciglen, endp, last_cig_oplen, last_cig_op);	\
        } else {											\
                last_ciglen = ciglen;									\
                last_cig_oplen = OPLEN;									\
                last_cig_op    = OP;									\
                ciglen = ciglen      + formatCIGAR(dst+last_ciglen, endp, last_cig_oplen, last_cig_op);	\
        }
    while ( !done ) {

        while ( delta < 0 ) {
            ref_pos += delta; /** we will make it to back up this way **/
            if ( ri > 0 ) { /** backing up on ref if possible ***/
                int avail_oplen = refOp[ri-1].oplen - ref_cop.oplen;
                if ( avail_oplen > 0 ) {
                    if ( ( -delta ) <= avail_oplen * ref_cop.ref_sign ) { /*** rewind within last operation **/
                        ref_cop.oplen -= delta;
                        delta -= delta * ref_cop.ref_sign;
                    } else { /*** rewind the whole ***/
                        ref_cop.oplen += avail_oplen;
                        delta += avail_oplen * ref_cop.ref_sign;
                    }
                } else {
                    ri--;
                    /** pick the previous as used up **/
                    ref_cop = refOp[ ri - 1 ];
                    ref_cop.oplen =0;
                }
            } else { /** extending the reference **/
                SetCigOp( &ref_cop,'=',ref_cop.oplen - delta );
                delta = 0;
            }
            ref_pos -= delta;
        }

        if ( ref_cop.oplen == 0 ) { /*** advance the reference ***/
            ref_cop = refOp[ ri++ ];
            if ( ref_cop.oplen == 0 ) { /** extending beyond the reference **/
                SetCigOp( &ref_cop, '=', 1000 );
            }
            assert( ref_cop.oplen > 0 );
        }

        if ( delta > 0 ) { /***  skip refOps ***/
            ref_pos += delta; /** may need to back up **/
            if ( delta >= ref_cop.oplen ) { /** full **/
                delta -= ref_cop.oplen * ref_cop.ref_sign;
                ref_cop.oplen = 0;
            } else { /** partial **/
                ref_cop.oplen -= delta;
                delta -= delta * ref_cop.ref_sign;
            }
            ref_pos -= delta; /** if something left - restore ***/
            continue;
        }

        /*** seq and ref should be synchronized here **/
        assert( delta == 0 );
        if ( seq_cop.oplen == 0 ) { /*** advance sequence ***/
            if ( seq_pos < seq_len ) {
                seq_cop = seqOp[si++];
                assert( seq_cop.oplen > 0 );
                seq_pos += seq_cop.oplen * seq_cop.seq_sign;
            } else {
                done = true;
            }
        }

        if ( !done ) {
            int seq_seq_step = seq_cop.oplen * seq_cop.seq_sign; /** sequence movement**/
            int seq_ref_step = seq_cop.oplen * seq_cop.ref_sign; /** influence of sequence movement on intermediate reference **/
            int ref_seq_step = ref_cop.oplen * ref_cop.seq_sign; /** movement of the intermediate reference ***/
            int ref_ref_step = ref_cop.oplen * ref_cop.ref_sign; /** influence of the intermediate reference movement on final reference ***/
            assert( ref_ref_step >= 0 ); /** no B in the reference **/
            if ( seq_ref_step <= 0 ) { /** BSIPH in the sequence against anything ***/
                MACRO_BUILD_CIGAR( seq_cop.op, seq_cop.oplen );
                seq_cop.oplen = 0;
                delta = seq_ref_step; /** if negative - will force rewind next cycle **/
            } else if ( ref_ref_step <= 0 ) { /** MX=DN against SIPH in the reference***/
                if ( ref_seq_step == 0 ) { /** MX=DN against PH **/
                    MACRO_BUILD_CIGAR( ref_cop.op,ref_cop.oplen );
                    ref_cop.oplen = 0;
                } else {
                    int min_len = ( seq_cop.oplen < ref_cop.oplen ) ? seq_cop.oplen : ref_cop.oplen;
                    if ( seq_seq_step == 0 ) { /** DN agains SI **/
                        MACRO_BUILD_CIGAR( 'P', min_len );
                    } else { /** MX= agains SI ***/
                        MACRO_BUILD_CIGAR( ref_cop.op, min_len );
                    }
                    seq_cop.oplen -= min_len;
                    ref_cop.oplen -= min_len;
                }
            } else { /*MX=DN  against MX=DN*/
                int min_len = ( seq_cop.oplen < ref_cop.oplen ) ? seq_cop.oplen : ref_cop.oplen;
                if ( seq_seq_step == 0 ) { /* DN against MX=DN */
                    if ( ref_seq_step == 0 ) { /** padding DN against DN **/
                        MACRO_BUILD_CIGAR( 'P', min_len );
                        ref_cop.oplen -= min_len;
                        seq_cop.oplen -= min_len;
                    } else { /* DN against MX= **/
                        MACRO_BUILD_CIGAR( seq_cop.op, min_len );
                        seq_cop.oplen -= min_len;
                    }
                } else if ( ref_cop.seq_sign == 0 ) { /* MX= against DN - always wins */
                    MACRO_BUILD_CIGAR( ref_cop.op, min_len );
                    ref_cop.oplen -= min_len;
                } else { /** MX= against MX= ***/
                    MACRO_BUILD_CIGAR( merge_M_type_ops( seq_cop.op, ref_cop.op ), min_len );
                    ref_cop.oplen -= min_len;
                    seq_cop.oplen -= min_len;
                }
                ref_pos += min_len;
            }
        }

    }
    return ciglen;
}

static rc_t DumpCGSAM( SAM_dump_ctx_t *const ctx, TAlignedRegion const *const rgn, int options,
                       int which, int64_t *rows, SCol const *const ids ) {
    if ( which == evidence_interval_IDS ) {
        rc_t rc = 0;
        unsigned i;

        for ( i = 0; i < ids->len; ++i ) {
            int64_t const rowInterval = ids->base.i64[ i ];

            rc = Cursor_Read( &ctx->evi, rowInterval, alg_REGION_FILTER, ~(0u) );
            if ( rc == 0 ) {
                if ( AlignRegionFilter( ctx->evi.cols ) ) {
                    rc = Cursor_ReadAlign( &ctx->evi.curs, rowInterval, ctx->evi.cols, 0 );
                    if ( rc == 0 ) {
                        unsigned const evidence = ctx->evi.cols[ alg_EVIDENCE_ALIGNMENT_IDS ].len;
                        if ( evidence != 0 ) {
                            INSDC_coord_len const *const refLen = ctx->evi.cols[ alg_READ_LEN ].base.coord_len;
                            INSDC_coord_len const *const cigLen = ctx->evi.cols[ alg_CIGAR_LEN ].base.coord_len;
                            unsigned const ploids = ctx->evi.cols[ alg_CIGAR_LEN ].len;
                            unsigned const totalIntervalReadLen = ctx->evi.cols[ alg_READ ].len;
                            CigOps refCigOps_stack[ 1024 ];
                            CigOps *refCigOps_heap = NULL;
                            CigOps *refCigOps;

                            if ( totalIntervalReadLen > 1000 ) {
                                refCigOps = refCigOps_heap = malloc( sizeof( refCigOps[ 0 ] ) * ( totalIntervalReadLen + ploids) );
                            } else {
                                refCigOps = refCigOps_stack;
                            }

                            if ( refCigOps != NULL ) {
                                unsigned j;
                                unsigned start;
                                unsigned cigop_starts[ 256 ];

                                assert( ploids < 256 );

                                for ( start = j = 0, cigop_starts[0]=0; j < ploids; ++j ) {
                                    unsigned len = cigLen[ j ];
                                    cigop_starts[ j + 1 ] =
                                        cigop_starts[ j ] +
                                        ExplodeCIGAR( refCigOps + cigop_starts[ j ], refLen[ j ],
                                                      ctx->evi.cols[ alg_CIGAR ].base.str + start, len );
                                    start += len;
                                }

                                for ( j = 0; j < evidence; ++j ) {
                                    int64_t const rowAlign = ctx -> evi.cols[ alg_EVIDENCE_ALIGNMENT_IDS ].base.i64[ j ];

                                    rc = Cursor_ReadAlign( &ctx->eva.curs, rowAlign, ctx->eva.cols, 0 );
                                    if ( rc == 0 ) {
                                        if ( param->cg_style != 0 ) {
                                            rc = GenerateCGData( ctx->eva.cols, param->cg_style );
                                        }
                                        if ( rc == 0 ) {
                                            int const ploidy = ctx -> eva . cols[ alg_REF_PLOIDY ] . base.u32[ 0 ];
                                            int const readLen = ctx -> eva . cols[ alg_READ ] . len;
                                            INSDC_coord_zero refPos = ctx -> eva . cols[ alg_REF_POS ] . base . coord0[ 0 ];
                                            CigOps op[ 36 ];
                                            char cigbuf[ CG_CIGAR_STRING_MAX ];

                                            memset( cigbuf, 0, CG_CIGAR_STRING_MAX );
                                            ExplodeCIGAR( op, readLen, ctx -> eva . cols[ alg_CIGAR ] . base . str,
                                                         ctx -> eva . cols[ alg_CIGAR ] . len );
                                            ctx -> eva . cols[ alg_CIGAR ] . len =
                                                CombineCIGAR( cigbuf, cigbuf + sizeof(cigbuf), op, readLen, refPos,
                                                              refCigOps + cigop_starts[ ploidy - 1 ], refLen[ ploidy - 1 ] );
                                            ctx -> eva . cols[ alg_CIGAR ] . base.str = cigbuf;
                                            ctx -> eva . cols[ alg_REF_POS ] . base.v = &refPos;
                                            refPos += ctx -> evi . cols[ alg_REF_POS ] . base . coord0[ 0 ] ;

                                            if ( refPos < 0 ) {
                                                ReferenceObj const *r = NULL;
                                                rc = ReferenceList_Find( gRefList, &r,
                                                    ctx -> evi . cols[ alg_REF_NAME ] . base . str,
                                                    ctx -> evi . cols[ alg_REF_NAME ] . len );
                                                if ( rc == 0 ) {
                                                    bool circular = false;
                                                    rc = ReferenceObj_Circular( r, &circular );
                                                    if ( rc == 0 && circular ) {
                                                        INSDC_coord_len len;
                                                        rc = ReferenceObj_SeqLength( r, &len );
                                                        if ( rc == 0 ) {
                                                            refPos += len;
                                                        }
                                                    }
                                                    ReferenceObj_Release( r );
                                                }
                                            }
                                            rc = DumpAlignedSAM( ctx, &ctx->eva, rowAlign, ctx->readGroup, 1 );
                                            ++*rows;
                                        } else {
                                            break;
                                        }
                                    } else {
                                        break;
                                    }
                                }
                                if ( refCigOps_heap ) {
                                    free( refCigOps_heap );
                                }
                            } else {
                                rc = RC( rcExe, rcTable, rcReading, rcMemory, rcExhausted );
                            }
                        }
                    }
                }
            } else {
                break;
            }
            rc = rc ? rc : Quitting();
            if ( rc != 0 ) {
                break;
            }
        }
        return rc;
    }
    return RC( rcExe, rcTable, rcReading, rcParam, rcUnexpected );
}


static rc_t ForEachAlignedRegion( SAM_dump_ctx_t *const ctx, enum e_IDS_opts const Options,
    rc_t ( *user_func )( SAM_dump_ctx_t *ctx, TAlignedRegion const *rgn, int options, int which, int64_t *rows, SCol const *IDS ) ) {

    SCol cols[] = {
        { "MAX_SEQ_LEN", 0, { NULL }, 0, false },
        { "OVERLAP_REF_POS", 0, { NULL }, 0, true },
        { "PRIMARY_ALIGNMENT_IDS", 0, { NULL }, 0, false },
        { "SECONDARY_ALIGNMENT_IDS", 0, { NULL }, 0, true },
        { "EVIDENCE_INTERVAL_IDS", 0, { NULL }, 0, true },
        { NULL, 0, { NULL }, 0, false }
    };
    enum eref_col {
        ref_MAX_SEQ_LEN = 0,
        ref_OVERLAP_REF_LEN,
        ref_PRIMARY_ALIGNMENT_IDS,
        ref_SECONDARY_ALIGNMENT_IDS,
        ref_EVIDENCE_INTERVAL_IDS
    };

    KIndex const *iname = NULL;
    int64_t rows = 0;
    rc_t rc = 0;
    int options = Options & 7;

    if ( Options & evidence_alignment_IDS ) {
        options |= evidence_interval_IDS;
    }
    if ( ( options & primary_IDS ) == 0 ) {
        cols[ ref_PRIMARY_ALIGNMENT_IDS ].name = "";
    }
    if ( ( options & secondary_IDS ) == 0 ) {
        cols[ ref_SECONDARY_ALIGNMENT_IDS ].name = "";
    }
    if ( ( options & evidence_interval_IDS ) == 0 ) {
        cols[ ref_EVIDENCE_INTERVAL_IDS ].name = "";
    }
    ctx -> ref . cols = cols;

    rc = VTableOpenIndexRead( ctx->ref.tbl.vtbl, &iname, "i_name" );
    if ( rc == 0 ) {
        rc = Cursor_Open( &ctx->ref.tbl, &ctx->ref.curs, ctx->ref.cols, NULL );
        if ( rc == 0 ) {
            int64_t rowid = 1;
            uint64_t count = 0;
            char refname[ 1024 ];
            size_t sz;

            if ( ctx->ref.cols[ ref_PRIMARY_ALIGNMENT_IDS ].idx == 0 ) {
                options &= ~primary_IDS;
            }
            if ( ctx->ref.cols[ ref_SECONDARY_ALIGNMENT_IDS ].idx == 0 ) {
                options &= ~secondary_IDS;
            }
            if ( ctx->ref.cols[ ref_EVIDENCE_INTERVAL_IDS ].idx == 0 ) {
                options &= ~evidence_interval_IDS;
            }
            /* new: if we have a list of regions, reset the their print-flag */
            if ( param->region_qty > 0 ) {
                unsigned r;
                for ( r = 0; r < param->region_qty; ++r ) {
                    param->region[ r ].printed = 0;
                }
            }

            while ( ( rc = KIndexProjectText(iname, rowid + count, &rowid, &count, refname, sizeof( refname ), &sz ) ) == 0 ) {
                bool include;
                unsigned r;
                unsigned max_to = UINT_MAX;
                unsigned min_from = 0;

                for ( include = false, r = 0; r < param->region_qty; ++r ) {
                    if ( sz == string_size( param->region[ r ].name ) &&
                         memcmp( param->region[ r ].name, refname, sz ) == 0 ) {
                        include = true;
                        max_to = param->region[ r ].max_to;
                        min_from = param->region[ r ].min_from;
                        param->region[ r ].printed++; /* new: mark a region as printed */
                        break;
                    }
                }

                if ( param->region_qty == 0 || include ) {
                    int64_t const endrow_exclusive = rowid + count;
                    unsigned m;
                    unsigned k;

                    for ( k = 0, m = 1; rc == 0 && k < 3; ++k, m <<= 1 ) {
                        if ( m & options ) {
                            unsigned lookback = 0;
                            int64_t row;
                            int32_t row_start_offset;
                            unsigned pos;
                            unsigned const maxseqlen = 5000; /***** TODO: this code is to be rewritten anyway ****/

                            if ( param->region_qty ) {
                                if ( ctx->ref.cols[ ref_OVERLAP_REF_LEN ].idx ) {
                                    rc = Cursor_Read( &ctx->ref, rowid, 0, ~(0u) );
                                    if ( rc != 0 ) {
                                        break;
                                    }
                                    if ( ctx->ref.cols[ ref_OVERLAP_REF_LEN ].len > k ) {
                                        unsigned const overlap = ctx->ref.cols[ ref_OVERLAP_REF_LEN ].base.coord0[ k ];
                                        if ( overlap != 0 ) {
                                            assert( overlap < rowid * maxseqlen );
                                            lookback = ( rowid - ( overlap / maxseqlen ) );
                                        }
                                    }
                                } else {
                                    lookback = 1;
                                }
                            }

                            row_start_offset = min_from / maxseqlen - lookback;
                            if ( row_start_offset < 0 ) {
                                row_start_offset=0;
                            }

                            for ( pos = row_start_offset * maxseqlen, row = rowid + row_start_offset;
                                  rc == 0 && row < endrow_exclusive;
                                  ++row ) {
                                if ( row < 1 ) {
                                    row = 0;
                                    continue;
                                }
                                rc = Cursor_Read( &ctx->ref, row, 0, ~(0u) );
                                if ( rc != 0 ) {
                                    break;
                                }
                                if ( ctx->ref.cols[ ref_PRIMARY_ALIGNMENT_IDS + k ].len > 0 ) {
                                    rc = user_func( ctx, &param->region[r], Options, m, &rows,
                                                    &ctx->ref.cols[ ref_PRIMARY_ALIGNMENT_IDS + k ] );
                                }
                                pos += ctx->ref.cols[ ref_MAX_SEQ_LEN ].base.u32[ 0 ];
                                if ( pos >= max_to ) {
                                    break;
                                }
                                if ( param->test_rows != 0 && rows > param->test_rows ) {
                                    break;
                                }
                            }
                        }
                    }
                }
                if ( param->test_rows != 0 && rows > param->test_rows ) {
                    break;
                }
                if ( rc != 0 ) {
                    break;
                }
            } /* while */

            /* new: walk the list of regions and report what was NOT printed ( found ) ... */
            if ( param->region_qty > 0 && GetRCState( rc ) != rcCanceled ) {
                unsigned r;
                for ( r = 0; r < param->region_qty; ++r ) {
                    if ( param->region[ r ].printed == 0 ) {
                        (void)PLOGMSG( klogWarn, ( klogWarn, "reference >$(a)< not found", "a=%s", param->region[ r ].name ) );
                    }
                }
            }

            if ( GetRCObject( rc ) == rcId && GetRCState( rc ) == rcNotFound ) {
                rc = 0;
            }

        }
    }
    Cursor_Close( &ctx -> ref . curs );
    KIndexRelease( iname );

    return rc;
}

static rc_t DumpAlignedTable( SAM_dump_ctx_t *const ctx, DataSource *const ds,
                              bool const primary, int const cg_style, unsigned *const rcount ) {
    unsigned i;
    int64_t start;
    uint64_t count;
    rc_t rc = VCursorIdRange( ds->curs.vcurs, 0, &start, &count );

    if ( rc != 0 ) {
        return rc;
    }
    for ( i = 0; i != (unsigned)count; ++i ) {
        if ( DumpAlignedRow(ctx, ds, start + i, primary, cg_style, &rc ) ) {
            ++*rcount;
        }
        if ( rc != 0 || ( rc = Quitting() ) != 0 ) {
            return rc;
        }
        if ( param->test_rows && *rcount > param->test_rows ) {
            break;
        }
    }
    return 0;
}

static rc_t DumpUnsorted( SAM_dump_ctx_t *const ctx ) {
    rc_t rc = 0;
    unsigned rcount;

    if ( rc == 0 && param->cg_evidence ) {
        SAM_DUMP_DBG( 2, ( "%s EVIDENCE_INTERVAL\n", ctx->accession ) );
        rcount = 0;
        rc = DumpAlignedTable( ctx, &ctx->evi, false, 0, &rcount );
        (void)PLOGMSG( klogInfo, ( klogInfo, "$(a): $(c) allele sequences", "a=%s,c=%lu", ctx->accession, rcount ) );
    }
    if ( rc == 0 && param->cg_ev_dnb ) {
        SAM_DUMP_DBG( 2, ( "%s EVIDENCE_ALIGNMENT\n", ctx->accession ) );
        rcount = 0;
        rc = DumpAlignedTable( ctx, &ctx->eva, false, param->cg_style, &rcount );
        (void)PLOGMSG( klogInfo, ( klogInfo, "$(a): $(c) support sequences", "a=%s,c=%lu", ctx->accession, rcount ) );
    }
    if ( rc == 0 && ctx->pri.curs.vcurs ) {
        SAM_DUMP_DBG( 2, ( "%s PRIMARY_ALIGNMENT\n", ctx->accession ) );
        rcount = 0;
        rc = DumpAlignedTable( ctx, &ctx->pri, true, param->cg_style, &rcount );
        (void)PLOGMSG( klogInfo, ( klogInfo, "$(a): $(c) primary sequences", "a=%s,c=%lu", ctx->accession, rcount ) );
    }
    if ( rc == 0 && ctx->sec.curs.vcurs ) {
        SAM_DUMP_DBG( 2, ( "%s SECONDARY_ALIGNMENT\n", ctx->accession ) );
        rcount = 0;
        rc = DumpAlignedTable( ctx, &ctx->sec, false, param->cg_style, &rcount );
        (void)PLOGMSG( klogInfo, ( klogInfo, "$(a): $(c) secondary sequences", "a=%s,c=%lu", ctx->accession, rcount ) );
    }
    return rc;
}

static rc_t DumpUnaligned( SAM_dump_ctx_t *const ctx, bool const aligned ) {
    rc_t rc = 0;
    int64_t start = 0;
    uint64_t count = 0;

    rc = VCursorIdRange( ctx->seq.curs.vcurs, 0, &start, &count );
    if ( rc == 0 ) {
        uint64_t rcount = 0;
        if ( param->test_rows != 0 && count > param->test_rows ) {
            count = param->test_rows;
        }
        SAM_DUMP_DBG( 2, ( "%s SEQUENCE table range from %ld qty %lu\n", ctx->accession, start, count ) );
        while ( count > 0 && rc == 0 ) {
            uint32_t i;
            if ( ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].idx == 0 ) {
                rc = DumpUnalignedReads( ctx, NULL, start, &rcount );
            } else {
                /* avoid reading whole sequence cursor data unnecessarily */
                rc = Cursor_Read( &ctx->seq, start, seq_PRIMARY_ALIGNMENT_ID, ~(unsigned)0 );
                if ( rc == 0 ) {
                    /* find if its completely unaligned */
                    int64_t min_prim_id = 0;
                    bool has_unaligned = false;
                    for ( i = 0; rc == 0 && i < ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].len; i++ ) {
                        int64_t x = ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[ i ];
                        has_unaligned |= x == 0;
                        if ( ( min_prim_id == 0 && x != 0 ) || min_prim_id < x ) {
                            min_prim_id = x;
                        }
                    }
                    if ( min_prim_id == 0 ) {
                        rc = DumpUnalignedReads( ctx, NULL, start, &rcount );
                    }
                    else if ( has_unaligned && !param->unaligned_spots ) {
                        if ( rc == 0 ) {
#if USE_MATE_CACHE
                            uint64_t val;
                            rc = Cache_Get( &ctx->pri.curs, min_prim_id, &val );
                            if ( rc == 0 ) {
                                ctx->pri.cols[ alg_REF_POS ].len = 0;
                                Cache_Unpack( val, 0, &ctx->pri.curs, ctx->pri.cols );
                                ctx->pri.cols[ alg_SEQ_SPOT_ID ].base.i64 = &start;
                                ctx->pri.cols[ alg_SEQ_SPOT_ID ].len = 1;
                                memmove( &ctx->pri.cols[ alg_REF_NAME ], &ctx->pri.cols[ alg_MATE_REF_NAME ], sizeof( SCol ) );
                                memmove( &ctx->pri.cols[ alg_REF_SEQ_ID ], &ctx->pri.cols[ alg_MATE_REF_NAME ], sizeof( SCol ) );
                                memmove( &ctx->pri.cols[ alg_REF_POS ], &ctx->pri.cols[ alg_MATE_REF_POS ], sizeof( SCol ) );
                            } else if ( !( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcItem ) ) {
                                break;
                            } else {
#endif /* USE_MATE_CACHE */
                                rc = Cursor_ReadAlign( &ctx->pri.curs, min_prim_id, ctx->pri.cols, alg_MATE_ALIGN_ID );
#if USE_MATE_CACHE
                            }
#endif /* USE_MATE_CACHE */
                            rc = rc ? rc : DumpUnalignedReads( ctx, ctx->pri.cols, min_prim_id, &rcount );
                        }
                    }
                } else if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcRow ) {
                    rc = 0;
                }
            }
            start++;
            count--;
            rc = rc ? rc : Quitting();
        }
        (void)PLOGMSG( klogInfo, ( klogInfo, "$(a): $(c) unaligned sequences", "a=%s,c=%lu", ctx->accession, rcount ) );
    }
    return rc;
}

#if USE_MATE_CACHE
static rc_t FlushUnalignedRead_cb( uint64_t key, bool value,void *user_data ) {
    rc_t rc;
    int64_t spot_id = (int64_t) key;
    int64_t aligned_mate_id = (int64_t) value;
    SAM_dump_ctx_t *const ctx = user_data;
    rc = Cursor_Read( &ctx->seq, spot_id, seq_PRIMARY_ALIGNMENT_ID, ~(unsigned)0 );
    if ( rc == 0 ) {
        uint64_t rcount;
        uint64_t val;
        assert(ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].len == 2);
        if ( ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[ 0 ] == 0 ) {
            aligned_mate_id = ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[1];
            assert( aligned_mate_id != 0 );
        } else if ( ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[1] == 0 ) {
            aligned_mate_id = ctx->seq.cols[ seq_PRIMARY_ALIGNMENT_ID ].base.i64[0];
            assert( aligned_mate_id != 0 );
        } else {
            assert( 0 );
        }
        rc = Cache_Get( &ctx->pri.curs, aligned_mate_id, &val );
        if ( rc == 0 ) {
            ctx->pri.cols[ alg_REF_POS ].len = 0;
            Cache_Unpack( val, 0, &ctx->pri.curs, ctx->pri.cols );
            ctx->pri.cols[ alg_SEQ_SPOT_ID ].base.i64 = &spot_id;
            ctx->pri.cols[ alg_SEQ_SPOT_ID ].len = 1;
            memmove( &ctx->pri.cols[ alg_REF_NAME ], &ctx->pri.cols[ alg_MATE_REF_NAME ], sizeof( SCol ) );
            memmove( &ctx->pri.cols[ alg_REF_SEQ_ID ], &ctx->pri.cols[ alg_MATE_REF_NAME ], sizeof( SCol ) );
            memmove( &ctx->pri.cols[ alg_REF_POS ], &ctx->pri.cols[ alg_MATE_REF_POS ], sizeof( SCol ) );
            rc =DumpUnalignedReads( ctx, ctx->pri.cols, aligned_mate_id, &rcount );
        }
    }
    return rc;
}

static rc_t FlushUnaligned( SAM_dump_ctx_t *const ctx, const SCursCache * cache )
{
    return KVectorVisitBool( cache -> cache_unaligned_mate, false, FlushUnalignedRead_cb, ctx );
}
#endif

static rc_t DumpHeader( SAM_dump_ctx_t const *ctx )
{
    bool reheader = param->reheader;
    rc_t rc = 0;

    if ( !reheader ) {
        if ( ctx -> db ) {
            /* grab header from db meta node */
            KMetadata const *meta;

            rc = VDatabaseOpenMetadataRead( ctx->db, &meta );
            if ( rc == 0 ) {
                KMDataNode const *node;

                rc = KMetadataOpenNodeRead( meta, &node, "BAM_HEADER" );
                KMetadataRelease( meta );

                if ( rc == 0 ) {
                    size_t offset = 0;

                    for ( offset = 0; ; ) {
                        size_t remain;
                        size_t nread;
                        char buffer[ 4096 ];

                        rc = KMDataNodeRead( node, offset, buffer, sizeof(buffer), &nread, &remain );
                        if ( rc != 0 ) {
                            break;
                        }
                        BufferedWriter( NULL, buffer, nread, NULL );
                        if ( remain == 0 ) {
                            if ( buffer[nread - 1] != '\n' ) {
                                 buffer[ 0 ] = '\n';
                                 rc = BufferedWriter( NULL, buffer, 1, NULL );
                            }
                            break;
                        }
                        offset += nread;
                    }
                    KMDataNodeRelease(node);
                } else if ( GetRCState( rc ) == rcNotFound ) {
                    reheader = true;
                    rc = 0;
                }
            }
        } else {
            reheader = true;
            rc = 0;
        }
    }

    if ( rc == 0 && reheader ) {
        rc = KOutMsg( "@HD\tVN:1.3\n" );
        if ( rc == 0 && gRefList ) {
            rc = RefSeqPrint();
        }
        if ( rc == 0 && ctx->readGroup ) {
            rc = ReadGroup_print( ctx->readGroup );
        }
        if ( rc == 0 && ctx->seq.tbl.vtbl ) {
            rc = DumpReadGroups( &ctx->seq.tbl );
        }
    }

    if ( rc == 0 && param->comments ) {
        unsigned i;
        for ( i = 0; rc == 0 && param->comments[ i ]; ++i ) {
            rc = KOutMsg( "@CO\t%s\n", param->comments[ i ] );
        }
    }
    return rc;
}

static rc_t DumpDB( SAM_dump_ctx_t *const ctx ) {
    rc_t rc = 0;

    if ( ctx->ref.tbl.vtbl != NULL ) {
        rc = ReferenceList_MakeTable( &gRefList, ctx->ref.tbl.vtbl, 0, CURSOR_CACHE, NULL, 0 );
    }
    if ( !param->noheader ) {
        rc = DumpHeader( ctx );
    }
    if ( rc == 0 ) {
        if ( param->region_qty ) {
            rc = ForEachAlignedRegion(  ctx
                                      ,   ( param->primaries   ? primary_IDS : 0 )
                                        | ( param->secondaries ? secondary_IDS : 0 )
                                        | ( param->cg_evidence ? evidence_interval_IDS : 0 )
                                        | ( param->cg_ev_dnb   ? evidence_alignment_IDS : 0 )
                                      , DumpAlignedRowList_cb );
#if USE_MATE_CACHE
            if ( rc == 0 && param->unaligned ) {
                    rc = FlushUnaligned( ctx,ctx->pri.curs.cache);
            }
#endif
        }

        if ( param->cg_sam ) {
            rc = ForEachAlignedRegion( ctx, evidence_interval_IDS, DumpCGSAM );
        }

        if ( param->region_qty == 0 ) {
            rc = DumpUnsorted( ctx );
            if ( rc == 0 && param->unaligned ) {
                rc = DumpUnaligned( ctx, ctx->pri.tbl.vtbl != NULL );
            }
        }
    }
    ReferenceList_Release( gRefList );
    return rc;
}

static rc_t DumpTable( SAM_dump_ctx_t *ctx )
{
    rc_t rc = 0;

    if ( !param->noheader ) {
        rc = DumpHeader( ctx );
    }
    if ( rc == 0 ) {
        rc = DumpUnaligned( ctx, false );
    }
    return rc;
}

static rc_t ProcessTable( VDBManager const *mgr, char const fullPath[],
                          char const accession[], char const readGroup[] ) {
    VTable const *tbl;
    rc_t rc = VDBManagerOpenTableRead( mgr, &tbl, 0, "%s", fullPath );

    if ( rc != 0 ) {
        VSchema *schema;

        rc = VDBManagerMakeSRASchema( mgr, &schema );
        if ( rc == 0 ) {
            rc = VDBManagerOpenTableRead( mgr, &tbl, schema, "%s", fullPath );
            VSchemaRelease( schema );
        }
    }

    if ( rc == 0 ) {
        SAM_dump_ctx_t ctx;
        SCol seq_cols[ sizeof( gSeqCol ) / sizeof( gSeqCol[ 0 ] ) ];

        memset( &ctx, 0, sizeof( ctx ) );

        ctx.fullPath = fullPath;
        ctx.accession = accession;
        ctx.readGroup = readGroup;

        DATASOURCE_INIT( ctx.seq, accession );
        ctx.seq.tbl.vtbl = tbl;
        ctx.seq.type = edstt_Sequence;

        ReportResetTable( fullPath, tbl );

        ctx.seq.cols = seq_cols;
        memmove( ctx.seq.cols, gSeqCol, sizeof( gSeqCol ) );
        rc = Cursor_Open( &ctx.seq.tbl, &ctx.seq.curs, ctx.seq.cols, NULL );
        if ( rc == 0 ) {
            rc = DumpTable( &ctx );
            Cursor_Close(&ctx.seq.curs);
        }
        VTableRelease( tbl );
    }
    return rc;
}

static void SetupColumns( DataSource *ds, enum eDSTableType type ) {
    memmove( ds->cols, g_alg_col_tmpl, sizeof( g_alg_col_tmpl ) );

    ds->type = type;

    if ( param->use_seqid ) {
        ds->cols[ alg_MATE_REF_NAME ].name = "MATE_REF_SEQ_ID";
    } else {
        ds->cols[ alg_MATE_REF_NAME ].name = "MATE_REF_NAME";
    }

    if ( param->fasta || param->fastq ) {
        ds->cols[ alg_READ ].name = "RAW_READ";
    } else if ( param->hide_identical ) {
        ds->cols[ alg_READ ].name = "MISMATCH_READ";
    } else {
        ds->cols[ alg_READ ].name = "READ";
    }

    if ( param->long_cigar ) {
        ds->cols[ alg_CIGAR ].name = "CIGAR_LONG";
        ds->cols[ alg_CIGAR_LEN ].name = "CIGAR_LONG_LEN";
    } else {
        ds->cols[ alg_CIGAR ].name = "CIGAR_SHORT";
        ds->cols[ alg_CIGAR_LEN ].name = "CIGAR_SHORT_LEN";
    }

    if ( param -> qualQuantSingle ) { /** we don't really need quality ***/
        ds->cols[ alg_SAM_QUALITY   ] . name = "";
    }

    switch ( type ) {
        case edstt_EvidenceInterval:
            ds->cols[ alg_SEQ_SPOT_ID   ].name = "";
            ds->cols[ alg_SEQ_NAME      ].name = "";
            ds->cols[ alg_SAM_QUALITY   ].name = "";
            ds->cols[ alg_SPOT_GROUP    ].name = "";
            ds->cols[ alg_SEQ_SPOT_GROUP].name = "";
            ds->cols[ alg_SEQ_READ_ID   ].name = "";
            ds->cols[ alg_MATE_ALIGN_ID ].name = "";
            ds->cols[ alg_MATE_REF_ID   ].name = "";
            ds->cols[ alg_MATE_REF_NAME ].name = "";
            ds->cols[ alg_SAM_FLAGS     ].name = "";
            ds->cols[ alg_MATE_REF_POS  ].name = "";
            ds->cols[ alg_ALIGN_GROUP   ].name = "";
            ds->cols[ alg_TEMPLATE_LEN  ].name = "";
            if ( param->cg_sam || param->cg_ev_dnb ) {
                ds->cols[ alg_EVIDENCE_ALIGNMENT_IDS ].name = "EVIDENCE_ALIGNMENT_IDS";
            }
            break;

        case edstt_EvidenceAlignment:
            ds->cols[ alg_REF_PLOIDY    ].name = "REF_PLOIDY";
            ds->cols[ alg_REF_ID        ].name = "REF_ID";
            ds->cols[ alg_MATE_ALIGN_ID ].name = "";
            ds->cols[ alg_MATE_REF_ID   ].name = "";
            ds->cols[ alg_MATE_REF_NAME ].name = "";
            ds->cols[ alg_SAM_FLAGS     ].name = "";
            ds->cols[ alg_MATE_REF_POS  ].name = "";
            ds->cols[ alg_ALIGN_GROUP   ].name = "";
            ds->cols[ alg_TEMPLATE_LEN  ].name = "";
            break;

        default:
            break;
    }
}


static rc_t ProcessDB( VDatabase const *db, char const fullPath[],
                       char const accession[], char const readGroup[] ) {
    char const *const refTableName = (   param->region_qty > 0
                                      || param->cg_evidence
                                      || param->cg_ev_dnb
                                      || param->cg_sam
                                      || param->reheader
                                      || param->primaries
                                      || param->secondaries
                                     )
                                   ? "REFERENCE"
                                   : 0;
    char const *const priTableName = param->primaries   ? "PRIMARY_ALIGNMENT" : 0;
    char const *const secTableName = param->secondaries ? "SECONDARY_ALIGNMENT" : 0;
    char const *const seqTableName = ( param->unaligned || param->reheader ) ? "SEQUENCE" : 0;
    char const *const eviTableName = ( param->cg_evidence || ( param->cg_ev_dnb & ( param->region_qty > 0 ) ) || param->cg_sam )
                                        ? "EVIDENCE_INTERVAL" : 0;
    char const *const evaTableName = ( param->cg_ev_dnb   || param->cg_sam ) ? "EVIDENCE_ALIGNMENT" : 0;

    SAM_dump_ctx_t ctx;
    SCol align_cols[ ( sizeof( g_alg_col_tmpl ) / sizeof( g_alg_col_tmpl[ 0 ] ) ) * 4 ];
    SCol seq_cols[ sizeof( gSeqCol ) / sizeof( gSeqCol[ 0 ] ) ];
    rc_t rc = 0;

    memset( &ctx, 0, sizeof( ctx ) );
    memset( align_cols, 0, sizeof( align_cols ) );
    memset( seq_cols, 0, sizeof( seq_cols ) );

    ctx.db = db;
    ctx.fullPath = fullPath;
    ctx.accession = accession;
    ctx.readGroup = readGroup;

    DATASOURCE_INIT( ctx.seq, seqTableName );
    DATASOURCE_INIT( ctx.ref, refTableName );
    DATASOURCE_INIT( ctx.pri, priTableName );
    DATASOURCE_INIT( ctx.sec, secTableName );
    DATASOURCE_INIT( ctx.evi, eviTableName );
    DATASOURCE_INIT( ctx.eva, evaTableName );

    if ( ctx.seq.tbl.name ) {
        rc = VDatabaseOpenTableRead( db, &ctx.seq.tbl.vtbl, "%s", ctx.seq.tbl.name );
        if ( rc == 0 ) {
            ctx.seq.cols = seq_cols;
            memmove(seq_cols, gSeqCol, sizeof(gSeqCol));
            rc = Cursor_Open( &ctx.seq.tbl, &ctx.seq.curs, ctx.seq.cols, NULL );
        }
    }
    ctx.pri.cols = &align_cols[ 0 * ( sizeof( g_alg_col_tmpl ) / sizeof( g_alg_col_tmpl[ 0 ] ) ) ];
    ctx.sec.cols = &align_cols[ 1 * ( sizeof( g_alg_col_tmpl ) / sizeof( g_alg_col_tmpl[ 0 ] ) ) ];
    ctx.evi.cols = &align_cols[ 2 * ( sizeof( g_alg_col_tmpl ) / sizeof( g_alg_col_tmpl[ 0 ] ) ) ];
    ctx.eva.cols = &align_cols[ 3 * ( sizeof( g_alg_col_tmpl ) / sizeof( g_alg_col_tmpl[ 0 ] ) ) ];

    SetupColumns( &ctx.pri, edstt_PrimaryAlignment );
    SetupColumns( &ctx.sec, edstt_SecondaryAlignment );
    SetupColumns( &ctx.evi, edstt_EvidenceInterval );
    SetupColumns( &ctx.eva, edstt_EvidenceAlignment );

    if ( ctx.pri.tbl.name ) {
        VDatabaseOpenTableRead( db, &ctx.pri.tbl.vtbl, "%s", ctx.pri.tbl.name );
    }
    if ( ctx.sec.tbl.name ) {
        VDatabaseOpenTableRead( db, &ctx.sec.tbl.vtbl, "%s", ctx.sec.tbl.name );
    }
    if ( ctx.evi.tbl.name ) {
        VDatabaseOpenTableRead( db, &ctx.evi.tbl.vtbl, "%s", ctx.evi.tbl.name );
    }
    if ( ctx.eva.tbl.name ) {
        VDatabaseOpenTableRead( db, &ctx.eva.tbl.vtbl, "%s", ctx.eva.tbl.name );
    }

    if (   ctx.pri.tbl.vtbl == NULL
        && ctx.sec.tbl.vtbl == NULL
        && ctx.evi.tbl.vtbl == NULL
        && ctx.eva.tbl.vtbl == NULL ) {

        memset( &ctx.pri, 0, sizeof( ctx.pri ) );
        memset( &ctx.sec, 0, sizeof( ctx.sec ) );
        memset( &ctx.evi, 0, sizeof( ctx.evi ) );
        memset( &ctx.eva, 0, sizeof( ctx.eva ) );
        if ( ctx.seq.tbl.name == NULL ) {
            ctx.seq.tbl.name = "SEQUENCE";
            rc = VDatabaseOpenTableRead( db, &ctx.seq.tbl.vtbl, "%s", ctx.seq.tbl.name );
        }
        if ( rc == 0 ) {
            ctx.seq.cols = seq_cols;
            memmove(seq_cols, gSeqCol, sizeof(gSeqCol));
            rc = Cursor_Open( &ctx.seq.tbl, &ctx.seq.curs, ctx.seq.cols, NULL );
            if ( rc == 0 ) {
                rc = DumpTable( &ctx );
                Cursor_Close( &ctx.seq.curs );
            }
            VTableRelease(ctx.seq.tbl.vtbl);
        }
        VDatabaseRelease( db );
        return rc;
    }
    ReportResetDatabase( fullPath, db );

    if ( ctx.ref.tbl.name ) {
        rc = VDatabaseOpenTableRead( db, &ctx.ref.tbl.vtbl, "%s", ctx.ref.tbl.name );
        ctx.ref.type = edstt_Reference;
    }
    if ( rc == 0 ) {
        rc = Cursor_Open( &ctx.pri.tbl, &ctx.pri.curs, ctx.pri.cols, ctx.seq.curs.cache );
    }
    if ( rc == 0 ) {
        rc = Cursor_Open( &ctx.sec.tbl, &ctx.sec.curs, ctx.sec.cols, NULL );
    }
    if ( rc == 0 ) {
        rc = Cursor_Open( &ctx.evi.tbl, &ctx.evi.curs, ctx.evi.cols, NULL );
    }
    if ( rc == 0 ) {
        rc = Cursor_Open( &ctx.eva.tbl, &ctx.eva.curs, ctx.eva.cols, NULL );
    }
    if ( rc == 0 ) {
        rc = DumpDB( &ctx );
    }

    Cursor_Close(&ctx.ref.curs);
    Cursor_Close(&ctx.pri.curs);
    Cursor_Close(&ctx.sec.curs);
    Cursor_Close(&ctx.evi.curs);
    Cursor_Close(&ctx.eva.curs);
    Cursor_Close(&ctx.seq.curs);

    VTableRelease( ctx.ref.tbl.vtbl );
    VTableRelease( ctx.pri.tbl.vtbl );
    VTableRelease( ctx.sec.tbl.vtbl );
    VTableRelease( ctx.evi.tbl.vtbl );
    VTableRelease( ctx.eva.tbl.vtbl );
    VTableRelease( ctx.seq.tbl.vtbl );

    return rc;
}

#if 0
rc_t ResolvePath( char const *accession, char const ** path ) {
    rc_t rc = 0;
    static char tblpath[ 4096 ];
#if TOOLS_USE_SRAPATH != 0
    static SRAPath* pmgr = NULL;

    if ( accession == NULL && path == NULL ) {
        SRAPathRelease( pmgr );
        return 0;
    }
    if (   pmgr != NULL
        || ( rc = SRAPathMake( &pmgr, NULL ) ) == 0
        || ( GetRCState( rc ) == rcNotFound && GetRCTarget( rc ) == rcDylib ) ) {
        *path = tblpath;
        tblpath[ 0 ] = '\0';
        rc = 0;
        do {
            if ( pmgr != NULL && !SRAPathTest( pmgr, accession ) ) {
                /* try to resolve the path using mgr */
                rc = SRAPathFind( pmgr, accession, tblpath, sizeof( tblpath ) );
                if ( rc == 0 )
                    break;
            }
            if ( string_size( accession ) >= sizeof( tblpath ) ) {
                rc = RC( rcExe, rcPath, rcResolving, rcBuffer, rcInsufficient );
            } else {
                string_copy( tblpath, ( sizeof tblpath ) - 1, accession, string_size( accession ) );
                rc = 0;
            }
        } while( false );
    }
#else
    if ( accession != NULL && path != NULL ) {
        *path = tblpath;
        string_copy( tblpath, ( sizeof tblpath ) - 1, accession, string_size( accession ) );
    }
#endif

    return rc;
}
#endif


static rc_t ProcessPath( VDBManager const *mgr, char const Path[] ) {
#if 0
    unsigned pathlen = string_size( Path );
    char *readGroup = NULL;
    char *path = NULL;
    unsigned i;
    rc_t rc;

    /* strip trailing path seperators */
    for ( i = pathlen; i > 0; ) {
        int const ch = Path[ --i ];

        if ( ch == '/' || ch == '\\' ) {
            --pathlen;
        } else {
            break;
}
    }

    /* find read group alias */
    for ( i = pathlen; i > 0; ) {
        int const ch = Path[ --i ];

        if ( ch == '=' ) {
            unsigned const pos = i + 1;
            unsigned const len = pathlen - pos;

            pathlen = i;

            readGroup = malloc( len + 1 );
            if ( readGroup == NULL ) {
                return RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
            }
            memmove( readGroup, &Path[ pos ], len );
            readGroup[ len ] = '\0';
            break;
        }
    }

    path = malloc( pathlen + 1 );
    if ( path == NULL ) {
        return RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
    }
    memmove( path, Path, pathlen );
    path[ pathlen ] = '\0';

    rc = ReportResetObject( path );
    if ( rc == 0 ) {
        char const *fullPath;

        rc = ResolvePath( path, &fullPath );
        if ( rc == 0 ) {
            char const *accession = fullPath;
            VDatabase const *db;

            /* use last path element as accession */
            for ( i = pathlen; i > 0; ) {
                int const ch = path[ --i ];

                if ( ch == '/' || ch == '\\' ) {
                    accession = &fullPath[ i + 1 ];
                    break;
                }
            }
            rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", fullPath );
            if ( rc == 0 ) {
                rc = ProcessDB( db, fullPath, accession, readGroup );
                VDatabaseRelease( db );
            } else {
                rc = ProcessTable( mgr, fullPath, accession, readGroup );
            }
        }
    }
    free( path );
    free( readGroup );
    return rc;

#else

    char * pc;
    rc_t rc = 0;
    size_t pathlen = string_size ( Path );

    /* look for scheme */
    pc = string_chr ( Path, pathlen, ':' );
    if ( pc == NULL ) {
        char *readGroup;
        char *path;
        size_t i;

        /* no scheme found - must be simple fs path */
    /* old: ---unused label */
        readGroup = NULL;
        path = NULL;

        /* strip trailing path separators */
        for ( i = pathlen; i > 0; -- pathlen ) {
            int const ch = Path[ --i ];
            if ( ch != '/' && ch != '\\' ) {
                break;
            }
        }

        /* find read group alias */
        for ( i = pathlen; i > 0; ) {
            int const ch = Path[ --i ];

            if ( ch == '=' ) {
                size_t const pos = i + 1;
                size_t const len = pathlen - pos;

                pathlen = i;

                readGroup = malloc( len + 1 );
                if ( readGroup == NULL ) {
                    return RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
                }
                memmove( readGroup, &Path[ pos ], len );
                readGroup[ len ] = '\0';
                break;
            }
        }

        path = malloc( pathlen + 1 );
        if ( path == NULL ) {
            return RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
        }
        memmove( path, Path, pathlen );
        path[ pathlen ] = '\0';

        rc = ReportResetObject( path );
        if ( rc == 0 ) {
            char const *accession = path;
            VDatabase const *db;

            /* use last path element as accession */
            for ( i = pathlen; i > 0; ) {
                int const ch = path[ --i ];

                if ( ch == '/' || ch == '\\' ) {
                    accession = &path[ i + 1 ];
                    break;
                }
            }
            rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", path );
            if ( rc == 0 ) {
                rc = ProcessDB( db, path, accession, readGroup );
                if ( UIError( rc, db, NULL ) ) {
                    UIDatabaseLOGError(rc, db, true);
/*                  *error_reported = true; */
                }
                VDatabaseRelease( db );
            } else {
                rc = ProcessTable( mgr, path, accession, readGroup );
                if ( UIError( rc, NULL, NULL ) ) {
                    UITableLOGError(rc, NULL, true);
/*                  *error_reported = true; */
                }
            }
        }
        free( path );
        free( readGroup );
        return rc;
    } else {
        VFSManager * vfs;
        rc = VFSManagerMake ( & vfs );
        if ( rc == 0 ) {
            VPath * vpath;
            char * basename;
            size_t zz;
            char buffer [8193];
            char readgroup_ [257];
            char * readgroup;

            rc = VFSManagerMakePath( vfs, &vpath, "%s", Path );
            VFSManagerRelease ( vfs );
            if ( rc == 0 ) {
                char * ext;

                rc = VPathReadPath( vpath, buffer, sizeof buffer - 1, &zz );
                if ( rc == 0 ) {
                loop:
                    basename = string_rchr( buffer, zz, '/' );
                    if ( basename == NULL ) {
                        basename = buffer;
                    } else {
                        if ( basename[1] == '\0' ) {
                            basename[0] = '\0';
                            goto loop;
                        }
                        ++ basename;
                        zz -= basename - buffer;
                    }

                    /* cut off [.lite].[c]sra[.nenc||.ncbi_enc] if any */
                    ext = string_rchr( basename, zz, '.' );
                    if ( ext != NULL ) {
#if WINDOWS
                        if ( _stricmp( ext, ".nenc" ) == 0 || _stricmp( ext, ".ncbi_enc" ) == 0 ) {
#else
                        if ( strcasecmp( ext, ".nenc" ) == 0 || strcasecmp( ext, ".ncbi_enc" ) == 0 ) {
#endif
                            zz -= ext - basename;
                            *ext = '\0';
                            ext = string_rchr( basename, zz, '.' );
                        }

                        if ( ext != NULL ) {
#if WINDOWS
                            if ( _stricmp( ext, ".sra" ) == 0 || _stricmp( ext, ".csra" ) == 0 ) {
#else
                            if ( strcasecmp( ext, ".sra" ) == 0 || strcasecmp( ext, ".csra" ) == 0 ) {
#endif
                                zz -= ext - basename;
                                *ext = '\0';
                                ext = string_rchr( basename, zz, '.' );
#if WINDOWS
                                if ( ext != NULL && _stricmp( ext, ".lite" ) == 0 ) {
#else
                                if ( ext != NULL && strcasecmp( ext, ".lite" ) == 0 ) {
#endif
                                    *ext = '\0';
                                }
                            }
                        }
                    }

                    rc = VPathOption (vpath, vpopt_readgroup, readgroup_, sizeof readgroup_ - 1, &zz);
                    if ( rc == 0 ) {
                        readgroup = readgroup_;
                    } else if ( GetRCState ( rc ) == rcNotFound ) {
                        rc = 0;
                        readgroup = NULL;
                    }

                    if ( rc == 0 ) {
                        VDatabase const *db;

                        rc = VDBManagerOpenDBRead( mgr, &db, NULL, "%s", Path );
                        if ( rc == 0 ) {
                            rc = ProcessDB( db, Path, basename, readgroup );
                            VDatabaseRelease( db );
                        } else {
                            rc = ProcessTable( mgr, Path, basename, readgroup );
                        }
                    }
                }
                VPathRelease( vpath );
            }
        }
    }
    return rc;
#endif
}

/*
ver_t CC KAppVersion( void )
{
    return SAM_DUMP_VERS;
}
*/

char const *unaligned_usage[] = {"Output unaligned reads", NULL};
char const *unaligned_only_usage[] = {"Only output unaligned spots", NULL};
char const *primaryonly_usage[] = {"Output only primary alignments", NULL};
char const *cigartype_usage[] = {"Output long version of CIGAR", NULL};
char const *cigarCG_usage[] = {"Output CG version of CIGAR", NULL};
char const *cigarCGMerge_usage[] = {"Apply CG fixups to CIGAR/SEQ/QUAL and outputs CG-specific columns", NULL};
char const *CG_evidence[] = {"Output CG evidence aligned to reference", NULL};
char const *CG_ev_dnb[] = {"Output CG evidence DNB's aligned to evidence", NULL};
char const *CG_SAM[] = {"Output CG evidence DNB's aligned to reference ", NULL};
char const *CG_mappings[] = {"Output CG sequences aligned to reference ", NULL};
char const *header_usage[] = {"Replace original BAM header with a reconstructed one", NULL};
char const *noheader_usage[] = {"Do not output headers", NULL};
char const *region_usage[] = {"Filter by position on genome.",
                              "Name can either be file specific name (ex: \"chr1\" or \"1\").",
                              "\"from\" and \"to\" are 1-based coordinates", NULL};
char const *distance_usage[] = {"Filter by distance between matepairs.",
                                "Use \"unknown\" to find matepairs split between the references.",
                                "Use from-to to limit matepair distance on the same reference", NULL};
char const *seq_id_usage[] = {"Print reference SEQ_ID in RNAME instead of NAME", NULL};
char const *identicalbases_usage[] = {"Output '=' if base is identical to reference", NULL};
char const *gzip_usage[] = {"Compress output using gzip", NULL};
char const *bzip2_usage[] = {"Compress output using bzip2", NULL};
char const *qname_usage[] = {"Add .SPOT_GROUP to QNAME", NULL};
char const *fasta_usage[] = {"Produce Fasta formatted output", NULL};
char const *fastq_usage[] = {"Produce FastQ formatted output", NULL};
char const *prefix_usage[] = {"Prefix QNAME: prefix.QNAME", NULL};
char const *reverse_usage[] = {"Reverse unaligned reads according to read type", NULL};
char const *comment_usage[] = {"Add comment to header. Use multiple times for several lines. Use quotes", NULL};
char const *XI_usage[] = {"Output cSRA alignment id in XI column", NULL};
char const *qual_quant_usage[] = {"Quality scores quantization level",
                                  "a string like '1:10,10:20,20:30,30:-'", NULL};
char const *CG_names[] = { "Generate CG friendly read names", NULL};

char const *usage_params[] = {
    NULL,                       /* unaligned */
    NULL,                       /* unaligned-only */
    NULL,                       /* primaryonly */
    NULL,                       /* cigartype */
    NULL,                       /* cigarCG */
    NULL,                       /* header */
    NULL,                       /* no-header */
    "text",                     /* comment */
    "name[:from-to]",           /* region */
    "from-to|'unknown'",        /* distance */
    NULL,                       /* seq-id */
    NULL,                       /* identical-bases */
    NULL,                       /* gzip */
    NULL,                       /* bzip2 */
    NULL,                       /* qname */
    NULL,                       /* fasta */
    NULL,                       /* fastq */
    "prefix",                   /* prefix */
    NULL,                       /* reverse */
    NULL,                       /* test-rows */
    NULL,                       /* mate-row-gap-cacheable */
    NULL,                       /* cigarCGMerge */
    NULL,                       /* XI */
    "quantization string",      /* qual-quant */
    NULL,                       /* cigarCGMerge */
    NULL,                       /* CG-evidence */
    NULL,                       /* CG-ev-dnb */
    NULL,                       /* CG-mappings */
    NULL,                       /* CG-SAM */
    NULL                        /* CG-names */
};

enum eArgs {
    earg_unaligned = 0,         /* unaligned */
    earg_unaligned_only,        /* unaligned */
    earg_prim_only,             /* primaryonly */
    earg_cigartype,             /* cigartype */
    earg_cigarCG,               /* cigarCG */
    earg_header,                /* header */
    earg_noheader,              /* no-header */
    earg_comment,               /* comment */
    earg_region,                /* region */
    earg_distance,              /* distance */
    earg_seq_id,                /* seq-id */
    earg_identicalbases,        /* identical-bases */
    earg_gzip,                  /* gzip */
    earg_bzip2,                 /* bzip2 */
    earg_qname,                 /* qname */
    earg_fastq,                 /* fasta */
    earg_fasta,                 /* fastq */
    earg_prefix,                /* prefix */
    earg_reverse,               /* reverse */
    earg_test_rows,             /* test-rows */
    earg_mate_row_gap_cachable, /* mate-row-gap-cacheable */
    earg_cigarCG_merge,         /* cigarCGMerge */
    earg_XI,                    /* XI */
    earg_QualQuant,             /* qual-quant */
    earg_CG_evidence,           /* CG-evidence */
    earg_CG_ev_dnb,             /* CG-ev-dnb */
    earg_CG_mappings,           /* CG-mappings */
    earg_CG_SAM,                /* CG-SAM */
    earg_CG_names               /* CG-names */
};

OptDef DumpArgs[] = {
    { "unaligned", "u", NULL, unaligned_usage, 0, false, false },           /* unaligned */
    { "unaligned-only", "U", NULL, unaligned_usage, 0, false, false },      /* unaligned-only */
    { "primary", "1", NULL, primaryonly_usage, 0, false, false },           /* primaryonly */
    { "cigar-long", "c", NULL, cigartype_usage, 0, false, false },          /* cigartype */
    { "cigar-CG", NULL, NULL, cigarCG_usage, 0, false, false },             /* cigarCG */
    { "header", "r", NULL, header_usage, 0, false, false },                 /* header */
    { "no-header", "n", NULL, noheader_usage, 0, false, false },            /* no-header */
    { "header-comment", NULL, NULL, comment_usage, 0, true, false },        /* comment */
    { "aligned-region", NULL, NULL, region_usage, 0, true, false },         /* region */
    { "matepair-distance", NULL, NULL, distance_usage, 0, true, false },    /* distance */
    { "seqid", "s", NULL, seq_id_usage, 0, false, false },                  /* seq-id */
    { "hide-identical", "=", NULL, identicalbases_usage, 0, false, false }, /* identical-bases */
    { "gzip", NULL, NULL, gzip_usage, 0, false, false },                    /* gzip */
    { "bzip2", NULL, NULL, bzip2_usage, 0, false, false },                  /* bzip2 */
    { "spot-group", "g", NULL, qname_usage, 0, false, false },              /* qname */
    { "fastq", NULL, NULL, fastq_usage, 0, false, false },                  /* fasta */
    { "fasta", NULL, NULL, fasta_usage, 0, false, false },                  /* fastq */
    { "prefix", "p", NULL, prefix_usage, 0, true, false },                  /* prefix */
    { "reverse", NULL, NULL, reverse_usage, 0, false, false },              /* reverse */
    { "test-rows", NULL, NULL, NULL, 0, true, false },                      /* test-rows */
    { "mate-cache-row-gap", NULL, NULL, NULL, 0, true, false },             /* mate-row-gap-cacheable */
    { "cigar-CG-merge", NULL, NULL, cigarCGMerge_usage, 0, false, false },  /* cigarCGMerge */
    { "XI", NULL, NULL, XI_usage, 0, false, false },                        /* XI */
    { "qual-quant", "Q", NULL, qual_quant_usage, 0, true, false },          /* qual-quant */
    { "CG-evidence", NULL, NULL, CG_evidence, 0, false, false },            /* CG-evidence */
    { "CG-ev-dnb"  , NULL, NULL, CG_ev_dnb  , 0, false, false },            /* CG-ev-dnb */
    { "CG-mappings", NULL, NULL, CG_mappings, 0, false, false },            /* CG-mappings */
    { "CG-SAM", NULL, NULL, CG_SAM, 0, false, false },                      /* CG-SAM */
    { "CG-names", NULL, NULL, CG_names, 0, false, false },                  /* CG-names */
    { "legacy", NULL, NULL, NULL, 0, false, false }
};

/*
const char UsageDefaultName[] = "sam-dump";

rc_t CC UsageSummary( char const *progname ) {
    return KOutMsg( "Usage:\n"
        "\t%s [options] path-to-run[ path-to-run ...] > output.sam\n\n", progname );
}

rc_t CC Usage( Args const *args) {
    char const *progname = UsageDefaultName;
    char const *fullpath = UsageDefaultName;
    rc_t rc;
    unsigned i;

    rc = ArgsProgram( args, &fullpath, &progname );

    UsageSummary( progname );

    KOutMsg( "Options:\n" );
    for( i = 0; i < sizeof( DumpArgs ) / sizeof( DumpArgs[ 0 ] ); i++ ) {
        if ( DumpArgs[ i ].help != NULL ) {
            HelpOptionLine( DumpArgs[ i ].aliases, DumpArgs[ i ].name,
                            usage_params[ i ], DumpArgs[ i ].help);
        }
    }
    KOutMsg( "\n" );
    HelpOptionsStandard();

    HelpVersion( fullpath, KAppVersion() );

    return rc;
}
*/

#define ARGS_COUNT (sizeof(DumpArgs)/sizeof(DumpArgs[0]))

static unsigned ParamCount( Args const *const args, rc_t *const prc ) {
    uint32_t y = 0;
    rc_t rc = ArgsParamCount( args, &y );
    if ( prc != NULL ) {
        *prc = rc;
    }
    return y;
}

static unsigned ArgCount( Args const *const args, rc_t *const prc ) {
    uint32_t y = 0;
    rc_t rc = ArgsArgvCount( args, &y );
    if ( prc != NULL ) {
        *prc = rc;
    }
    return y;
}

static rc_t CountArgs( Args const *const args, unsigned count[],
                       char const **const errmsg ) {
    rc_t rc;
    unsigned const pcount = ParamCount( args, &rc );

    memset( count, 0, ARGS_COUNT );
    if ( rc != 0 || pcount == 0 ) {
        *errmsg = "";
        MiniUsage( args );
        return ( ArgCount( args, NULL ) < 2 )
             ? 0
             : RC( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
    }
#define COUNT_ARG(E) do {\
    char const *const optname = DumpArgs[ (E) ].name; \
    if ( ( rc = ArgsOptionCount( args, optname, &count[ (E) ] ) ) != 0 ) { \
        *errmsg = optname; \
        return rc; \
    } } while( 0 )


    /* record source options */
    COUNT_ARG( earg_unaligned );
    COUNT_ARG( earg_unaligned_only );
    COUNT_ARG( earg_prim_only );
    COUNT_ARG( earg_CG_evidence );
    COUNT_ARG( earg_CG_ev_dnb );
    COUNT_ARG( earg_CG_mappings );
    COUNT_ARG( earg_CG_SAM );

    /* record filter options */
    COUNT_ARG( earg_region );
    COUNT_ARG( earg_distance );

    /* output format options */
    COUNT_ARG( earg_fastq );
    COUNT_ARG( earg_fasta );

    /* SAM header options */
    COUNT_ARG( earg_header );
    COUNT_ARG( earg_noheader );
    COUNT_ARG( earg_comment );

    /* SAM field options */
    COUNT_ARG( earg_prefix );
    COUNT_ARG( earg_qname );
    COUNT_ARG( earg_seq_id );
    COUNT_ARG( earg_CG_names );

    COUNT_ARG( earg_cigartype );
    COUNT_ARG( earg_cigarCG );
    COUNT_ARG( earg_cigarCG_merge );

    COUNT_ARG( earg_identicalbases );
    COUNT_ARG( earg_reverse );
    COUNT_ARG( earg_QualQuant );

    /* output encoding options */
    COUNT_ARG( earg_gzip );
    COUNT_ARG( earg_bzip2 );

    COUNT_ARG( earg_mate_row_gap_cachable );

    /* debug options */
    COUNT_ARG( earg_XI );
    COUNT_ARG( earg_test_rows );
#undef COUNT_ARG
    return 0;
}

static unsigned GetOptValU( Args const *const args, char const *const argname,
                            unsigned const defval, rc_t *const prc ) {
    unsigned y = defval;
    char const *valstr;
    rc_t rc = ArgsOptionValue( args, argname, 0, (const void **)&valstr );

    if ( rc == 0 ) {
        char *endp;

        y = strtou32( valstr, &endp, 0 );
        if ( *endp != '\0' ) {
            rc = RC( rcExe, rcArgv, rcProcessing, rcParam, rcInvalid );
            y = 0;
        }
        if ( y == 0 ) {
            y = defval;
        }
    }
    if ( prc ) {
        *prc = rc;
    }
    return y;
}

static rc_t GetComments( Args const *const args, struct params_s *const parms, unsigned const n ) {
    parms->comments = calloc( n + 1, sizeof( parms->comments[ 0 ] ) );
    if ( parms->comments != NULL ) {
        unsigned i;
        for ( i = 0; i < n; ++i ) {
            rc_t const rc = ArgsOptionValue( args, DumpArgs[ earg_comment ].name, i, (const void **)&parms->comments[ i ] );
            if ( rc != 0 ) {
                free( ( void * )parms->comments );
                parms->comments = NULL;
                return rc;
            }
        }
        return 0;
    } else {
        return RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
    }
}

static rc_t ParseFromTo( int *const pFrom, int *const pTo, char const str[] ) {
    /* $str =~ /((\d+)-(\d+))|(-(\d+))|(\d+)/ */
    int n;
    char fr_str[ 16 ];
    char to_str[ 16 ];
    uint32_t fr = 0;
    uint32_t to = 0;
    int i = sscanf( str, "%15[0-9]-%15[0-9]%n", fr_str, to_str, &n );

    if ( i != 2 ) {
        unsigned const offset = ( str[ 0 ] == '-' ) ? 1 : 0;

        fr = 0;
        i = sscanf( str + offset, "%15[0-9]%n", to_str, &n );
        if ( i != 1 ) {
            return RC( rcExe, rcArgv, rcProcessing, rcParam, rcOutofrange );
        }
        to = strtou32( to_str, 0, 0 );
    } else {
        fr = strtou32( fr_str, 0, 0 );
        to = strtou32( to_str, 0, 0 );

        if ( to < fr ) {
            uint32_t const tmp = to;
            to = fr;
            fr = tmp;
        }
    }
    /* was the entire string consumed */
    if ( str[ n ] != 0 ) {
        return RC( rcExe, rcArgv, rcProcessing, rcParam, rcInvalid );
    }
    *pFrom = ( int )fr;
    *pTo = ( int )to;
    return 0;
}


static rc_t GetDistance( Args const *const args, struct params_s *const parms, unsigned const n ) {
    rc_t rc;
    TMatepairDistance *const mpd = calloc( n, sizeof( mpd[ 0 ] ) );

    if ( mpd == NULL ) {
        rc = RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
    } else {
        unsigned i;
        unsigned j;

        for ( j = i = 0; i < n; ++i ) {
            char const *valstr;
            int from = 0;
            int to = 0;

            rc = ArgsOptionValue( args, DumpArgs[earg_distance].name, i, (const void **)&valstr );
            if ( rc != 0 ) {
                break;
            }
#if WINDOWS
            if ( _stricmp( valstr, "unknown" ) == 0 ) {
#else
            if ( strcasecmp( valstr, "unknown" ) == 0 ) {
#endif
                parms->mp_dist_unknown = true;
                continue;
            }
            rc = ParseFromTo( &from, &to, valstr );
            if ( rc != 0 ) {
                break;
            }
            if ( ( from != 0 ) || ( to != 0 ) ) {
                mpd[ j ].from = from;
                mpd[ j ].to = to;
                ++j;
            }
        }
        if ( rc == 0 ) {
            parms->mp_dist = mpd;
            parms->mp_dist_qty = j;

            SAM_DUMP_DBG( 2, ( "filter by mate pair distance\n" ) );
            if ( parms->mp_dist_unknown ) {
                SAM_DUMP_DBG( 2, ( "    distance: unknown\n" ) );
            }
            for ( i = 0; i < j; ++i ) {
                SAM_DUMP_DBG( 2, ( "    distance [%lu-%lu]\n",
                              parms->mp_dist[ i ].from, parms->mp_dist[ i ].to ) );
            }
        } else {
            free( mpd );
        }
    }
    return rc;
}

static rc_t GetRegion( Args const *const args, struct params_s *const parms, unsigned const n ) {
    rc_t rc;
    TAlignedRegion *const reg = calloc( n, sizeof( reg[ 0 ] ) );

    if ( reg == NULL ) {
        rc = RC( rcExe, rcArgv, rcProcessing, rcMemory, rcExhausted );
    } else {
        unsigned i;
        unsigned j;

        for ( rc = 0, j = i = 0; i < n; ++i ) {
            char const *valstr;
            char const *sep;
            unsigned namelen;
            unsigned f;
            unsigned e;
            TAlignedRegion r;

            memset( &r, 0, sizeof( r ) );

            rc = ArgsOptionValue( args, DumpArgs[ earg_region ].name, i, (const void **)&valstr );
            if ( rc != 0 ) {
                break;
            }

            sep = strchr( valstr, ':' );
            if ( sep != NULL ) {
                namelen = sep - valstr;
            } else {
                namelen = string_size( valstr );
            }
            if ( namelen >= sizeof( r.name ) ) {
                return RC( rcExe, rcArgv, rcProcessing, rcParam, rcTooLong );
            }
            memmove( r.name, valstr, namelen );
            r.name[ namelen ] = '\0';

            r.rq = UINT_MAX;
            if ( sep != NULL ) {
                int from = -1;
                int to = -1;

                rc = ParseFromTo( &from, &to, sep + 1 );
                if ( rc != 0 ) {
                    break;
                }

                if ( from > 0 ) {/* convert to 0-based */
                    --from;
                } else if ( to > 0 ) {
                    from = 0;
                }

                if ( to > 0 ) {
                    --to;
                } else if ( from >= 0 && to < 0 ) {
                    to = from;
                }

                if ( from >= 0 || to >= 0 ) {
                    if ( from > to ) {
                        int tmp = to;
                        to = from;
                        from = tmp;
                    }
                    r.r[ 0 ].from = from;
                    r.r[ 0 ].to = to;
                    r.rq = 1;
                }
            }

            for ( f = 0, e = j; f < e; ) {
                unsigned const m = ( ( f + e ) / 2 );
                int const diff = strcmp( r.name, reg[ m ].name );

                if ( diff < 0 ) {
                    e = m;
                } else {
                    f = m + 1;
                }
            }

            if ( 0 < e && e < j && strcmp( r.name, reg[ e-1 ].name ) == 0 ) {
                TAlignedRegion *const fnd = &reg[ e - 1 ];

                if ( fnd->rq != UINT_MAX ) {
                    for ( f = 0, e = fnd->rq; f < e; ) {
                        unsigned const m = ( ( f + e ) / 2 );

                        if ( r.r[ 0 ].from < fnd->r[ m ].from ) {
                            e = m;
                        } else {
                            f = m + 1;
                        }
                    }
                    if ( fnd->rq >= ( sizeof( r.r ) / sizeof( r.r[0] ) ) ) {
                        rc = RC( rcExe, rcArgv, rcProcessing, rcBuffer, rcInsufficient );
                        break;
                    }
                    memmove( &fnd->r[ e +1 ], &fnd->r[ e ], ( fnd->rq - e ) * sizeof( fnd->r[ 0 ] ) );
                    fnd->r[ e ] = r.r[ 0 ];
                    ++fnd->rq;
                }
            } else {
                memmove( &reg[ e + 1 ], &reg[ e ], ( j - e ) * sizeof( reg[ 0 ] ) );
                reg[ e ] = r;
                ++j;
            }
        }

        if ( rc == 0 ) {
            parms->region = reg;
            parms->region_qty = j;

            for ( i = 0; i < parms->region_qty; ++i ) {
                TAlignedRegion *const r = &parms->region[ i ];

                SAM_DUMP_DBG( 2, ( "filter by %s\n", r->name ) );
                if ( r->rq == UINT_MAX ) {
                    r->rq = 1;
                    r->r[ 0 ].from = 0;
                    r->r[ 0 ].to = UINT_MAX;
                }
                for ( j = 0; j < r->rq; ++j ) {
                    unsigned const to = r->r[ j ].to;
                    unsigned const from = r->r[ j ].from;

                    SAM_DUMP_DBG( 2, ( "   range: [%u:%u]\n", r->r[ j ].from, to ) );
                    if ( r->max_to < to ) r->max_to = to;
                    if ( r->min_from < from ) r->min_from = from;
                }
            }
        } else {
            free( reg );
        }
    }
    return rc;
}

static rc_t GetQualQuant( Args const *const args, struct params_s *const parms ) {
    char const *valstr;
    int i;
    rc_t rc = ArgsOptionValue( args, DumpArgs[ earg_QualQuant ].name, 0, (const void **)&valstr );

    if ( rc == 0 && strcmp( valstr, "0" ) != 0 ) {
        parms->quantizeQual = QualityQuantizerInitMatrix( parms->qualQuant, valstr );
    }
    for ( i = 1; parms -> qualQuant[ i ] == parms -> qualQuant[ 0 ] && i < 256; i++ ){
    }
    if ( i < 256 ) {
        parms -> qualQuantSingle = 0;
    } else {
        parms->qualQuantSingle = parms->qualQuant[ 0 ];
    }
    return rc;
}

static rc_t GetArgs( Args const *const args, unsigned const count[],
                     char const **const errmsg ) {
    static struct params_s parms;
    bool const multipass = ParamCount( args, 0 ) > 1;
    rc_t rc;

    memset( &parms, 0, sizeof( parms ) );

    if ( count[ earg_comment ] ) {
        rc = GetComments( args, &parms, count[ earg_comment ] );
        if ( rc != 0 ) {
            *errmsg = DumpArgs[ earg_comment ].name;
            return rc;
        }
    }

    if ( count[ earg_region ] && count[ earg_unaligned_only ] == 0 ) {
        rc = GetRegion( args, &parms, count[ earg_region ] );
        if ( rc != 0 ) {
            *errmsg = DumpArgs[ earg_region ].name;
            return rc;
        }
    }

    if ( count[ earg_distance ] && count[ earg_unaligned_only ] == 0 ) {
        rc = GetDistance( args, &parms, count[ earg_distance ] );
        if ( rc != 0 ) {
            *errmsg = DumpArgs[ earg_distance ].name;
            return rc;
        }
    }

    if ( count[ earg_QualQuant ] ) {
        rc = GetQualQuant( args, &parms );
        if ( rc != 0 ) {
            *errmsg = DumpArgs[ earg_QualQuant ].name;
            return rc;
        }
    }

    parms.cg_sam = ( count[ earg_CG_SAM ] != 0 );
    parms.cg_evidence = ( count[ earg_CG_evidence ] != 0 );
    parms.cg_ev_dnb = ( count[ earg_CG_ev_dnb ] != 0 );

    if ( count[ earg_CG_mappings ] == 0 &&
         ( parms.cg_sam || parms.cg_evidence || parms.cg_ev_dnb ) ) {
        parms.primaries = false;
        parms.secondaries = false;
        parms.unaligned = false;
    } else {
        parms.primaries = true;
        parms.secondaries = ( count[ earg_prim_only ] == 0 );
        parms.unaligned = ( ( count[ earg_unaligned ] != 0 ) /*&& ( parms.region_qty == 0 )*/ );
    }

    parms.long_cigar = ( count[ earg_cigartype ] != 0 );
    parms.use_seqid = ( ( count[ earg_seq_id ] != 0 ) || multipass );
    parms.hide_identical = ( count[ earg_identicalbases ] != 0 );
    parms.fasta = ( count[ earg_fasta ] != 0 );
    parms.fastq = ( count[ earg_fastq ] != 0 );
    parms.reverse_unaligned = ( count[ earg_reverse ] != 0 );
    parms.cg_friendly_names = count[ earg_CG_names ] != 0;
    parms.spot_group_in_name = ( count[ earg_qname ] != 0 || multipass );
    parms.noheader = ( ( count[ earg_noheader ] != 0 ) || parms.fasta || parms.fastq || multipass );
    parms.reheader = ( ( count[ earg_header ] != 0 ) && !parms.noheader );
    parms.xi = ( count[ earg_XI ] != 0 );

    if ( ( count[ earg_cigarCG ] == 0 ) && ( count[ earg_cigarCG_merge ] == 0 ) ) {
        parms.cg_style = 0;
    } else if ( count[ earg_cigarCG ] == 0 ) {
        parms.cg_style = 1;
        parms.long_cigar = false;
    } else if ( count[ earg_cigarCG_merge ] == 0 ) {
        parms.cg_style = 2;
    } else {
        rc = RC( rcExe, rcArgv, rcProcessing, rcParam, rcInconsistent );
        *errmsg = "cigar-CG and CG are mutually exclusive";
        parms.cg_style = 0;
    }

    if ( count[ earg_unaligned_only ] > 0 ) {
        parms.unaligned_spots = true;

        parms.primaries = false;
        parms.secondaries = false;
        parms.unaligned = true;
        parms.cg_ev_dnb = false;
        parms.cg_evidence = false;
        parms.cg_sam = false;
        parms.cg_style = 0;
    }

    parms.test_rows = GetOptValU( args, DumpArgs[ earg_test_rows ].name, 0, NULL );
    parms.mate_row_gap_cachable = GetOptValU( args, DumpArgs[ earg_mate_row_gap_cachable ].name, 1000000, NULL );

    param = &parms;
    return 0;
}

static rc_t GetParams( Args const *const args, char const **const errmsg ) {
    unsigned count[ ARGS_COUNT ];
    rc_t rc = CountArgs( args, count, errmsg );

    if ( rc == 0 ) {
        rc = GetArgs( args, count, errmsg );
    }
    return rc;
}

rc_t CC SAM_Dump_Main( int argc, char* argv[] ) {
    rc_t rc = 0;
    Args* args;
    char const *errmsg = "stop";
    bool error_reported = false;

    memset( &g_out_writer, 0, sizeof( g_out_writer ) );
    KOutHandlerSetStdOut();
    KStsHandlerSetStdErr();
    KLogHandlerSetStdErr();
    (void)KDbgHandlerSetStdErr();

    ReportBuildDate( __DATE__ );

    rc = ArgsMakeAndHandle( &args, argc, argv, 1, DumpArgs, ARGS_COUNT );
    if ( rc == 0 ) {
        uint32_t pcount;

        rc = ArgsParamCount( args, &pcount );
        if ( rc != 0 || pcount < 1 ) {
            errmsg = "";
            rc = argc < 2 ? 0 : RC( rcExe, rcArgv, rcParsing, rcParam, rcInsufficient );
            MiniUsage( args );
        } else if ( ( rc = GetParams( args, &errmsg ) ) != 0 ) {

        } else {
            VDBManager const *mgr;

            rc = VDBManagerMakeRead( &mgr, NULL );
            if ( rc == 0 ) {
                rc = BufferedWriterMake( param->output_gzip, param->output_bz2 );
                if ( rc == 0 ) {
                    unsigned i;

                    for ( i = 0; i < pcount; ++i ) {
                        char const *arg;

                        rc = ArgsParamValue( args, i, (const void **)&arg );
                        if ( rc != 0 ) break;
                        rc = ProcessPath( mgr, arg );
#if _ARCH_BITS < 64
                        /* look for "param excessive while writing vector within container module" */
                        if (GetRCState(rc) == rcExcessive &&
                            GetRCObject(rc) == rcParam &&
                            GetRCContext(rc) == rcWriting &&
                            GetRCTarget(rc) == rcVector &&
                            GetRCModule(rc) == rcCont) {
                            ( void )PLOGMSG( klogErr, (klogErr, "This run '$(run)' contains too many reads to be processed with a $(bits)-bit executable; please try again with a 64-bit executable.", "run=%s,bits=%u", arg, _ARCH_BITS));
                        }
#endif
                        if ( rc != 0 ) break;
                    }
                    BufferedWriterRelease( rc == 0 );
                }
                VDBManagerRelease( mgr );
            }
        }
        ArgsWhack( args );
    }

    if ( rc != 0 && !error_reported ) {
        if ( errmsg[ 0 ] ) {
            ( void )LOGERR( klogErr, rc, errmsg );
        } else if ( KLogLastErrorCode() != rc ) {
            ( void )LOGERR( klogErr, rc, "stop" );
        }
    }

    {
        /* Report execution environment if necessary */
        rc_t rc2 = ReportFinalize( rc );
        if ( rc == 0 ) {
            rc = rc2;
        }
    }
    return rc;
}

rc_t CC Legacy_KMain( int argc, char* argv[] ) {
    char filename[ 4096 ];
    /*
       Try to find a option-file - parameter in the original parameters
       This is a hack to teach sam-dump to read it's parameters from a file/pipe instead
       of the commandline
       It is neccessary because the code above does not use libkapp to parse parameters!
    */
    rc_t rc = Args_find_option_in_argv( argc, argv, "--option-file", filename, sizeof filename );
    if ( rc == 0 ) {
        int new_argc;
        char ** new_argv;
        /* we found it! Now comes the special treatment: we fake a new argc/argv-pair! */
        rc = Args_tokenize_file_and_progname_into_argv( filename, "sam-dump", &new_argc, &new_argv );
        if ( rc == 0 ) {
            /* pass the faked input-parameters from */
            rc = SAM_Dump_Main( new_argc, new_argv );

            Args_free_token_argv( new_argc, new_argv );
        }
    } else {
        /* we did not found the special option, just use the commandline-parameters unchanged */
        rc = SAM_Dump_Main( argc, argv );
    }
    return rc;
}
