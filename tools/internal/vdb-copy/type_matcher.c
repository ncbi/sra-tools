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
#include "vdb-copy-includes.h"
#include "matcher_input.h"
#include "helper.h"

#include <sysalloc.h>
#include <stdlib.h>

/* internal definition of a type to be matched */
typedef struct mtype {
    char *name;
    uint32_t dflt; /* 0=yes, 1=no (this way for re-ordering) */
    uint32_t order;
    uint32_t lossy_score;
    VTypedecl type_decl;
    VTypedesc type_desc;
} mtype;
typedef mtype* p_mtype;


/* internal definition of a pair of source- and dest-type */
typedef struct mpair {
    p_mtype src;
    p_mtype dst;
    uint32_t compatible; /* 0=yes, 1=no (this way for re-ordering) */
    uint32_t distance;
} mpair;
typedef mpair* p_mpair;


/* internal definition of a column */
typedef struct mcol {
    char *name;
    uint32_t src_idx;       /* index of this column relative to the
                               read-cursor */

    uint32_t dst_idx;       /* index of this column relative to the
                               write-cursor */

    bool excluded;          /* for excludes via config-file */

    bool to_copy;           /* this column has a correspondig column 
                               in the list of writable columns of the
                               destination-schema with writable types */

    VTypedecl type_decl;    /* type-decl of this column via read-schema */
    VTypedesc type_desc;    /* type-desc of this column via read-schema */

    Vector src_types;       /* list of src-types */
    Vector dst_types;       /* list of dst-types */
    Vector pairs;           /* list of (src->dst)-pairs... */

    /* the src->dst type-pair, that won the type-matching-procedure */
    p_mpair type_cast;
} mcol;
typedef mcol* p_mcol;

/* internal definition of the matcher */
typedef struct matcher {
    Vector mcols;
} matcher;
typedef matcher* p_matcher;

/* allocate a type-pair */
static p_mpair matcher_init_pair( const p_mtype src, 
                                  const p_mtype dst ) {
    p_mpair res = NULL;
    if ( src == NULL ) return res;
    if ( dst == NULL ) return res;
    res = calloc( 1, sizeof( mpair ) );
    if ( NULL != res ) {
        res -> src = src;
        res -> dst = dst;
    }
    return res;
}

/* destroys a type-pair */
static void CC matcher_destroy_pair( void* node, void* data ) {
    p_mpair pair = ( p_mpair )node;
    if ( NULL != pair ) {
        free( pair );
    }
}

/* allocate a m-type definition */
static p_mtype matcher_init_type( const char* name, 
                                  const bool dflt,
                                  const uint32_t order ) {
    p_mtype res = NULL;
    if ( NULL == name ) return res;
    if ( 0 == name[ 0 ] ) return res;
    res = calloc( 1, sizeof( mtype ) );
    if ( NULL != res ) {
        res -> name = string_dup_measure ( name, NULL );
        res -> dflt = ( dflt ? 0 : 1 );
        res -> order = order;
    }
    return res;
}

/* destroys a m-type definition */
static void CC matcher_destroy_type( void* node, void* data ) {
    p_mtype type = ( p_mtype )node;
    if ( NULL != type ) {
        if ( NULL != type -> name ) {
            free( type -> name );
        }
        free( type );
    }
}

/* allocate a matcher-column */
static p_mcol matcher_make_col( const char* name ) {
    p_mcol res = NULL;
    if ( NULL == name ) return res;
    if ( 0 == name[ 0] ) return res;
    res = calloc( 1, sizeof( mcol ) );
    /* because of calloc all members are zero! */
    if ( NULL != res ) {
        res -> name = string_dup_measure ( name, NULL );
        VectorInit( &( res -> src_types ), 0, 3 );
        VectorInit( &( res -> dst_types ), 0, 3 );
        VectorInit( &( res -> pairs ), 0, 6 );
        res -> type_cast = NULL;
    }
    return res;
}

/* destroys a matcher-column */
static void CC matcher_destroy_col( void* node, void* data ) {
    p_mcol col = ( p_mcol )node;
    if ( NULL == col ) return;
    if ( NULL != col -> name ) {
        free( col -> name );
    }
    VectorWhack( &( col -> src_types ), matcher_destroy_type, NULL );
    VectorWhack( &( col -> dst_types) , matcher_destroy_type, NULL );
    VectorWhack( &( col -> pairs ), matcher_destroy_pair, NULL );
    free( col );
}

/* initializes the matcher */
rc_t matcher_init( matcher** self ) {
    if ( NULL == self ) {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcSelf, rcNull );
    }
    (*self) = calloc( 1, sizeof( matcher ) );
    if ( NULL == *self ) {
        return RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    }
    VectorInit( &( ( *self ) -> mcols ), 0, 5 );
    return 0;
}

/* destroys the matcher */
rc_t matcher_destroy( matcher* self ) {
    if ( NULL == self ) {
        return RC( rcVDB, rcNoTarg, rcDestroying, rcSelf, rcNull );
    }
    VectorWhack( &( self -> mcols ), matcher_destroy_col, NULL );
    free( self );
    return 0;
}

static p_mcol matcher_find_col( const matcher* self, const char *name ) {
    p_mcol res = NULL;
    uint32_t idx, count;
    count =  VectorLength( &( self -> mcols ) );
    for ( idx = 0; idx < count && NULL == res; ++idx ) {
        p_mcol col = ( p_mcol ) VectorGet( &( self -> mcols ), idx );
        if ( NULL != col ) {
            if ( 0 == nlt_strcmp( col -> name, name ) ) {
                res = col;
            }
        }
    }
    return res;
}

static char * matcher_get_col_cast( const p_mcol col, const char *s_type ) {
    char * res;
    size_t idx;
    uint32_t len = string_measure ( col -> name, NULL ) + 4;
    len += string_measure ( s_type, NULL );
    res = malloc( len );
    if ( NULL == res ) return res;
    res[ 0 ] = '(';
    idx = string_copy_measure ( &( res[ 1 ] ), len - 1, s_type );
    res[ idx + 1 ] = ')';
    string_copy_measure ( &( res[ idx + 2 ] ), len - ( idx + 2 ), col -> name );
    return res;
}

/* makes a typecast-string for the src/dest-table by column-name */
static rc_t matcher_get_cast( const matcher* self, const char *name, 
                              const bool src, char **cast ) {
    p_mcol col;
    if ( NULL == self ) {
        return RC( rcVDB, rcNoTarg, rcSearching, rcSelf, rcNull );
    }
    if ( NULL == cast || NULL == name ) {
        return RC( rcVDB, rcNoTarg, rcSearching, rcParam, rcNull );
    }
    *cast = NULL;
    col = matcher_find_col( self, name );
    if ( NULL == col ) return 0;

    if ( NULL == col -> type_cast ) {
        /* if the column has no match, just use the undecorated column-name*/
        if ( src ) {
            *cast = string_dup_measure ( col -> name, NULL );
        } else {
            if ( col -> to_copy && !( col -> excluded ) )
                *cast = string_dup_measure ( col -> name, NULL );
        }
    } else {
        /* if the column has a match, construct the type-cast*/
        if ( src ) {
            *cast = matcher_get_col_cast( col, col -> type_cast -> src -> name );
        } else {
            if ( col -> to_copy && !( col -> excluded ) ) {
                *cast = matcher_get_col_cast( col, col -> type_cast -> dst -> name );
            }
        }
    }
    return 0;
}

/* makes a typecast-string for the source-table by column-name */
rc_t matcher_src_cast( const matcher* self, const char *name, char **cast ) {
    return matcher_get_cast( self, name, true, cast );
}

/* makes a typecast-string for the destination-table by column-name */
rc_t matcher_dst_cast( const matcher* self, const char *name, char **cast ) {
    return matcher_get_cast( self, name, false, cast );
}

static bool match_type_with_id_vector( const VSchema * s,
                const VTypedecl * td, const Vector * id_vector ) {
    bool res = false;
    uint32_t idx, len;

    len = VectorLength( id_vector );
    for ( idx = 0;  idx < len && !res; ++idx ) {
        uint32_t *id = (uint32_t *) VectorGet ( id_vector, idx );
        if ( NULL != id ) {
            VTypedecl cast;
            uint32_t distance;
            res = VTypedeclToType ( td, s, *id, &cast, &distance );
        }
    }
    return res;
}

/* checks if a column with the given name has at least one type
   that is also in the given typelist... */
bool matcher_src_has_type( const matcher* self, const VSchema * s,
                           const char *name, const Vector *id_vector ) {
    bool res = false;
    p_mcol col;
    VTypedecl td;

    if ( NULL == self || NULL == s || NULL == name || NULL == id_vector ) {
        return res;
    }
    col = matcher_find_col( self, name );
    if ( NULL == col ) return res; /* column not found */
    if ( NULL == col -> type_cast ) return res; /* column has no typecast */ 

    /* we use the destination-type-cast */
    if ( 0 == VSchemaResolveTypedecl ( s, &td, "%s", col -> type_cast -> dst -> name ) ) {
        res = match_type_with_id_vector( s, &td, id_vector );
    }
    return res;
}

static void matcher_report_types( const char * s, const Vector *v ) {
    uint32_t idx, len;
    len = VectorLength( v );
    if ( len > 0 ) {
        KOutMsg( "%s: ", s );
        for ( idx = 0;  idx < len; ++idx ) {
            p_mtype item = (p_mtype) VectorGet ( v, idx );
            if ( NULL != item ) {
                KOutMsg( "[ %s ] ", item->name );
            }
        }
        KOutMsg( "\n" );
    }
}

static void matcher_report_pair( const p_mpair pair ) {
    if ( NULL == pair -> src || NULL == pair -> dst ) {
        return;
    }
    if ( 0 == pair -> compatible ) {
        KOutMsg( "[%s](l=%u/o=%u/d=%u) --> [%s] (c) dist=%u\n", 
                  pair -> src -> name, 
                  pair -> src -> lossy_score, 
                  pair -> src -> order,
                  pair -> src -> dflt,
                  pair -> dst -> name,
                  pair -> distance );
    } else {
        KOutMsg( "[%s](l=%u/o=%u/d=%u) --> [%s] dist=%u\n",
                  pair -> src -> name,
                  pair -> src -> lossy_score, 
                  pair -> src -> order,
                  pair -> src -> dflt,
                  pair -> dst -> name,
                  pair -> distance );
    }
}

static void matcher_report_pairs( const Vector *v ) {
    uint32_t idx, len;
    len = VectorLength( v );
    for ( idx = 0;  idx < len; ++idx ) {
       matcher_report_pair( ( p_mpair ) VectorGet ( v, idx ) );
    }
}

static void matcher_report_col( const p_mcol item ) {
    KOutMsg( "----------------------------------\n" );
    if ( item -> to_copy ) {
        KOutMsg( "col: %s (c)\n", item -> name );
    } else {
        KOutMsg( "col: %s\n", item -> name );
    }
    matcher_report_types( " src", &( item -> src_types ) );
    matcher_report_types( " dst", &( item -> dst_types ) );
    matcher_report_pairs( &( item -> pairs ) );
    KOutMsg( "\n" );
}

rc_t matcher_report( matcher* self, const bool only_copy_columns ) {
    if ( NULL == self ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    } else {
        uint32_t idx, len;
        len = VectorLength( &( self -> mcols ) );
        for ( idx = 0; idx < len; ++idx ) {
            p_mcol item = ( p_mcol ) VectorGet ( &( self -> mcols ), idx );
            if ( NULL != item ) {
                if ( only_copy_columns ) {
                    if ( item -> to_copy ) {
                        matcher_report_col( item );
                    }
                } else {
                    matcher_report_col( item );
                }
            }
        }
    }
    return 0;
}

static rc_t matcher_append_type( const char *name, const bool dflt,
                                 const uint32_t order,
                                 const VSchema *schema, Vector *v ) {
    rc_t rc = 0;
    p_mtype t = matcher_init_type( name, dflt, order );
    if ( NULL == t ) {
        rc = RC( rcExe, rcNoTarg, rcResolving, rcMemory, rcExhausted );
    }
    if ( 0 == rc ) {
        rc = VectorAppend( v, NULL, t );
        if ( 0 == rc ) {
            rc = VSchemaResolveTypedecl( schema, &( t -> type_decl ), "%s", name );
            if ( 0 == rc ) {
                rc = VSchemaDescribeTypedecl( schema, &( t -> type_desc ), &( t -> type_decl ) );
            }
        }
    }
    return rc;
}

static rc_t matcher_read_col_src_types( p_mcol col, 
        const KNamelist *names, const uint32_t dflt_idx, const VSchema *schema ) {
    uint32_t count;
    rc_t rc = KNamelistCount( names, &count );
    if ( 0 == rc ) {
        uint32_t idx;
        for ( idx = 0; idx < count && 0 == rc; ++idx ) {
            const char *name;
            rc = KNamelistGet( names, idx, &name );
            if ( 0 == rc ) {
                rc = matcher_append_type( name, ( idx == dflt_idx ), idx,
                                          schema, &( col -> src_types ) );
            }
        }
    }
    return rc;
}

static rc_t matcher_read_src_types( matcher* self, const VTable *table,
                                    const VSchema *schema ) {
    rc_t rc = 0;
    uint32_t idx, len;
    if ( NULL == self ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    }
    if ( NULL == table || NULL == schema ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    }
    len = VectorLength( &(self->mcols) );
    for ( idx = 0;  idx < len && 0 == rc; ++idx ) {
        p_mcol item = ( p_mcol ) VectorGet ( &( self -> mcols ), idx );
        if ( NULL != item ) {
            uint32_t dflt_idx;
            KNamelist *names;
            rc = VTableListReadableDatatypes( table, item -> name, &dflt_idx, &names );
            if ( 0 == rc ) {
                rc = matcher_read_col_src_types( item, names, dflt_idx, schema );
                KNamelistRelease( names );
            }
        }
    }
    return rc;
}

static rc_t matcher_read_dst_types( matcher* self, const VTable *table,
                             const VSchema *schema ) {
    rc_t rc = 0;
    uint32_t idx, len;
    if ( NULL == self ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    }
    if ( NULL == table || NULL == schema ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    }
    len = VectorLength( &(self->mcols) );
    for ( idx = 0;  idx < len && 0 == rc; ++idx ) {
        p_mcol item = ( p_mcol ) VectorGet ( &( self -> mcols ), idx );
        if ( NULL != item ) {
            KNamelist *names;
            rc = VTableListWritableDatatypes ( ( VTable* )table, item -> name, &names );
            if ( 0 == rc ) {
                uint32_t type_count;
                rc = KNamelistCount( names, &type_count );
                if ( 0 == rc && type_count > 0 ) {
                    uint32_t type_idx;
                    item -> to_copy = true; /* !!! this column has to be copied */
                    for ( type_idx = 0; type_idx < type_count && 0 == rc; ++type_idx ) {
                        const char *name;
                        rc = KNamelistGet( names, type_idx, &name );
                        if ( 0 == rc ) {
                            rc = matcher_append_type( name, false, idx,
                                                      schema, &( item -> dst_types ) );
                        }
                    }
                }
                KNamelistRelease( names );
            }
        }
    }
    return rc;
}

static rc_t matcher_make_column_matrix( p_mcol col ) {
    rc_t rc = 0;
    uint32_t src_idx, src_len;
    src_len = VectorLength( &( col -> src_types ) );
    for ( src_idx = 0;  src_idx < src_len && 0 == rc; ++src_idx ) {
        p_mtype src_type = ( p_mtype ) VectorGet ( &( col -> src_types ), src_idx );
        if ( NULL != src_type ) {
            uint32_t dst_idx, dst_len;
            dst_len = VectorLength( &( col -> dst_types ) );
            for ( dst_idx = 0;  dst_idx < dst_len && 0 == rc; ++dst_idx ) {
                p_mtype dst_type = ( p_mtype ) VectorGet ( &( col -> dst_types ), dst_idx );
                if ( NULL != dst_type ) {
                    p_mpair pair = matcher_init_pair( src_type, dst_type );
                    if ( NULL != pair ) {
                        rc = VectorAppend( &( col -> pairs ), NULL, pair );
                    } else {
                        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
                    }
                }
            }
        }
    }
    return rc;
}

static rc_t matcher_make_type_matrix( matcher* self ) {
    rc_t rc = 0;
    uint32_t idx, len;
    if ( NULL == self ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    }
    len = VectorLength( &( self -> mcols ) );
    for ( idx = 0;  idx < len && 0 == rc; ++idx ) {
        p_mcol col = ( p_mcol ) VectorGet ( &( self -> mcols ), idx );
        if ( NULL != col ) {
            if ( col -> to_copy ) {
                rc = matcher_make_column_matrix( col );
            }
        }
    }
    return rc;
}

static int64_t CC matcher_match_cb( const void ** p1, const void ** p2, void *data ) {
    int64_t res = 0;
    const p_mpair pair_1 = ( p_mpair )( *p1 );
    const p_mpair pair_2 = ( p_mpair )( *p2 );
    if ( NULL == pair_1 || NULL == pair_2 ) {
        return res;
    }
    /* first we order by compatibility */
    res = ( int64_t )( pair_1 -> compatible ) - ( int64_t )( pair_2 -> compatible );
    if ( 0 == res ) {
        /* second we order by lossy-ness, lowest value first */
        res = ( pair_1 -> src -> lossy_score ) - ( pair_2 -> src -> lossy_score );
        if ( 0 == res ) {
            /* if the lossy-ness is the same, we order by distance */
            res = ( int64_t )( pair_1 -> distance ) - ( int64_t )( pair_2 -> distance );
            if ( 0 == res ) {
                /* if the distance is the same, we order by default-value */
                res = ( int64_t )( pair_1 -> src -> dflt ) - ( int64_t )( pair_2 -> src -> dflt );
                if ( 0 == res ) {
                    /* if there is not default-value, we use the org. order */
                    res = ( int64_t )( pair_1 -> src -> order ) - ( int64_t )( pair_2 -> src -> order );
                }
            }
        }
    }
    return res;
}

static void CC matcher_enter_type_score_cb( void * item, void * data ) {
    p_mtype type = ( p_mtype )item;
    const KConfig *cfg = ( const KConfig * ) data;
    if ( NULL != type && NULL != cfg ) {
        type -> lossy_score = helper_rd_type_score( cfg, type -> name );
    }
}

static void CC matcher_measure_dist_cb( void * item, void * data ) {
    p_mpair pair = ( p_mpair )item;
    const VSchema *schema = ( const VSchema * )data;
    if ( NULL != pair && NULL != schema ) {
        bool compatible = VTypedeclCommonAncestor ( 
                &( pair -> src -> type_decl ),
                schema,
                &( pair -> dst -> type_decl ),
                NULL,
                &( pair -> distance ) );
        pair -> compatible = ( compatible ? 0 : 1 );
    }
}

static void matcher_match_column( p_mcol col, const VSchema *schema, const KConfig *cfg ) {
    uint32_t pair_count = VectorLength( &( col -> pairs ) );
    col -> type_cast = NULL;
    if ( col -> excluded ) return;
    /* call VTypedeclCommonAncestor for every type-pair */
    VectorForEach ( &( col -> pairs ), false, matcher_measure_dist_cb, ( void* )schema );
    /* if we have more than one pair left... */
    if ( pair_count > 1 ) {
        /* enter the lossy-ness into the src-types... */
        VectorForEach ( &( col -> src_types ), false, matcher_enter_type_score_cb, ( void* )cfg );
            /* reorder the remaining pair's by:
           compatibility, lossy-ness, distance, default, order */
        VectorReorder ( &( col -> pairs ), matcher_match_cb, NULL );
    }
    /* pick the winner = first item in the vector */
    if ( pair_count > 0 ) {
        col -> type_cast = ( p_mpair )VectorFirst ( &( col -> pairs ) );
        /* if the winner is not a compatible pair, we have no cast ! */
        if ( 0 != col -> type_cast -> compatible ) {
            col -> type_cast = NULL;
        }
    }
}

static rc_t matcher_match_matrix( matcher* self, const VSchema *schema, const KConfig *cfg ) {
    rc_t rc = 0;
    uint32_t idx, len;
    if ( NULL == self ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    }
    if ( NULL == schema || NULL == cfg ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    }
    len = VectorLength( &( self -> mcols ) );
    for ( idx = 0;  idx < len; ++idx ) {
        p_mcol col = ( p_mcol ) VectorGet ( &( self -> mcols ), idx );
        if ( NULL != col ) {
            if ( col -> to_copy ) {
                matcher_match_column( col, schema, cfg );
            }
        }
    }
    return rc;
}

static rc_t matcher_build_column_vector( matcher* self, const char * columns ) {
    const VNamelist *list;
    uint32_t count, idx;
    rc_t rc = nlt_make_VNamelist_from_string( &list, columns );
    if ( 0 != rc ) return rc;
    rc = VNameListCount( list, &count );
    for ( idx = 0; idx < count && 0 == rc; ++idx ) {
        const char *s;
        rc = VNameListGet( list, idx, &s );
        if ( 0 == rc ) {
            p_mcol new_col = matcher_make_col( s );
            if ( NULL == new_col ) {
                rc = RC( rcExe, rcNoTarg, rcResolving, rcMemory, rcExhausted );
            }
            if ( 0 == rc ) {
                rc = VectorAppend( &( self -> mcols ), NULL, new_col );
            }
        }
    }
    VNamelistRelease( list );
    return rc;
}

static rc_t matcher_exclude_columns( matcher* self, const char * columns ) {
    rc_t rc = 0;
    if ( NULL != columns ) {
        const VNamelist *list;
        uint32_t len, idx;
        rc = nlt_make_VNamelist_from_string( &list, columns );
        len = VectorLength( &( self -> mcols ) );
        for ( idx = 0;  idx < len; ++idx ) {
            p_mcol col = ( p_mcol ) VectorGet ( &( self -> mcols ), idx );
            if ( NULL != col ) {
                col -> excluded = nlt_is_name_in_VNamelist( list, col -> name );
            }
        }
        VNamelistRelease( list );
    }
    return rc;    
}

rc_t matcher_execute( matcher* self, const p_matcher_input in ) {
    VSchema * dflt_schema;
    const VTable * src_table;
    rc_t rc;

    if ( NULL == self ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcSelf, rcNull );
    }
    if ( NULL == in -> manager  || NULL == in -> add_schemas || 
         NULL == in -> cfg      || NULL == in -> columns     || 
         NULL == in -> src_path || NULL == in -> dst_path    ||
         NULL == in -> dst_tabname ) {
        return RC( rcExe, rcNoTarg, rcResolving, rcParam, rcNull );
    }
    rc = matcher_build_column_vector( self, in -> columns );
    if ( 0 != rc ) return rc;

    rc = matcher_exclude_columns( self, in -> excluded_columns );
    if ( 0 != rc ) return rc;

    rc = helper_parse_schema( in -> manager, &dflt_schema, in -> add_schemas );
    if ( 0 != rc ) return rc;

    rc = VDBManagerOpenTableRead( in -> manager, &src_table, dflt_schema, "%s", in -> src_path );
    if ( 0 == rc ) {
        const VSchema * src_schema;
        rc = VTableOpenSchema ( src_table, &src_schema );
        if ( 0 == rc ) {
            rc = matcher_read_src_types( self, src_table, src_schema );
            if ( 0 == rc ) {
                if ( NULL != in -> legacy_schema ) {
                    rc = VSchemaParseFile ( dflt_schema, "%s", in -> legacy_schema );
                }
                if ( 0 == rc ) {
                    VTable * dst_table;
                    KCreateMode cmode = kcmParents;
                    const VSchema * dst_schema = src_schema;

                    if ( NULL != in -> legacy_schema ) {
                        dst_schema = dflt_schema;
                    }

                    if ( in -> force_unlock ) {
                        VDBManagerUnlock ( in -> manager, "%s", in -> dst_path );
                    }

                    if ( in -> force_kcmInit ) {
                        cmode |= kcmInit;
                    } else {
                        cmode |= kcmCreate;
                    }
                    rc = VDBManagerCreateTable( in -> manager,
                                                &dst_table, 
                                                dst_schema,
                                                in -> dst_tabname,
                                                cmode,
                                                "%s", in -> dst_path );
                    if ( 0 == rc ) {
                        rc = matcher_read_dst_types( self, dst_table, dst_schema );
                        if ( 0 == rc ) {
                            rc = matcher_make_type_matrix( self );
                            if ( 0 == rc ) {
                                rc = matcher_match_matrix( self, src_schema, in -> cfg );
                            }
                        }
                        VTableRelease( dst_table );
                        if ( !( in -> force_kcmInit ) ) {
                            KDirectoryRemove ( in -> dir, true, "%s", in -> dst_path );
                        }
                    }
                }
            }
            VSchemaRelease( src_schema );
        }
        VTableRelease( src_table );
    }
    VSchemaRelease( dflt_schema );
    return rc;
}


rc_t matcher_db_execute( matcher* self, const VTable * src_tab, VTable * dst_tab,
                         const VSchema * schema, const char * columns, 
                         const char * kfg_path ) {
    KConfig * cfg;
    rc_t rc = helper_make_config_mgr( &cfg, kfg_path );
    if ( 0 == rc ) {
        rc = matcher_build_column_vector( self, columns );
        if ( 0 != rc ) return rc;
        rc = matcher_read_src_types( self, src_tab, schema );
        if ( 0 == rc ) {
            rc = matcher_read_dst_types( self, dst_tab, schema );
            if ( 0 == rc ) {
                rc = matcher_make_type_matrix( self );
                if ( 0 == rc ) {
                    rc = matcher_match_matrix( self, schema, cfg );
                }
            }
        }
        KConfigRelease( cfg );
    }
    return rc;
}
