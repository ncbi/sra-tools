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
#include <klib/log.h>
#include <klib/out.h>
#include <klib/container.h>
#include <klib/text.h>
#include <kapp/main.h>

#include <sra/sradb.h>
#include <sra/types.h>
#include <sra/fastq.h>

#include <os-native.h>
#include <sysalloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef WINDOWS
#include <strings.h> /* strncasecmp */
#endif
#include <ctype.h>
#include <errno.h>
#include <strtol.h>

#include "core.h"
#include "debug.h"

#define DATABUFFERINITSIZE 10240

typedef struct TAlignedRegion_struct
{
    const char* name;
    uint32_t name_len;
    /* 1-based, 0 - means not set */
    uint64_t from;
    uint64_t to;
} TAlignedRegion;


typedef struct TMatepairDistance_struct
{
    uint64_t from;
    uint64_t to;
} TMatepairDistance;


struct FastqArgs_struct
{
    bool is_platform_cs_native;

    int maxReads;
    bool skipTechnical;

    uint32_t minReadLen;
    bool applyClip;
    bool dumpOrigFmt;
    bool dumpBase;
    bool dumpCs;
    bool readIds;
    bool SuppressQualForCSKey;      /* added Jan 15th 2014 ( a new fastq-variation! ) */
    int offset;
    bool qual_filter;
    bool qual_filter1;
    const char* b_deffmt;
    SLList* b_defline;
    const char* q_deffmt;
    SLList* q_defline;
    const char *desiredCsKey;
    bool split_files;
    bool split_3;
    bool split_spot;
    uint64_t fasta;
    const char* file_extension;
    bool aligned;
    bool unaligned;
    TAlignedRegion* alregion;
    uint32_t alregion_qty;
    bool mp_dist_unknown;
    TMatepairDistance* mp_dist;
    uint32_t mp_dist_qty;
} FastqArgs;


typedef enum DefNodeType_enum
{
    DefNode_Unknown = 0,
    DefNode_Text = 1,
    DefNode_Optional,
    DefNode_Accession,
    DefNode_SpotId,
    DefNode_SpotName,
    DefNode_SpotGroup,
    DefNode_SpotLen,
    DefNode_ReadId,
    DefNode_ReadName,
    DefNode_ReadLen,
    DefNode_Last
} DefNodeType;


typedef struct DefNode_struct
{
    SLNode node;
    DefNodeType type;
    union
    {
        SLList* optional;
        char* text;
    } data;
} DefNode;


typedef struct DeflineData_struct
{
    rc_t rc;
    bool optional;
    union
    {
        spotid_t* id;
        struct
        {
            const char* s;
            size_t sz;
        } str;
        uint32_t* u32;
    } values[ DefNode_Last ];
    char* buf;
    size_t buf_sz;
    size_t* writ;
} DeflineData;


static bool CC Defline_Builder( SLNode *node, void *data )
{
    DefNode* n = ( DefNode* )node;
    DeflineData* d = ( DeflineData* )data;
    char s[ 256 ];
    size_t w;
    int x = 0;

    s[ 0 ] = '\0';
    switch ( n->type )
    {
        case DefNode_Optional :
            d->optional = true;
            w = *d->writ;
            *d->writ = 0;
            SLListDoUntil( n->data.optional, Defline_Builder, data );
            x = ( *d->writ == 0 ) ? 0 : 1;
            d->optional = false;
            *d->writ = w;
            if ( x > 0 )
            {
                SLListDoUntil( n->data.optional, Defline_Builder, data );
            }
            break;

        case DefNode_Text :
            if ( d->optional )
            {
                break;
            }
            d->values[n->type].str.s = n->data.text;
            d->values[n->type].str.sz = strlen( n->data.text );

        case DefNode_Accession :
        case DefNode_SpotName :
        case DefNode_SpotGroup :
        case DefNode_ReadName :
            if ( d->values[n->type].str.s != NULL )
            {
                x = d->values[ n->type ].str.sz;
                if ( x < sizeof( s ) )
                {
                    strncpy( s, d->values[ n->type ].str.s, x );
                    s[ x ] = '\0';
                    *d->writ = *d->writ + x;
                }
                else
                {
                    d->rc = RC( rcExe, rcNamelist, rcExecuting, rcBuffer, rcInsufficient );
                }
            }
            break;

        case DefNode_SpotId :
            if ( d->values[ n->type ].id != NULL &&
                ( !d->optional || *d->values[ n->type ].id > 0 ) )
            {
                x = snprintf( s, sizeof( s ), "%lld", (long long int)*d->values[ n->type ].id );
                if ( x < 0 || x >= sizeof( s ) )
                {
                    d->rc = RC( rcExe, rcNamelist, rcExecuting, rcBuffer, rcInsufficient );
                }
                else
                {
                    *d->writ = *d->writ + x;
                }
            }
            break;

        case DefNode_ReadId :
        case DefNode_SpotLen :
        case DefNode_ReadLen :
            if ( d->values[ n->type] .u32 != NULL &&
                 ( !d->optional || *d->values[ n->type ].u32 > 0 ) )
            {
                x = snprintf( s, sizeof( s ), "%u", *d->values[ n->type ].u32 );
                if ( x < 0 || x >= sizeof( s ) )
                {
                    d->rc = RC( rcExe, rcNamelist, rcExecuting, rcBuffer, rcInsufficient );
                }
                else
                {
                    *d->writ = *d->writ + x;
                }
            }
            break;

        default:
            d->rc = RC( rcExe, rcNamelist, rcExecuting, rcId, rcInvalid );
    } /* switch */

    if ( d->rc == 0 && !d->optional && n->type != DefNode_Optional )
    {
        if ( *d->writ < d->buf_sz )
        {
            strcat( d->buf, s );
        }
        else
        {
            d->rc = RC( rcExe, rcNamelist, rcExecuting, rcBuffer, rcInsufficient );
        }
    }
    return d->rc != 0;
}


static rc_t Defline_Bind( DeflineData* data, const char* accession,
            spotid_t* spotId, const char* spot_name, size_t spotname_sz,
            const char* spot_group, size_t sgrp_sz, uint32_t* spot_len,
            uint32_t* readId, const char* read_name, INSDC_coord_len rlabel_sz,
            INSDC_coord_len* read_len )
{
    if ( data == NULL )
    {
        return RC( rcExe, rcNamelist, rcExecuting, rcMemory, rcInsufficient );
    }
    data->values[ DefNode_Unknown ].str.s = NULL;
    data->values[ DefNode_Text ].str.s = NULL;
    data->values[ DefNode_Optional ].str.s = NULL;
    data->values[ DefNode_Accession ].str.s = accession;
    data->values[ DefNode_Accession ].str.sz = strlen( accession );
    data->values[ DefNode_SpotId ].id = spotId;
    data->values[ DefNode_SpotName ].str.s = spot_name;
    data->values[ DefNode_SpotName ].str.sz = spotname_sz;
    data->values[ DefNode_SpotGroup ].str.s = spot_group;
    data->values[ DefNode_SpotGroup ].str.sz = sgrp_sz;
    data->values[ DefNode_SpotLen ].u32 = spot_len;
    data->values[ DefNode_ReadId ].u32 = readId;
    data->values[ DefNode_ReadName ].str.s = read_name;
    data->values[ DefNode_ReadName ].str.sz = rlabel_sz;
    data->values[ DefNode_ReadLen ].u32 = read_len;
    return 0;
}


static rc_t Defline_Build( const SLList* def, DeflineData* data, char* buf,
                           size_t buf_sz, size_t* writ )
{
    if ( data == NULL )
    {
        return RC( rcExe, rcNamelist, rcExecuting, rcMemory, rcInsufficient );
    }

    data->rc = 0;
    data->optional = false;
    data->buf = buf;
    data->buf_sz = buf_sz;
    data->writ = writ;

    data->buf[ 0 ] = '\0';
    *data->writ = 0;

    SLListDoUntil( def, Defline_Builder, data );
    return data->rc;
}


static rc_t DeflineNode_Add( SLList* list, DefNode** node,
                            DefNodeType type, const char* text, size_t text_sz )
{
    rc_t rc = 0;

    *node = calloc( 1, sizeof( **node ) );
    if ( *node == NULL )
    {
        rc = RC( rcExe, rcNamelist, rcConstructing, rcMemory, rcExhausted );
    }
    else if ( type == DefNode_Text && ( text == NULL || text_sz == 0 ) )
    {
        rc = RC( rcExe, rcNamelist, rcConstructing, rcParam, rcInvalid );
    }
    else
    {
        ( *node )->type = type;
        if ( type == DefNode_Text )
        {
            ( *node )->data.text = string_dup( text, text_sz );
            if ( ( *node )->data.text == NULL )
            {
                rc = RC( rcExe, rcNamelist, rcConstructing, rcMemory, rcExhausted );
                free( *node );
            }
        }
        else if ( type == DefNode_Optional )
        {
            ( *node )->data.optional = malloc( sizeof( SLList ) );
            if ( ( *node )->data.optional == NULL )
            {
                rc = RC( rcExe, rcNamelist, rcConstructing, rcMemory, rcExhausted );
                free( *node );
            }
            else
            {
                SLListInit( ( *node )->data.optional );
            }
        }
        if ( rc == 0 )
        {
            SLListPushTail( list, &( *node )->node );
        }
    }
    return rc;
}


static void Defline_Release( SLList* list );


static void CC DeflineNode_Whack( SLNode* node, void* data )
{
    if ( node != NULL )
    {
        DefNode* n = (DefNode*)node;
        if ( n->type == DefNode_Text )
        {
            free( n->data.text );
        }
        else if ( n->type == DefNode_Optional )
        {
            Defline_Release( n->data.optional );
        }
        free( node );
    }
}


static void Defline_Release( SLList* list )
{
    if ( list != NULL )
    {
        SLListForEach( list, DeflineNode_Whack, NULL );
        free( list );
    }
}


#if _DEBUGGING
static void CC Defline_Dump( SLNode* node, void* data )
{
    DefNode* n = ( DefNode* )node;
    const char* s = NULL, *t;
    if ( n->type == DefNode_Text )
    {
        s = n->data.text;
    }
    switch ( n->type )
    {
        case DefNode_Unknown :      t = "Unknown";   break;
        case DefNode_Text :         t = "Text";      break;
        case DefNode_Optional :     t = "Optional";  break;
        case DefNode_Accession :    t = "Accession"; break;
        case DefNode_SpotId :       t = "SpotId";    break;
        case DefNode_SpotName :     t = "SpotName";  break;
        case DefNode_SpotGroup :    t = "SpotGroup"; break;
        case DefNode_SpotLen :      t = "SpotLen";   break;
        case DefNode_ReadId :       t = "ReadId";    break;
        case DefNode_ReadName :     t = "ReadName";  break;
        case DefNode_ReadLen :      t = "ReadLen";   break;
        default :  t = "ERROR";
    }

    SRA_DUMP_DBG( 3, ( "%s type %s", data, t ) );
    if ( s )
    {
        SRA_DUMP_DBG( 3, ( ": '%s'", s ) );
    }
    SRA_DUMP_DBG( 3, ( "\n" ) );
    if ( n->type == DefNode_Optional )
    {
        SLListForEach( n->data.optional, Defline_Dump, "+-->" );
    }
}
#endif


static rc_t Defline_Parse( SLList** def, const char* line )
{
    rc_t rc = 0;
    size_t i, sz, text = 0, opt_vars = 0;
    DefNode* node;
    SLList* list = NULL;

    if ( def == NULL || line == NULL )
    {
        return RC( rcExe, rcNamelist, rcConstructing, rcParam, rcInvalid );
    }
    *def = malloc( sizeof( SLList ) );
    if ( *def  == NULL )
    {
        return RC( rcExe, rcNamelist, rcConstructing, rcMemory, rcExhausted );
    }
    sz = strlen( line );
    list = *def;
    SLListInit( list );

    for ( i = 0; rc == 0 && i < sz; i++ )
    {
        if ( line[ i ] == '[' )
        {
            if ( ( i + 1 ) < sz && line[ i + 1 ] == '[' )
            {
                i++;
            }
            else
            {
                if ( list != *def )
                {
                    rc = RC( rcExe, rcNamelist, rcConstructing, rcFormat, rcIncorrect );
                }
                else if ( i > text )
                {
                    rc = DeflineNode_Add( list, &node, DefNode_Text, &line[text], i - text );
                }
                if ( rc == 0 )
                {
                    rc = DeflineNode_Add( list, &node, DefNode_Optional, NULL, 0 );
                    if ( rc == 0 )
                    {
                        opt_vars = 0;
                        list = node->data.optional;
                        text = i + 1;
                    }
                }
            }
        }
        else if ( line[ i ] == ']' )
        {
            if ( ( i + 1 ) < sz && line[ i + 1 ] == ']' )
            {
                i++;
            }
            else
            {
                if ( list == *def )
                {
                    rc = RC( rcExe, rcNamelist, rcConstructing, rcFormat, rcIncorrect );
                }
                else
                {
                    if ( opt_vars < 1 )
                    {
                        rc = RC( rcExe, rcNamelist, rcConstructing, rcConstraint, rcEmpty );
                    }
                    else if ( i > text )
                    {
                        rc = DeflineNode_Add( list, &node, DefNode_Text, &line[text], i - text );
                    }
                    list = *def;
                    text = i + 1;
                }
            }
        }
        else if ( line[ i ] == '$' )
        {
            if ( ( i + 1 ) < sz && line[ i + 1 ] == '$' )
            {
                i++;
            }
            else
            {
                DefNodeType type = DefNode_Unknown;
                switch ( line[ ++i] )
                {
                    case 'a' :
                        switch( line[ ++i ] )
                        {
                            case 'c': type = DefNode_Accession; break;
                        }
                        break;

                    case 's' :
                        switch ( line[ ++i ] )
                        {
                            case 'i': type = DefNode_SpotId; break;
                            case 'n': type = DefNode_SpotName; break;
                            case 'g': type = DefNode_SpotGroup; break;
                            case 'l': type = DefNode_SpotLen; break;
                        }
                        break;
                    case 'r' :
                        switch( line[ ++i ] )
                        {
                            case 'i': type = DefNode_ReadId; break;
                            case 'n': type = DefNode_ReadName; break;
                            case 'l': type = DefNode_ReadLen; break;
                        }
                        break;
                }
                if ( type == DefNode_Unknown )
                {
                    rc = RC( rcExe, rcNamelist, rcConstructing, rcName, rcUnrecognized );
                }
                else
                {
                    if ( ( i - 2 ) > text )
                    {
                        rc = DeflineNode_Add( list, &node, DefNode_Text, &line[ text ], i - text - 2 );
                    }
                    if ( rc == 0 )
                    {
                        rc = DeflineNode_Add( list, &node, type, NULL, 0 );
                        if ( rc == 0 )
                        {
                            opt_vars++;
                            text = i + 1;
                        }
                    }
                }
            }
        }
    }
    if ( rc == 0 )
    {
        if ( list != *def )
        {
            rc = RC( rcExe, rcNamelist, rcConstructing, rcFormat, rcInvalid );
        }
        else if ( i > text )
        {
            rc = DeflineNode_Add( list, &node, DefNode_Text, &line[ text ], i - text );
        }
    }
    if ( rc != 0 )
    {
        i = i < sz ? i : sz;
        PLOGERR( klogErr, ( klogErr, rc, "$(l1) -->$(c)<-- $(l2)",
                 "l1=%.*s,c=%c,l2=%.*s", i - 1, line, line[ i - 1 ], sz - i, &line[ i ] ) );
    }
#if _DEBUGGING
    SRA_DUMP_DBG( 3, ( "| defline\n" ) );
    SLListForEach( *def, Defline_Dump, "+->" );
#endif
    return rc;
}

/* ### ALIGNMENT_COUNT based filtering ##################################################### */

typedef struct AlignedFilter_struct
{
    const SRAColumn* col;
    uint64_t rejected_reads;
    bool aligned;
    bool unaligned;
} AlignedFilter;


static rc_t AlignedFilter_GetKey( const SRASplitter* cself, const char** key,
                                  spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    AlignedFilter* self = ( AlignedFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcSRA, rcNode, rcExecuting, rcParam, rcNull );
    }
    else
    {
        *key = "";
        if ( self->col != NULL )
        {
            const uint8_t* ac = NULL;
            bitsz_t o = 0, sz = 0;
            rc = SRAColumnRead( self->col, spot, (const void **)&ac, &o, &sz );
            if ( rc == 0 && sz > 0 )
            {
                sz = sz / 8 / sizeof( *ac );
                for( o = 0; o < sz; o++ )
                {
                    if ( ( self->aligned   && !self->unaligned && ac[ o ] == 0 ) ||
                         ( self->unaligned && !self->aligned   && ac[ o ] != 0 ) )
                    {
                        unset_readmask( readmask, o );
                        self->rejected_reads ++;
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t AlignedFilter_Release( const SRASplitter* cself )
{
    rc_t rc = 0;
    AlignedFilter* self = ( AlignedFilter* )cself;
    if ( self == NULL )
    {
        rc = RC( rcSRA, rcNode, rcExecuting, rcParam, rcNull );
    }
    else
    {
        if ( self->rejected_reads > 0 && !g_legacy_report )
            rc = KOutMsg( "Rejected %lu READS because of aligned/unaligned filter\n", self->rejected_reads );
    }
    return rc;
}

typedef struct AlignedFilterFactory_struct
{
    const SRATable* table;
    const SRAColumn* col;
    bool aligned;
    bool unaligned;
} AlignedFilterFactory;


static rc_t AlignedFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    AlignedFilterFactory* self = ( AlignedFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRATableOpenColumnRead( self->table, &self->col, "ALIGNMENT_COUNT", vdb_uint8_t );
        if ( rc != 0 )
        {
            if ( GetRCState( rc ) == rcNotFound )
            {
                LOGMSG( klogWarn, "Column ALIGNMENT_COUNT was not found, param ignored" );
                rc = 0;
            }
            else if ( GetRCState( rc ) == rcExists )
            {
                rc = 0;
            }
        }
    }
    return rc;
}


static rc_t AlignedFilterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    AlignedFilterFactory* self = ( AlignedFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( AlignedFilter ),
                               AlignedFilter_GetKey, NULL, NULL, AlignedFilter_Release );
        if ( rc == 0 )
        {
            AlignedFilter * filter = ( AlignedFilter * )( * splitter );
            filter->col = self->col;
            filter->aligned = self->aligned;
            filter->unaligned = self->unaligned;
            filter->rejected_reads = 0;
        }
    }
    return rc;
}


static void AlignedFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        AlignedFilterFactory* self = ( AlignedFilterFactory* )cself;
        SRAColumnRelease( self->col );
    }
}


static rc_t AlignedFilterFactory_Make( const SRASplitterFactory** cself,
                    const SRATable* table, bool aligned, bool unaligned )
{
    rc_t rc = 0;
    AlignedFilterFactory* obj = NULL;

    if ( cself == NULL || table == NULL )
    {
        rc = RC( rcSRA, rcType, rcAllocating, rcParam, rcNull );
    }
    else
    {
        SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                 AlignedFilterFactory_Init,
                                 AlignedFilterFactory_NewObj,
                                 AlignedFilterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( AlignedFilterFactory* )*cself;
            obj->table = table;
            obj->aligned = aligned;
            obj->unaligned = unaligned;
        }
    }
    return rc;
}


/* ### alignment region filtering ##################################################### */

typedef struct AlignRegionFilter_struct
{
    const SRAColumn* col_pos;
    const SRAColumn* col_name;
    const SRAColumn* col_seqid;
    const TAlignedRegion* alregion;
    uint64_t rejected_spots;
    uint32_t alregion_qty;
} AlignRegionFilter;


static rc_t AlignRegionFilter_GetKey( const SRASplitter* cself, const char** key,
                                      spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    AlignRegionFilter* self = ( AlignRegionFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcSRA, rcNode, rcExecuting, rcParam, rcNull );
    }
    else
    {
        *key = "";
        reset_readmask( readmask );
        if ( self->col_name || self->col_seqid )
        {
            bool match = false;
            uint32_t i;
            const char* nm, *si;
            bitsz_t o, nm_len = 0, si_len = 0;

            if ( self->col_name )
            {
                rc = SRAColumnRead( self->col_name, spot, (const void **)&nm, &o, &nm_len );
                if ( rc == 0 && nm_len > 0 )
                    nm_len /= 8;
            }
            if ( rc == 0 && self->col_seqid )
            {
                rc = SRAColumnRead( self->col_seqid, spot, (const void **)&si, &o, &si_len );
                if ( rc == 0 && si_len > 0 )
                    si_len /= 8;
            }
            for ( i = 0; rc == 0 && i < self->alregion_qty; i++ )
            {
                if ( ( self->alregion[i].name_len == nm_len &&
                       strncasecmp( self->alregion[ i ].name, nm, nm_len ) == 0 ) ||
                     ( self->alregion[i].name_len == si_len &&
                       strncasecmp(self->alregion[i].name, si, si_len) == 0 ) )
                {
                    if ( self->col_pos )
                    {
                        INSDC_coord_zero* pos;
                        bitsz_t nreads, j;
                        rc = SRAColumnRead( self->col_pos, spot, ( const void ** )&pos, &o, &nreads );
                        if ( rc == 0 && nreads > 0 )
                        {
                            nreads = nreads / sizeof(*pos) / 8;
                            for ( i = 0; !match && i < self->alregion_qty; i++ )
                            {
                                for ( j = 0; j < nreads; j++ )
                                {
                                    if ( self->alregion[i].from <= pos[j] &&
                                         pos[j] <= self->alregion[i].to )
                                    {
                                        match = true;
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        match = true;
                    }
                    break;
                }
            }
            if ( !match )
            {
                clear_readmask( readmask );
                self->rejected_spots ++;
            }
        }
    }
    return rc;
}


static rc_t AlignRegionFilter_Release( const SRASplitter* cself )
{
    rc_t rc = 0;
    AlignRegionFilter* self = ( AlignRegionFilter* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcNode, rcExecuting, rcParam, rcNull );
    }
    else
    {
        if ( self->rejected_spots > 0 && !g_legacy_report )
            rc = KOutMsg( "Rejected %lu SPOTS because of AlignRegionFilter\n", self->rejected_spots );
    }
    return rc;
}


typedef struct AlignRegionFilterFactory_struct
{
    const SRATable* table;
    const SRAColumn* col_pos;
    const SRAColumn* col_name;
    const SRAColumn* col_seqid;
    TAlignedRegion* alregion;
    uint32_t alregion_qty;
} AlignRegionFilterFactory;


static rc_t AlignRegionFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    AlignRegionFilterFactory* self = ( AlignRegionFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {

#define COLNF(v,n,t) (!(rc == 0 && ((rc = SRATableOpenColumnRead(self->table, &v, n, t)) == 0 || \
                       GetRCState(rc) == rcExists)) && GetRCState(rc) == rcNotFound)

        if ( COLNF( self->col_name, "REF_NAME", vdb_ascii_t ) )
        {
            SRAColumnRelease( self->col_name );
            self->col_name = NULL;
            rc = 0;
        }

        if ( COLNF( self->col_seqid, "REF_SEQ_ID", vdb_ascii_t ) )
        {
            SRAColumnRelease( self->col_seqid );
            self->col_seqid = NULL;
            rc = 0;
        }

        if ( self->col_name == NULL && self->col_seqid == NULL )
        {
            LOGMSG( klogWarn, "None of columns REF_NAME or REF_SEQ_ID was found, no region filtering" );
        }
        else
        {
            bool range_used = false;
            uint32_t i;
            /* see if there are any actual ranges set and open col only if yes */
            for ( i = 0; i < self->alregion_qty; i++ )
            {
                if ( self->alregion[ i ].from != 0 || self->alregion[ i ].to != 0 )
                {
                    range_used = true;
                    break;
                }
            }
            if ( range_used )
            {
                if ( COLNF( self->col_pos, "REF_POS", "INSDC:coord:zero" ) )
                {
                    LOGMSG( klogWarn, "Column REF_POS is not found, no region position filtering" );
                    rc = 0;
                }
                else
                {
                    for ( i = 0; i < self->alregion_qty; i++ )
                    {
                        if ( self->alregion[ i ].from > 0 )
                        {
                            self->alregion[ i ].from--;
                        }
                        if ( self->alregion[ i ].to > 0 )
                        {
                            self->alregion[ i ].to--;
                        }
                        else if ( self->alregion[ i ].to == 0 )
                        {
                            self->alregion[ i ].to = ~0;
                        }
                    }
                }
            }
        }
    }
#undef COLNF
    return rc;
}


static rc_t AlignRegionFilterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    AlignRegionFilterFactory* self = ( AlignRegionFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( AlignRegionFilter ),
                               AlignRegionFilter_GetKey, NULL, NULL, AlignRegionFilter_Release );
        if ( rc == 0 )
        {
            AlignRegionFilter * filter = ( AlignRegionFilter * )( * splitter );
            filter->col_pos = self->col_pos;
            filter->col_name = self->col_name;
            filter->col_seqid = self->col_seqid;
            filter->alregion = self->alregion;
            filter->alregion_qty = self->alregion_qty;
            filter->rejected_spots = 0;
        }
    }
    return rc;
}


static void AlignRegionFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        AlignRegionFilterFactory* self = ( AlignRegionFilterFactory* )cself;
        SRAColumnRelease( self->col_pos );
        SRAColumnRelease( self->col_name );
        SRAColumnRelease( self->col_seqid );
    }
}


static rc_t AlignRegionFilterFactory_Make( const SRASplitterFactory** cself,
            const SRATable* table, TAlignedRegion* alregion, uint32_t alregion_qty )
{
    rc_t rc = 0;
    AlignRegionFilterFactory* obj = NULL;

    if ( cself == NULL || table == NULL )
    {
        rc = RC( rcSRA, rcType, rcAllocating, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                      AlignRegionFilterFactory_Init,
                                      AlignRegionFilterFactory_NewObj,
                                      AlignRegionFilterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( AlignRegionFilterFactory* )*cself;
            obj->table = table;
            obj->alregion = alregion;
            obj->alregion_qty = alregion_qty;
        }
    }
    return rc;
}

/* ### alignment meta-pair distance filtering ##################################################### */

typedef struct AlignPairDistanceFilter_struct
{
    const SRAColumn* col_tlen;
    const TMatepairDistance* mp_dist;
    uint64_t rejected_reads;
    bool mp_dist_unknown;
    uint32_t mp_dist_qty;
} AlignPairDistanceFilter;


static rc_t AlignPairDistanceFilter_GetKey( const SRASplitter* cself,
                const char** key, spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    AlignPairDistanceFilter* self = ( AlignPairDistanceFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcSRA, rcNode, rcExecuting, rcParam, rcNull );
    }
    else
    {
        *key = "";
        reset_readmask( readmask );
        if ( self->col_tlen )
        {
            uint32_t i, j;
            bitsz_t o, nreads;
            int32_t* tlen;
            rc = SRAColumnRead( self->col_tlen, spot, (const void **)&tlen, &o, &nreads );
            if ( rc == 0 )
            {
                nreads = nreads / sizeof(*tlen) / 8;
                for ( i = 0; i < self->mp_dist_qty; i++ )
                {
                    for ( j = 0; j < nreads; j++ )
                    {
                        if ( ( tlen[ j ] == 0 && !self->mp_dist_unknown) ||
                             ( tlen[ j ] != 0 &&
                               ( tlen[ j ] < self->mp_dist[ i ].from || self->mp_dist[ i ].to < tlen[ j ] ) ) )
                        {
                            unset_readmask( readmask, j );
                            self->rejected_reads ++;
                        }
                    }
                }
            }
        }
    }
    return rc;
}


static rc_t AlignPairDistanceFilter_Release( const SRASplitter* cself )
{
    rc_t rc = 0;
    AlignPairDistanceFilter* self = ( AlignPairDistanceFilter* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcNode, rcExecuting, rcParam, rcNull );
    }
    else
    {
        if ( self->rejected_reads > 0 && !g_legacy_report )
            rc = KOutMsg( "Rejected %lu READS because of AlignPairDistanceFilter\n", self->rejected_reads );
    }
    return rc;
}

typedef struct AlignPairDistanceFilterFactory_struct
{
    const SRATable* table;
    const SRAColumn* col_tlen;
    bool mp_dist_unknown;
    TMatepairDistance* mp_dist;
    uint32_t mp_dist_qty;
} AlignPairDistanceFilterFactory;


static rc_t AlignPairDistanceFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    AlignPairDistanceFilterFactory* self = ( AlignPairDistanceFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRATableOpenColumnRead( self->table, &self->col_tlen, "TEMPLATE_LEN", "I32" );
        if( !( rc == 0 || GetRCState( rc ) == rcExists ) && GetRCState( rc ) == rcNotFound )
        {
            LOGMSG( klogWarn, "Column TLEN was not found, no distance filtering" );
            self->col_tlen = NULL;
            rc = 0;
        }
        if ( rc == 0 )
        {
            uint32_t i;
            for ( i = 0; i < self->mp_dist_qty; i++ )
            {
                if ( self->mp_dist[ i ].to == 0 )
                {
                    self->mp_dist[ i ].to = ~0;
                }
            }
        }
    }
    return rc;
}


static rc_t AlignPairDistanceFilterFactory_NewObj( const SRASplitterFactory* cself,
                                                   const SRASplitter** splitter )
{
    rc_t rc = 0;
    AlignPairDistanceFilterFactory* self = ( AlignPairDistanceFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcSRA, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( AlignPairDistanceFilter ),
                               AlignPairDistanceFilter_GetKey, NULL, NULL, AlignPairDistanceFilter_Release );
        if ( rc == 0 )
        {
            AlignPairDistanceFilter * filter = ( AlignPairDistanceFilter * )( * splitter );
            filter->col_tlen = self->col_tlen;
            filter->mp_dist_unknown = self->mp_dist_unknown;
            filter->mp_dist = self->mp_dist;
            filter->mp_dist_qty = self->mp_dist_qty;
            filter->rejected_reads = 0;
        }
    }
    return rc;
}


static void AlignPairDistanceFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        AlignPairDistanceFilterFactory* self = ( AlignPairDistanceFilterFactory* )cself;
        SRAColumnRelease( self->col_tlen );
    }
}


static rc_t AlignPairDistanceFilterFactory_Make( const SRASplitterFactory** cself,
        const SRATable* table, bool mp_dist_unknown, TMatepairDistance* mp_dist, uint32_t mp_dist_qty )
{
    rc_t rc = 0;
    AlignPairDistanceFilterFactory* obj = NULL;

    if ( cself == NULL || table == NULL )
    {
        rc = RC( rcSRA, rcType, rcAllocating, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                      AlignPairDistanceFilterFactory_Init,
                                      AlignPairDistanceFilterFactory_NewObj,
                                      AlignPairDistanceFilterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( AlignPairDistanceFilterFactory* ) *cself;
            obj->table = table;
            obj->mp_dist_unknown = mp_dist_unknown;
            obj->mp_dist = mp_dist;
            obj->mp_dist_qty = mp_dist_qty;
        }
    }
    return rc;
}


/* ============== FASTQ read type (bio/tech) filter ============================ */

typedef struct FastqBioFilter_struct
{
    const FastqReader* reader;
    uint64_t rejected_reads;
} FastqBioFilter;


/* filter out non-bio reads */
static rc_t FastqBioFilter_GetKey( const SRASplitter* cself, const char** key,
                                   spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    FastqBioFilter* self = ( FastqBioFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        uint32_t num_reads = 0;

        *key = NULL;
        rc = FastqReaderSeekSpot( self->reader, spot );
        if ( rc == 0 )
        {
            rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, NULL, &num_reads );
            if ( rc == 0 )
            {
                uint32_t readId;
                SRA_DUMP_DBG( 3, ( "%s %u row reads: ", __func__, spot ) );
                for (readId = 0; rc == 0 && readId < num_reads; readId++ )
                {
                    if ( isset_readmask( readmask, readId ) )
                    {
                        SRAReadTypes read_type;
                        rc = FastqReader_SpotReadInfo( self->reader, readId + 1, &read_type,
                                                       NULL, NULL, NULL, NULL);
                        if ( rc == 0 )
                        {
                            if ( !( read_type & SRA_READ_TYPE_BIOLOGICAL ) )
                            {
                                unset_readmask( readmask, readId );
                                self->rejected_reads ++;
                            }
                            else
                            {
                                SRA_DUMP_DBG( 3, ( " %u", readId ) );
                            }
                        }
                    }
                }
                *key = "";
                SRA_DUMP_DBG( 3, ( " key '%s'\n", *key ) );
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}


static rc_t FastqBioFilter_Release( const SRASplitter * cself )
{
    rc_t rc = 0;
    FastqBioFilter * self = ( FastqBioFilter * )cself;

    if ( self == NULL )
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    else
    {
        if ( self->rejected_reads > 0 && !g_legacy_report )
            rc = KOutMsg( "Rejected %lu READS because of filtering out non-biological READS\n", self->rejected_reads );
    }
    return rc;
}


typedef struct FastqBioFilterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
} FastqBioFilterFactory;


static rc_t FastqBioFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    FastqBioFilterFactory* self = ( FastqBioFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderMake( &self->reader, self->table, self->accession,
                              /* preserve orig spot format to save on conversions */
                              FastqArgs.is_platform_cs_native, false, FastqArgs.fasta > 0, false,
                              false, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                              FastqArgs.offset, '\0', 0, 0) ;
    }
    return rc;
}


static rc_t FastqBioFilterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    FastqBioFilterFactory* self = ( FastqBioFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( FastqBioFilter ),
                               FastqBioFilter_GetKey, NULL, NULL, FastqBioFilter_Release );
        if ( rc == 0 )
        {
            FastqBioFilter * filter = ( FastqBioFilter * )( * splitter );
            filter->reader = self->reader;
            filter->rejected_reads = 0;
        }
    }
    return rc;
}


static void FastqBioFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        FastqBioFilterFactory* self = ( FastqBioFilterFactory* )cself;
        FastqReaderWhack( self->reader );
    }
}


static rc_t FastqBioFilterFactory_Make( const SRASplitterFactory** cself,
                        const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    FastqBioFilterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                      FastqBioFilterFactory_Init,
                                      FastqBioFilterFactory_NewObj,
                                      FastqBioFilterFactory_Release );

        if ( rc == 0 )
        {
            obj = ( FastqBioFilterFactory* )*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}


/* ============== FASTQ number of reads filter ============================ */

typedef struct FastqRNumberFilter_struct
{
    const FastqReader* reader;
    uint64_t rejected_reads;
} FastqRNumberFilter;


static rc_t FastqRNumberFilter_GetKey( const SRASplitter* cself, const char** key,
                                       spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    FastqRNumberFilter* self = ( FastqRNumberFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        uint32_t num_reads = 0;

        *key = NULL;
        rc = FastqReaderSeekSpot( self->reader, spot );
        if ( rc == 0 )
        {
            rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, NULL, &num_reads );
            if ( rc == 0 )
            {
                int readId, q;

                SRA_DUMP_DBG( 3, ( "%s %u row reads: ", __func__, spot ) );
                for ( readId = 0, q = 0; readId < num_reads; readId++ )
                {
                    if ( isset_readmask( readmask, readId ) )
                    {
                        if ( ++q > FastqArgs.maxReads )
                        {
                            unset_readmask( readmask, 1 << readId );
                        }
                        else
                        {
                            SRA_DUMP_DBG( 3, ( " %u", readId ) );
                        }
                    }
                }
                *key = "";
                SRA_DUMP_DBG( 3, ( " key '%s'\n", *key ) );
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}


static rc_t FastqRNumberFilter_Release( const SRASplitter* cself )
{
    rc_t rc = 0;
    FastqRNumberFilter * self = ( FastqRNumberFilter * )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        if ( self->rejected_reads > 0 && !g_legacy_report )
            rc = KOutMsg( "Rejected %lu READS because of max. number of READS = %u\n", self->rejected_reads, FastqArgs.maxReads );
    }
    return rc;
}

typedef struct FastqRNumberFilterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
} FastqRNumberFilterFactory;


static rc_t FastqRNumberFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    FastqRNumberFilterFactory* self = ( FastqRNumberFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderMake( &self->reader, self->table, self->accession,
                              /* preserve orig spot format to save on conversions */
                              FastqArgs.is_platform_cs_native, false, FastqArgs.fasta > 0, false,
                              false, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                              FastqArgs.offset, '\0', 0, 0 );
    }
    return rc;
}


static rc_t FastqRNumberFilterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    FastqRNumberFilterFactory* self = ( FastqRNumberFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( FastqRNumberFilter ),
                               FastqRNumberFilter_GetKey, NULL, NULL, FastqRNumberFilter_Release );
        if ( rc == 0 )
        {
            FastqRNumberFilter * filter = ( FastqRNumberFilter * )( * splitter );
            filter->reader = self->reader;
            filter->rejected_reads = 0;
        }
    }
    return rc;
}


static void FastqRNumberFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        FastqRNumberFilterFactory* self = ( FastqRNumberFilterFactory* )cself;
        FastqReaderWhack( self->reader );
    }
}


static rc_t FastqRNumberFilterFactory_Make( const SRASplitterFactory** cself,
                                const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    FastqRNumberFilterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                      FastqRNumberFilterFactory_Init,
                                      FastqRNumberFilterFactory_NewObj,
                                      FastqRNumberFilterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( FastqRNumberFilterFactory* )*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}

/* ============== FASTQ quality filter ============================ */

typedef struct FastqQFilter_struct
{
    const FastqReader * reader;
    KDataBuffer * buffer_for_read_column;
    KDataBuffer * buffer_for_quality;
    uint64_t rejected_spots;
    uint64_t rejected_reads;
} FastqQFilter;


#define MIN_QUAL_FOR_RULE_3 ( 33 + 2 )

static bool QFilter_Test_new( const char * read, const char * quality, uint32_t readlen, uint32_t minlen, char N_char )
{
    uint32_t idx, cnt, cnt1, start;
    char b0;

    /* 1st criteria : READ has to be longer than the minlen */
    if ( readlen < minlen ) return false;

    /* 2nd criteria : READ does not contain any N's in the first minlen bases */
    for ( idx = 0; idx < minlen; ++idx )
    {
        if ( read[ idx ] == N_char ) return false;
    }

    start = ( N_char == '.' ) ? 1 : 0;
    /* 3rd criteria : QUALITY values are all 2 or higher in the first minlen values */
    for ( idx = start; idx < minlen; ++idx )
    {
        if ( quality[ idx ] < MIN_QUAL_FOR_RULE_3 ) return false;
    }

    /* 4th criteria : READ contains more than one type of base in the first minlen bases */
    b0 = read[ 0 ];
    for ( idx = 1, cnt = 1; idx < minlen; ++idx )
    {
        if ( read[ idx ] == b0 ) cnt++;
    }
    if ( cnt == minlen ) return false;

    /* 5th criteria : READ does not contain more than 50% N's in its whole length */
    cnt = ( readlen / 2 );
    for ( idx = 0, cnt1 = 0; idx < readlen; ++idx )
    {
        if ( read[ idx ] == N_char ) cnt1++;
    }
    if ( cnt1 > cnt ) return false;

    /* 6th criteria : READ does not contain values other than ACGTN in its whole length */
    if ( N_char != '.' )
    {
        for ( idx = 0; idx < readlen; ++idx )
        {
            if ( ! ( ( read[ idx ] == 'A' ) || ( read[ idx ] == 'C' ) || ( read[ idx ] == 'G' ) ||
                    ( read[ idx ] == 'T' ) || ( read[ idx ] == 'N' ) ) )
                return false;
        }
    }

    return true;
}


static bool QFilter_Test_old( const char * read, uint32_t readlen, bool cs_native )
{
    static char const* const baseStr = "NNNNNNNNNN";
    static char const* const colorStr = "..........";
    static const uint32_t strLen = 10;
    uint32_t xLen = readlen < strLen ? readlen : strLen;

    if ( cs_native )
    {
        if ( strncmp( &read[ 1 ], colorStr, xLen ) == 0 || strcmp( &read[ readlen - xLen + 1 ], colorStr ) == 0 )
            return false;
    }
    else
    {
        if ( strncmp( read, baseStr, xLen ) == 0 || strcmp( &read[ readlen - xLen ], baseStr ) == 0 )
            return false;
    }
    return true;
}


/* filter out reads by leading/trialing quality */
static rc_t FastqQFilter_GetKey( const SRASplitter* cself, const char** key,
                                 spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    FastqQFilter* self = ( FastqQFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        uint32_t num_reads = 0, spot_len;

        *key = NULL;
        rc = FastqReaderSeekSpot( self->reader, spot );
        if ( rc == 0 )
        {
            rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, &spot_len, &num_reads );
            if ( rc == 0 )
            {
                SRA_DUMP_DBG( 3, ( "%s %u row reads: ", __func__, spot ) );
                if ( FastqArgs.split_spot )
                {
                    uint32_t readId;
                    INSDC_coord_len read_len;
                    for ( readId = 0; rc == 0 && readId < num_reads; readId++ )
                    {
                        if ( isset_readmask( readmask, readId ) )
                        {
                            rc = FastqReader_SpotReadInfo( self->reader, readId + 1, NULL, NULL, NULL, NULL, &read_len );
                            if ( rc == 0 )
                            {
                                /* FastqReaderBase() in libsra! libs/sra/fastq.h
                                   IF_BUF ... very, very bad macro defined in factory.h !!
                                   it combines reading from table with resizing of a KDatabuffer in a loop until rc == 0 is reached!
                                */
                                IF_BUF( ( FastqReaderBase( self->reader, readId + 1,
                                          self->buffer_for_read_column->base,
                                          KDataBufferBytes( self->buffer_for_read_column ), NULL) ),
                                          self->buffer_for_read_column, read_len )
                                {
                                    const char * b = self->buffer_for_read_column->base;
                                    bool filter_passed = true;

                                    if ( FastqArgs.qual_filter1 )
                                    {
                                        if ( quality_N_limit > 0 )
                                        {
                                            IF_BUF( ( FastqReaderQuality( self->reader, readId + 1,
                                                      self->buffer_for_quality->base,
                                                      KDataBufferBytes( self->buffer_for_quality ), NULL) ),
                                                      self->buffer_for_quality, read_len )
                                            {
                                                const char * q = self->buffer_for_quality->base;
                                                char N_char = FastqArgs.is_platform_cs_native ? '.' : 'N';
                                                filter_passed = QFilter_Test_new( b, q, read_len, quality_N_limit, N_char );
                                            }
                                        }
                                    }
                                    else
                                        filter_passed = QFilter_Test_old( b, read_len, FastqArgs.is_platform_cs_native );

                                    if ( filter_passed )
                                        SRA_DUMP_DBG( 3, ( " %u", readId ) );
                                    else
                                    {
                                        unset_readmask( readmask, readId );
                                        /* READ did not pass filter */
                                        self->rejected_reads ++;
                                    }
                                }
                            }
                        }
                    }
                    *key = "";
                }
                else
                {
                    IF_BUF( ( FastqReaderBase( self->reader, 0, self->buffer_for_read_column->base,
                              KDataBufferBytes( self->buffer_for_read_column ), NULL ) ), self->buffer_for_read_column, spot_len )
                    {
                        const char* b = self->buffer_for_read_column->base;
                        bool filter_passed = true;

                        if ( FastqArgs.qual_filter1 )
                        {
                            if ( quality_N_limit > 0 )
                            {
                                IF_BUF( ( FastqReaderQuality( self->reader, 0, self->buffer_for_quality->base,
                                          KDataBufferBytes( self->buffer_for_quality ), NULL ) ), self->buffer_for_quality, spot_len )
                                {
                                    const char * q = self->buffer_for_quality->base;
                                    char N_char = FastqArgs.is_platform_cs_native ? '.' : 'N';
                                    filter_passed = QFilter_Test_new( b, q, spot_len, quality_N_limit, N_char );
                                }
                            }
                        }
                        else
                            filter_passed = QFilter_Test_old( b, spot_len, FastqArgs.is_platform_cs_native );

                        if ( filter_passed )
                        {
                            *key = "";
                            SRA_DUMP_DBG( 3, ( " whole spot" ) );
                        }
                        else
                        {
                            /* SPOT did not pass filter */
                            self->rejected_spots ++;
                        }
                    }
                }
                SRA_DUMP_DBG( 3, ( " key '%s'\n", *key ) );
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}


static rc_t FastqQFilter_Release( const SRASplitter * cself )
{
    rc_t rc = 0;
    FastqQFilter* self = ( FastqQFilter* )cself;

    if ( self == NULL )
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    else if ( !g_legacy_report )
    {
        if ( self->rejected_reads > 0 )
            rc = KOutMsg( "Rejected %lu READS because of Quality-Filtering\n", self->rejected_reads );
        if ( rc == 0 && self->rejected_spots > 0 )
            rc = KOutMsg( "Rejected %lu SPOTS because of Quality-Filtering\n", self->rejected_spots );

    }
    return rc;
}


typedef struct FastqQFilterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
    KDataBuffer kdbuf;
    KDataBuffer kdbuf2;
} FastqQFilterFactory;


static rc_t FastqQFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    FastqQFilterFactory* self = ( FastqQFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = KDataBufferMakeBytes( &self->kdbuf, DATABUFFERINITSIZE );
        if ( rc == 0 )
        {
            rc = KDataBufferMakeBytes( &self->kdbuf2, DATABUFFERINITSIZE );
            if ( rc == 0 )
            {
                rc = FastqReaderMake( &self->reader, self->table, self->accession,
                             /* preserve orig spot format to save on conversions */
                             FastqArgs.is_platform_cs_native, false, FastqArgs.fasta > 0, false,
                             false, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                             FastqArgs.offset, '\0', 0, 0 );
            }
        }
    }
    return rc;
}


static rc_t FastqQFilterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    FastqQFilterFactory* self = ( FastqQFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( FastqQFilter ), FastqQFilter_GetKey, NULL, NULL, FastqQFilter_Release );
        if ( rc == 0 )
        {
            FastqQFilter * filter = ( FastqQFilter * )( * splitter );
            filter->reader = self->reader;
            filter->buffer_for_read_column = &self->kdbuf;
            filter->buffer_for_quality = &self->kdbuf2;
            filter->rejected_spots = 0;
            filter->rejected_reads = 0;
        }
    }
    return rc;
}


static void FastqQFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        FastqQFilterFactory* self = ( FastqQFilterFactory* )cself;
        KDataBufferWhack( &self->kdbuf );
        KDataBufferWhack( &self->kdbuf2 );
        FastqReaderWhack( self->reader );
    }
}


static rc_t FastqQFilterFactory_Make( const SRASplitterFactory** cself,
                                      const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    FastqQFilterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                      FastqQFilterFactory_Init,
                                      FastqQFilterFactory_NewObj,
                                      FastqQFilterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( FastqQFilterFactory* )*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}


/* ============== FASTQ min read length filter ============================ */


typedef struct FastqReadLenFilter_struct
{
    const FastqReader* reader;
    uint64_t rejected_spots;
    uint64_t rejected_reads;
} FastqReadLenFilter;


static rc_t FastqReadLenFilter_GetKey( const SRASplitter* cself, const char** key,
                                       spotid_t spot, readmask_t* readmask )
{
    rc_t rc = 0;
    FastqReadLenFilter* self = ( FastqReadLenFilter* )cself;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        uint32_t num_reads = 0, spot_len = 0;

        *key = NULL;
        rc = FastqReaderSeekSpot( self->reader, spot );
        if ( rc == 0 )
        {
            rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, &spot_len, &num_reads );
            if ( rc == 0 )
            {
                SRA_DUMP_DBG( 3, ( "%s %u row reads:", __func__, spot ) );
                if ( FastqArgs.split_spot )
                {
                    uint32_t readId;
                    INSDC_coord_len read_len = 0;
                    for ( readId = 0; rc == 0 && readId < num_reads; readId++ )
                    {
                        if ( isset_readmask( readmask, readId ) )
                        {
                            rc = FastqReader_SpotReadInfo( self->reader, readId + 1, NULL, NULL, NULL, NULL, &read_len );
                            if ( rc == 0 )
                            {
                                if ( read_len < FastqArgs.minReadLen )
                                {
                                    unset_readmask( readmask, readId );
                                    self->rejected_reads ++;
                                }
                                else
                                {
                                    SRA_DUMP_DBG( 3, ( " %u", readId ) );
                                }
                            }
                        }
                    }
                    *key = "";
                }
                else
                {
                    if ( spot_len >= FastqArgs.minReadLen )
                    {
                        *key = "";
                        SRA_DUMP_DBG( 3, ( " whole spot" ) );
                    }
                    else
                        self->rejected_spots ++;
                }
                SRA_DUMP_DBG( 3, ( "\n" ) );
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}


static rc_t FastqReadLenFilter_Release( const SRASplitter * cself )
{
    rc_t rc = 0;
    FastqReadLenFilter * self = ( FastqReadLenFilter* )cself;

    if ( self == NULL )
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    else if ( !g_legacy_report )
    {
        if ( self->rejected_reads > 0 )
            rc = KOutMsg( "Rejected %lu READS because READLEN < %u\n", self->rejected_reads, FastqArgs.minReadLen );
        if ( self->rejected_spots > 0 && rc == 0 )
            rc = KOutMsg( "Rejected %lu SPOTS because SPOTLEN < %u\n", self->rejected_spots, FastqArgs.minReadLen );
    }
    return rc;
}


typedef struct FastqReadLenFilterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
} FastqReadLenFilterFactory;


static rc_t FastqReadLenFilterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    FastqReadLenFilterFactory* self = ( FastqReadLenFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderMake( &self->reader, self->table, self->accession,
                             /* preserve orig spot format to save on conversions */
                              FastqArgs.is_platform_cs_native, false, FastqArgs.fasta > 0, false,
                              false, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                              FastqArgs.offset, '\0', 0, 0 );
    }
    return rc;
}


static rc_t FastqReadLenFilterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    FastqReadLenFilterFactory* self = ( FastqReadLenFilterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( FastqReadLenFilter ),
                               FastqReadLenFilter_GetKey, NULL, NULL, FastqReadLenFilter_Release );
        if ( rc == 0 )
        {
            FastqReadLenFilter * filter = ( FastqReadLenFilter* )( *splitter );
            filter->reader = self->reader;
            filter->rejected_spots = 0;
            filter->rejected_reads = 0;
        }
    }
    return rc;
}


static void FastqReadLenFilterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        FastqReadLenFilterFactory* self = ( FastqReadLenFilterFactory* )cself;
        FastqReaderWhack( self->reader );
    }
}


static rc_t FastqReadLenFilterFactory_Make( const SRASplitterFactory** cself,
                            const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    FastqReadLenFilterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterSpot, sizeof( *obj ),
                                      FastqReadLenFilterFactory_Init,
                                      FastqReadLenFilterFactory_NewObj,
                                      FastqReadLenFilterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( FastqReadLenFilterFactory* )*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}


/* ============== FASTQ read splitter ============================ */

char* FastqReadSplitter_key_buf = NULL;


typedef struct FastqReadSplitter_struct
{
    const FastqReader* reader;
    SRASplitter_Keys* keys;
    uint32_t keys_max;
} FastqReadSplitter;


static rc_t FastqReadSplitter_GetKeySet( const SRASplitter* cself, const SRASplitter_Keys** key,
                    uint32_t* keys, spotid_t spot, const readmask_t* readmask )
{
    rc_t rc = 0;
    FastqReadSplitter* self = ( FastqReadSplitter* )cself;
    const size_t key_offset = 5;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        uint32_t num_reads = 0;

        *keys = 0;
        if ( FastqReadSplitter_key_buf == NULL )
        {
            /* initial alloc key_buf: "  1\0  2\0...\0  9\0 10\0 11\0...\03220..\08192\0" */
            if ( nreads_max > 9999 )
            {
                /* key_offset and sprintf format size are insufficient for keys longer than 4 digits */
                rc = RC( rcExe, rcNode, rcExecuting, rcBuffer, rcInsufficient );
            }
            else
            {
                FastqReadSplitter_key_buf = malloc( nreads_max * key_offset );
                if ( FastqReadSplitter_key_buf == NULL )
                {
                    rc = RC( rcExe, rcNode, rcExecuting, rcMemory, rcExhausted );
                }
                else
                {
                    /* fill buffer w/keys */
                    int i;
                    char* p = FastqReadSplitter_key_buf;
                    char *end = p + nreads_max * key_offset;
                    for ( i = 1; rc == 0 && i <= nreads_max; i++ )
                    {
                        int n = snprintf( p, end - p, "%4u", i );
                        if ( p + n < end ) {
                            p += key_offset;
                        }
                        else
                        {
                            rc = RC( rcExe, rcNode, rcExecuting, rcTransfer, rcIncomplete );
                            break;
                        }
                    }
                }
            }
        }

        if ( rc == 0 )
        {
            rc = FastqReaderSeekSpot( self->reader, spot );
            if ( rc == 0 )
            {
                rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, NULL, &num_reads );
                if ( rc == 0 )
                {
                    uint32_t readId, good = 0;

                    SRA_DUMP_DBG( 3, ( "%s %u row reads:", __func__, spot ) );
                    for ( readId = 0; rc == 0 && readId < num_reads; readId++ )
                    {
                        rc = FastqReader_SpotReadInfo( self->reader, readId + 1, NULL, NULL, NULL, NULL, NULL );
                        if ( !isset_readmask( readmask, readId ) )
                        {
                            continue;
                        }
                        if ( self->keys_max < ( good + 1 ) )
                        {
                            void* p = realloc( self->keys, sizeof( *self->keys ) * ( good + 1 ) );
                            if ( p == NULL )
                            {
                                rc = RC( rcExe, rcNode, rcExecuting, rcMemory, rcExhausted );
                                break;
                            }
                            else
                            {
                                self->keys = p;
                                self->keys_max = good + 1;
                            }
                        }
                        self->keys[ good ].key = &FastqReadSplitter_key_buf[ readId * key_offset ];
                        while ( self->keys[ good ].key[ 0 ] == ' ' && self->keys[ good ].key[0] != '\0' )
                        {
                            self->keys[ good ].key++;
                        }
                        clear_readmask( self->keys[ good ].readmask );
                        set_readmask( self->keys[good].readmask, readId );
                        SRA_DUMP_DBG( 3, ( " key['%s']+=%u", self->keys[ good ].key, readId ) );
                        good++;
                    }
                    if ( rc == 0 )
                    {
                        *key = self->keys;
                        *keys = good;
                    }
                    SRA_DUMP_DBG( 3, ( "\n" ) );
                }
            }
            else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
            {
                SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
                rc = 0;
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
                SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
                rc = 0;
        }
    }
    return rc;
}


rc_t FastqReadSplitter_Release( const SRASplitter* cself )
{
    if ( cself != NULL )
    {
        FastqReadSplitter* self = ( FastqReadSplitter* )cself;
        free( self->keys );
    }
    return 0;
}


typedef struct FastqReadSplitterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
} FastqReadSplitterFactory;


static rc_t FastqReadSplitterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    FastqReadSplitterFactory* self = ( FastqReadSplitterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderMake( &self->reader, self->table, self->accession,
                              /* preserve orig spot format to save on conversions */
                              FastqArgs.is_platform_cs_native, false, FastqArgs.fasta > 0, false,
                              false, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                              FastqArgs.offset, '\0', 0, 0 );
    }
    return rc;
}


static rc_t FastqReadSplitterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    FastqReadSplitterFactory* self = ( FastqReadSplitterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( FastqReadSplitter ), NULL,
                               FastqReadSplitter_GetKeySet, NULL, FastqReadSplitter_Release );
        if ( rc == 0 )
        {
            ( (FastqReadSplitter*)(*splitter) )->reader = self->reader;
        }
    }
    return rc;
}


static void FastqReadSplitterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        FastqReadSplitterFactory* self = ( FastqReadSplitterFactory* )cself;
        FastqReaderWhack( self->reader );
        free( FastqReadSplitter_key_buf );
        FastqReadSplitter_key_buf = NULL;
    }
}


static rc_t FastqReadSplitterFactory_Make( const SRASplitterFactory** cself,
                const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    FastqReadSplitterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterRead, sizeof( *obj ),
                                      FastqReadSplitterFactory_Init,
                                      FastqReadSplitterFactory_NewObj,
                                      FastqReadSplitterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( FastqReadSplitterFactory* )*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}


/* ============== FASTQ 3 read splitter ============================ */

char* Fastq3ReadSplitter_key_buf = NULL;

typedef struct Fastq3ReadSplitter_struct
{
    const FastqReader* reader;
    SRASplitter_Keys keys[ 2 ];
} Fastq3ReadSplitter;


/* if all active reads len >= minreadlen, reads are splitted into separate files, otherwise on single key "" is returnded for all reads */
static rc_t Fastq3ReadSplitter_GetKeySet( const SRASplitter* cself, const SRASplitter_Keys** key,
            uint32_t* keys, spotid_t spot, const readmask_t* readmask )
{
    rc_t rc = 0;
    Fastq3ReadSplitter* self = ( Fastq3ReadSplitter* )cself;
    const size_t key_offset = 5;

    if ( self == NULL || key == NULL )
    {
        rc = RC( rcExe, rcNode, rcExecuting, rcParam, rcInvalid );
    }
    else
    {
        uint32_t num_reads = 0;

        *keys = 0;
        if ( Fastq3ReadSplitter_key_buf == NULL )
        {
            /* initial alloc key_buf: "  1\0  2\0...\0  9\0 10\0 11\0...\03220..\08192\0" */
            if ( nreads_max > 9999 )
            {
                /* key_offset and sprintf format size are insufficient for keys longer than 4 digits */
                rc = RC( rcExe, rcNode, rcExecuting, rcBuffer, rcInsufficient );
            }
            else
            {
                Fastq3ReadSplitter_key_buf = malloc( nreads_max * key_offset );
                if ( Fastq3ReadSplitter_key_buf == NULL )
                {
                    rc = RC( rcExe, rcNode, rcExecuting, rcMemory, rcExhausted );
                }
                else
                {
                    /* fill buffer w/keys */
                    int i;
                    char* p = Fastq3ReadSplitter_key_buf;
                    char *end = p + nreads_max * key_offset;
                    for ( i = 1; rc == 0 && i <= nreads_max; i++ )
                    {
                        int n = snprintf( p, end - p, "%4u", i );
                        if ( p + n < end ) {
                            p += key_offset;
                        }
                        else
                        {
                            rc = RC( rcExe, rcNode, rcExecuting, rcTransfer, rcIncomplete );
                            break;
                        }
                    }
                }
            }
        }

        if ( rc == 0 )
        {
            rc = FastqReaderSeekSpot( self->reader, spot );
            if ( rc == 0 )
            {
                rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, NULL, &num_reads );
                if ( rc == 0 )
                {
                    uint32_t readId, good  = 0;
                    const uint32_t max_reads = sizeof( self->keys ) /sizeof( self->keys[ 0 ] );

                    SRA_DUMP_DBG( 3, ( "%s %u row reads:", __func__, spot ) );
                    for ( readId = 0; rc == 0 && readId < num_reads && good < max_reads; readId++ )
                    {
                        rc = FastqReader_SpotReadInfo( self->reader, readId + 1, NULL, NULL, NULL, NULL, NULL );
                        if ( !isset_readmask( readmask, readId ) )
                        {
                            continue;
                        }
                        self->keys[ good ].key = &Fastq3ReadSplitter_key_buf[ good * key_offset ];
                        while ( self->keys[ good ].key[ 0 ] == ' ' && self->keys[good].key[ 0 ] != '\0' )
                        {
                            self->keys[ good ].key++;
                        }
                        clear_readmask( self->keys[ good ].readmask );
                        set_readmask( self->keys[ good ].readmask, readId );
                        SRA_DUMP_DBG( 3, ( " key['%s']+=%u", self->keys[ good ].key, readId ) );
                        good++;
                    }
                    if ( rc == 0 )
                    {
                        *key = self->keys;
                        *keys = good;
                        if ( good != max_reads )
                        {
                            /* some are short -> reset keys to same value for all valid reads */
                            /* run has just one read -> no suffix */
                            /* or single file was requested */
                            for ( readId = 0; readId < good; readId++ )
                            {
                                self->keys[ readId ].key = "";
                            }
                            SRA_DUMP_DBG( 3, ( " all keys joined to ''" ) );
                        }
                    }
                    SRA_DUMP_DBG( 3, ( "\n" ) );
                }
            }
            else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
            {
                SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
                rc = 0;
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}

typedef struct Fastq3ReadSplitterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
} Fastq3ReadSplitterFactory;


static rc_t Fastq3ReadSplitterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    Fastq3ReadSplitterFactory* self = ( Fastq3ReadSplitterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderMake( &self->reader, self->table, self->accession,
                              /* preserve orig spot format to save on conversions */
                              FastqArgs.is_platform_cs_native, false, FastqArgs.fasta > 0, false,
                              false, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                              FastqArgs.offset, '\0', 0, 0 );
    }
    return rc;
}


static rc_t Fastq3ReadSplitterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    Fastq3ReadSplitterFactory* self = ( Fastq3ReadSplitterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make( splitter, sizeof( Fastq3ReadSplitter ),
                               NULL, Fastq3ReadSplitter_GetKeySet, NULL, NULL );
        if ( rc == 0 )
        {
            ( (Fastq3ReadSplitter*)(*splitter) )->reader = self->reader;
        }
    }
    return rc;
}


static void Fastq3ReadSplitterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        Fastq3ReadSplitterFactory* self = ( Fastq3ReadSplitterFactory* )cself;
        FastqReaderWhack( self->reader );
        memset ( self, 0, sizeof * self );

        free( Fastq3ReadSplitter_key_buf );
        Fastq3ReadSplitter_key_buf = NULL;
    }
}


static rc_t Fastq3ReadSplitterFactory_Make( const SRASplitterFactory** cself,
                    const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    Fastq3ReadSplitterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterRead, sizeof( *obj ),
                                      Fastq3ReadSplitterFactory_Init,
                                      Fastq3ReadSplitterFactory_NewObj,
                                      Fastq3ReadSplitterFactory_Release );
        if ( rc == 0 )
        {
            obj = ( Fastq3ReadSplitterFactory* )*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}


/* ============== FASTQ formatter object  ============================ */


typedef struct FastqFormatterSplitter_struct
{
    const char* accession;
    const FastqReader* reader;
    KDataBuffer* b[ 5 ];
    size_t bsz[ 5 ]; /* fifth is for fasta line wrap */
} FastqFormatterSplitter;


static rc_t FastqFormatterSplitter_WrapLine( const KDataBuffer* src, size_t src_sz,
                                             KDataBuffer* dst, size_t* dst_sz, const uint64_t width )
{
    rc_t rc = 0;
    size_t sz = src_sz + ( src_sz - 1 ) / width;

    if ( KDataBufferBytes( dst ) < sz )
    {
        SRA_DUMP_DBG( 10, ( "%s grow buffer from %u to %u\n", __func__, KDataBufferBytes( dst ), sz + 10 ) );
        rc = KDataBufferResize( dst, sz + 10 );
    }
    if ( rc == 0 )
    {
        const char* s = src->base;
        char* d = dst->base;
        *dst_sz = sz = src_sz;
        while ( sz > 0 )
        {
            memmove( d, s, sz > width ? width : sz );
            if ( sz <= width )
            {
                break;
            }
            d += width;
            *d++ = '\n';
            *dst_sz = *dst_sz + 1;
            s += width;
            sz -= width;
        }
    }
    return rc;
}


static rc_t FastqFormatterSplitter_DumpByRead( const SRASplitter* cself, spotid_t spot, const readmask_t* readmask )
{
    rc_t rc = 0;
    FastqFormatterSplitter* self = ( FastqFormatterSplitter* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderSeekSpot( self->reader, spot );
        if ( rc == 0 )
        {
            rc = SRASplitter_FileActivate( cself, FastqArgs.file_extension );
            if ( rc == 0 )
            {
                DeflineData def_data;
                const char* spot_name = NULL, *spot_group = NULL, *read_name = NULL;
                uint32_t readIdx, spot_len = 0;
                uint32_t num_reads, readId, k;
                size_t sname_sz, sgrp_sz;
                INSDC_coord_len rlabel_sz = 0, read_len = 0;

                if ( FastqArgs.b_defline || FastqArgs.q_defline )
                {
                    rc = FastqReader_SpotInfo( self->reader, &spot_name, &sname_sz, &spot_group, &sgrp_sz, &spot_len, &num_reads );
                }
                else
                {
                    rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, NULL, &num_reads );
                }

                for ( readId = 1, readIdx = 1; rc == 0 && readId <= num_reads; readId++, readIdx++ )
                {
                    if ( !isset_readmask( readmask, readId - 1 ) )
                    {
                        continue;
                    }
                    if ( FastqArgs.b_defline || FastqArgs.q_defline )
                    {
                        rc = FastqReader_SpotReadInfo( self->reader, readId, NULL, &read_name, &rlabel_sz, NULL, &read_len );
                        if ( rc == 0 )
                        {
                            rc = Defline_Bind( &def_data, self->accession, &spot, spot_name, sname_sz, spot_group, sgrp_sz,
                                               &spot_len, &readIdx, read_name, rlabel_sz, &read_len );
                        }
                    }
                    if ( rc == 0 )
                    {
                        if ( FastqArgs.b_defline )
                        {
                            IF_BUF( ( Defline_Build( FastqArgs.b_defline, &def_data, self->b[0]->base,
                                      KDataBufferBytes( self->b[0] ), &self->bsz[0] ) ), self->b[0], self->bsz[0] );
                        }
                        else
                        {
                            IF_BUF( ( FastqReaderBaseName( self->reader, readId, &FastqArgs.dumpCs, self->b[0]->base,
                                      KDataBufferBytes( self->b[0] ), &self->bsz[0] ) ), self->b[0], self->bsz[0] );
                        }
                    }
                    if ( rc == 0 )
                    {
                        if ( FastqArgs.fasta > 0 )
                        {
                            IF_BUF( ( FastqReaderBase( self->reader, readId, self->b[4]->base, KDataBufferBytes( self->b[4] ),
                                      &self->bsz[4] ) ), self->b[4], self->bsz[4] )
                            {
                                rc = FastqFormatterSplitter_WrapLine( self->b[4], self->bsz[4], self->b[1], &self->bsz[1], FastqArgs.fasta );
                            }
                        }
                        else
                        {
                            IF_BUF( ( FastqReaderBase( self->reader, readId, self->b[1]->base,
                                      KDataBufferBytes( self->b[1] ), &self->bsz[1] ) ), self->b[1], self->bsz[1] );
                        }
                    }
                    if ( !FastqArgs.fasta && rc == 0 )
                    {
                        if ( FastqArgs.q_defline )
                        {
                            IF_BUF( ( Defline_Build( FastqArgs.q_defline, &def_data, self->b[2]->base,
                                      KDataBufferBytes( self->b[2] ), &self->bsz[2] ) ), self->b[2], self->bsz[2] );
                        }
                        else
                        {
                            IF_BUF( ( FastqReaderQualityName( self->reader, readId, &FastqArgs.dumpCs, self->b[2]->base,
                                      KDataBufferBytes( self->b[2] ), &self->bsz[2] ) ), self->b[2], self->bsz[2] );
                        }
                        if ( rc == 0 )
                        {
                            IF_BUF( ( FastqReaderQuality( self->reader, readId, self->b[3]->base,
                                      KDataBufferBytes( self->b[3] ), &self->bsz[3] ) ), self->b[3], self->bsz[3] );
                        }
                    }
                    else if ( ( (char*)(self->b[0]->base))[0] == '@' )
                    {
                        ( (char*)(self->b[0]->base) )[0] = '>';
                    }
                    for ( k = 0; rc == 0 && k < ( FastqArgs.fasta ? 2 : 4); k++ )
                    {
                        rc = SRASplitter_FileWrite( cself, spot, self->b[ k ]->base, self->bsz[ k ] );
                        if ( rc == 0 )
                        {
                            rc = SRASplitter_FileWrite( cself, spot, "\n", 1 );
                        }
                    }
                }
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}


static rc_t Fasta_dump( const SRASplitter* cself, FastqFormatterSplitter* self, spotid_t spot, uint32_t columns )
{
    uint32_t readId;
    rc_t rc = FastqFormatterSplitter_WrapLine( self->b[4], self->bsz[4], self->b[1], &self->bsz[1], columns );
    for ( readId = 0; rc == 0 && readId < 2; readId++ )
    {
        rc = SRASplitter_FileWrite( cself, spot, self->b[readId]->base, self->bsz[ readId ] );
        if ( rc == 0 )
        {
            rc = SRASplitter_FileWrite( cself, spot, "\n", 1 );
        }
    }
    return rc;
}


static rc_t FastqFormatterSplitter_DumpBySpot( const SRASplitter* cself, spotid_t spot, const readmask_t* readmask )
{
    rc_t rc = 0;
    FastqFormatterSplitter* self = ( FastqFormatterSplitter* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderSeekSpot( self->reader, spot );
        if ( rc == 0 )
        {
            uint32_t num_reads, readId;

            rc = FastqReader_SpotInfo( self->reader, NULL, NULL, NULL, NULL, NULL, &num_reads );
            for ( readId = 0; rc == 0 && readId < num_reads; readId++ )
            {
                /* if any read in readmask is not set - skip whole spot */
                if ( !isset_readmask( readmask, readId ) )
                {
                    return 0;
                }
            }
            if ( rc == 0 )
            {
                rc = SRASplitter_FileActivate( cself, FastqArgs.file_extension );
                if ( rc == 0 )
                {
                    DeflineData def_data;
                    const char* spot_name = NULL, *spot_group = NULL;
                    uint32_t spot_len = 0;
                    size_t writ, sname_sz, sgrp_sz;
                    const int base_i = FastqArgs.fasta ? 4 : 1;

                    if ( FastqArgs.b_defline || FastqArgs.q_defline )
                    {
                        rc = FastqReader_SpotInfo( self->reader, &spot_name, &sname_sz,
                                                   &spot_group, &sgrp_sz, &spot_len, NULL );
                        if ( rc == 0 )
                        {
                            rc = Defline_Bind( &def_data, self->accession, &spot, spot_name,
                                               sname_sz, spot_group, sgrp_sz,
                                               &spot_len, &readId, NULL, 0, &spot_len );
                        }
                    }

                    if ( rc == 0 )
                    {
                        if ( FastqArgs.b_defline )
                        {
                            IF_BUF( ( Defline_Build( FastqArgs.b_defline, &def_data, self->b[0]->base,
                                      KDataBufferBytes( self->b[0] ), &self->bsz[0] ) ), self->b[0], self->bsz[0] );
                        }
                        else
                        {
                            IF_BUF( ( FastqReaderBaseName( self->reader, 0, &FastqArgs.dumpCs, self->b[0]->base,
                                      KDataBufferBytes( self->b[0] ), &self->bsz[0] ) ), self->b[0], self->bsz[0] );
                        }
                        self->bsz[ base_i ] = 0;
                        if ( !FastqArgs.fasta && rc == 0 )
                        {
                            if ( FastqArgs.q_defline )
                            {
                                IF_BUF( ( Defline_Build( FastqArgs.q_defline, &def_data, self->b[2]->base,
                                          KDataBufferBytes( self->b[2] ), &self->bsz[2] ) ), self->b[2], self->bsz[2] );
                            }
                            else
                            {
                                IF_BUF( ( FastqReaderQualityName( self->reader, 0, &FastqArgs.dumpCs, self->b[2]->base,
                                          KDataBufferBytes( self->b[2] ), &self->bsz[2] ) ), self->b[2], self->bsz[2] );
                            }
                            self->bsz[3] = 0;
                        }
                        else if ( ( (char*)(self->b[0]->base) )[0] == '@' )
                        {
                            ( (char*)(self->b[0]->base) )[0] = '>';
                        }
                    }

                    for ( readId = 1; rc == 0 && readId <= num_reads; readId++ )
                    {
                        IF_BUF( ( FastqReaderBase( self->reader, readId, &( (char*)(self->b[base_i]->base) )[ self->bsz[ base_i ] ],
                                  KDataBufferBytes( self->b[ base_i ] ) - self->bsz[ base_i ], &writ) ), self->b[ base_i ], self->bsz[ base_i ] + writ )
                        {
                            self->bsz[base_i] += writ;
                            if ( !FastqArgs.fasta )
                            {
                                IF_BUF( ( FastqReaderQuality( self->reader, readId, &( (char*)(self->b[3]->base) )[ self->bsz[3] ],
                                        KDataBufferBytes( self->b[3] ) - self->bsz[3], &writ ) ), self->b[3], self->bsz[3] + writ )
                                {
                                    self->bsz[ 3 ] += writ;
                                }
                            }
                        }
                    }

                    if ( rc == 0 )
                    {
                        if ( FastqArgs.fasta > 0 )
                        {
                            /* special printint of fasta-output ( line-wrap... ) */
                            rc = Fasta_dump( cself, self, spot, FastqArgs.fasta );
                        }
                        else
                        {
                            for ( readId = 0; rc == 0 && readId < 4; readId++ )
                            {
                                rc = SRASplitter_FileWrite( cself, spot, self->b[readId]->base, self->bsz[ readId ] );
                                if ( rc == 0 )
                                {
                                    rc = SRASplitter_FileWrite( cself, spot, "\n", 1 );
                                }
                            }
                        }
                    }
                }
            }
        }
        else if ( GetRCObject( rc ) == rcRow && GetRCState( rc ) == rcNotFound )
        {
            SRA_DUMP_DBG( 3, ( "%s skipped %u row\n", __func__, spot ) );
            rc = 0;
        }
    }
    return rc;
}


typedef struct FastqFormatterFactory_struct
{
    const char* accession;
    const SRATable* table;
    const FastqReader* reader;
    KDataBuffer buf[ 5 ]; /* fifth is for fasta line wrap */
} FastqFormatterFactory;


static rc_t FastqFormatterFactory_Init( const SRASplitterFactory* cself )
{
    rc_t rc = 0;
    FastqFormatterFactory* self = ( FastqFormatterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = FastqReaderMake( &self->reader, self->table, self->accession,
                     FastqArgs.dumpCs, FastqArgs.dumpOrigFmt, FastqArgs.fasta > 0, FastqArgs.dumpCs,
                     FastqArgs.readIds, !FastqArgs.applyClip, FastqArgs.SuppressQualForCSKey, 0,
                     FastqArgs.offset, FastqArgs.desiredCsKey[ 0 ], 0, 0 );
        if ( rc == 0 )
        {
            int i;
            for( i = 0; rc == 0 && i < sizeof( self->buf ) / sizeof( self->buf[ 0 ] ); i++ )
            {
                rc = KDataBufferMakeBytes( &self->buf[ i ], DATABUFFERINITSIZE );
            }
        }
    }
    return rc;
}


static rc_t FastqFormatterFactory_NewObj( const SRASplitterFactory* cself, const SRASplitter** splitter )
{
    rc_t rc = 0;
    FastqFormatterFactory* self = ( FastqFormatterFactory* )cself;

    if ( self == NULL )
    {
        rc = RC( rcExe, rcType, rcExecuting, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitter_Make ( splitter, sizeof( FastqFormatterSplitter ), NULL, NULL,
                                FastqArgs.split_spot ?
                                    FastqFormatterSplitter_DumpByRead :
                                    FastqFormatterSplitter_DumpBySpot, NULL );
        if ( rc == 0 )
        {
            int i;
            ( (FastqFormatterSplitter*)(*splitter) )->accession = self->accession;
            ( (FastqFormatterSplitter*)(*splitter) )->reader = self->reader;
            for ( i = 0; i < sizeof( self->buf ) / sizeof( self->buf[ 0 ] ); i++ )
            {
                ( (FastqFormatterSplitter*)(*splitter) )->b[ i ] = &self->buf[ i ];
            }
        }
    }
    return rc;
}


static void FastqFormatterFactory_Release( const SRASplitterFactory* cself )
{
    if ( cself != NULL )
    {
        int i;
        FastqFormatterFactory* self = ( FastqFormatterFactory* )cself;
        FastqReaderWhack( self->reader );
        for ( i = 0; i < sizeof( self->buf ) / sizeof( self->buf[ 0 ] ); i++ )
        {
            KDataBufferWhack( &self->buf[ i ] );
        }
    }
}


static rc_t FastqFormatterFactory_Make( const SRASplitterFactory** cself,
                const char* accession, const SRATable* table )
{
    rc_t rc = 0;
    FastqFormatterFactory* obj = NULL;

    if ( cself == NULL || accession == NULL || table == NULL )
    {
        rc = RC( rcExe, rcType, rcConstructing, rcParam, rcNull );
    }
    else
    {
        rc = SRASplitterFactory_Make( cself, eSplitterFormat, sizeof( *obj ),
                                      FastqFormatterFactory_Init,
                                      FastqFormatterFactory_NewObj,
                                      FastqFormatterFactory_Release );
        if ( rc == 0 )
        {
            obj = (FastqFormatterFactory*)*cself;
            obj->accession = accession;
            obj->table = table;
        }
    }
    return rc;
}


/* ### External entry points ##################################################### */


const char UsageDefaultName[] = "fastq-dump";


rc_t CC UsageSummary ( const char * progname )
{
    return 0;
}

#define H_splip_sot 0
#define H_clip 1
#define H_minReadLen 2
#define H_qual_filter 3
#define H_skip_tech 4
#define H_split_files 5
#define H_split_3 6
#define H_dumpcs 7
#define H_dumpbase 8
#define H_offset 9
#define H_fasta 10
#define H_origfmt 11
#define H_readids 12
#define H_helicos 13
#define H_defline_seq 14
#define H_defline_qual 15
#define H_aligned 16
#define H_unaligned 17
#define H_aligned_region 18
#define H_matepair_distance 19
#define H_qual_filter_1 20
#define H_SuppressQualForCSKey 21

rc_t FastqDumper_Usage( const SRADumperFmt* fmt, const SRADumperFmt_Arg* core_args, int first )
{
    int i, j;
    /* known core options */
    const SRADumperFmt_Arg* core[ 13 ];

    memset( (void*)core, 0, sizeof( core ) );
    for ( i = first; core_args[i].abbr != NULL || core_args[ i ].full != NULL; i++ )
    {
        const char* nm = core_args[ i ].abbr;
        nm = nm ? nm : core_args[ i ].full;
        if ( strcmp( nm, "A" ) == 0 )
        {
            core[ 0 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "O" ) == 0 )
        {
            core[ 1 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "N" ) == 0 )
        {
            core[ 2 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "X" ) == 0 )
        {
            core[ 3 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "G" ) == 0 )
        {
            core[ 4 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "spot-groups" ) == 0 )
        {
            core[ 5 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "R" ) == 0 )
        {
            core[ 6 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "T" ) == 0 )
        {
            core[ 7 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "K" ) == 0 )
        {
            core[ 8 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "table" ) == 0 )
        {
            core[ 9 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "Z" ) == 0 )
        {
            core[ 10 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "gzip" ) == 0 )
        {
            core[ 11 ] = &core_args[ i ];
        }
        else if ( strcmp( nm, "bzip2" ) == 0 )
        {
            core[ 12 ] = &core_args[ i ];
        }
    }

#define OARG(arg,msg) (arg ? HelpOptionLine((arg)->abbr, (arg)->full, (arg)->param, \
                       msg ? msg : (const char**)((arg)->descr)) : ((void)(0)))

    OUTMSG(( "INPUT\n" ));
    OARG( core[ 0 ], NULL );
    OARG( core[ 9 ], NULL );

    OUTMSG(( "\nPROCESSING\n" ));
    OUTMSG(( "\nRead Splitting                     Sequence data may be used in raw form or\n"));
    OUTMSG(( "                                     split into individual reads\n" ));
    OARG( &fmt->arg_desc[ 0 ], NULL);

    OUTMSG(( "\nFull Spot Filters                  Applied to the full spot independently\n" ));
    OUTMSG(( "                                     of --%s\n", fmt->arg_desc[0].full ));
    OARG( core[ 2 ], NULL );
    OARG( core[ 3 ], NULL );
    OARG( core[ 5 ], NULL );
    OARG( &fmt->arg_desc[ 1 ], NULL );

    OUTMSG(( "\nCommon Filters                     Applied to spots when --%s is not\n",
                                                  fmt->arg_desc[0].full ));
    OUTMSG(( "                                     set, otherwise - to individual reads\n" ));
    OARG( &fmt->arg_desc[ 2 ], NULL );
    OARG( core[ 6 ], NULL );
    OARG( &fmt->arg_desc[ 3 ], NULL );
    OARG( &fmt->arg_desc[ H_qual_filter_1 ], NULL );

    OUTMSG(( "\nFilters based on alignments        Filters are active when alignment\n" ));
    OUTMSG(( "                                     data are present\n" ));
    OARG( &fmt->arg_desc[ 16 ], NULL );
    OARG( &fmt->arg_desc[ 17 ], NULL );
    OARG( &fmt->arg_desc[ 18 ], NULL );
    OARG( &fmt->arg_desc[ 19 ], NULL );

    OUTMSG(( "\nFilters for individual reads       Applied only with --%s set\n",
                                                  fmt->arg_desc[0].full ));
    OARG( &fmt->arg_desc[ 4 ], NULL );

    OUTMSG(( "\nOUTPUT\n" ));
    OARG( core[ 1 ], NULL );
    OARG( core[ 10 ], NULL );
    OARG( core[ 11 ], NULL  );
    OARG( core[ 12 ], NULL);

    OUTMSG(( "\nMultiple File Options              Setting these options will produce more\n" ));
    OUTMSG(( "                                     than 1 file, each of which will be suffixed\n" ));
    OUTMSG(( "                                     according to splitting criteria.\n" ));
    OARG( &fmt->arg_desc[ 5 ], NULL );
    OARG( &fmt->arg_desc[ 6 ], NULL );
    OARG( core[ 4 ], NULL );
    OARG( core[ 6 ], NULL );
    OARG( core[ 7 ], NULL );
    OARG( core[ 8 ], NULL );

    OUTMSG(( "\nFORMATTING\n" ));
    OUTMSG(( "\nSequence\n" ));
    OARG( &fmt->arg_desc[ 7 ], NULL );
    OARG( &fmt->arg_desc[ 8 ], NULL );

    OUTMSG(( "\nQuality\n" ));
    OARG( &fmt->arg_desc[ 9 ], NULL );
    OARG( &fmt->arg_desc[ 10 ], NULL );
    OARG( &fmt->arg_desc[ H_SuppressQualForCSKey ], NULL ); /* added Jan 15th 2014 ( a new fastq-variation! ) */

    OUTMSG(( "\nDefline\n" ));
    OARG( &fmt->arg_desc[ 11 ], NULL );
    OARG( &fmt->arg_desc[ 12 ], NULL );
    OARG( &fmt->arg_desc[ 13 ], NULL );
    OARG( &fmt->arg_desc[ 14 ], NULL );
    OARG( &fmt->arg_desc[ 15 ], NULL );

    OUTMSG(( "OTHER:\n" ));
    for ( i = first; core_args[ i ].abbr != NULL || core_args[ i ].full != NULL; i++ )
    {
        bool print = true;
        for ( j = 0; j < sizeof( core ) / sizeof( core[ 0 ] ); j++ )
        {
            if ( core[ j ] == &core_args[ i ] )
            {
                print = false;
                break;
            }
        }
        if ( print )
        {
            OARG( &core_args[ i ], NULL );
        }
    }
    return 0;
}


rc_t FastqDumper_Release( const SRADumperFmt* fmt )
{
    if ( fmt == NULL )
    {
        return RC( rcExe, rcFormatter, rcDestroying, rcParam, rcInvalid );
    }
    else
    {
        Defline_Release( FastqArgs.b_defline );
        Defline_Release( FastqArgs.q_defline );
        free( FastqArgs.alregion );
    }
    return 0;
}


bool FastqDumper_AddArg( const SRADumperFmt* fmt, GetArg* f, int* i, int argc, char *argv[] )
{
    const char* arg = NULL;

    if ( fmt == NULL || f == NULL || i == NULL || argv == NULL )
    {
        LOGERR( klogErr, RC( rcExe, rcArgv, rcReading, rcParam, rcInvalid ), "null param" );
        return false;
    }
    else if ( f( fmt, "M", "minReadLen", i, argc, argv, &arg ) )
    {
        FastqArgs.minReadLen = AsciiToU32( arg, NULL, NULL );
    }
    else if ( f( fmt, "W", "clip", i, argc, argv, NULL ) )
    {
        FastqArgs.applyClip = true;
    }
    else if ( f( fmt, "SU", "suppress-qual-for-cskey", i, argc, argv, NULL ) )
    {
        FastqArgs.SuppressQualForCSKey = true;
    }
    else if ( f( fmt, "F", "origfmt", i, argc, argv, NULL ) )
    {
        FastqArgs.dumpOrigFmt = true;
    }
    else if ( f( fmt, "C", "dumpcs", i, argc, argv, NULL ) )
    {
        int k = ( *i ) + 1;
        FastqArgs.dumpCs = true;
        if ( k < argc && strlen( argv[ k ] ) == 1 && strchr( "acgtACGT", argv[ k ][ 0 ] ) != NULL )
        {
            FastqArgs.desiredCsKey = argv[ k ];
            *i = k;
        }
    }
    else if ( f( fmt, "B", "dumpbase", i, argc, argv, NULL ) )
    {
        FastqArgs.dumpBase = true;
    }
    else if ( f( fmt, "Q", "offset", i, argc, argv, &arg ) )
    {
        FastqArgs.offset = AsciiToU32( arg, NULL, NULL );
    }
    else if ( f( fmt, "I", "readids", i, argc, argv, NULL ) )
    {
        FastqArgs.readIds = true;
    }
    else if ( f( fmt, "E", "qual-filter", i, argc, argv, NULL ) )
    {
        FastqArgs.qual_filter = true;
    }
    else if ( f( fmt, "E1", "qual-filter-1", i, argc, argv, NULL ) )
    {
        FastqArgs.qual_filter1 = true;
    }
    else if ( f( fmt, "DB", "defline-seq", i, argc, argv, &arg ) )
    {
        FastqArgs.b_deffmt = arg;
    }
    else if ( f( fmt, "DQ", "defline-qual", i, argc, argv, &arg ) )
    {
        FastqArgs.q_deffmt = arg;
    }
    else if ( f( fmt, "TR", "skip-technical", i, argc, argv, NULL ) )
    {
        FastqArgs.skipTechnical = true;
    }
    else if ( f( fmt, "SF", "split-files", i, argc, argv, NULL ) )
    {
        SRADumperFmt * nc_fmt = ( SRADumperFmt * )fmt;
        FastqArgs.split_spot = true;
        FastqArgs.split_files = true;
        nc_fmt->split_files = true;
    }
    else if ( f( fmt, NULL, "split-3", i, argc, argv, NULL ) )
    {
        FastqArgs.split_3 = true;
        FastqArgs.split_spot = true;
        FastqArgs.maxReads = 2;
        FastqArgs.skipTechnical = true;
    }
    else if ( f( fmt, "SL", "split-spot", i, argc, argv, NULL ) )
    {
        FastqArgs.split_spot = true;
    }
    else if ( f( fmt, "HS", "helicos", i, argc, argv, NULL ) )
    {
        FastqArgs.b_deffmt = "@$sn";
        FastqArgs.q_deffmt =  "+";
    }
    else if ( f( fmt, "FA", "fasta", i, argc, argv, NULL ) )
    {
        int k = (*i) + 1;
        FastqArgs.fasta = 70;
        if ( k < argc && isdigit( argv[ k ][ 0 ] ) )
        {
            char* endp = NULL;
            errno = 0;
            FastqArgs.fasta = strtou64( argv[ k ], &endp, 10 );
            if ( errno != 0 || endp == NULL || *endp != '\0' )
            {
                return false;
            }
            *i = k;
            if ( FastqArgs.fasta == 0 )
            {
                FastqArgs.fasta = ~0;
            }
        }
        FastqArgs.file_extension = ".fasta";
    }
    else if ( f( fmt, NULL, "aligned", i, argc, argv, NULL ) )
    {
        FastqArgs.aligned = true;
    }
    else if ( f( fmt, NULL, "unaligned", i, argc, argv, NULL ) )
    {
        FastqArgs.unaligned = true;
    }
    else if ( f( fmt, NULL, "aligned-region", i, argc, argv, &arg ) )
    {
        /* name[:[from][-[to]]] */
        TAlignedRegion* r = realloc( FastqArgs.alregion, sizeof( *FastqArgs.alregion ) * ++FastqArgs.alregion_qty );
        if ( r == NULL )
        {
            return false;
        }
        FastqArgs.alregion = r;
        r = &FastqArgs.alregion[ FastqArgs.alregion_qty - 1 ];
        r->name = strchr( arg, ':' );
        r->from = 0;
        r->to = 0;
        if ( r->name == NULL )
        {
            r->name = arg;
            r->name_len = strlen( arg );
        }
        else
        {
            const char* c;
            uint64_t* v;

            r->name_len = r->name - arg;
            c = r->name;
            r->name = arg;

            v = &r->from;
            while ( *++c != '\0')
            {
                if ( *c == '-' )
                {
                    v = &r->to;
                }
                else if ( *c == '+' )
                {
                    if ( *v != 0 )
                    {
                        return false;
                    }
                }
                else if ( !isdigit( *c ) )
                {
                    return false;
                }
                else
                {
                    *v = *v * 10 + ( *c - '0' );
                }
            }
            if ( r->from > r->to && r->to != 0 )
            {
                uint64_t x = r->from;
                r->from = r->to;
                r->to = x;
            }
        }
    }
    else if ( f( fmt, NULL, "matepair-distance", i, argc, argv, &arg ) )
    {
        /* from[-to] | [from]-to | unknown */
        if ( strcmp( arg, "unknown" ) == 0 )
        {
            FastqArgs.mp_dist_unknown = true;
        }
        else
        {
            uint64_t* v;
            TMatepairDistance* p = realloc( FastqArgs.mp_dist, sizeof( *FastqArgs.mp_dist ) * ++FastqArgs.mp_dist_qty );
            if ( p == NULL )
            {
                return false;
            }
            FastqArgs.mp_dist = p;
            p = &FastqArgs.mp_dist[ FastqArgs.mp_dist_qty - 1 ];
            p->from = 0;
            p->to = 0;

            v = &p->from;
            while ( *++arg != '\0' )
            {
                if ( *arg == '-' )
                {
                    v = &p->to;
                }
                else if ( *arg == '+' )
                {
                    if ( *v != 0 )
                    {
                        return false;
                    }
                }
                else if ( !isdigit( *arg ) )
                {
                    return false;
                }
                else
                {
                    *v = *v * 10 + ( *arg - '0' );
                }
            }
            if ( p->from > p->to && p->to != 0 )
            {
                uint64_t x = p->from;
                p->from = p->to;
                p->to = x;
            }
            if ( p->from == 0 && p->to == 0 )
            {
                FastqArgs.mp_dist_qty--;
            }
        }
    }
    else
    {
        return false;
    }
    return true;
}


rc_t FastqDumper_Factories( const SRADumperFmt* fmt, const SRASplitterFactory** factory )
{
    rc_t rc = 0;
    const SRASplitterFactory* parent = NULL, *child = NULL;

    if ( fmt == NULL || factory == NULL )
    {
        return RC( rcExe, rcFormatter, rcReading, rcParam, rcInvalid );
    }
    *factory = NULL;
    {
        const SRAColumn* c = NULL;
        rc = SRATableOpenColumnRead( fmt->table, &c, "PLATFORM", sra_platform_id_t );
        if ( rc == 0 )
        {
            const INSDC_SRA_platform_id * platform;
            bitsz_t o, z;
            rc = SRAColumnRead( c, 1, ( const void ** )&platform, &o, &z );
            if ( rc == 0 )
            {
                if ( !FastqArgs.dumpCs && !FastqArgs.dumpBase )
                {
                    if ( platform != NULL && *platform == SRA_PLATFORM_ABSOLID )
                    {
                        FastqArgs.dumpCs = true;
                    }
                    else
                    {
                        FastqArgs.dumpBase = true;
                    }
                }
                if ( platform != NULL && *platform == SRA_PLATFORM_ABSOLID )
                {
                    FastqArgs.is_platform_cs_native = true;
                }
            }
            SRAColumnRelease( c );
        }
        else if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == ( enum RCObject )rcColumn )
        {
            rc = 0;
        }
    }

    if ( ( FastqArgs.aligned || FastqArgs.unaligned ) && !( FastqArgs.aligned && FastqArgs.unaligned ) )
    {
        rc = AlignedFilterFactory_Make( &child, fmt->table, FastqArgs.aligned, FastqArgs.unaligned );
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release(child);
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( FastqArgs.alregion_qty > 0 )
    {
        rc = AlignRegionFilterFactory_Make( &child, fmt->table, FastqArgs.alregion, FastqArgs.alregion_qty );
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release( child );
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( FastqArgs.mp_dist_unknown || FastqArgs.mp_dist_qty > 0 )
    {
        rc = AlignPairDistanceFilterFactory_Make( &child, fmt->table,
                FastqArgs.mp_dist_unknown, FastqArgs.mp_dist, FastqArgs.mp_dist_qty);
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release(child);
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( rc == 0 && FastqArgs.skipTechnical && FastqArgs.split_spot )
    {
        rc = FastqBioFilterFactory_Make( &child, fmt->accession, fmt->table );
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release( child );
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( rc == 0 && FastqArgs.maxReads > 0 )
    {
        rc = FastqRNumberFilterFactory_Make( &child, fmt->accession, fmt->table );
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release( child );
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( rc == 0 && ( FastqArgs.qual_filter || FastqArgs.qual_filter1 ) )
    {
        rc = FastqQFilterFactory_Make( &child, fmt->accession, fmt->table );
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release( child );
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( rc == 0 )
    {
        rc = FastqReadLenFilterFactory_Make( &child, fmt->accession, fmt->table );
        if ( rc == 0 )
        {
            if ( parent != NULL )
            {
                rc = SRASplitterFactory_AddNext( parent, child );
                if ( rc != 0 )
                {
                    SRASplitterFactory_Release( child );
                }
                else
                {
                    parent = child;
                }
            }
            else
            {
                parent = child;
                *factory = parent;
            }
        }
    }

    if ( rc == 0 )
    {
        if ( FastqArgs.split_3 )
        {
            rc = Fastq3ReadSplitterFactory_Make( &child, fmt->accession, fmt->table );
            if ( rc == 0 )
            {
                if ( parent != NULL )
                {
                    rc = SRASplitterFactory_AddNext( parent, child );
                    if ( rc != 0 )
                    {
                        SRASplitterFactory_Release( child );
                    }
                    else
                    {
                        parent = child;
                    }
                }
                else
                {
                    parent = child;
                    *factory = parent;
                }
            }
        }
        else if ( FastqArgs.split_files )
        {
            rc = FastqReadSplitterFactory_Make( &child, fmt->accession, fmt->table );
            if ( rc == 0 )
            {
                if ( parent != NULL )
                {
                    rc = SRASplitterFactory_AddNext( parent, child );
                    if ( rc != 0 )
                    {
                        SRASplitterFactory_Release(child);
                    }
                    else
                    {
                        parent = child;
                    }
                }
                else
                {
                    parent = child;
                    *factory = parent;
                }
            }
        }
    }

    if ( rc == 0 )
    {
        if ( FastqArgs.b_deffmt != NULL )
        {
            rc = Defline_Parse( &FastqArgs.b_defline, FastqArgs.b_deffmt );
        }
        if ( FastqArgs.q_deffmt != NULL )
        {
            rc = Defline_Parse( &FastqArgs.q_defline, FastqArgs.q_deffmt );
        }
        if ( rc == 0 )
        {
            rc = FastqFormatterFactory_Make( &child, fmt->accession, fmt->table );
            if ( rc == 0 )
            {
                if ( parent != NULL )
                {
                    rc = SRASplitterFactory_AddNext( parent, child );
                    if ( rc != 0 )
                    {
                        SRASplitterFactory_Release( child );
                    }
                }
                else
                {
                    *factory = child;
                }
            }
        }
    }
    return rc;
}

/* main entry point of the file */
rc_t SRADumper_Init( SRADumperFmt* fmt )
{
    static const SRADumperFmt_Arg arg[] =
        {

            /* DO NOT ADD IN THE MIDDLE ORDER IS IMPORTANT IN USAGE FUNCTION ABOVE!!! */
            {NULL, "split-spot", NULL, {"Split spots into individual reads", NULL}},            /* H_splip_sot = 0 */

            {"W", "clip", NULL, {"Remove adapter sequences from reads", NULL}},                          /* H_clip = 1 */

            {"M", "minReadLen", "len", {"Filter by sequence length >= <len>", NULL}},           /* H_minReadLen = 2 */
            {"E", "qual-filter", NULL, {"Filter used in early 1000 Genomes data:",              /* H_qual_filter = 3 */
                                        "no sequences starting or ending with >= 10N", NULL}},

            {NULL, "skip-technical", NULL, {"Dump only biological reads", NULL}},               /* H_skip_tech = 4 */

            {NULL, "split-files", NULL, {"Write reads into separate files. "                /* H_split_files = 5 */
                                         "Read number will be suffixed to the file name. ",
                                         "NOTE! The `--split-3` option is recommended.",
                                         "In cases where not all spots have the same number of reads,",
                                         "this option will produce files",
                                         "that WILL CAUSE ERRORS in most programs",
                                         "which process split pair fastq files.", NULL}},

            /* DO NOT ADD IN THE MIDDLE ORDER IS IMPORTANT IN USAGE FUNCTION ABOVE!!! */
            {NULL, "split-3", NULL, {"3-way splitting for mate-pairs.",          /* H_split_3 = 6 */
                                     "For each spot, if there are two biological reads satisfying filter conditions,",
                                     "the first is placed in the `*_1.fastq` file,",
                                     "and the second is placed in the `*_2.fastq` file.",
                                     "If there is only one biological read satisfying the filter conditions,",
                                     "it is placed in the `*.fastq` file.All other reads in the spot are ignored.",
                                     NULL}},

            {"C", "dumpcs", "[cskey]", {"Formats sequence using color space (default for SOLiD),"   /* H_dumpcs = 7 */
                                        "\"cskey\" may be specified for translation", NULL}},
            {"B", "dumpbase", NULL, {"Formats sequence using base space (default for other than SOLiD).", NULL}}, /* H_dumpbase = 8 */

            /* DO NOT ADD IN THE MIDDLE ORDER IS IMPORTANT IN USAGE FUNCTION ABOVE!!! */
            {"Q", "offset", "integer", {"Offset to use for quality conversion, default is 33", NULL}},  /* H_offset = 9 */
            {NULL, "fasta", "[line width]", {"FASTA only, no qualities, optional line wrap width (set to zero for no wrapping)", NULL}}, /* H_fasta = 10 */

            {"F", "origfmt", NULL, {"Defline contains only original sequence name", NULL}}, /* H_origfmt = 11 */
            {"I", "readids", NULL, {"Append read id after spot id as 'accession.spot.readid' on defline", NULL}}, /* H_readids = 12 */
            {NULL, "helicos", NULL, {"Helicos style defline", NULL}}, /* H_helicos = 13 */
            {NULL, "defline-seq", "fmt", {"Defline format specification for sequence.", NULL}}, /* H_defline_seq = 14 */
            {NULL, "defline-qual", "fmt", {"Defline format specification for quality.", /* H_defline_qual = 15 */
                              "<fmt> is a string of characters and/or variables. The variables can be one of:",
                              "$ac - accession, $si - spot id, $sn - spot name, $sg - spot group (barcode),",
                              "$sl - spot length in bases, $ri - read number, $rn - read name, $rl - read length in bases.",
                              "'[]' could be used for an optional output: if all vars in [] yield empty values whole group is not printed.",
                              "Empty value is empty string or 0 for numeric variables.",
                              "Ex: @$sn[_$rn]/$ri - '_$rn' is omitted if name is empty\n", NULL}},

            {NULL, "aligned", NULL, {"Dump only aligned sequences", NULL}}, /* H_aligned = 16 */
            {NULL, "unaligned", NULL, {"Dump only unaligned sequences", NULL}}, /* H_unaligned = 17 */

            /* DO NOT ADD IN THE MIDDLE ORDER IS IMPORTANT IN USAGE FUNCTION ABOVE!!! */
            {NULL, "aligned-region", "name[:from-to]", {"Filter by position on genome.", /* H_aligned_region = 18 */
                            "Name can either be accession.version (ex: NC_000001.10) or",
                            "file specific name (ex: \"chr1\" or \"1\").",
                            "\"from\" and \"to\" are 1-based coordinates", NULL}},
            {NULL, "matepair-distance", "from-to|unknown", {"Filter by distance between matepairs.", /* H_matepair-distance = 19 */
                            "Use \"unknown\" to find matepairs split between the references.",
                            "Use from-to to limit matepair distance on the same reference", NULL}},

            {NULL, "qual-filter-1", NULL, {"Filter used in current 1000 Genomes data", NULL}}, /* H_qual_filter_1 = 20 */

            {NULL, "suppress-qual-for-cskey", NULL, {"suppress quality-value for cskey", NULL}}, /* H_SuppressQualForCSKey = 21 */

            {NULL, NULL, NULL, {NULL}}
        };

    if ( fmt == NULL )
    {
        return RC( rcExe, rcFileFormat, rcConstructing, rcParam, rcNull );
    }

    memset( &FastqArgs, 0, sizeof( FastqArgs ) );
    FastqArgs.is_platform_cs_native = false;
    FastqArgs.maxReads = 0;
    FastqArgs.skipTechnical = false;
    FastqArgs.minReadLen = 1;
    FastqArgs.applyClip = false;
    FastqArgs.SuppressQualForCSKey = false;     /* added Jan 15th 2014 ( a new fastq-variation! ) */
    FastqArgs.dumpOrigFmt = false;
    FastqArgs.dumpBase = false;
    FastqArgs.dumpCs = false;
    FastqArgs.readIds = false;
    FastqArgs.offset = 33;
    FastqArgs.desiredCsKey = "";
    FastqArgs.qual_filter = false;
    FastqArgs.b_deffmt = NULL;
    FastqArgs.b_defline = NULL;
    FastqArgs.q_deffmt = NULL;
    FastqArgs.q_defline = NULL;
    FastqArgs.split_files = false;
    FastqArgs.split_3 = false;
    FastqArgs.split_spot = false;
    FastqArgs.fasta = 0;
    FastqArgs.file_extension = ".fastq";
    FastqArgs.aligned = false;
    FastqArgs.unaligned = false;
    FastqArgs.alregion = NULL;
    FastqArgs.alregion_qty = 0;
    FastqArgs.mp_dist = NULL;
    FastqArgs.mp_dist_unknown = false;
    FastqArgs.mp_dist_qty = 0;

    fmt->usage = FastqDumper_Usage;
    fmt->release = FastqDumper_Release;
    fmt->arg_desc = arg;
    fmt->add_arg = FastqDumper_AddArg;
    fmt->get_factory = FastqDumper_Factories;
    fmt->gzip = true;
    fmt->bzip2 = true;
    fmt->split_files = false;

    return 0;
}
