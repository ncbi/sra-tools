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

#include "cg_tools.h"
#include "debug.h"
#include <klib/out.h>

#include <klib/printf.h>
#include <sysalloc.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


static void SetCigOp( CigOps * dst, char op, uint32_t oplen )
{
    dst->op    = op;
    dst->oplen = oplen;
    switch( op ) 
    { /*MX= DN B IS PH*/

    case 'M' :
    case 'X' :
    case '=' :  dst->ref_sign = +1;
                dst->seq_sign = +1;
                break;

    case 'D' :
    case 'N' :  dst->ref_sign = +1;
                dst->seq_sign = 0;
                break;

    case 'B' :  dst->ref_sign = -1;
                dst->seq_sign = 0;
                break;

    case 'S' :
    case 'I' :  dst->ref_sign = 0;
                dst->seq_sign = +1;
                break;

    case 'P' :
    case 'H' :
    case 0   :  dst->ref_sign= 0;   /* terminating op */
                dst->seq_sign= 0;
                break;

    default :   assert( 0 );
                break;
    }
}


int32_t ExplodeCIGAR( CigOps dst[], uint32_t len, char const cigar[], uint32_t ciglen )
{
    uint32_t i;
    uint32_t j;
    int32_t opLen;
    
    for ( i = j = opLen = 0; i < ciglen; ++i )
    {
        if ( isdigit( cigar[ i ] ) )
        {
            opLen = opLen * 10 + cigar[ i ] - '0';
        }
        else
        {
            assert( isalpha( cigar[ i ] ) );
            SetCigOp( dst + j, cigar[ i ], opLen );
            opLen = 0;
            j++;
        }
    }
    SetCigOp( dst + j, 0, 0 );
    j++;
    return j;
}


typedef struct cgOp_s
{
    uint16_t length;
    uint8_t type; /* 0: match, 1: insert, 2: delete */
    char code;
} cgOp;


static void print_CG_cigar( int line, cgOp const op[], unsigned const ops, unsigned const gap[ 3 ] )
{
#if _DEBUGGING
    unsigned i;
    
    SAM_DUMP_DBG( 3, ( "%5i: ", line ) );
    for ( i = 0; i < ops; ++i )
    {
        if ( gap && ( i == gap[ 0 ] || i == gap[ 1 ] || i == gap[ 2 ] ) )
        {
            SAM_DUMP_DBG( 3, ( "%u%c", op[ i ].length, tolower( op[ i ].code ) ) );
        }
        else
        {
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

typedef struct cg_cigar_temp
{
    unsigned gap[ 3 ];
    cgOp cigOp[ MAX_READ_LEN ];
    unsigned opCnt;
    unsigned S_adjust;
    unsigned CG_adjust;
    bool     has_ref_offset_type;
} cg_cigar_temp;


static rc_t CIGAR_to_CG_Ops( const cg_cigar_input * input,
                             cg_cigar_temp * tmp )
{
    unsigned i;
    unsigned ops = 0;
    unsigned gapno;

    tmp->opCnt = 0;
    tmp->S_adjust = 0;
    tmp->CG_adjust = 0;
    tmp->has_ref_offset_type = false;
    for ( i = 0; i < input->p_cigar.len; ++ops )
    {
        char opChar = 0;
        int opLen = 0;
        int n;

        for ( n = 0; ( ( n + i ) < input->p_cigar.len ) && ( isdigit( input->p_cigar.ptr[ n + i ] ) ); n++ )
        {
            opLen = opLen * 10 + input->p_cigar.ptr[ n + i ] - '0';
        }

        if ( ( n + i ) < input->p_cigar.len )
        {
            opChar = input->p_cigar.ptr[ n + i ];
            n++;
        }

        if ( ( ops + 1 ) >= MAX_READ_LEN )
            return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );

        i += n;
        
        tmp->cigOp[ ops ].length = opLen;
        tmp->cigOp[ ops ].code = opChar;
        switch ( opChar )
        {
            case 'M' :
            case '=' :
            case 'X' :  tmp->cigOp[ ops ].type = 0;
                        break;

            case 'S' :  tmp->S_adjust += opLen;
            case 'I' :  tmp->cigOp[ ops ].type = 1;
                        tmp->cigOp[ ops ].code = 'I';
                        break;
	    case 'N':   tmp->has_ref_offset_type = true;
            case 'D':   tmp->cigOp[ ops ].type = 2;
                        break;

            default :   return RC( rcExe, rcData, rcReading, rcConstraint, rcViolated );
        }
    }

    tmp->opCnt = ops;
    tmp->gap[ 0 ] = tmp->gap[ 1 ] = tmp->gap[ 2 ] = ops;
    print_CG_cigar( __LINE__, tmp->cigOp, ops, NULL );


    if ( ops < 3 )
        return RC( rcExe, rcData, rcReading, rcFormat, rcNotFound ); /* CG pattern not found */

    {
        unsigned fwd = 0; /* 5 10 10 10 */
        unsigned rev = 0; /* 10 10 10 5 */
        unsigned acc; /* accumulated length */
        
        if ( ( input->seq_req_id == 1 && !input->orientation ) ||
             ( input->seq_req_id == 2 && input->orientation ) )
        {
            for ( i = 0, acc = 0; i < ops  && acc <= 5; ++i )
            {
                if ( tmp->cigOp[ i ].type != 2 )
                {
                    acc += tmp->cigOp[ i ].length;
                    if ( acc == 5 && tmp->cigOp[ i + 1 ].type == 1 )
                    {
                        fwd = i + 1;
                        break;
                    }
                    else if ( acc > 5 )
                    {
                        unsigned const right = acc - 5;
                        unsigned const left = tmp->cigOp[ i ].length - right;
                        
                        memmove( &tmp->cigOp[ i + 2 ], &tmp->cigOp[ i ], ( ops - i ) * sizeof( tmp->cigOp[ 0 ] ) );
                        ops += 2;
                        tmp->cigOp[ i ].length = left;
                        tmp->cigOp[ i + 2 ].length = right;
                        
                        tmp->cigOp[ i + 1 ].type = 1;
                        tmp->cigOp[ i + 1 ].code = 'B';
                        tmp->cigOp[ i + 1 ].length = 0;
                        
                        fwd = i + 1;
                        break;
                    }
                }
            }
        }
        else if ( ( input->seq_req_id == 2 && !input->orientation ) ||
                  ( input->seq_req_id == 1 && input->orientation ) )
        {
            for ( i = ops, acc = 0; i > 0 && acc <= 5; )
            {
                --i;
                if ( tmp->cigOp[ i ].type != 2 )
                {
                    acc += tmp->cigOp[ i ].length;
                    if ( acc == 5 && tmp->cigOp[ i ].type == 1 )
                    {
                        rev = i;
                        break;
                    }
                    else if ( acc > 5 )
                    {
                        unsigned const left = acc - 5;
                        unsigned const right = tmp->cigOp[ i ].length - left;
                        
                        memmove( &tmp->cigOp[ i + 2 ], &tmp->cigOp[ i ], ( ops - i ) * sizeof( tmp->cigOp[ 0 ] ) );
                        ops += 2;
                        tmp->cigOp[ i ].length = left;
                        tmp->cigOp[ i + 2 ].length = right;
                        
                        tmp->cigOp[ i + 1 ].type = 1;
                        tmp->cigOp[ i + 1 ].code = 'B';
                        tmp->cigOp[ i + 1 ].length = 0;
                         
                        rev = i + 1;
                        break;
                    }
                }
            }
        }
        else
        {
            /* fprintf(stderr, "guessing layout\n"); */
            for ( i = 0, acc = 0; i < ops  && acc <= 5; ++i )
            {
                if ( tmp->cigOp[ i ].type != 2 )
                {
                    acc += tmp->cigOp[ i ].length;
                    if ( acc == 5 && tmp->cigOp[ i + 1 ].type == 1 )
                    {
                        fwd = i + 1;
                    }
                }
            }
            for ( i = ops, acc = 0; i > 0 && acc <= 5; )
            {
                --i;
                if ( tmp->cigOp[ i ].type != 2 )
                {
                    acc += tmp->cigOp[ i ].length;
                    if ( acc == 5 && tmp->cigOp[i].type == 1 )
                    {
                        rev = i;
                    }
                }
            }
            if ( !tmp->has_ref_offset_type && (( fwd == 0 && rev == 0 ) || ( fwd != 0 && rev != 0 ) ))
            {
                for ( i = 0; i < ops; ++i )
                {
                    if ( tmp->cigOp[ i ].type == 2 )
                    {
                        tmp->cigOp[ i ].code = 'N';
                        tmp->CG_adjust += tmp->cigOp[ i ].length;
                    }
                }
                return RC( rcExe, rcData, rcReading, rcFormat, rcNotFound ); /* CG pattern not found */
            }
        }
        if ( fwd && tmp->cigOp[ fwd ].type == 1 )
        {
            for ( i = ops, acc = 0; i > fwd + 1; )
            {
                --i;
                if ( tmp->cigOp[ i ].type != 2 )
                {
                    acc += tmp->cigOp[ i ].length;
                    if ( acc >= 10 )
                    {
                        if ( acc > 10 )
                        {
                            unsigned const r = 10 + tmp->cigOp[ i ].length - acc;
                            unsigned const l = tmp->cigOp[ i ].length - r;
                            
                            if ( ops + 2 >= MAX_READ_LEN )
                                return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                            memmove( &tmp->cigOp[ i + 2 ], &tmp->cigOp[ i ], ( ops - i ) * sizeof( tmp->cigOp[ 0 ] ) );
                            ops += 2;
                            tmp->cigOp[ i + 2 ].length = r;
                            tmp->cigOp[ i ].length = l;
                            
                            tmp->cigOp[ i + 1 ].length = 0;
                            tmp->cigOp[ i + 1 ].type = 2;
                            tmp->cigOp[ i + 1 ].code = 'N';
                            i += 2;
                        }
                        else if ( i - 1 > fwd )
                        {
                            if ( tmp->cigOp[ i - 1 ].type == 2 )
                                 tmp->cigOp[ i - 1 ].code = 'N';
                            else
                            {
                                if ( ops + 1 >= MAX_READ_LEN )
                                    return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                                memmove( &tmp->cigOp[ i + 1 ], &tmp->cigOp[ i ], ( ops - i ) * sizeof( tmp->cigOp[ 0 ] ) );
                                ops += 1;
                                tmp->cigOp[ i ].length = 0;
                                tmp->cigOp[ i ].type = 2;
                                tmp->cigOp[ i ].code = 'N';
                                i += 1;
                            }
                        }
                        acc = 0;
                    }
                }
            }
            /** change I to B+M **/
            tmp->cigOp[ fwd ].code = 'B';
            memmove( &tmp->cigOp[ fwd + 1 ], &tmp->cigOp[ fwd ], ( ops - fwd ) * sizeof( tmp->cigOp[ 0 ] ) );
            ops += 1;
            tmp->cigOp[ fwd + 1 ].code = 'M';
            tmp->opCnt = ops;
            /** set the gaps now **/
            for ( gapno = 3, i = ops; gapno > 1 && i > 0; )
            {
                --i;
                if ( tmp->cigOp[ i ].code == 'N' )
                    tmp->gap[ --gapno ] = i;
            }
            tmp->gap[ 0 ] = fwd;
            print_CG_cigar( __LINE__, tmp->cigOp, ops, tmp->gap );
            return 0;
        }
        if ( rev && tmp->cigOp[ rev ].type == 1 )
        {
            for ( acc = i = 0; i < rev; ++i )
            {
                if ( tmp->cigOp[ i ].type != 2 )
                {
                    acc += tmp->cigOp[ i ].length;
                    if ( acc >= 10 )
                    {
                        if ( acc > 10 )
                        {
                            unsigned const l = 10 + tmp->cigOp[ i ].length - acc;
                            unsigned const r = tmp->cigOp[ i ].length - l;
                            
                            if ( ops + 2 >= MAX_READ_LEN )
                                return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                            memmove( &tmp->cigOp[ i + 2 ], &tmp->cigOp[ i ], ( ops - i ) * sizeof( tmp->cigOp[ 0 ] ) );
                            ops += 2;
                            tmp->cigOp[ i + 2 ].length = r;
                            tmp->cigOp[ i ].length = l;
                            
                            tmp->cigOp[ i + 1 ].length = 0;
                            tmp->cigOp[ i + 1 ].type = 2;
                            tmp->cigOp[ i + 1 ].code = 'N';
                            rev += 2;
                            i += 2;
                        }
                        else if ( i + 1 < rev )
                        {
                            if ( tmp->cigOp[ i + 1 ].type == 2 )
                                 tmp->cigOp[ i + 1 ].code = 'N';
                            else
                            {
                                if ( ops + 1 >= MAX_READ_LEN )
                                    return RC( rcExe, rcData, rcReading, rcBuffer, rcInsufficient );
                                memmove( &tmp->cigOp[ i + 1 ], &tmp->cigOp[ i ], ( ops - i ) * sizeof( tmp->cigOp[ 0 ] ) );
                                ops += 1;
                                tmp->cigOp[ i + 1 ].length = 0;
                                tmp->cigOp[ i + 1 ].type = 2;
                                tmp->cigOp[ i + 1 ].code = 'N';
                                rev += 1;
                                i += 1;
                            }
                        }
                        acc = 0;
                    }
                }
            }
            for ( gapno = 3, i = 0; gapno > 1 && i < ops; ++i )
            {
                if ( tmp->cigOp[ i ].code == 'N' )
                    tmp->gap[ --gapno ] = i;
            }
            tmp->gap[ 0 ] = rev;
            tmp->cigOp[ rev ].code = 'B';
            memmove( &tmp->cigOp[ rev + 1 ], &tmp->cigOp[ rev ], ( ops - rev ) * sizeof( tmp->cigOp[ 0 ] ) );
            ops += 1;
            tmp->cigOp[ rev + 1 ].code = 'M';
            tmp->opCnt = ops;
            print_CG_cigar( __LINE__, tmp->cigOp, ops, tmp->gap );
            return 0;
        }
    }
    return RC( rcExe, rcData, rcReading, rcFormat, rcNotFound ); /* CG pattern not found */
}


static rc_t adjust_cigar( const cg_cigar_input * input, cg_cigar_temp * tmp, cg_cigar_output * output )
{
    rc_t rc = 0;
    unsigned i, j;

    print_CG_cigar( __LINE__, tmp->cigOp, tmp->opCnt, NULL );

    /* remove zero length ops */
    for ( j = i = 0; i < tmp->opCnt; )
    {
        if ( tmp->cigOp[ j ].length == 0 )
        {
            ++j;
            --(tmp->opCnt);
            continue;
        }
        tmp->cigOp[ i++ ] = tmp->cigOp[ j++ ];
    }

    print_CG_cigar( __LINE__, tmp->cigOp, tmp->opCnt, NULL );

    if ( input->edit_dist_available )
    {
        int const adjusted = input->edit_dist + tmp->S_adjust - tmp->CG_adjust;
        output->edit_dist = adjusted > 0 ? adjusted : 0;
        SAM_DUMP_DBG( 4, ( "NM: before: %u, after: %u(+%u-%u)\n", input->edit_dist, output->edit_dist, tmp->S_adjust, tmp->CG_adjust ) );
    }
    else
    {
        output->edit_dist = input->edit_dist;
    }

    /* merge adjacent ops */
    for ( i = tmp->opCnt; i > 1; )
    {
        --i;
        if ( tmp->cigOp[ i - 1 ].code == tmp->cigOp[ i ].code )
        {
            tmp->cigOp[ i - 1 ].length += tmp->cigOp[ i ].length;
            memmove( &tmp->cigOp[ i ], &tmp->cigOp[ i + 1 ], ( tmp->opCnt - 1 - i ) * sizeof( tmp->cigOp[ 0 ] ) );
            --(tmp->opCnt);
        }
    }
    print_CG_cigar( __LINE__, tmp->cigOp, tmp->opCnt, NULL );
    for ( i = j = 0; i < tmp->opCnt && rc == 0; ++i )
    {
        size_t sz;
        rc = string_printf( &output->cigar[ j ], sizeof( output->cigar ) - j, &sz, "%u%c", tmp->cigOp[ i ].length, tmp->cigOp[ i ].code );
        j += sz;
    }
    output->cigar_len = j;
    return rc;
}


rc_t make_cg_cigar( const cg_cigar_input * input, cg_cigar_output * output )
{
    rc_t rc;
    cg_cigar_temp tmp;
    memset( &tmp, 0, sizeof tmp );

    rc = CIGAR_to_CG_Ops( input, &tmp );
    if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcFormat )
    {
        memmove( &output->cigar[ 0 ], &input->p_cigar.ptr[ 0 ], input->p_cigar.len );
        output->cigar_len = input->p_cigar.len;
        output->cigar[ output->cigar_len ] = 0;
        output->edit_dist = input->edit_dist;
        output->p_cigar.ptr = output->cigar;
        output->p_cigar.len = output->cigar_len;
        rc = 0;
    }
    else if ( rc == 0 )
    {
        if ( tmp.CG_adjust == 0 && !tmp.has_ref_offset_type)
        {
            if ( tmp.gap[ 0 ] < tmp.opCnt )
                tmp.CG_adjust = tmp.cigOp[ tmp.gap[ 0 ] ].length;

            if ( tmp.gap[ 1 ] < tmp.opCnt )
                tmp.CG_adjust += tmp.cigOp[ tmp.gap[ 1 ] ].length;

            if ( tmp.gap[ 2 ] < tmp.opCnt )
                tmp.CG_adjust += tmp.cigOp[ tmp.gap[ 2 ] ].length;
        }

        rc = adjust_cigar( input, &tmp, output );
    }
    return rc;
}


static char merge_M_type_ops( char a, char b )
{ /*MX=*/
    char c = 0;
    switch( b )
    {
        case 'X' :  switch( a )
                    {
                        case '=' : c = 'X'; break;
                        case 'X' : c = 'M'; break; /**we don't know - 2X may create '=' **/
                        case 'M' : c = 'M'; break;
                    }
                    break;
        case 'M' :  c = 'M'; break;
        case '=' :  c = a; break;
    }
    assert( c != 0 );
    return c;
}


static size_t fmt_cigar_elem( char * dst, uint32_t cig_oplen, char cig_op )
{
    size_t num_writ;
    rc_t rc = string_printf ( dst, 11, &num_writ, "%d%c", cig_oplen, cig_op );
    assert ( rc == 0 && num_writ > 1 );
    return num_writ;
}

uint32_t CombineCIGAR( char dst[], CigOps const seqOp[], uint32_t seq_len,
                       uint32_t refPos, CigOps const refOp[], uint32_t ref_len )
{
    bool done = false;
    uint32_t ciglen = 0, last_ciglen = 0, last_cig_oplen = 0;
    int32_t si = 0, ri = 0;
    char last_cig_op = '0'; /* never used operation, forces a mismatch in MACRO_BUILD_CIGAR */
    CigOps seq_cop = { 0, 0, 0, 0 }, ref_cop = { 0, 0, 0, 0 };
    int32_t seq_pos = 0;        /** seq_pos is tracked roughly - with every extraction from seqOp **/
    int32_t ref_pos = 0;        /** ref_pos is tracked precisely - with every delta and consumption in cigar **/
    int32_t delta = refPos;     /*** delta in relative positions of seq and ref **/
                                /*** when delta < 0 - rewind or extend the reference ***/
                                /*** wher delta > 0 - skip reference  ***/
#define MACRO_BUILD_CIGAR(OP,OPLEN) \
	if( last_cig_oplen > 0 && last_cig_op == OP){							\
                last_cig_oplen += OPLEN;								\
                ciglen = last_ciglen + fmt_cigar_elem( dst + last_ciglen, last_cig_oplen,last_cig_op );	\
        } else {											\
                last_ciglen = ciglen;									\
                last_cig_oplen = OPLEN;									\
                last_cig_op    = OP;									\
                ciglen = ciglen      + fmt_cigar_elem( dst + last_ciglen, last_cig_oplen, last_cig_op );	\
        }
    while( !done )
    {
        while ( delta < 0 )
        { 
            ref_pos += delta; /** we will make it to back up this way **/
            if( ri > 0 )
            { /** backing up on ref if possible ***/
                int avail_oplen = refOp[ ri - 1 ].oplen - ref_cop.oplen;
                if ( avail_oplen > 0 )
                {
                    if ( ( -delta ) <= avail_oplen * ref_cop.ref_sign )
                    { /*** rewind within last operation **/
                        ref_cop.oplen -= delta;
                        delta -= delta * ref_cop.ref_sign;
                    }
                    else
                    { /*** rewind the whole ***/
                        ref_cop.oplen += avail_oplen;
                        delta += avail_oplen * ref_cop.ref_sign;
                    }
                }
                else
                {
                    ri--;
                    /** pick the previous as used up **/
                    ref_cop = refOp[ri-1];
                    ref_cop.oplen =0; 
                }
            }
            else
            { /** extending the reference **/
                SetCigOp( &ref_cop, '=', ref_cop.oplen - delta );
                delta = 0;
            }
            ref_pos -= delta; 
        }
        if ( ref_cop.oplen == 0 )
        { /*** advance the reference ***/
            ref_cop = refOp[ri++];
            if ( ref_cop.oplen == 0 )
            { /** extending beyond the reference **/
                SetCigOp( &ref_cop,'=', 1000 );
            }
            assert( ref_cop.oplen > 0 );
        }
        if ( delta > 0 )
        { /***  skip refOps ***/
            ref_pos += delta; /** may need to back up **/
            if ( delta >=  ref_cop.oplen )
            { /** full **/
                delta -= ref_cop.oplen * ref_cop.ref_sign;
                ref_cop.oplen = 0;
            }
            else
            { /** partial **/
                ref_cop.oplen -= delta;
                delta -= delta * ref_cop.ref_sign;
            }
            ref_pos -= delta; /** if something left - restore ***/
            continue;
        }

        /*** seq and ref should be synchronized here **/
        assert( delta == 0 );
        if ( seq_cop.oplen == 0 )
        { /*** advance sequence ***/
            if ( seq_pos < seq_len )
            {
                seq_cop = seqOp[ si++ ];
                assert( seq_cop.oplen > 0 );
                seq_pos += seq_cop.oplen * seq_cop.seq_sign;
            }
            else
            {
                done=true;
            }
        }

        if( !done )
        {
            int seq_seq_step = seq_cop.oplen * seq_cop.seq_sign; /** sequence movement**/
            int seq_ref_step = seq_cop.oplen * seq_cop.ref_sign; /** influence of sequence movement on intermediate reference **/
            int ref_seq_step = ref_cop.oplen * ref_cop.seq_sign; /** movement of the intermediate reference ***/
            int ref_ref_step = ref_cop.oplen * ref_cop.ref_sign; /** influence of the intermediate reference movement on final reference ***/
            assert( ref_ref_step >= 0 ); /** no B in the reference **/
            if ( seq_ref_step <= 0 )
            { /** BSIPH in the sequence against anything ***/
                MACRO_BUILD_CIGAR( seq_cop.op, seq_cop.oplen );
                seq_cop.oplen = 0;
                delta = seq_ref_step; /** if negative - will force rewind next cycle **/
            }
            else if ( ref_ref_step <= 0 )
            { /** MX=DN against SIPH in the reference***/
                if ( ref_seq_step == 0 )
                { /** MX=DN against PH **/
                    MACRO_BUILD_CIGAR( ref_cop.op,ref_cop.oplen);
                    ref_cop.oplen = 0;
                }
                else
                {
                    int min_len = ( seq_cop.oplen < ref_cop.oplen ) ? seq_cop.oplen : ref_cop.oplen;
                    if( seq_seq_step == 0 )
                    { /** DN agains SI **/
                        MACRO_BUILD_CIGAR( 'P', min_len );
                    }
                    else
                    { /** MX= agains SI ***/
                        MACRO_BUILD_CIGAR( ref_cop.op,min_len );
                    }
                    seq_cop.oplen -= min_len;
                    ref_cop.oplen -= min_len;
                }
            }
            else
            {
                /*MX=DN  against MX=DN*/
                int min_len = ( seq_cop.oplen < ref_cop.oplen ) ? seq_cop.oplen : ref_cop.oplen;
                if ( seq_seq_step == 0 )
                { /* DN against MX=DN */
                    if ( ref_seq_step == 0 )
                    { /** padding DN against DN **/
                        MACRO_BUILD_CIGAR( 'P', min_len );
                        ref_cop.oplen -= min_len;
                        seq_cop.oplen -= min_len;
                    }
                    else
                    { /* DN against MX= **/
                        MACRO_BUILD_CIGAR( seq_cop.op, min_len );
                        seq_cop.oplen -= min_len;
                    }
                }
                else if ( ref_cop.seq_sign == 0 )
                { /* MX= against DN - always wins */
                    MACRO_BUILD_CIGAR( ref_cop.op, min_len );
                    ref_cop.oplen -= min_len;
                }
                else
                { /** MX= against MX= ***/
                    char curr_op = merge_M_type_ops( seq_cop.op, ref_cop.op );
                    /* or otherwise merge_M_type_ops() will be called twice by the macro! */
                    MACRO_BUILD_CIGAR( curr_op, min_len );
                    ref_cop.oplen -= min_len;
                    seq_cop.oplen -= min_len;
                }
                ref_pos += min_len;
            }
        }
    }
    return ciglen;
}


typedef struct cg_merger
{
    char newSeq[ MAX_READ_LEN ];
    char newQual[ MAX_READ_LEN ];
    char tags[ MAX_CG_CIGAR_LEN * 2 ];
} cg_merger;


rc_t merge_cg_cigar( const cg_cigar_input * input, cg_cigar_output * output )
{
    rc_t rc;
    cg_cigar_temp tmp;
    memset( &tmp, 0, sizeof tmp );

    rc = CIGAR_to_CG_Ops( input, &tmp );
    if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcFormat )
    {
        memmove( &output->cigar[ 0 ], &input->p_cigar.ptr[ 0 ], input->p_cigar.len );
        output->cigar_len = input->p_cigar.len;
        output->cigar[ output->cigar_len ] = 0;
        output->edit_dist = input->edit_dist;
        rc = 0;
    }
    else if ( rc == 0 )
    {

        if ( tmp.CG_adjust == 0  && !tmp.has_ref_offset_type )
        {
            if ( tmp.gap[ 0 ] < tmp.opCnt )
                tmp.CG_adjust = tmp.cigOp[ tmp.gap[ 0 ] ].length;

            if ( tmp.gap[ 1 ] < tmp.opCnt )
                tmp.CG_adjust += tmp.cigOp[ tmp.gap[ 1 ] ].length;

            if ( tmp.gap[ 2 ] < tmp.opCnt )
                tmp.CG_adjust += tmp.cigOp[ tmp.gap[ 2 ] ].length;
        }

        rc = adjust_cigar( input, &tmp, output );

    }
    return rc;
}


rc_t make_cg_merge( const cg_cigar_input * input, cg_cigar_output * output )
{
    rc_t rc;
    cg_cigar_temp tmp;
    memset( &tmp, 0, sizeof tmp );

    rc = CIGAR_to_CG_Ops( input, &tmp );
    if ( GetRCState( rc ) == rcNotFound && GetRCObject( rc ) == rcFormat )
    {
        memmove( &output->cigar[ 0 ], &input->p_cigar.ptr[ 0 ], input->p_cigar.len );
        output->cigar_len = input->p_cigar.len;
        output->cigar[ output->cigar_len ] = 0;
        output->edit_dist = input->edit_dist;
        rc = 0;
    }
    else if ( rc == 0 )
    {

        if ( tmp.CG_adjust == 0  && !tmp.has_ref_offset_type )
        {
            if ( tmp.gap[ 0 ] < tmp.opCnt )
                tmp.CG_adjust = tmp.cigOp[ tmp.gap[ 0 ] ].length;

            if ( tmp.gap[ 1 ] < tmp.opCnt )
                tmp.CG_adjust += tmp.cigOp[ tmp.gap[ 1 ] ].length;

            if ( tmp.gap[ 2 ] < tmp.opCnt )
                tmp.CG_adjust += tmp.cigOp[ tmp.gap[ 2 ] ].length;
        }
    }

    if ( rc == 0 )
    {
        uint32_t const B_len = tmp.cigOp[ tmp.gap[ 0 ] ].length;
        uint32_t const B_at = tmp.gap[ 0 ] < tmp.gap[ 2 ] ? 5 : 30;
            
        if ( 0 < B_len && B_len < 5 )
        {
            memcpy( output->newSeq,  input->p_read.ptr, MAX_READ_LEN );
            memcpy( output->newQual, input->p_quality.ptr, MAX_READ_LEN );
            
            output->p_read.ptr = output->newSeq;
            output->p_read.len = ( MAX_READ_LEN - B_len );
            
            output->p_quality.ptr = output->newQual;
            output->p_quality.len = ( MAX_READ_LEN - B_len );
            
            output->p_tags.ptr = output->tags;
            output->p_tags.len = 0;

            /* nBnM -> nB0M */
            tmp.cigOp[ tmp.gap[ 0 ] + 1 ].length -= B_len;
            if ( tmp.gap[ 0 ] < tmp.gap[ 2 ] )
            {
                size_t written;
                rc = string_printf( output->tags, sizeof( output->tags ), &written,
                                    "GC:Z:%uS%uG%uS\tGS:Z:%.*s\tGQ:Z:%.*s",
                                    5 - B_len, B_len, 30 - B_len, 2 * B_len, &output->newSeq[ 5 - B_len ], 2 * B_len, &output->newQual[ 5 - B_len ] );
                if ( rc == 0 )
                    output->p_tags.len = written;
                memmove( &tmp.cigOp[ tmp.gap[ 0 ] ],
                         &tmp.cigOp[ tmp.gap[ 0 ] + 1 ],
                         ( tmp.opCnt - ( tmp.gap[ 0 ] + 1 ) ) * sizeof( tmp.cigOp[ 0 ] ) );
                --tmp.opCnt;
            }
            else
            {
                size_t written;
                rc = string_printf( output->tags, sizeof( output->tags ), &written,
                                    "GC:Z:%uS%uG%uS\tGS:Z:%.*s\tGQ:Z:%.*s",
                                    30 - B_len, B_len, 5 - B_len, 2 * B_len, &output->newSeq[ 30 - B_len ], 2 * B_len, &output->newQual[ 30 - B_len ] );
                if ( rc == 0 )
                    output->p_tags.len = written;
                memmove( &tmp.cigOp[ tmp.gap[ 0 ] ],
                         &tmp.cigOp[ tmp.gap[ 0 ] + 1 ],
                         ( tmp.opCnt - ( tmp.gap[ 0 ] + 1 ) ) * sizeof( tmp.cigOp[ 0 ] ) );
                --tmp.opCnt;
            }
            if ( rc == 0 )
            {
                uint32_t i;
                for ( i = B_at; i < B_at + B_len; ++i )
                {
                    uint32_t const Lq = output->newQual[ i - B_len ];
                    uint32_t const Rq = output->newQual[ i ];

                    if ( Lq <= Rq )
                    {
                        output->newSeq[ i - B_len ] = output->newSeq[ i ];
                        output->newQual[ i - B_len ] = Rq;
                    }
                    else
                    {
                        output->newSeq[ i ] = output->newSeq[ i - B_len ];
                        output->newQual[ i ] = Lq;
                    }
                }
                memmove( &output->newSeq [ B_at ], &output->newSeq [ B_at + B_len ], MAX_READ_LEN - B_at - B_len );
                memmove( &output->newQual[ B_at ], &output->newQual[ B_at + B_len ], MAX_READ_LEN - B_at - B_len );
            }
        }
        else
        {
            uint32_t i, len = tmp.cigOp[ tmp.gap[ 0 ] ].length;
            
            tmp.cigOp[ tmp.gap[ 0 ] ].code = 'I';
            for ( i = tmp.gap[ 0 ] + 1; i < tmp.opCnt && len > 0; ++i )
            {
                if ( tmp.cigOp[ i ].length <= len )
                {
                    len -= tmp.cigOp[ i ].length;
                    tmp.cigOp[ i ].length = 0;
                }
                else
                {
                    tmp.cigOp[ i ].length -= len;
                    len = 0;
                }
            }
            tmp.CG_adjust -= tmp.cigOp[ tmp.gap[ 0 ] ].length;
        }
    }

    if ( rc == 0 )
        rc = adjust_cigar( input, &tmp, output );

    return rc;
}


/* A-->0x00, C-->0x01 G-->0x02 T-->0x03 */

static const uint8_t ASCII_to_2na[] = {
/*      00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F  */
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,

/*      10  11  12  13  14  15  16  17  18  19  1A  1B  1C  1D  1E  1F  */
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,

/*      20  21  22  23  24  25  26  27  28  29  2A  2B  2C  2D  2E  2F  */
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,

/*      30  31  32  33  34  35  36  37  38  39  3A  3B  3C  3D  3E  3F  */
        0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,

/*      40  41  42  43  44  45  46  47  48  49  4A  4B  4C  4D  4E  4F  */
        0,  0,  0,  1,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,  0,  0,

/*      50  51  52  53  54  55  56  57  58  59  5A  5B  5C  5D  5E  5F  */
        0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,

/*      60  61  62  63  64  65  66  67  68  69  6A  6B  6C  6D  6E  6F  */
        0,  0,  0,  1,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,  0,  0,

/*      70  71  72  73  74  75  76  77  78  79  7A  7B  7C  7D  7E  7F  */
        0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
};

static uint8_t compress_4_bases_to_byte( uint8_t * bases )
{
    uint8_t res = ASCII_to_2na[ bases[ 0 ] & 0x7F ];
    res <<= 2;
    res |= ASCII_to_2na[ bases[ 1 ] & 0x7F ];
    res <<= 2;
    res |= ASCII_to_2na[ bases[ 2 ] & 0x7F ];
    res <<= 2;
    res |= ASCII_to_2na[ bases[ 3 ] & 0x7F ];
    return res;
}


static const uint8_t compressed_to_fwd_reverse_0[] = {

    /* 0000.0000    0x00 ... AAAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0000.0001    0x01 ... AAAC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0000.0010    0x02 ... AAAG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0000.0011    0x03 ... AAAT */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0000.0100    0x04 ... AACA */ RNA_SPLICE_UNKNOWN,
    /* 0000.0101    0x05 ... AACC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0000.0110    0x06 ... AACG */ RNA_SPLICE_UNKNOWN,
    /* 0000.0111    0x07 ... AACT */ RNA_SPLICE_UNKNOWN,
    /* 0000.1000    0x08 ... AAGA */ RNA_SPLICE_UNKNOWN,
    /* 0000.1001    0x09 ... AAGC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0000.1010    0x0A ... AAGG */ RNA_SPLICE_UNKNOWN,
    /* 0000.1011    0x0B ... AAGT */ RNA_SPLICE_UNKNOWN,
    /* 0000.1100    0x0C ... AATA */ RNA_SPLICE_UNKNOWN,
    /* 0000.1101    0x0D ... AATC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0000.1110    0x0E ... AATG */ RNA_SPLICE_UNKNOWN,
    /* 0000.1111    0x0F ... AATT */ RNA_SPLICE_UNKNOWN,

    /* 0001.0000    0x10 ... ACAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0001.0001    0x11 ... ACAC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0001.0010    0x12 ... ACAG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0001.0011    0x13 ... ACAT */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0001.0100    0x14 ... ACCA */ RNA_SPLICE_UNKNOWN,
    /* 0001.0101    0x15 ... ACCC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0001.0110    0x16 ... ACCG */ RNA_SPLICE_UNKNOWN,
    /* 0001.0111    0x17 ... ACCT */ RNA_SPLICE_UNKNOWN,
    /* 0001.1000    0x18 ... ACGA */ RNA_SPLICE_UNKNOWN,
    /* 0001.1001    0x19 ... ACGC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0001.1010    0x1A ... ACGG */ RNA_SPLICE_UNKNOWN,
    /* 0001.1011    0x1B ... ACGT */ RNA_SPLICE_UNKNOWN,
    /* 0001.1100    0x1C ... ACTA */ RNA_SPLICE_UNKNOWN,
    /* 0001.1101    0x1D ... ACTC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0001.1110    0x1E ... ACTG */ RNA_SPLICE_UNKNOWN,
    /* 0001.1111    0x1F ... ACTT */ RNA_SPLICE_UNKNOWN,

    /* 0010.0000    0x20 ... AGAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0010.0001    0x21 ... AGAC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0010.0010    0x22 ... AGAG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0010.0011    0x23 ... AGAT */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0010.0100    0x24 ... AGCA */ RNA_SPLICE_UNKNOWN,
    /* 0010.0101    0x25 ... AGCC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0010.0110    0x26 ... AGCG */ RNA_SPLICE_UNKNOWN,
    /* 0010.0111    0x27 ... AGCT */ RNA_SPLICE_UNKNOWN,
    /* 0010.1000    0x28 ... AGGA */ RNA_SPLICE_UNKNOWN,
    /* 0010.1001    0x29 ... AGGC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0010.1010    0x2A ... AGGG */ RNA_SPLICE_UNKNOWN,
    /* 0010.1011    0x2B ... AGGT */ RNA_SPLICE_UNKNOWN,
    /* 0010.1100    0x2C ... AGTA */ RNA_SPLICE_UNKNOWN,
    /* 0010.1101    0x2D ... AGTC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0010.1110    0x2E ... AGTG */ RNA_SPLICE_UNKNOWN,
    /* 0010.1111    0x2F ... AGTT */ RNA_SPLICE_UNKNOWN,

    /* 0011.0000    0x30 ... ATAA */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0011.0001    0x31 ... ATAC */ RNA_SPLICE_FWD,            /* MINOR forward */
    /* 0011.0010    0x32 ... ATAG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0011.0011    0x33 ... ATAT */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0011.0100    0x34 ... ATCA */ RNA_SPLICE_UNKNOWN,
    /* 0011.0101    0x35 ... ATCC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0011.0110    0x36 ... ATCG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0011.0111    0x37 ... ATCT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0011.1000    0x38 ... ATGA */ RNA_SPLICE_UNKNOWN,
    /* 0011.1001    0x39 ... ATGC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0011.1010    0x3A ... ATGG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0011.1011    0x3B ... ATGT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0011.1100    0x3C ... ATTA */ RNA_SPLICE_UNKNOWN,
    /* 0011.1101    0x3D ... ATTC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0011.1110    0x3E ... ATTG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0011.1111    0x3F ... ATTT */ ( RNA_SPLICE_REV | 0x20 ),

    /* 0100.0000    0x40 ... CAAA */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0100.0001    0x41 ... CAAC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0100.0010    0x42 ... CAAG */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0100.0011    0x43 ... CAAT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0100.0100    0x44 ... CACA */ RNA_SPLICE_UNKNOWN,
    /* 0100.0101    0x45 ... CACC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0100.0110    0x46 ... CACG */ RNA_SPLICE_UNKNOWN,
    /* 0100.0111    0x47 ... CACT */ RNA_SPLICE_UNKNOWN,
    /* 0100.1000    0x48 ... CAGA */ RNA_SPLICE_UNKNOWN,
    /* 0100.1001    0x49 ... CAGC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0100.1010    0x4A ... CAGG */ RNA_SPLICE_UNKNOWN,
    /* 0100.1011    0x4B ... CAGT */ RNA_SPLICE_UNKNOWN,
    /* 0100.1100    0x4C ... CATA */ RNA_SPLICE_UNKNOWN,
    /* 0100.1101    0x4D ... CATC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0100.1110    0x4E ... CATG */ RNA_SPLICE_UNKNOWN,
    /* 0100.1111    0x4F ... CATT */ RNA_SPLICE_UNKNOWN,

    /* 0101.0000    0x50 ... CCAA */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0101.0001    0x51 ... CCAC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0101.0010    0x52 ... CCAG */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0101.0011    0x53 ... CCAT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0101.0100    0x54 ... CCCA */ RNA_SPLICE_UNKNOWN,
    /* 0101.0101    0x55 ... CCCC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0101.0110    0x56 ... CCCG */ RNA_SPLICE_UNKNOWN,
    /* 0101.0111    0x57 ... CCCT */ RNA_SPLICE_UNKNOWN,
    /* 0101.1000    0x58 ... CCGA */ RNA_SPLICE_UNKNOWN,
    /* 0101.1001    0x59 ... CCGC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0101.1010    0x5A ... CCGG */ RNA_SPLICE_UNKNOWN,
    /* 0101.1011    0x5B ... CCGT */ RNA_SPLICE_UNKNOWN,
    /* 0101.1100    0x5C ... CCTA */ RNA_SPLICE_UNKNOWN,
    /* 0101.1101    0x5D ... CCTC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0101.1110    0x5E ... CCTG */ RNA_SPLICE_UNKNOWN,
    /* 0101.1111    0x5F ... CCTT */ RNA_SPLICE_UNKNOWN,

    /* 0110.0000    0x60 ... CGAA */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0110.0001    0x61 ... CGAC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0110.0010    0x62 ... CGAG */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0110.0011    0x63 ... CGAT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0110.0100    0x64 ... CGCA */ RNA_SPLICE_UNKNOWN,
    /* 0110.0101    0x65 ... CGCC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0110.0110    0x66 ... CGCG */ RNA_SPLICE_UNKNOWN,
    /* 0110.0111    0x67 ... CGCT */ RNA_SPLICE_UNKNOWN,
    /* 0110.1000    0x68 ... CGGA */ RNA_SPLICE_UNKNOWN,
    /* 0110.1001    0x69 ... CGGC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0110.1010    0x6A ... CGGG */ RNA_SPLICE_UNKNOWN,
    /* 0110.1011    0x6B ... CGGT */ RNA_SPLICE_UNKNOWN,
    /* 0110.1100    0x6C ... CGTA */ RNA_SPLICE_UNKNOWN,
    /* 0110.1101    0x6D ... CGTC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0110.1110    0x6E ... CGTG */ RNA_SPLICE_UNKNOWN,
    /* 0110.1111    0x6F ... CGTT */ RNA_SPLICE_UNKNOWN,

    /* 0111.0000    0x70 ... CTAA */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0111.0001    0x71 ... CTAC */ RNA_SPLICE_REV,            /* MAJOR reverse */
    /* 0111.0010    0x72 ... CTAG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 0111.0011    0x73 ... CTAT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0111.0100    0x74 ... CTCA */ RNA_SPLICE_UNKNOWN,
    /* 0111.0101    0x75 ... CTCC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0111.0110    0x76 ... CTCG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0111.0111    0x77 ... CTCT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0111.1000    0x78 ... CTGA */ RNA_SPLICE_UNKNOWN,
    /* 0111.1001    0x79 ... CTGC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0111.1010    0x7A ... CTGG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0111.1011    0x7B ... CTGT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 0111.1100    0x7C ... CTTA */ RNA_SPLICE_UNKNOWN,
    /* 0111.1101    0x7D ... CTTC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 0111.1110    0x7E ... CTTG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 0111.1111    0x7F ... CTTT */ ( RNA_SPLICE_REV | 0x20 ),

    /* 1000.0000    0x80 ... GAAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1000.0001    0x81 ... GAAC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1000.0010    0x82 ... GAAG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1000.0011    0x83 ... GAAT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1000.0100    0x84 ... GACA */ RNA_SPLICE_UNKNOWN,
    /* 1000.0101    0x85 ... GACC */ RNA_SPLICE_UNKNOWN,
    /* 1000.0110    0x86 ... GACG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1000.0111    0x87 ... GACT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1000.1000    0x88 ... GAGA */ RNA_SPLICE_UNKNOWN,
    /* 1000.1001    0x89 ... GAGC */ RNA_SPLICE_UNKNOWN,
    /* 1000.1010    0x8A ... GAGG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1000.1011    0x8B ... GAGT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1000.1100    0x8C ... GATA */ RNA_SPLICE_UNKNOWN,
    /* 1000.1101    0x8D ... GATC */ RNA_SPLICE_UNKNOWN,
    /* 1000.1110    0x8E ... GATG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1000.1111    0x8F ... GATT */ ( RNA_SPLICE_REV | 0x20 ),

    /* 1001.0000    0x90 ... GCAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1001.0001    0x91 ... GCAC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1001.0010    0x92 ... GCAG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1001.0011    0x93 ... GCAT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1001.0100    0x94 ... GCCA */ RNA_SPLICE_UNKNOWN,
    /* 1001.0101    0x95 ... GCCC */ RNA_SPLICE_UNKNOWN,
    /* 1001.0110    0x96 ... GCCG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1001.0111    0x97 ... GCCT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1001.1000    0x98 ... GCGA */ RNA_SPLICE_UNKNOWN,
    /* 1001.1001    0x99 ... GCGC */ RNA_SPLICE_UNKNOWN,
    /* 1001.1010    0x9A ... GCGG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1001.1011    0x9B ... GCGT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1001.1100    0x9C ... GCTA */ RNA_SPLICE_UNKNOWN,
    /* 1001.1101    0x9D ... GCTC */ RNA_SPLICE_UNKNOWN,
    /* 1001.1110    0x9E ... GCTG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1001.1111    0x9F ... GCTT */ ( RNA_SPLICE_REV | 0x20 ),

    /* 1010.0000    0xA0 ... GGAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1010.0001    0xA1 ... GGAC */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1010.0010    0xA2 ... GGAG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1010.0011    0xA3 ... GGAT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1010.0100    0xA4 ... GGCA */ RNA_SPLICE_UNKNOWN,
    /* 1010.0101    0xA5 ... GGCC */ RNA_SPLICE_UNKNOWN,
    /* 1010.0110    0xA6 ... GGCG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1010.0111    0xA7 ... GGCT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1010.1000    0xA8 ... GGGA */ RNA_SPLICE_UNKNOWN,
    /* 1010.1001    0xA9 ... GGGC */ RNA_SPLICE_UNKNOWN,
    /* 1010.1010    0xAA ... GGGG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1010.1011    0xAB ... GGGT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1010.1100    0xAC ... GGTA */ RNA_SPLICE_UNKNOWN,
    /* 1010.1101    0xAD ... GGTC */ RNA_SPLICE_UNKNOWN,
    /* 1010.1110    0xAE ... GGTG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1010.1111    0xAF ... GGTT */ ( RNA_SPLICE_REV | 0x20 ),

    /* 1011.0000    0xB0 ... GTAA */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1011.0001    0xB1 ... GTAC */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1011.0010    0xB2 ... GTAG */ RNA_SPLICE_FWD,            /* MAJOR forward */
    /* 1011.0011    0xB3 ... GTAT */ RNA_SPLICE_REV,            /* MINOR reverse */
    /* 1011.0100    0xB4 ... GTCA */ RNA_SPLICE_UNKNOWN,
    /* 1011.0101    0xB5 ... GTCC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1011.0110    0xB6 ... GTCG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1011.0111    0xB7 ... GTCT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1011.1000    0xB8 ... GTGA */ RNA_SPLICE_UNKNOWN,
    /* 1011.1001    0xB9 ... GTGC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1011.1010    0xBA ... GTGG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1011.1011    0xBB ... GTGT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1011.1100    0xBC ... GTTA */ RNA_SPLICE_UNKNOWN,
    /* 1011.1101    0xBD ... GTTC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1011.1110    0xBE ... GTTG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1011.1111    0xBF ... GTTT */ ( RNA_SPLICE_REV | 0x10 ),

    /* 1100.0000    0xC0 ... TAAA */ RNA_SPLICE_UNKNOWN,
    /* 1100.0001    0xC1 ... TAAC */ RNA_SPLICE_UNKNOWN,
    /* 1100.0010    0xC2 ... TAAG */ RNA_SPLICE_UNKNOWN,
    /* 1100.0011    0xC3 ... TAAT */ RNA_SPLICE_UNKNOWN,
    /* 1100.0100    0xC4 ... TACA */ RNA_SPLICE_UNKNOWN,
    /* 1100.0101    0xC5 ... TACC */ RNA_SPLICE_UNKNOWN,
    /* 1100.0110    0xC6 ... TACG */ RNA_SPLICE_UNKNOWN,
    /* 1100.0111    0xC7 ... TACT */ RNA_SPLICE_UNKNOWN,
    /* 1100.1000    0xC8 ... TAGA */ RNA_SPLICE_UNKNOWN,
    /* 1100.1001    0xC9 ... TAGC */ RNA_SPLICE_UNKNOWN,
    /* 1100.1010    0xCA ... TAGG */ RNA_SPLICE_UNKNOWN,
    /* 1100.1011    0xCB ... TAGT */ RNA_SPLICE_UNKNOWN,
    /* 1100.1100    0xCC ... TATA */ RNA_SPLICE_UNKNOWN,
    /* 1100.1101    0xCD ... TATC */ RNA_SPLICE_UNKNOWN,
    /* 1100.1110    0xCE ... TATG */ RNA_SPLICE_UNKNOWN,
    /* 1100.1111    0xCF ... TATT */ RNA_SPLICE_UNKNOWN,

    /* 1101.0000    0xE0 ... TCAA */ RNA_SPLICE_UNKNOWN,
    /* 1101.0001    0xE1 ... TCAC */ RNA_SPLICE_UNKNOWN,
    /* 1101.0010    0xE2 ... TCAG */ RNA_SPLICE_UNKNOWN,
    /* 1101.0011    0xE3 ... TCAT */ RNA_SPLICE_UNKNOWN,
    /* 1101.0100    0xE4 ... TCCA */ RNA_SPLICE_UNKNOWN,
    /* 1101.0101    0xE5 ... TCCC */ RNA_SPLICE_UNKNOWN,
    /* 1101.0110    0xE6 ... TCCG */ RNA_SPLICE_UNKNOWN,
    /* 1101.0111    0xE7 ... TCCT */ RNA_SPLICE_UNKNOWN,
    /* 1101.1000    0xE8 ... TCGA */ RNA_SPLICE_UNKNOWN,
    /* 1101.1001    0xE9 ... TCGC */ RNA_SPLICE_UNKNOWN,
    /* 1101.1010    0xEA ... TCGG */ RNA_SPLICE_UNKNOWN,
    /* 1101.1011    0xEB ... TCGT */ RNA_SPLICE_UNKNOWN,
    /* 1101.1100    0xEC ... TCTA */ RNA_SPLICE_UNKNOWN,
    /* 1101.1101    0xED ... TCTC */ RNA_SPLICE_UNKNOWN,
    /* 1101.1110    0xEE ... TCTG */ RNA_SPLICE_UNKNOWN,
    /* 1101.1111    0xEF ... TCTT */ RNA_SPLICE_UNKNOWN,

    /* 1111.0000    0xF0 ... TTAA */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1111.0001    0xF1 ... TTAC */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1111.0010    0xF2 ... TTAG */ ( RNA_SPLICE_FWD | 0x10 ),
    /* 1111.0011    0xF3 ... TTAT */ ( RNA_SPLICE_REV | 0x10 ),
    /* 1111.0100    0xF4 ... TTCA */ RNA_SPLICE_UNKNOWN,
    /* 1111.0101    0xF5 ... TTCC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1111.0110    0xF6 ... TTCG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1111.0111    0xF7 ... TTCT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1111.1000    0xF8 ... TTGA */ RNA_SPLICE_UNKNOWN,
    /* 1111.1001    0xF9 ... TTGC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1111.1010    0xFA ... TTGG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1111.1011    0xFB ... TTGT */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1111.1100    0xFC ... TTTA */ RNA_SPLICE_UNKNOWN,
    /* 1111.1101    0xFD ... TTTC */ ( RNA_SPLICE_REV | 0x20 ),
    /* 1111.1110    0xFE ... TTTG */ ( RNA_SPLICE_FWD | 0x20 ),
    /* 1111.1111    0xFF ... TTTT */ ( RNA_SPLICE_REV | 0x20 )
};


/*************************************************************
    RNA-splice detector:

    base  0   1   .......  n-2  n-1     direction

          G   T             A    G      forward
          A   T             A    C      forward

          C   T             A    C      reverse
          G   T             A    T      reverse


    =========================================================

    zero mismatches ( aka full matches ) :

    ATAC    ... MINOR   [0x31]=RNA_SPLICE_FWD
    CTAC    ... MAJOR   [0x71]=RNA_SPLICE_REV
    GTAG    ... MAJOR   [0xB2]=RNA_SPLICE_FWD
    GTAT    ... MINOR   [0xB3]=RNA_SPLICE_REV

    =========================================================

    one mismatch:

    ATAC    ... MINOR, forward

        *
        CTAC ( is also MAJOR reverse ... )
        GTAC ( is alow MAJOR reverse, 1 mismatch )
        TTAC ( is alow MAJOR reverse, 1 mismatch )

         *
        AAAC [0x01]=( RNA_SPLICE_FWD | 0x10 )
        ACAC [0x11]=( RNA_SPLICE_FWD | 0x10 )
        AGAC [0x21]=( RNA_SPLICE_FWD | 0x10 )

          *
        ATCC [0x35]=( RNA_SPLICE_FWD | 0x10 )
        ATGC [0x39]=( RNA_SPLICE_FWD | 0x10 )
        ATTC [0x3D]=( RNA_SPLICE_FWD | 0x10 )

           *
        ATAA [0x30]=( RNA_SPLICE_FWD | 0x10 )
        ATAG ( is also MAJOR forward, 1 mismatch )
        ATAT [0x33]=( RNA_SPLICE_FWD | 0x10 )

    -----------------------------------------------------------
    CTAC    ... MAJOR, reverse

        *
        ATAC ( is also MINOR forward, full match )
        GTAC ( is also MAJOR forward, 1 mismatch )
        TTAC [0xF1]=( RNA_SPLICE_REV | 0x10 )

         *
        CAAC [0x41]=( RNA_SPLICE_REV | 0x10 )
        CCAC [0x51]=( RNA_SPLICE_REV | 0x10 )
        CGAC [0x61]=( RNA_SPLICE_REV | 0x10 )

          *
        CTCC [0x75]=( RNA_SPLICE_REV | 0x10 )
        CTGC [0x79]=( RNA_SPLICE_REV | 0x10 )
        CTTC [0x7D]=( RNA_SPLICE_REV | 0x10 )

           *
        CTAA [0x70]=( RNA_SPLICE_REV | 0x10 )
        CTAG ( is also MAJOR forward, 1 mismatch )
        CTAT [0x73]=( RNA_SPLICE_REV | 0x10 )

    -----------------------------------------------------------
    GTAG    ... MAJOR, forward

        *
        ATAG [0x32]=( RNA_SPLICE_FWD | 0x10 )
        CTAG [0x72]=( RNA_SPLICE_FWD | 0x10 )
        TTAG [0xF2]=( RNA_SPLICE_FWD | 0x10 )

         *
        GAAG [0x82]=( RNA_SPLICE_FWD | 0x10 )
        GCAG [0x92]=( RNA_SPLICE_FWD | 0x10 )
        GGAG [0xA2]=( RNA_SPLICE_FWD | 0x10 )

          *
        GTCG [0xB6]=( RNA_SPLICE_FWD | 0x10 )
        GTGG [0xBA]=( RNA_SPLICE_FWD | 0x10 )
        GTTG [0xBE]=( RNA_SPLICE_FWD | 0x10 )

           *
        GTAA [0xB0]=( RNA_SPLICE_FWD | 0x10 )
        GTAC [0xB1]=( RNA_SPLICE_FWD | 0x10 )
        GTAT ( is also MINOR reverse, full match )

    -----------------------------------------------------------
    GTAT    ... MINOR, reverse

        *
        ATAT ( is also MINOR forward, 1 mismatch )
        CTAT ( is also MAJOR reverse, 1 mismatch )
        TTAT [0xF3]=( RNA_SPLICE_REV | 0x10 )

         *
        GAAT [0x83]=( RNA_SPLICE_REV | 0x10 )
        GCAT [0x93]=( RNA_SPLICE_REV | 0x10 )
        GGAT [0xA3]=( RNA_SPLICE_REV | 0x10 )

          *
        GTCT [0xB7]=( RNA_SPLICE_REV | 0x10 )
        GTGT [0xBB]=( RNA_SPLICE_REV | 0x10 )
        GTTT [0xBF]=( RNA_SPLICE_REV | 0x10 )

           *
        GTAA ( is also MAJOR forward, 1 mismatch )
        GTAC ( is also MAJOR forward, 1 mismatch )
        GTAG ( is also MAJOR forward, full match )

    =========================================================

    two mismatches:

    ATAC    ... MINOR, forward

        * *
        CTAC ( is also MAJOR reverse, full match )
        CTCC ( is also MAJOR reverse, 1 mismatch )
        CTGC ( is also MAJOR reverse, 1 mismatch )
        CTTC ( is also MAJOR reverse, 1 mismatch )
        GTAC ( is also MAJOR forward, 1 mismatch )
        GTCC ( is also MAJOR reverse, 2 mismatches )
        GTGC ( is also MAJOR reverse, 2 mismatches )
        GTTC ( is also MAJOR reverse, 2 mismatches )
        TTAC ( is also MAJOR reverse, 1 mismatch )
        TTCC ( is also MAJOR reverse, 2 mismatches )
        TTGC ( is also MAJOR reverse, 2 mismatches )
        TTTC ( is also MAJOR reverse, 2 mismatches )

         **
        AAAC ( is also MAJOR forward, 1 mismatch )
        AACC [0x05]=( RNA_SPLICE_FWD | 0x20 )
        AAGC [0x09]=( RNA_SPLICE_FWD | 0x20 )
        AATC [0x0D]=( RNA_SPLICE_FWD | 0x20 )
        ACAC ( is also MINOR forward, 1 mismatch )
        ACCC [0x15]=( RNA_SPLICE_FWD | 0x20 )
        ACGC [0x19]=( RNA_SPLICE_FWD | 0x20 )
        ACTC [0x1D]=( RNA_SPLICE_FWD | 0x20 )
        AGAC ( is also MINOR forward, 1 mismatch )
        AGCC [0x25]=( RNA_SPLICE_FWD | 0x20 )
        AGGC [0x29]=( RNA_SPLICE_FWD | 0x20 )
        AGTC [0x2D]=( RNA_SPLICE_FWD | 0x20 )

        *  *
        CTAA ( is also MAJOR reverse, 1 mismatch )
        CTAC ( is also MAJOR reverse, full match )
        CTAG ( is also MAJOR forward, 1 mismatch )
        CTAT ( is also MAJOR reverse, 1 mismatch )
        GTAA ( is also MAJOR forward, 1 mismatch )
        GTAC ( is also MAJOR forward, 1 mismatch )
        GTAG ( is also MAJOR forward, full match )
        GTAT ( is also MINOR reverse, full match )
        TTAA ( is also MAJOR reverse, 2 mismatches )
        TTAC ( is also MAJOR reverse, 1 mismatch )
        TTAG ( is also MAJOR forward, 1 mismatch )
        TTAT ( is also MINOR reverse, 1 mismatch )

         * *
        AAAA [0x00]=( RNA_SPLICE_FWD | 0x20 )
        AAAC ( is also MAJOR forward, 1 mismatch )
        AAAG [0x02]=( RNA_SPLICE_FWD | 0x20 )
        AAAT [0x03]=( RNA_SPLICE_FWD | 0x20 )
        ACAA [0x10]=( RNA_SPLICE_FWD | 0x20 )
        ACAC ( is also MINOR forward, 1 mismatch )
        ACAG [0x12]=( RNA_SPLICE_FWD | 0x20 )
        ACAT [0x13]=( RNA_SPLICE_FWD | 0x20 )
        AGAA [0x20]=( RNA_SPLICE_FWD | 0x20 )
        AGAC ( is also MINOR forward, 1 mismatch )
        AGAG [0x22]=( RNA_SPLICE_FWD | 0x20 )
        AGAT [0x23]=( RNA_SPLICE_FWD | 0x20 )

    -----------------------------------------------------------
    CTAC    ... MAJOR, reverse

        * *
        ATAC ( is also MINOR forward, full match )
        ATCC ( is also MINOR forward, 1 mismatch )
        ATGC ( is also MINOR forward, 1 mismatch )
        ATTC ( is also MINOR forward, 1 mismatch )
        GTAC ( is also MAJOR forward, 1 mismatch )
        GTCC [0xB5]=( RNA_SPLICE_REV | 0x20 )
        GTGC [0xB9]=( RNA_SPLICE_REV | 0x20 )
        GTTC [0xBD]=( RNA_SPLICE_REV | 0x20 )
        TTAC ( is also MAJOR reverse, 1 mismatch )
        TTCC [0xF5]=( RNA_SPLICE_REV | 0x20 )
        TTGC [0xF9]=( RNA_SPLICE_REV | 0x20 )
        TTTC [0xFD]=( RNA_SPLICE_REV | 0x20 )

         **
        CAAC ( is also MAJOR reverse, 1 mismatch )
        CACC [0x45]=( RNA_SPLICE_REV | 0x20 )
        CAGC [0x49]=( RNA_SPLICE_REV | 0x20 )
        CATC [0x4D]=( RNA_SPLICE_REV | 0x20 )
        CCAC ( is also MAJOR reverse, 1 mismatch )
        CCCC [0x55]=( RNA_SPLICE_REV | 0x20 )
        CCGC [0x59]=( RNA_SPLICE_REV | 0x20 )
        CCTC [0x5D]=( RNA_SPLICE_REV | 0x20 )
        CGAC ( is also MAJOR reverse, 1 mismatch )
        CGCC [0x65]=( RNA_SPLICE_REV | 0x20 )
        CGGC [0x69]=( RNA_SPLICE_REV | 0x20 )
        CGTC [0x6D]=( RNA_SPLICE_REV | 0x20 )

        *  *
        ATAA ( is also MINOR forward, 1 mismatch )
        ATAC ( is also MINOR forward, full match )
        ATAG ( is also MAJOR forward, 1 mismatch )
        ATAT ( is also MINOR forward, 1 mismatch )
        GTAA ( is also MAJOR forward, 1 mismatch )
        GTAC ( is also MAJOR forward, 1 mismatch )
        GTAG ( is also MAJOR forward, full match )
        GTAT ( is also MINOR reverse, full match )
        TTAA ( is also MAJOR forward, 2 mismatches )
        TTAC ( is also MAJOR reverse, 1 mismatch )
        TTAG ( is also MAJOR forward, 1 mismatch )
        TTAT ( is also MINOR reverse, 1 mismatch )

         * *
        CAAA [0x40]=( RNA_SPLICE_REV | 0x20 )
        CAAC ( is also MAJOR reverse, 1 mismatch )
        CAAG [0x42]=( RNA_SPLICE_REV | 0x20 )
        CAAT [0x43]=( RNA_SPLICE_REV | 0x20 )
        CCAA [0x50]=( RNA_SPLICE_REV | 0x20 )
        CCAC ( is also MAJOR reverse, 1 mismatch )
        CCAG [0x52]=( RNA_SPLICE_REV | 0x20 )
        CCAT [0x53]=( RNA_SPLICE_REV | 0x20 )
        CGAA [0x60]=( RNA_SPLICE_REV | 0x20 )
        CGAC ( is also MAJOR reverse, 1 mismatch )
        CGAG [0x62]=( RNA_SPLICE_REV | 0x20 )
        CGAT [0x63]=( RNA_SPLICE_REV | 0x20 )

    -----------------------------------------------------------
    GTAG    ... MAJOR, forward

        * *
        ATAG ( is also MAJOR forward, 1 mismatch )
        ATCG [0x36]=( RNA_SPLICE_FWD | 0x20 )
        ATGG [0x3A]=( RNA_SPLICE_FWD | 0x20 )
        ATTG [0x3E]=( RNA_SPLICE_FWD | 0x20 )
        CTAG ( is also MAJOR forward, 1 mismatch )
        CTCG [0x76]=( RNA_SPLICE_FWD | 0x20 )
        CTGG [0x7A]=( RNA_SPLICE_FWD | 0x20 )
        CTTG [0x7E]=( RNA_SPLICE_FWD | 0x20 )
        TTAG ( is also MAJOR forward, 1 mismatch )
        TTCG [0xF6]=( RNA_SPLICE_FWD | 0x20 )
        TTGG [0xFA]=( RNA_SPLICE_FWD | 0x20 )
        TTTG [0xFE]=( RNA_SPLICE_FWD | 0x20 )

         **
        GAAG ( is also MAJOR forward, 1 mismatch )
        GACG [0x86]=( RNA_SPLICE_FWD | 0x20 )
        GAGG [0x8A]=( RNA_SPLICE_FWD | 0x20 )
        GATG [0x8E]=( RNA_SPLICE_FWD | 0x20 )
        GCAG ( is also MAJOR forward, 1 mismatch )
        GCCG [0x96]=( RNA_SPLICE_FWD | 0x20 )
        GCGG [0x9A]=( RNA_SPLICE_FWD | 0x20 )
        GCTG [0x9E]=( RNA_SPLICE_FWD | 0x20 )
        GGAG ( is also MAJOR forward, 1 mismatch )
        GGCG [0xA6]=( RNA_SPLICE_FWD | 0x20 )
        GGGG [0xAA]=( RNA_SPLICE_FWD | 0x20 )
        GGTG [0xAE]=( RNA_SPLICE_FWD | 0x20 )

        *  *
        ATAA ( is also MINOR forward, 1 mismatch )
        ATAC ( is also MINOR forward, full match )
        ATAG ( is also MAJOR forward, 1 mismatch )
        ATAT ( is also MINOR forward, 1 mismatch )
        CTAA ( is also MAJOR reverse, 1 mismatch )
        CTAC ( is also MAJOR reverse, full match )
        CTAG ( is also MAJOR forward, 1 mismatch )
        CTAT ( is also MAJOR reverse, 1 mismatch )
        TTAA [0xF0]=( RNA_SPLICE_FWD | 0x20 )
        TTAC ( is also MAJOR reverse, 1 mismatch )
        TTAG ( is also MAJOR forward, 1 mismatch )
        TTAT ( is also MINOR reverse, 1 mismatch )

         * *
        GAAA [0x80]=( RNA_SPLICE_FWD | 0x20 )
        GAAC [0x81]=( RNA_SPLICE_FWD | 0x20 )
        GAAG ( is also MAJOR forward, 1 mismatch )
        GAAT ( is also MINOR reverse, 1 mismatch )
        GCAA [0x90]=( RNA_SPLICE_FWD | 0x20 )
        GCAC [0x91]=( RNA_SPLICE_FWD | 0x20 )
        GCAG ( is also MAJOR forward, 1 mismatch )
        GCAT ( is also MINOR reverse, 1 mismatch )
        GGAA [0xA0]=( RNA_SPLICE_FWD | 0x20 )
        GGAC [0xA1]=( RNA_SPLICE_FWD | 0x20 )
        GGAG ( is also MAJOR forward, 1 mismatch )
        GGAT ( is also MINOR reverse, 1 mismatch )

    -----------------------------------------------------------
    GTAT    ... MINOR, reverse

        * *
        ATAT ( is also MINOR forward, 1 mismatch )
        ATCT [0x37]=( RNA_SPLICE_REV | 0x20 )
        ATGT [0x3B]=( RNA_SPLICE_REV | 0x20 )
        ATTT [0x3F]=( RNA_SPLICE_REV | 0x20 )
        CTAT ( is also MAJOR reverse, 1 mismatch )
        CTCT [0x77]=( RNA_SPLICE_REV | 0x20 )
        CTGT [0x7B]=( RNA_SPLICE_REV | 0x20 )
        CTTT [0x7F]=( RNA_SPLICE_REV | 0x20 )
        TTAT ( is also MINOR reverse, 1 mismatch )
        TTCT [0xF7]=( RNA_SPLICE_REV | 0x20 )
        TTGT [0xFB]=( RNA_SPLICE_REV | 0x20 )
        TTTT [0xFF]=( RNA_SPLICE_REV | 0x20 )

         **
        GAAT ( is also MINOR reverse, 1 mismatch )
        GACT [0x87]=( RNA_SPLICE_REV | 0x20 )
        GAGT [0x8B]=( RNA_SPLICE_REV | 0x20 )
        GATT [0x8F]=( RNA_SPLICE_REV | 0x20 )
        GCAT ( is also MINOR reverse, 1 mismatch )
        GCCT [0x97]=( RNA_SPLICE_REV | 0x20 )
        GCGT [0x9B]=( RNA_SPLICE_REV | 0x20 )
        GCTT [0x9F]=( RNA_SPLICE_REV | 0x20 )
        GGAT ( is also MINOR reverse, 1 mismatch )
        GGCT [0xA7]=( RNA_SPLICE_REV | 0x20 )
        GGGT [0xAB]=( RNA_SPLICE_REV | 0x20 )
        GGTT [0xAF]=( RNA_SPLICE_REV | 0x20 )

        *  *
        ATAA ( is also MINOR forward, 1 mismatch )
        ATAC ( is also MINOR forward, full match )
        ATAG ( is also MAJOR forward, 1 mismatch )
        ATAT ( is also MINOR forward, 1 mismatch )
        CTAA ( is also MAJOR reverse, 1 mismatch )
        CTAC ( is also MAJOR reverse, full match )
        CTAG ( is also MAJOR forward, 1 mismatch )
        CTAT ( is also MAJOR reverse, 1 mismatch )
        TTAA ( is also MAJOR forward, 2 mismatches )
        TTAC ( is also MAJOR reverse, 1 mismatch )
        TTAG ( is also MAJOR forward, 1 mismatch )
        TTAT ( is also MINOR reverse, 1 mismatch )

         * *
        GAAA ( is also MAJOR forward, 2 mismatches )
        GAAC ( is also MAJOR forward, 2 mismatches )
        GAAG ( is also MAJOR forward, 1 mismatch )
        GAAT ( is also MINOR reverse, 1 mismatch )
        GCAA ( is also MAJOR forward, 2 mismatches )
        GCAC ( is also MAJOR forward, 2 mismatches )
        GCAG ( is also MAJOR forward, 1 mismatch )
        GCAT ( is also MINOR reverse, 1 mismatch )
        GGAA ( is also MAJOR forward, 2 mismatches )
        GGAC ( is also MAJOR forward, 2 mismatches )
        GGAG ( is also MAJOR forward, 1 mismatch )
        GGAT ( is also MINOR reverse, 1 mismatch )

*************************************************************/

rc_t check_rna_splicing_candidates_against_ref( struct ReferenceObj const * ref_obj,
                                                uint32_t splice_level, /* 0, 1, 2 ... allowed mismatches */
                                                INSDC_coord_zero pos,
                                                rna_splice_candidates * candidates )
{
    rc_t rc = 0;
    uint32_t idx;
    for ( idx = 0; idx < candidates->count && rc == 0; ++idx )
    {
        uint8_t splice[ 4 ];
        INSDC_coord_len written;
        rna_splice_candidate * rsc = &candidates->candidates[ idx ];
        INSDC_coord_zero rd_pos = ( pos + rsc->ref_offset );
        rc = ReferenceObj_Read( ref_obj, rd_pos, 2, splice, &written );
        if ( rc == 0 && written == 2 )
        {
            rd_pos += ( rsc->len - 2 );
            rc = ReferenceObj_Read( ref_obj, rd_pos, 2, &splice[ 2 ], &written );
            if ( rc == 0 && written == 2 )
            {
                uint8_t compressed = compress_4_bases_to_byte( splice );   /* 4 bases --> 1 byte */
                uint8_t match = compressed_to_fwd_reverse_0[ compressed ]; /* table lookup */
                uint8_t mismatches = ( match >> 4 );

                if ( mismatches <= splice_level )
                {
                    rsc->matched = match;

                    if ( ( match & RNA_SPLICE_FWD ) == RNA_SPLICE_FWD )
                        candidates->fwd_matched++;
                    else if ( ( match & RNA_SPLICE_REV ) == RNA_SPLICE_REV )
                        candidates->rev_matched++;
                }
            }
        }
    }
    return rc;
}


rc_t discover_rna_splicing_candidates( uint32_t cigar_len, const char * cigar, uint32_t min_len, rna_splice_candidates * candidates )
{
    rc_t rc = 0;
    candidates->cigops_len = ( cigar_len / 2 ) + 1;
    candidates->cigops = malloc( ( sizeof * candidates->cigops ) * candidates->cigops_len );
    if ( candidates->cigops == NULL )
        rc = RC( rcExe, rcNoTarg, rcConstructing, rcMemory, rcExhausted );
    else
    {
        uint32_t ref_offset = 0;
        int32_t op_idx;
        CigOps * cigops = candidates->cigops;

        candidates->n_cigops = ExplodeCIGAR( cigops, candidates->cigops_len, cigar, cigar_len );
        candidates->count = 0;
        for ( op_idx = 0; op_idx < ( candidates->n_cigops - 1 ); op_idx++ )
        {
            char op_code = cigops[ op_idx ].op;
            uint32_t op_len = cigops[ op_idx ].oplen;
            if ( (op_code == 'D' || op_code == 'N') && op_len >= min_len && candidates->count < MAX_RNA_SPLICE_CANDIDATES )
            {
                rna_splice_candidate * rsc = &candidates->candidates[ candidates->count++ ];
                rsc->ref_offset = ref_offset;
                rsc->len = op_len;
                rsc->op_idx = op_idx;
                rsc->matched = RNA_SPLICE_UNKNOWN;  /* we dont know that yet, caller has to do that ( sam-aligned.c ) */
            }
            if ( op_code == 'M' || op_code == 'X' || op_code == '=' || op_code == 'D' || op_code == 'N' )
                ref_offset += op_len;
        }
    }
    return rc;
}



rc_t change_rna_splicing_cigar( uint32_t cigar_len, char * cigar, rna_splice_candidates * candidates, uint32_t * NM_adjustment )
{
    rc_t rc = 0;
    uint32_t winner, sum_of_n_lengths = 0;
    int32_t idx, dst;
    CigOps * cigops = candidates->cigops;

    /* handle the special case that we do have forward and reverse candidates in one alignement, that cannot be!
       we declare a winner ( the direction that occurs most ), zero out the looser(s) and give a warning */
    if ( candidates->fwd_matched > candidates->rev_matched )
        winner = RNA_SPLICE_FWD;
    else
        winner = RNA_SPLICE_REV;

    for ( idx = 0; idx < candidates->count; ++idx )    
    {
        rna_splice_candidate * rsc = &candidates->candidates[ idx ];
        if ( ( rsc->matched & 0x0F ) == winner && cigops[ rsc->op_idx ].op == 'D' )
        {
            cigops[ rsc->op_idx ].op = 'N';
            sum_of_n_lengths += rsc->len;
        }
    }

    for ( idx = 0, dst = 0; idx < ( candidates->n_cigops - 1 ) && rc == 0; ++idx )
    {
        size_t sz;
        rc = string_printf( &cigar[ dst ], cigar_len + 1 - dst, &sz, "%u%c", cigops[ idx ].oplen, cigops[ idx ].op );
        dst += sz;
    }
    if ( NM_adjustment != NULL )
        *NM_adjustment = sum_of_n_lengths;
    return rc;
}
rc_t cg_canonical_print_cigar( const char * cigar, size_t cigar_len)
{
    rc_t rc;
    if ( cigar_len > 0 )
    {
        int i,total_cnt,cnt;
        char op;
        for(i=0,cnt=0,op=0,cnt=0;i<cigar_len;i++){
                if(isdigit(cigar[i])){
                        cnt=cnt*10+(cigar[i]-'0');
                } else if(isalpha(cigar[i])){
                        if(op=='\0'){ /** first op **/
                                total_cnt=cnt;
                        } else if(op==cigar[i]){ /** merging consequitive ops **/
                                total_cnt+=cnt;
                        } else {
                                if(total_cnt > 0) KOutMsg( "%d%c", total_cnt,op );
                                total_cnt=cnt;
                        }
                        op=cigar[i];
                        cnt=0;
                } else {
                        assert(0); /*** should never happen inside this function ***/
                }
        }
        if(total_cnt && op) KOutMsg( "%d%c", total_cnt,op );
    }
    else
        rc = KOutMsg( "*" );
    return rc;
}

