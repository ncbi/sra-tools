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

#include <search/ref-variation.h>
#include <search/smith-waterman.h>

#include <klib/rc.h>
#include <klib/refcount.h>
#include <klib/text.h>

#include <insdc/insdc.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if WINDOWS
#include <intrin.h>
#ifndef __builtin_popcount
#define __builtin_popcount __popcnt
#endif
#endif

#include <sysalloc.h>

#ifndef min
#define min(x,y) ((y) < (x) ? (y) : (x))
#endif

#ifndef max
#define max(x,y) ((y) >= (x) ? (y) : (x))
#endif

#define max4(x1, x2, x3, x4) (max( max((x1),(x2)), max((x3),(x4)) ))

#define COMPARE_4NA 0
#define CACHE_MAX_ROWS 0 /* and columns as well */
#define GAP_SCORE_LINEAR 0
#define SIMILARITY_MATCH 2
#define SIMILARITY_MISMATCH -1
#define SW_DEBUG_PRINT 0

struct RefVariation
{
    KRefcount refcount;

    INSDC_dna_text* var_buffer; /* in the case of deletion
        it contains <ref_base_before><allele><ref_base_after>
        otherwise it contains allele only */
    INSDC_dna_text const* allele; /* points to allele in the var_buffer */
    size_t allele_start;
    size_t var_buffer_size;
    size_t allele_size;
    size_t allele_len_on_ref;
};


#if COMPARE_4NA == 1

unsigned char const map_char_to_4na [256] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1,14, 2,13, 0, 0, 4,11, 0, 0,12, 0, 3,15, 0,
    0, 0, 5, 6, 8, 0, 7, 9, 0,10, 0, 0, 0, 0, 0, 0,
    0, 1,14, 2,13, 0, 0, 4,11, 0, 0,12, 0, 3,15, 0,
    0, 0, 5, 6, 8, 0, 7, 9, 0,10, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static int compare_4na ( INSDC_dna_text ch2na, INSDC_dna_text ch4na )
{
    unsigned char bits4na = map_char_to_4na [(unsigned char)ch4na];
    unsigned char bits2na = map_char_to_4na [(unsigned char)ch2na];

    /*return (bits2na & bits4na) != 0 ? 2 : -1;*/

    unsigned char popcnt4na = (unsigned char) __builtin_popcount ( bits4na );

    return (bits2na & bits4na) != 0 ? 12 / popcnt4na : -6;
}
#endif

static int similarity_func (INSDC_dna_text ch2na, INSDC_dna_text ch4na)
{
#if COMPARE_4NA == 1
    return compare_4na ( ch2na, ch4na );
#else
    return tolower(ch2na) == tolower(ch4na) ? SIMILARITY_MATCH : SIMILARITY_MISMATCH;
#endif
}

static int gap_score_const ( size_t idx )
{
    return -1;
}

static int gap_score_linear ( size_t idx )
{
#if COMPARE_4NA == 1
    return -6*(int)idx;
#else
    return -(int)idx;
#endif
}

static int (*gap_score_func) (size_t ) =
#if GAP_SCORE_LINEAR != 0
    gap_score_linear
#else
    gap_score_const
#endif
; 

typedef struct ValueIndexPair
{
    size_t index;
    int value;
} ValueIndexPair;


static char get_char (INSDC_dna_text const* str, size_t size, size_t pos, bool reverse)
{
    if ( !reverse )
        return str [pos];
    else
        return str [size - pos - 1];
}

rc_t calculate_similarity_matrix (
    INSDC_dna_text const* text, size_t size_text,
    INSDC_dna_text const* query, size_t size_query,
    bool gap_score_constant,
    int* matrix, bool reverse, 
    int* max_score, size_t* max_row, size_t* max_col )
{

    size_t ROWS = size_text + 1;
    size_t COLUMNS = size_query + 1;
    size_t i, j;

    /* arrays to store maximums for all previous rows and columns */
#if CACHE_MAX_ROWS != 0

    ValueIndexPair* vec_max_cols = NULL;
    ValueIndexPair* vec_max_rows = NULL;

    vec_max_cols = calloc ( COLUMNS, sizeof vec_max_cols [ 0 ] );
    if ( vec_max_cols == NULL)
        return RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);

    vec_max_rows = calloc ( ROWS, sizeof vec_max_rows [ 0 ] );
    if ( vec_max_rows == NULL)
    {
        free (vec_max_cols);
        return RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
    }

#endif
    gap_score_func = gap_score_constant ? gap_score_const : gap_score_linear;

    if ( max_score != NULL )
    {
        *max_score = 0;
    }
    if ( max_row != NULL )
    {
        *max_row = 0;
    }
    if ( max_col != NULL )
    {
        *max_col = 0;
    }
    // init 1st row and column with zeros
    memset ( matrix, 0, COLUMNS * sizeof(matrix[0]) );
    for ( i = 1; i < ROWS; ++i )
        matrix [i * COLUMNS] = 0;

    for ( i = 1; i < ROWS; ++i )
    {
        for ( j = 1; j < COLUMNS; ++j )
        {
#if CACHE_MAX_ROWS == 0
            size_t k, l;
#endif
            int cur_score_del, cur_score_ins;
            int sim = similarity_func (
                            get_char (text, size_text, i-1, reverse),
                            get_char (query, size_query, j-1, reverse) );

#if CACHE_MAX_ROWS != 0
            /* TODO: incorrect logic: we cache max{matrix[x,y]}
                instead of max{matrix[x,y] + gap_score_func(v)}
                when it's fixed we probably will need to make adjustments here
            */
            cur_score_del = vec_max_cols[j].value + gap_score_func(j - vec_max_cols[j].index);
#else
            cur_score_del = -1;
            for ( k = 1; k < i; ++k )
            {
                int cur = matrix [ (i - k)*COLUMNS + j ] + gap_score_func(k);
                if ( cur > cur_score_del )
                    cur_score_del = cur;
            }
#endif

#if CACHE_MAX_ROWS != 0
            /* TODO: incorrect logic: we cache max{matrix[x,y]}
                instead of max{matrix[x,y] + gap_score_func(v)}
                when it's fixed we probably will need to make adjustments here
            */
            cur_score_ins = vec_max_rows[i].value + gap_score_func(i - vec_max_rows[i].index);;
#else
            
            cur_score_ins = -1;
            for ( l = 1; l < j; ++l )
            {
                int cur = matrix [ i*COLUMNS + (j - l) ] + gap_score_func(l);
                if ( cur > cur_score_ins )
                    cur_score_ins = cur;
            }
#endif
            {
                int score = max4 ( 0,
                                   matrix[(i-1)*COLUMNS + j - 1] + sim,
                                   cur_score_del,
                                   cur_score_ins);
                matrix[i*COLUMNS + j] = score;
                if ( max_score != NULL && score > *max_score )
                {
                    *max_score = score;
                    if ( max_row != NULL )
                    {
                        *max_row = i;
                    }
                    if ( max_col != NULL )
                    {
                        *max_col = j;
                    }
                }
            }

#if CACHE_MAX_ROWS != 0
            /* TODO: incorrect logic: we cache max{matrix[x,y]}
                instead of max{matrix[x,y] + gap_score_func(v)}
            */
            if ( matrix[i*COLUMNS + j] > vec_max_cols[j].value )
            {
                vec_max_cols[j].value = matrix[i*COLUMNS + j];
                vec_max_cols[j].index = j;
            }
#if GAP_SCORE_LINEAR != 0
            vec_max_cols[j].value += gap_score_func(1);
#endif

#endif
#if CACHE_MAX_ROWS != 0
            /* TODO: incorrect logic: we cache max{matrix[x,y]}
                instead of max{matrix[x,y] + gap_score_func(v)}
            */
            if ( matrix[i*COLUMNS + j] > vec_max_rows[i].value )
            {
                vec_max_rows[i].value = matrix[i*COLUMNS + j];
                vec_max_rows[i].index = i;
            }
#if GAP_SCORE_LINEAR != 0
            vec_max_rows[i].value += gap_score_func(1);
#endif

#endif
        }
    }

#if CACHE_MAX_ROWS != 0
    free (vec_max_cols);
    free (vec_max_rows);
#endif

    return 0;
}

void 
sw_find_indel_box ( int* matrix, size_t ROWS, size_t COLUMNS,
    int* ret_row_start, int* ret_row_end,
    int* ret_col_start, int* ret_col_end )
{
    size_t max_row = 0, max_col = 0;
    size_t max_i = ROWS*COLUMNS - 1;

    size_t i = max_i, j;
    int prev_indel = 0;
    
    max_row = max_i / COLUMNS;
    max_col = max_i % COLUMNS;

    *ret_row_start = *ret_row_end = *ret_col_start = *ret_col_end = -1;

    i = max_row;
    j = max_col;

    /* traceback to (0,0)-th element of the matrix */
    while (1)
    {
        if (i > 0 && j > 0)
        {
            /* TODO: ? strong '>' - because we want to prefer indels over matches/mismatches here
            (expand the window of ambiguity as much as possible)*/
            if ( matrix [(i - 1)*COLUMNS + (j - 1)] >= matrix [i*COLUMNS + (j - 1)] &&
                matrix [(i - 1)*COLUMNS + (j - 1)] >= matrix [(i - 1)*COLUMNS + j])
            {
                int diag_diff = matrix [i*COLUMNS + j] - matrix [(i - 1)*COLUMNS + (j - 1)];
                int mismatch = diag_diff == SIMILARITY_MATCH ? 0 : 1;

                if (mismatch && *ret_row_end == -1 )
                {
                    *ret_row_end = (int)i;
                    *ret_col_end = (int)j;
                }

                --i;
                --j;

                if (prev_indel || mismatch)
                {
                    *ret_row_start = (int)i;
                    *ret_col_start = (int)j;
                }

                prev_indel = 0;
            }
            else if ( matrix [(i - 1)*COLUMNS + (j - 1)] < matrix [i*COLUMNS + (j - 1)] )
            {
                if ( *ret_row_end == -1 )
                {
                    *ret_row_end = (int)i;
                    *ret_col_end = (int)j;
                }
                --j;
                prev_indel = 1;
            }
            else
            {
                if ( *ret_row_end == -1 )
                {
                    *ret_row_end = (int)i;
                    *ret_col_end = (int)j;
                }
                --i;
                prev_indel = 1;
            }
        }
        else if ( i > 0 )
        {
            if ( *ret_row_end == -1 )
            {
                *ret_row_end = (int)i;
                *ret_col_end = 0;
            }
            *ret_row_start = 0;
            *ret_col_start = 0;
            break;
        }
        else if ( j > 0 )
        {
            if ( *ret_row_end == -1 )
            {
                *ret_row_end = 0;
                *ret_col_end = (int)j;
            }
            *ret_row_start = 0;
            *ret_col_start = 0;
            break;
        }
        else
        {
            break;
        }
    }
}

#if SW_DEBUG_PRINT != 0
#include <stdio.h>
void print_matrix ( int const* matrix,
                    char const* ref_slice, size_t ref_slice_size,
                    char const* query, size_t query_size,
                    bool reverse )
{
    size_t COLUMNS = ref_slice_size + 1;
    size_t ROWS = query_size + 1;

    int print_width = 2;
    size_t i, j;

    printf ("  %*c ", print_width, '-');
    for (j = 1; j < COLUMNS; ++j)
        printf ("%*c ", print_width, get_char ( ref_slice, ref_slice_size, j-1, reverse ));
    printf ("\n");

    for (i = 0; i < ROWS; ++i)
    {
        if ( i == 0 )
            printf ("%c ", '-');
        else
            printf ("%c ", get_char (query, query_size, i-1, reverse ));
    
        for (j = 0; j < COLUMNS; ++j)
        {
            printf ("%*d ", print_width, matrix[i*COLUMNS + j]);
        }
        printf ("\n");
    }
}

#endif

/*
    FindRefVariationBounds uses Smith-Waterman algorithm
    to find theoretical bounds of the variation for
    the given reference slice and the query (properly preapared,
    i.e. containing sequences of bases at the beginning and
    the end matching the reference)

    ref_slice, ref_slice_size [IN] - the reference slice on which the
                                     variation will be looked for
    query, query_size [IN] - the query that represents the variation placed
                             inside the reference slice
    ref_start, ref_len [OUT, NULL OK] - the region of ambiguity on the reference
    have_indel [OUT] - pointer to flag indication if there is an insertion or deletion
                       (1 - there is an indel, 0 - there is match/mismatch only)
*/
static
rc_t FindRefVariationBounds (
    INSDC_dna_text const* ref_slice, size_t ref_slice_size,
    INSDC_dna_text const* query, size_t query_size,
    size_t* ref_start, size_t* ref_len, bool * has_indel
    )
{
    /* building sw-matrix for chosen reference slice and sequence */

    size_t COLUMNS = ref_slice_size + 1;
    size_t ROWS = query_size + 1;
    rc_t rc = 0;
    
    bool gap_score_constant = ( GAP_SCORE_LINEAR == 0 );

    int row_start, col_start, row_end, col_end;
    int row_start_rev, col_start_rev, row_end_rev, col_end_rev;
    int* matrix = malloc( ROWS * COLUMNS * sizeof (int) );
    if (matrix == NULL)
        return RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
    * has_indel = true;
    
    
    /* forward scan */
    rc = calculate_similarity_matrix ( query, query_size, ref_slice, ref_slice_size, gap_score_constant, matrix, false, NULL, NULL, NULL );
    if ( rc != 0 )
        goto free_resources;
#if SW_DEBUG_PRINT != 0
    print_matrix ( matrix, ref_slice, ref_slice_size, query, query_size, false );
#endif

    sw_find_indel_box ( matrix, ROWS, COLUMNS, &row_start, &row_end, &col_start, &col_end );
    if ( row_start == -1 && row_end == -1 && col_start == -1 && col_end == -1 )
    {
        * has_indel = 0;
        goto free_resources;
    }
#if SW_DEBUG_PRINT != 0
    printf ("start=(%d, %d), end=(%d, %d)\n", row_start, col_start, row_end, col_end);
#endif

    /* reverse scan */
    rc = calculate_similarity_matrix ( query, query_size, ref_slice, ref_slice_size, gap_score_constant, matrix, true, NULL, NULL, NULL );
    if ( rc != 0 )
        goto free_resources;
#if SW_DEBUG_PRINT != 0
    print_matrix ( matrix, ref_slice, ref_slice_size, query, query_size, true );
#endif

    sw_find_indel_box ( matrix, ROWS, COLUMNS, &row_start_rev, &row_end_rev, &col_start_rev, &col_end_rev );
#if SW_DEBUG_PRINT != 0
    printf ("start_rev=(%d, %d), end_rev=(%d, %d)\n", row_start_rev, col_start_rev, row_end_rev, col_end_rev);
#endif
    if ( row_start_rev != -1 || row_end_rev != -1 || col_start_rev != -1 || col_end_rev != -1 )
    {
        row_start = min ( (int)query_size - row_end_rev - 1, row_start );
        row_end   = max ( (int)query_size - row_start_rev - 1, row_end );
        col_start = min ( (int)ref_slice_size - col_end_rev - 1, col_start );
        col_end   = max ( (int)ref_slice_size - col_start_rev - 1, col_end );
    }
#if SW_DEBUG_PRINT != 0
    printf ("COMBINED: start=(%d, %d), end=(%d, %d)\n", row_start, col_start, row_end, col_end);
#endif

    if ( ref_start != NULL )
        *ref_start = col_start + 1;
    if ( ref_len != NULL )
        *ref_len = col_end - col_start - 1;

free_resources:
    free (matrix);

    return rc;
}

/****************************************************/
/* yet another string helper */
typedef struct c_string_const
{
    char const* str;
    size_t size;
} c_string_const;

static void c_string_const_assign ( c_string_const* self, char const* src, size_t size )
{
    self -> str = src;
    self -> size = size;
}

typedef struct c_string
{
    char* str;
    size_t size;
    size_t capacity;
} c_string;

static int c_string_make ( c_string* self, size_t capacity )
{
    assert ( capacity > 0 );
    self -> str = malloc (capacity + 1);
    if ( self -> str != NULL )
    {
        self -> str [0] = '\0';
        self -> size = 0;
        self -> capacity = capacity;
        return 1;
    }
    else
        return 0;
}

static void c_string_destruct ( c_string* self )
{
    if ( self->str != NULL )
    {
        free ( self -> str );
        self -> str = NULL;
        self -> size = 0;
        self -> capacity = 0;
    }
}

static int c_string_realloc_no_preserve ( c_string* self, size_t new_capacity )
{
    if ( self -> capacity < new_capacity )
    {
        c_string_destruct ( self );

        return c_string_make ( self, new_capacity );
    }
    else
    {
        self -> str [0] = '\0';
        self -> size = 0;
    }

    return 1;
}

static int c_string_assign ( c_string* self, char const* src, size_t src_size )
{
    assert ( self->capacity >= src_size );
    if ( self->capacity < src_size && !c_string_realloc_no_preserve (self, max(self->capacity * 2, src_size)) )
        return 0;

    memmove ( self -> str, src, src_size );
    self -> str [src_size] = '\0';
    self -> size = src_size;

    return 1;
}

static int c_string_append ( c_string* self, char const* append, size_t append_size)
{
    if ( append_size != 0 )
    {
        size_t new_size = self->size + append_size;
        if ( self->capacity >= new_size )
        {
            memmove ( self->str + self->size, append, append_size );
            self->size = new_size;
            self->str [new_size] = '\0';
        }
        else
        {
            size_t new_capacity = max (new_size + 1, self->capacity * 2);
            char* new_str = malloc ( new_capacity );
            if (new_str == NULL)
                return 0;

            memmove (new_str, self->str, self->size);
            memmove (new_str + self->size, append, append_size );
            new_str [ new_size ] = '\0';

            c_string_destruct ( self );
        
            self->str = new_str;
            self->size = new_size;
            self->capacity = new_capacity;
        }
    }

    return 1;
}

#if 0
static int c_string_wrap ( c_string* self,
    char const* prefix, size_t prefix_size,
    char const* postfix, size_t postfix_size)
{
    assert ( self -> str != NULL );
    size_t new_size = self->size + prefix_size + postfix_size;

    if ( new_size > self->capacity )
    {
        size_t new_capacity = max (new_size + 1, self->capacity * 2);
        char* new_str = malloc ( new_capacity );
        if (new_str == NULL)
            return 0;

        memmove ( new_str, prefix, prefix_size );
        memmove ( new_str + prefix_size, self -> str, self -> size );
        memmove ( new_str + prefix_size + self->size, postfix, postfix_size );
        new_str [ new_size ] = '\0';

        c_string_destruct ( self );
        
        self->str = new_str;
        self->size = new_size;
        self->capacity = new_capacity;
    }
    else
    {
        memmove ( self->str + prefix_size, self->str, self->size );
        memmove ( self->str, prefix, prefix_size );
        memmove (self->str + prefix_size + self->size, postfix, postfix_size );
        self->str [new_size] = '\0';
    }
    return 1;
}
#endif
/************************************************/


/*
   returns true if a new ref_slice is selected
   returns false if the new ref_slice is the same as the previous one passed in ref_slice
*/

static bool get_ref_slice (
            INSDC_dna_text const* ref, size_t ref_size, size_t ref_pos_var,
            size_t var_len_on_ref,
            size_t slice_expand_left, size_t slice_expand_right,
            c_string_const* ref_slice)
{
    size_t ref_start, ref_xend;
    if ( ref_pos_var < slice_expand_left )
        ref_start = 0;
    else
        ref_start = ref_pos_var - slice_expand_left;

    if ( ref_pos_var + slice_expand_right + var_len_on_ref >= ref_size )
        ref_xend = ref_size;
    else
        ref_xend = ref_pos_var + slice_expand_right + var_len_on_ref;

    if ( ref_slice->str == ref + ref_start && ref_slice->size == ref_xend - ref_start)
        return false;

    c_string_const_assign ( ref_slice, ref + ref_start, ref_xend - ref_start );
    return true;
}

#if 1
static bool make_query ( c_string_const const* ref_slice,
        INSDC_dna_text const* variation, size_t variation_size, size_t var_len_on_ref,
        int64_t var_start_pos_adj, /* ref_pos adjusted to the beginning of ref_slice (in the simplest case - the middle of ref_slice) */
        c_string* query
    )
{
    if ( !c_string_realloc_no_preserve (query, variation_size + ref_slice->size - var_len_on_ref) )
        return false;

    if ( !c_string_append (query, ref_slice->str, var_start_pos_adj) ||
         !c_string_append (query, variation, variation_size) ||
         !c_string_append (query, ref_slice->str + var_start_pos_adj + var_len_on_ref, ref_slice->size - var_start_pos_adj - var_len_on_ref) )
    {
         return false;
    }

    return true;
}

static bool compose_variation ( c_string_const const* ref,
        size_t ref_start, size_t ref_len,
        INSDC_dna_text const* query, size_t query_len,
        int64_t ref_pos_var, size_t var_len_on_ref,
        c_string* variation, char const** pallele, size_t* pallele_size )
{
    bool ret = true;

    size_t ref_end_orig = (size_t)ref_pos_var + var_len_on_ref;
    size_t ref_end_new = ref_start + ref_len;

    size_t prefix_start = ref_start, prefix_len, query_trim_l;
    size_t postfix_start = ref_end_orig, postfix_len, query_trim_r;

    size_t query_len_new, var_len;

    size_t allele_expanded_l = 0, allele_expanded_r = 0;

    if ((int64_t)ref_start <= ref_pos_var) /* left bound is expanded */
    {
        prefix_len = (size_t)ref_pos_var - ref_start;
        query_trim_l = 0;

        assert ((int64_t)prefix_len >= 0);
    }
    else /* left bound is shrinked */
    {
        prefix_len = 0;
        query_trim_l = ref_start - (size_t)ref_pos_var;

        assert ((int64_t)query_trim_l >= 0);
    }

    if (ref_end_new >= ref_end_orig) /* right bound is expanded */
    {
        postfix_start = ref_end_orig;
        postfix_len = ref_end_new - ref_end_orig;
        query_trim_r = 0;
    }
    else /* right bound is shrinked */
    {
        postfix_start = ref_end_new;
        postfix_len = 0;
        query_trim_r = ref_end_orig - ref_end_new;
    }

    /*
    special case: pure match/mismatch
    algorithm gives ref_len = 0, but in this case
    we want to have variation = input query
    */
    if ( ref_len == 0 && query_len == var_len_on_ref )
    {
        assert ( prefix_len == 0 );
        assert ( postfix_len == 0 );
        assert ( query_trim_l == 0 );
        assert ( query_trim_r > 0 );

        query_trim_r = 0;
    }
    /*
    special case: deletion
    we have to create a query whis is the allele
    expanded one base to the left and to the right
    on the reference if possible
    */
    else if ( var_len_on_ref > query_len )
    {
        if ( prefix_start > 0 )
        {
            allele_expanded_l = 1;
            prefix_start -= 1;
            ++prefix_len;
        }

        if ( postfix_start + postfix_len + 1 < ref->size)
        {
            allele_expanded_r = 1;
            ++postfix_len;
        }
    }

    query_len_new = query_len - query_trim_l - query_trim_r;
    assert ((int64_t)query_len_new >= 0);
    var_len = prefix_len + query_len_new + postfix_len;

    if ( var_len > 0 )
    {
        /* non-empty variation */
        if ( !c_string_realloc_no_preserve( variation, var_len ) )
            ret = false;

        if ( prefix_len > 0 )
            ret = ret && c_string_assign (variation, ref->str + prefix_start, prefix_len);
        
        if ( query_len_new > 0 )
            ret = ret && c_string_append (variation, query + query_trim_l, query_len_new);

        if ( postfix_len > 0 )
            ret = ret && c_string_append (variation, ref->str + postfix_start, postfix_len);

        if ( ! ret )
            c_string_destruct ( variation );

        *pallele = variation->str + allele_expanded_l;
        *pallele_size = variation->size - allele_expanded_l - allele_expanded_r;
    }
    else
    {
        /* in this case there is no query - don't allocate anything */
        ret = true;
        assert ( 0 ); /* since we expand deletions,
                      this code shall not be reached.
                      theoretically it can be reached if 
                      reference has length == 0 only */
    }

    return ret;
}

#endif

#if 0
static bool make_query_ (
        INSDC_dna_text const* ref, size_t ref_size, size_t ref_pos_var,
        INSDC_dna_text const* variation, size_t variation_size, size_t var_len_on_ref,
        size_t slice_expand_left, size_t slice_expand_right,
        c_string* query
    )
{
    size_t ref_prefix_start, ref_prefix_len, ref_suffix_start, ref_suffix_len;
    if ( !c_string_realloc_no_preserve (query, variation_size + slice_expand_left + slice_expand_right + var_len_on_ref) )
        return false;

    if ( ref_pos_var < slice_expand_left )
    {
        ref_prefix_start = 0;
        ref_prefix_len = slice_expand_left - (ref_pos_var - 1);
    }
    else
    {
        ref_prefix_start = ref_pos_var - slice_expand_left;
        ref_prefix_len = slice_expand_left;
    }

    ref_suffix_start = ref_pos_var + var_len_on_ref;

    if ( ref_suffix_start + slice_expand_right >= ref_size )
        ref_suffix_len = ref_size - (slice_expand_right + 1);
    else
        ref_suffix_len = slice_expand_right;

    if ( !c_string_append (query, ref + ref_prefix_start, ref_prefix_len) ||
         !c_string_append (query, variation, variation_size) ||
         !c_string_append (query, ref + ref_suffix_start, ref_suffix_len) )
    {
         return false;
    }

    return true;
}
#endif

/*
    FindRefVariationRegionIUPAC_SW uses Smith-Waterman algorithm
    to find theoretical bounds of the variation for
    the given reference, position on the reference
    and the raw query, or variation to look for at the given
    reference position

    ref, ref_size [IN]     - the reference on which the
                             variation will be looked for
    ref_pos_var [IN]       - the position on reference to look for the variation
    variation, variation_size [IN] - the variation to look for at the ref_pos_var
    var_len_on_ref [IN]    - the length of the variation on the reference, e.g.:
                           - mismatch, 2 bases: variation = "XY", var_len_on_ref = 2
                           - deletion, 3 bases: variation = "", var_len_on_ref = 3
                           - insertion, 2 bases:  variation = "XY", var_len_on_ref = 0

    p_ref_start, p_ref_len [OUT, NULL OK] - the region of ambiguity on the reference
                                            (return values)
*/

static rc_t CC FindRefVariationRegionIUPAC_SW (
        INSDC_dna_text const* ref, size_t ref_size, size_t ref_pos_var,
        INSDC_dna_text const* variation, size_t variation_size, size_t var_len_on_ref,
        size_t* p_ref_start, size_t* p_ref_len
    )
{
    rc_t rc = 0;

    size_t var_half_len = 1;/*variation_size / 2 + 1;*/

    size_t exp_l = var_half_len;
    size_t exp_r = var_half_len;

    /* previous start and end for reference slice */
    int64_t slice_start = -1, slice_end = -1;

    c_string_const ref_slice;
    c_string query;

    size_t ref_start = 0, ref_len = 0;

    ref_slice.str = NULL;
    ref_slice.size = 0;

    if ( !c_string_make ( & query, ( variation_size + 1 ) * 2 ) )
        return RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);

    while ( 1 )
    {
        int64_t new_slice_start, new_slice_end;
        int64_t ref_pos_adj;
        int cont = 0;
        bool has_indel = false;

        /* get new expanded slice and check if it has not reached the bounds of ref */
        bool slice_expanded = get_ref_slice ( ref, ref_size, ref_pos_var, var_len_on_ref, exp_l, exp_r, & ref_slice );
        if ( !slice_expanded )
            break;

        /* get ref_pos relative to ref_slice start and new slice_start and end */
        ref_pos_adj = (int64_t)ref_pos_var - ( ref_slice.str - ref );
        new_slice_start = ref_slice.str - ref;
        new_slice_end = new_slice_start + ref_slice.size;

        /* compose a new query for newly extended ref slice */
        /*if ( !make_query_( ref, ref_size, ref_pos_var, variation, variation_size, var_len_on_ref, exp_l, exp_r, & query ) )*/
        if ( !make_query ( & ref_slice, variation, variation_size, var_len_on_ref, ref_pos_adj, & query ) )
        {
            rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
            goto free_resources;
        }

        rc = FindRefVariationBounds ( ref_slice.str, ref_slice.size,
                        query.str, query.size, & ref_start, & ref_len, & has_indel );

        /* if there are no indels report that there is no ambiguity
           for the given ref_pos_var: region starting at ref_pos_var has length = 0
           ambiguity
        */
        if ( !has_indel )
        {
            ref_start = ref_pos_adj;
            ref_len = 0;
        }

        if ( rc != 0 )
            goto free_resources;

        /* if we've found the ambiguity window starting at the very
           beginning of the ref slice and if we're still able to extend
           ref slice to the left (haven't bumped into boundary) - extend to the left
           and repeat the search
        */
        if ( ref_start == 0 && (slice_start == -1 || new_slice_start != slice_start ) )
        {
            exp_l *= 2;
            cont = 1;
        }

        /* if we've found the ambiguity window ending at the very
           end of the ref slice and if we're still able to extend
           ref slice to the right (haven't bumped into boundary) - extend to the right
           and repeat the search
        */
        if ( ref_start + ref_len == ref_slice.size && (slice_end == -1 || new_slice_end != slice_end) )
        {
            exp_r *= 2;
            cont = 1;
        }

        if ( !cont )
            break;

        slice_start = new_slice_start;
        slice_end = new_slice_end;
    }
    if ( p_ref_start != NULL )
        *p_ref_start = ref_start + (ref_slice.str - ref);
    if ( p_ref_len != NULL )
        *p_ref_len = ref_len;

free_resources:
    c_string_destruct ( &query );

    return rc;
}

/*
    FindRefVariationRegionIUPAC_RA uses Rolling-bulldozer algorithm
    to find theoretical bounds of the variation for
    the given reference, position on the reference
    and the raw query, or variation to look for at the given
    reference position

    ref, ref_size [IN]     - the reference on which the
                             variation will be looked for
    ref_pos_var [IN]       - the position on reference to look for the variation
    variation, variation_size [IN] - the variation to look for at the ref_pos_var
    var_len_on_ref [IN]    - the length of the variation on the reference, e.g.:
                           - mismatch, 2 bases: variation = "XY", var_len_on_ref = 2
                           - deletion, 3 bases: variation = "", var_len_on_ref = 3
                           - insertion, 2 bases:  variation = "XY", var_len_on_ref = 0

    p_ref_start, p_ref_len [OUT, NULL OK] - the region of ambiguity on the reference
                                            (return values)
*/

static rc_t CC FindRefVariationRegionIUPAC_RA (
        INSDC_dna_text const* ref, size_t ref_size, size_t ref_pos_var,
        INSDC_dna_text const* variation, size_t variation_size, size_t var_len_on_ref,
        size_t* p_ref_start, size_t* p_ref_len
    )
{
    rc_t rc = 0;
    size_t del_pos_start, del_pos_xend;
    size_t ins_pos_start, ins_pos_xend;

    /* Stage 1: trying to expand deletion */

    /* expanding to the left */
    if (var_len_on_ref > 0)
    {
        for (del_pos_start = ref_pos_var;
            del_pos_start != 0 && ref[del_pos_start-1] == ref[del_pos_start-1 + var_len_on_ref];
            --del_pos_start);

        /* expanding to the right */
        for (del_pos_xend = ref_pos_var + var_len_on_ref;
            del_pos_xend < ref_size && ref[del_pos_xend] == ref[del_pos_xend - var_len_on_ref];
            ++del_pos_xend);
    }
    else
    {
        del_pos_start = ref_pos_var;
        del_pos_xend = ref_pos_var;
    }

    /* Stage 2: trying to expand insertion */

    /* expanding to the left */
    /* roll first repetition to the left (avoiding % operation) */
    if (variation_size > 0)
    {
        if (del_pos_start > 0)
        {
            for (ins_pos_start = ref_pos_var; ins_pos_start != 0; --ins_pos_start)
            {
                size_t pos_in_var = ins_pos_start-1 - ref_pos_var + variation_size;
                size_t pos_in_ref = ins_pos_start-1;
                if ( (int64_t)pos_in_var == -1l || ref[pos_in_ref] != variation[pos_in_var] )
                    break;
            }
            /* roll beyond first repetition (still avoiding %) - now can compare with reference rather than with variation */
            for (; ins_pos_start != 0 && ref[ins_pos_start-1] == ref[ins_pos_start-1 + variation_size];
                --ins_pos_start);
        }
        else
            ins_pos_start = 0;

        /* roll first repetition to the right (avoiding % operation) */
        if (del_pos_xend < ref_size)
        {
            for (ins_pos_xend = ref_pos_var + var_len_on_ref; ins_pos_xend < ref_size; ++ins_pos_xend)
            {
                size_t pos_in_var = ins_pos_xend - ref_pos_var - var_len_on_ref;
                if (pos_in_var == variation_size || ref[ins_pos_xend] != variation[pos_in_var])
                    break;
            }
            /* roll beyond first repetition (still avoiding %) - now can compare with reference rather than with variation */
            if (ins_pos_xend - ref_pos_var - var_len_on_ref == variation_size)
            {
                for (; ins_pos_xend < ref_size && ref[ins_pos_xend] == ref[ins_pos_xend - variation_size];
                    ++ins_pos_xend);
            }
        }
        else
            ins_pos_xend = ref_size;
    }
    else
    {
        ins_pos_start = ref_pos_var;
        ins_pos_xend = ref_pos_var;
    }


    if (del_pos_start > ins_pos_start)
        del_pos_start = ins_pos_start;
    if (del_pos_xend < ins_pos_xend)
        del_pos_xend = ins_pos_xend;

    if ( p_ref_start != NULL )
        *p_ref_start = del_pos_start;
    if ( p_ref_len != NULL )
        *p_ref_len = del_pos_xend - del_pos_start;

    return rc;
}

/*
    FindRefVariationRegionIUPAC
    to find theoretical bounds of the variation for
    the given reference, position on the reference
    and the raw query, or variation to look for at the given
    reference position

    alg                    - algorithm to use for the search (one of RefVarAlg enum)
    ref, ref_size [IN]     - the reference on which the
                             variation will be looked for
    ref_pos_var [IN]       - the position on reference to look for the variation
    variation, variation_size [IN] - the variation to look for at the ref_pos_var
    var_len_on_ref [IN]    - the length of the variation on the reference, e.g.:
                           - mismatch, 2 bases: variation = "XY", var_len_on_ref = 2
                           - deletion, 3 bases: variation = "", var_len_on_ref = 3
                           - insertion, 2 bases:  variation = "XY", var_len_on_ref = 0

    p_ref_start, p_ref_len [OUT, NULL OK] - the region of ambiguity on the reference
                                            (return values)
*/

static rc_t CC FindRefVariationRegionIUPAC (
        RefVarAlg alg, INSDC_dna_text const* ref, size_t ref_size, size_t ref_pos_var,
        INSDC_dna_text const* variation, size_t variation_size, size_t var_len_on_ref,
        size_t* p_ref_start, size_t* p_ref_len
    )
{
    switch (alg)
    {
    case refvarAlgSW:
        return FindRefVariationRegionIUPAC_SW ( ref, ref_size, ref_pos_var, variation, variation_size, var_len_on_ref, p_ref_start, p_ref_len );
    case refvarAlgRA:
        return FindRefVariationRegionIUPAC_RA ( ref, ref_size, ref_pos_var, variation, variation_size, var_len_on_ref, p_ref_start, p_ref_len );
    }
    return RC ( rcVDB, rcExpression, rcConstructing, rcParam, rcUnrecognized );
}

rc_t CC RefVariationIUPACMake (RefVariation ** obj,
        INSDC_dna_text const* ref, size_t ref_len,
        size_t deletion_pos, size_t deletion_len,
        INSDC_dna_text const* insertion, size_t insertion_len
#if REF_VAR_ALG
        , RefVarAlg alg
#endif
    )
{
    struct RefVariation* new_obj;
    rc_t rc = 0;

    if ( ( insertion_len == 0 && deletion_len == 0 )
        || ref_len == 0 )
    {
        return RC (rcText, rcString, rcSearching, rcParam, rcEmpty);
    }

    if ( (deletion_pos + deletion_len) > ref_len )
    {
        return RC (rcText, rcString, rcSearching, rcParam, rcOutofrange);
    }

    assert ( obj != NULL );

    new_obj = calloc ( 1, sizeof * new_obj );

    if ( new_obj == NULL )
    {
        rc = RC ( rcVDB, rcExpression, rcConstructing, rcMemory, rcExhausted );
    }
    else
    {
        size_t ref_window_start = 0, ref_window_len = 0;
        rc = FindRefVariationRegionIUPAC ( alg, ref, ref_len,
                                           deletion_pos,
                                           insertion, insertion_len, deletion_len,
                                           & ref_window_start, & ref_window_len );
        if ( rc != 0 )
        {
            free ( new_obj );
            new_obj = NULL;
        }
        else
        {
            size_t allele_size = 0;
            char const* allele = NULL;

            c_string_const ref_str;
            
            c_string var_str;
            var_str.capacity = var_str.size = 0;
            var_str.str = NULL;

            c_string_const_assign ( & ref_str, ref, ref_len );

            if ( ! compose_variation ( & ref_str,
                                       ref_window_start, ref_window_len,
                                       insertion, insertion_len,
                                       deletion_pos, deletion_len, & var_str,
                                       & allele, & allele_size ) )
            {
                rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
                free ( new_obj );
                new_obj = NULL;
            }
            else
            {
                KRefcountInit ( & new_obj->refcount, 1, "RefVariation", "make", "ref-var" );
                /* moving var_str to the object (so no need to destruct var_str */

                new_obj->var_buffer = var_str.str;
                new_obj->var_buffer_size = var_str.size;

                new_obj->allele = allele;
                new_obj->allele_size = allele_size;

                new_obj->allele_start = ref_window_start;
                new_obj->allele_len_on_ref = ref_window_len == 0 && insertion_len == deletion_len
                    ? deletion_len : ref_window_len;
            }
        }
    }

    * obj = new_obj;

    /* TODO: if Kurt insists, return non-zero rc if var_start == 0 or var_start + var_len == ref_size */
    return rc;
}


rc_t CC RefVariationAddRef ( RefVariation const* self )
{
    if ( self != NULL )
    {
        switch ( KRefcountAdd ( & self -> refcount, "RefVariation" ) )
        {
        case krefLimit:
            return RC ( rcVDB, rcExpression, rcAttaching, rcRange, rcExcessive );
        }
    }
    return 0;
}

static rc_t CC RefVariationIUPACWhack ( RefVariation* self )
{
    KRefcountWhack ( & self -> refcount, "RefVariation" );

    assert ( self->var_buffer != NULL || self->var_buffer_size == 0 );
    if ( self->var_buffer != NULL )
        free ( self->var_buffer );

    memset ( self, 0, sizeof * self );

    free ( self );

    return 0;
}

rc_t CC RefVariationRelease ( RefVariation const* self )
{
    if ( self != NULL )
    {
        switch ( KRefcountDrop ( & self -> refcount, "RefVariation" ) )
        {
        case krefWhack:
            return RefVariationIUPACWhack ( ( RefVariation* ) self );
        case krefNegative:
            return RC ( rcVDB, rcExpression, rcReleasing, rcRange, rcExcessive );
        }
    }
    return 0;
}

rc_t CC RefVariationGetIUPACSearchQuery ( RefVariation const* self,
    INSDC_dna_text const ** query, size_t * query_len, size_t * query_start )
{
    if ( self == NULL )
        return RC ( rcVDB, rcExpression, rcAccessing, rcParam, rcNull );

    if ( query != NULL )
        * query = self->var_buffer;
    if ( query_len != NULL )
        * query_len = self->var_buffer_size;
    if ( query_start != NULL )
        * query_start = self->allele_start - (self->allele - self->var_buffer);

    return 0;
}

rc_t CC RefVariationGetSearchQueryLenOnRef ( RefVariation const* self, size_t * query_len_on_ref )
{
    if ( self == NULL )
        return RC ( rcVDB, rcExpression, rcAccessing, rcParam, rcNull );

    if ( query_len_on_ref != NULL )
        * query_len_on_ref = self->allele_len_on_ref + self->var_buffer_size - self->allele_size;

    return 0;
}

rc_t CC RefVariationGetAllele ( RefVariation const* self,
    INSDC_dna_text const ** allele, size_t * allele_len, size_t * allele_start )
{
    if ( self == NULL )
        return RC ( rcVDB, rcExpression, rcAccessing, rcParam, rcNull );

    if ( allele != NULL )
        * allele = self->allele;
    if ( allele_len != NULL )
        * allele_len = self->allele_size;
    if ( allele_start != NULL )
        * allele_start = self->allele_start;

    return 0;
}

rc_t CC RefVariationGetAlleleLenOnRef ( RefVariation const* self, size_t * allele_len_on_ref )
{
    if ( self == NULL )
        return RC ( rcVDB, rcExpression, rcAccessing, rcParam, rcNull );

    if ( allele_len_on_ref != NULL )
        * allele_len_on_ref = self->allele_len_on_ref;

    return 0;
}

//////////////// Search-oriented SmithWaterman+

struct SmithWaterman
{
    char*   query;
    size_t  query_size;
    size_t  max_rows;  
    int*    matrix; // originally NULL, grows as needed to hold enough memory for query_size * max_rows
};

LIB_EXPORT rc_t CC SmithWatermanMake( SmithWaterman** p_self, const char* p_query )
{
    rc_t rc = 0;

    if( p_self != NULL && p_query != NULL ) 
    {
        SmithWaterman* ret = malloc ( sizeof ( SmithWaterman ) );
        if ( ret != NULL )
        {
            ret -> query = string_dup_measure ( p_query, & ret -> query_size );
            if ( ret -> query != NULL )
            {
                ret -> max_rows = 0;
                ret -> matrix = NULL;
                *p_self = ret;
                return 0;
            }
            else
            {
                rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
            }
            free ( ret );
        }
        else
        {
            rc = RC(rcText, rcString, rcSearching, rcMemory, rcExhausted);
        }
    }
    else
    {
        rc = RC(rcText, rcString, rcSearching, rcParam, rcNull);
    } 

    return rc;
}

LIB_EXPORT void CC 
SmithWatermanWhack( SmithWaterman* self )
{
    free ( self -> matrix  );
    free ( self -> query );
    free ( self );
}

LIB_EXPORT rc_t CC 
SmithWatermanFindFirst( SmithWaterman* p_self, uint32_t p_threshold, const char* p_buf, size_t p_buf_size, SmithWatermanMatch* p_match )
{
    rc_t rc = 0;
    int score;
    size_t max_row;
    size_t max_col;

    if (p_buf_size == 0)
    {
        return SILENT_RC(rcText, rcString, rcSearching, rcQuery, rcNotFound);
    }
    
    if ( p_buf_size > p_self -> max_rows )
    {
        /* calculate_similarity_matrix adds a row and a column, adjust matrix dimensions accordingly */
        int* new_matrix = realloc ( p_self -> matrix, (p_self->query_size + 1) * (p_buf_size + 1) * sizeof(*p_self->matrix) ); 
        if ( new_matrix == NULL )
        {   /* p_self -> matrix is unchanged and can be reused */
            return RC ( rcText, rcString, rcSearching, rcMemory, rcExhausted );
        }
        p_self -> max_rows = p_buf_size; 
        p_self -> matrix = new_matrix;
    }
    /*TODO: pass threshold into calculate_similarity_matrix, have it stop as soon as the score is sufficient */
    rc = calculate_similarity_matrix ( p_buf, p_buf_size, p_self -> query, p_self -> query_size, false, p_self -> matrix, false, &score, &max_row, &max_col );
    if ( rc == 0 )
    {
        if ( p_threshold > p_self->query_size * 2 )
        {
            p_threshold = p_self->query_size * 2;
        }
        if ( score >= p_threshold )
        {
            if ( p_match != NULL )
            {
                /* walk back from the max score row */
                const size_t Columns = p_self->query_size + 1;
                int row = max_row;
                int col = max_col;
                while ( row > 0 && col > 0 )
                {
                    int curr = p_self -> matrix [ row*Columns + col ];
                    if ( curr == 0 )
                    {
                        break;
                    } 
					else
					{
						int left = p_self -> matrix [ row * Columns + (col - 1) ];
						int up   = p_self -> matrix [ (row - 1)*Columns + col ];
						int diag = p_self -> matrix [ (row - 1)*Columns + (col - 1) ]; 
						if ( diag >= left && diag >= up )
						{
							--row;
							--col;
						}
						else if ( diag < left )
						{
							--col;
						}
						else
						{
							--row;
						}
					}
                }
                
                p_match -> position = row;
                p_match -> length = max_row - row;
                p_match -> score = score;
            }    
            return 0;
        }
        rc = SILENT_RC ( rcText, rcString, rcSearching, rcQuery, rcNotFound );
    }
    
    return rc;
}


