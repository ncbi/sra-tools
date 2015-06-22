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

#include <ngs/ncbi/NGS.hpp>
#include <ngs/ReadCollection.hpp>
#include <ngs/Reference.hpp>
#include <kapp/main.h>

#include <iostream>
#include <string.h>
#include <vector>
#include <stdio.h>
#include <stdint.h>

#include "ref-variation.vers.h"

#ifndef min
#define min(x,y) ((y) < (x) ? (y) : (x))
#endif

#ifndef max
#define max(x,y) ((y) >= (x) ? (y) : (x))
#endif

#define max4(x1, x2, x3, x4) (max( max((x1),(x2)), max((x3),(x4)) ))

namespace RefVariation
{
    struct Params
    {
        // command line params

        // command line options
        char const* accession;
        char const* ref_name;
        int64_t ref_pos_ins;
        char const* query;
        int verbosity;

    } g_Params =
    {
        "",
        "",
        -1,
        "",
        0
    };

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

    int compare_4na ( char ch2na, char ch4na )
    {
        unsigned char bits4na = map_char_to_4na [(int)ch4na];
        unsigned char bits2na = map_char_to_4na [(int)ch2na];

        //return (bits2na & bits4na) != 0 ? 2 : -1;

        unsigned char popcnt4na;
        // TODO: optimize, maybe using _popcnt
        switch ( bits4na )
        {
        case 1:
        case 2:
        case 4:
        case 8:
            popcnt4na = 1;
            break;
        case 7:
        case 11:
        case 13:
        case 14:
            popcnt4na = 3;
            break;
        case 15:
            popcnt4na = 4;
            break;
        case 0:
            popcnt4na = 0;
            break;
        //case 3:
        //case 5:
        //case 6:
        //case 9:
        //case 10:
        //case 12:
        //    popcnt4na = 2;
        //    break;
        default:
            popcnt4na = 2;
            break;
        }

        return (bits2na & bits4na) != 0 ? 12 / popcnt4na : -6;
    }

    #define COMPARE_4NA 0

    int similarity_func (char ch2na, char ch4na)
    {
    #if COMPARE_4NA == 1
        return compare_4na ( ch2na, ch4na );
    #else
        return ch2na == ch4na ? 2 : -1;
    #endif
    }

    int gap_score_func ( size_t idx )
    {
    #if COMPARE_4NA == 1
        return -6*(int)idx;
    #else
        return -(int)idx;
    #endif
    }

    #define CACHE_MAX_COLS 1
    #define CACHE_MAX_ROWS 1

    template <bool t_reverse> class CStringIterator
    {
        char const* m_arr;
        size_t m_size;
    public:
        CStringIterator (char const* arr, size_t size)
            : m_arr(arr), m_size(size)
        {
        }

        char const& operator[] (size_t i) const;
        size_t get_straight_index (size_t logical_index);
    };

    template <> char const& CStringIterator<false>::operator[] (size_t i) const
    {
        return m_arr [i];
    }
    template <> char const& CStringIterator<true>::operator[] (size_t i) const
    {
        return m_arr [m_size - i - 1];
    }
    template <> size_t CStringIterator<false>::get_straight_index (size_t logical_index)
    {
        return logical_index;
    }
    template <> size_t CStringIterator<true>::get_straight_index (size_t logical_index)
    {
        return m_size - logical_index - 1;
    }


    template <bool reverse> void calculate_similarity_matrix (
        char const* text, size_t size_text,
        char const* query, size_t size_query,
        int* matrix)
    {
        size_t ROWS = size_text + 1;
        size_t COLUMNS = size_query + 1;

        // init 1st row and column with zeros
        memset ( matrix, 0, COLUMNS * sizeof(matrix[0]) );
        for ( size_t i = 1; i < ROWS; ++i )
            matrix [i * COLUMNS] = 0;

        // arrays to store maximums for all previous rows and columns
    #ifdef CACHE_MAX_COLS
        typedef std::pair<int, size_t> TMaxPos;
        std::vector<TMaxPos> vec_max_cols(COLUMNS, TMaxPos(0, 0));
        std::vector<TMaxPos> vec_max_rows(ROWS, TMaxPos(0, 0));
    #endif

        CStringIterator<reverse> text_iterator(text, size_text);
        CStringIterator<reverse> query_iterator(query, size_query);

        for ( size_t i = 1; i < ROWS; ++i )
        {
            for ( size_t j = 1; j < COLUMNS; ++j )
            {
                int sim = similarity_func ( text_iterator[i-1], query_iterator[j-1] );

    #ifdef CACHE_MAX_COLS
                int cur_score_del = vec_max_cols[j].first + gap_score_func(j - vec_max_cols[j].second);
    #else
                int cur_score_del = -1;
                for ( size_t k = 1; k < i; ++k )
                {
                    int cur = matrix [ (i - k)*COLUMNS + j ] + gap_score_func(k);
                    if ( cur > cur_score_del )
                        cur_score_del = cur;
                }
    #endif

    #ifdef CACHE_MAX_ROWS
                int cur_score_ins = vec_max_rows[i].first + gap_score_func(i - vec_max_rows[i].second);;
    #else
                int cur_score_ins = -1;
                for ( size_t l = 1; l < j; ++l )
                {
                    int cur = matrix [ i*COLUMNS + (j - l) ] + gap_score_func(l);
                    if ( cur > cur_score_ins )
                        cur_score_ins = cur;
                }
    #endif

                matrix[i*COLUMNS + j] = max4 (0,
                                              matrix[(i-1)*COLUMNS + j - 1] + sim,
                                              cur_score_del,
                                              cur_score_ins);

    #ifdef CACHE_MAX_COLS
                if ( matrix[i*COLUMNS + j] > vec_max_cols[j].first )
                    vec_max_cols[j] = TMaxPos(matrix[i*COLUMNS + j], j);

                vec_max_cols[j].first += gap_score_func(1);
    #endif
    #ifdef CACHE_MAX_ROWS
                if ( matrix[i*COLUMNS + j] > vec_max_rows[i].first )
                    vec_max_rows[i] = TMaxPos(matrix[i*COLUMNS + j], i);

                vec_max_rows[i].first += gap_score_func(1);
    #endif

            }
        }

    }

    void sw_find_indel_box ( int* matrix, size_t ROWS, size_t COLUMNS,
                             int* ret_row_start, int* ret_row_end,
                             int* ret_col_start, int* ret_col_end )
    {
        // find maximum score in the matrix
        size_t max_row = 0, max_col = 0;
        size_t max_i = 0;

        size_t i = ROWS*COLUMNS - 1;
        //do
        //{
        //    if ( matrix[i] > matrix[max_i] )
        //        max_i = i;
        //    --i;
        //}
        //while (i > 0);

        // TODO: prove the lemma: for all i: matrix[i] <= matrix[ROWS*COLUMNS - 1]
        // (i.e. matrix[ROWS*COLUMNS - 1] is always the maximum element in the valid SW-matrix)

        max_i = ROWS*COLUMNS - 1;

        max_row = max_i / COLUMNS;
        max_col = max_i % COLUMNS;


        // traceback to (0,0)-th element of the matrix
        *ret_row_start = *ret_row_end = *ret_col_start = *ret_col_end = -1;

        i = max_row;
        size_t j = max_col;
        bool prev_indel = false;
        while (true)
        {
            if (i > 0 && j > 0)
            {
                if ( matrix [(i - 1)*COLUMNS + (j - 1)] >= matrix [i*COLUMNS + (j - 1)] &&
                     matrix [(i - 1)*COLUMNS + (j - 1)] >= matrix [(i - 1)*COLUMNS + j])
                {
                    --i;
                    --j;

                    if (prev_indel)
                    {
                        *ret_row_start = i;
                        *ret_col_start = j;
                    }
                    prev_indel = false;
                }
                else if ( matrix [(i - 1)*COLUMNS + (j - 1)] < matrix [i*COLUMNS + (j - 1)] )
                {
                    if ( *ret_row_end == -1 )
                    {
                        *ret_row_end = i;
                        *ret_col_end = j;
                    }
                    --j;
                    prev_indel = true;
                }
                else
                {
                    if ( *ret_row_end == -1 )
                    {
                        *ret_row_end = i;
                        *ret_col_end = j;
                    }
                    --i;
                    prev_indel = true;
                }
            }
            else if ( i > 0 )
            {
                if ( *ret_row_end == -1 )
                {
                    *ret_row_end = i;
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
                    *ret_col_end = j;
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

    // get_ref_slice returns reference slice of sufficient length for Smith-Waterman algorithm
    ngs::String get_ref_slice ( ngs::Reference const& ref,
                                int64_t ref_pos_ins,
                                int64_t ins_bases_length)
    {
        int64_t safe_half_length = ins_bases_length / 2 + 1;

        int64_t ref_start = (ref_pos_ins - safe_half_length) >= 0 ?
                            (ref_pos_ins - safe_half_length) : 0;

        return ref.getReferenceBases ( ref_start, safe_half_length * 2 );
    }

    // make_query returns the query for Smith-Waterman algorithm as follows:
    // <1st half of reference slice> + <insertion bases> + <2nd half of the reference slice>
    // reference slice must be of sufficient length - get_ref_slice() retruns it
    ngs::String make_query ( ngs::String const& ref_slice,
                             char const* ins_bases, size_t ins_bases_length)
    {
        assert ( ref_slice.size() >= 2 * ( ins_bases_length / 2 + 1 ) );

        ngs::String ret;
        ret.reserve ( ref_slice.size() + ins_bases_length );

        ret.append ( ref_slice.begin(), ref_slice.begin() + ref_slice.size()/2 );
        ret.append ( ins_bases, ins_bases + ins_bases_length );
        ret.append ( ref_slice.begin() + ref_slice.size()/2, ref_slice.end() );

        return ret;
    }

    template <bool reverse> void print_matrix ( int const* matrix,
                                                char const* ref_slice, size_t ref_slice_size,
                                                char const* query, size_t query_size)
    {
        size_t COLUMNS = ref_slice_size + 1;
        size_t ROWS = query_size + 1;

        int print_width = 2;

        CStringIterator<reverse> ref_slice_iterator(ref_slice, ref_slice_size);
        CStringIterator<reverse> query_iterator(query, query_size);

        printf ("  %*c ", print_width, '-');
        for (size_t j = 0; j < COLUMNS; ++j)
            printf ("%*c ", print_width, ref_slice_iterator[j]);
        printf ("\n");

        for (size_t i = 0; i < ROWS; ++i)
        {
            if ( i == 0 )
                printf ("%c ", '-');
            else
                printf ("%c ", query_iterator[i-1]);
        
            for (size_t j = 0; j < COLUMNS; ++j)
            {
                printf ("%*d ", print_width, matrix[i*COLUMNS + j]);
            }
            printf ("\n");
        }
    }

    void print_indel (char const* name, char const* text, size_t text_size, int indel_start, int indel_end)
    {
        printf ( "%s: %.*s[%.*s]%.*s\n",
                    name,
                    (indel_start + 1), text,
                    indel_end - (indel_start + 1), text + (indel_start + 1),
                    (int)(text_size - indel_end), text + indel_end
               );
    }

    void analyse_run ( ngs::String const& ref_slice, char const* ins_bases, size_t ins_bases_length )
    {
        ngs::String query = make_query ( ref_slice, ins_bases, ins_bases_length );

        std::cout
            << "ref_slice: "
            << ref_slice << std::endl
            << "query    : " << query << std::endl;

        // building sw-matrix for chosen reference slice and sequence

        size_t COLUMNS = ref_slice.size() + 1;
        size_t ROWS = query.size() + 1;
        std::vector<int> matrix( ROWS * COLUMNS );

        calculate_similarity_matrix<false> ( query.c_str(), query.size(), ref_slice.c_str(), ref_slice.size(), &matrix[0] );
        //print_matrix<reverse> (&matrix[0], ref_slice.c_str(), ref_slice.size(), query.c_str(), query.size());
        int row_start, col_start, row_end, col_end;
        sw_find_indel_box ( & matrix[0], ROWS, COLUMNS, &row_start, &row_end, &col_start, &col_end );


        calculate_similarity_matrix<true> ( query.c_str(), query.size(), ref_slice.c_str(), ref_slice.size(), &matrix[0] );
        int row_start_rev, col_start_rev, row_end_rev, col_end_rev;
        sw_find_indel_box ( & matrix[0], ROWS, COLUMNS, &row_start_rev, &row_end_rev, &col_start_rev, &col_end_rev );

        CStringIterator<false> ref_slice_iterator(ref_slice.c_str(), ref_slice.size());
        CStringIterator<false> query_iterator(query.c_str(), query.size());

        row_start = min ( (int)query.size() - row_end_rev - 1, row_start );
        row_end   = max ( (int)query.size() - row_start_rev - 1, row_end );
        col_start = min ( (int)ref_slice.size() - col_end_rev - 1, col_start );
        col_end   = max ( (int)ref_slice.size() - col_start_rev - 1, col_end );

        printf ("indel box found: (%d, %d) - (%d, %d)\n", row_start, col_start, row_end, col_end );

        print_indel ( "reference", ref_slice.c_str(), ref_slice.size(), col_start, col_end );
        print_indel ( "query    ", query.c_str(), query.size(), row_start, row_end );
    }

    void find_ref_in_runs (char const* ref_name, int64_t pos)
    {
        //"SRR341578" "gi|218511148|ref|NC_011752.1|" 2018
        ngs::ReadCollection run = ncbi::NGS::openReadCollection ("SRR341578");
        ngs::Reference ref = run.getReference ( /*"gi|218511148|ref|*/ref_name );

        ngs::String ref_slice = get_ref_slice ( ref, pos, 2 );

        std::cout << "Reference around pos=" << pos << ": " << ref_slice << std::endl;

        char const* db_names[] =
        {
            "SRR341575",
            "SRR341576",
            "SRR341577",
            "SRR341579",
            "SRR341580",
            "SRR341581",
            "SRR341582",
            "SRR341578"
        };

        for (size_t i = 0; i < sizeof (db_names)/sizeof (db_names[0]); ++i )
        {
            try
            {
                ngs::ReadCollection r = ncbi::NGS::openReadCollection ( db_names[i] );
                ngs::Reference rr = r.getReference ( ref_name );
                ngs::String ref_slice_cur = get_ref_slice ( rr, pos, 2 );

                if ( ref_slice_cur ==  ref_slice )
                {
                    std::cout << db_names[i] << " has the same reference" << std::endl;
                    analyse_run( ref_slice_cur, "CA", 2 );
                }
                else
                {
                    std::cout << db_names[i] << " has DIFFERENT reference: " << ref_slice_cur << std::endl;
                }
            }
            catch (ngs::ErrorMsg const& e)
            {
                std::cout << db_names[i] << " failed: " << e.what() << std::endl;
            }
        }
    }

/*    static void run (  )
    {
        try
        {
            std::cout << "Under construction..." << std::endl;
        }
        catch ( ngs::ErrorMsg & x )
        {
            throw;
        }
        catch ( const char x [] )
        {
            throw;
        }
        catch ( ... )
        {
            throw;
        }
    }*/

    void run ()
    {
        ngs::ReadCollection run = ncbi::NGS::openReadCollection (g_Params.accession);
        ngs::Reference ref = run.getReference ( g_Params.ref_name );
        int64_t query_len = strlen(g_Params.query);

        ngs::String ref_slice = get_ref_slice ( ref, g_Params.ref_pos_ins, query_len );

        std::cout << "Reference around pos=" << g_Params.ref_pos_ins << ": " << ref_slice << std::endl;

        analyse_run( ref_slice, g_Params.query, query_len );
    }
}

extern "C"
{
    ver_t CC KAppVersion ()
    {
        return REF_VARIATION_VERS;
    }

    rc_t CC UsageSummary (const char * progname)
    {
        printf (
        "Usage example:\n"
        "  %s -s <accession> -p <position on reference> -q <query to look for> -r <reference name>\n"
        "\n"
        "Summary:\n"
        "  Find a possible indel window\n"
        "\n", progname);
        return 0;
    }

    rc_t CC Usage ( struct Args const * args )
    {
        return 0;
    }

    static 
    const char * getArg ( int & i, int argc, char * argv [] )
    {
        
        if ( ++ i == argc )
            throw "Missing argument";
        
        return argv [ i ];
    }

    static
    const char * findArg ( const char*  &arg, int & i, int argc, char * argv [] )
    {
        if ( arg [ 1 ] != 0 )
        {
            const char * next = arg + 1;
            arg = "\0";
            return next;
        }

        return getArg ( i, argc, argv );
    }

    static void handle_help ( const char *appName )
    {
        const char *appLeaf = strrchr ( appName, '/' );
        if ( appLeaf ++ == 0 )
            appLeaf = appName;

        ver_t vers = KAppVersion ();

        /* TODO: add actual help info */
        std :: cout
            << '\n'
            << "Usage:\n"
            << "  " << appLeaf << " [options]"
            << "\n\n"
            << "Options:\n"
            << "  -s|--source-accession            search for the query in this db\n"
            << "  -r|--reference-name              look for the query against this reference\n"
            << "  -p|--position                    look for the query at this position on reference\n"
            << "  -q|--query                       query to find in given db on given reference\n"
            << "  -h|--help                        output brief explanation of the program\n"
            << "  -v|--verbose                     increase the verbosity of the program.\n"
            << "                                   use multiple times for more verbosity.\n"
            << '\n'
            << appName << " : "
            << ( vers >> 24 )
            << '.'
            << ( ( vers >> 16 ) & 0xFF )
            << '.'
            << ( vers & 0xFFFF )
            << '\n'
            << '\n'
            ;
    }

    static void CC handle_error ( const char *arg, void *message )
    {
        throw ( const char * ) message;
    }

    static void check_value ( char const* str, char const* name )
    {
        if (str == NULL || str[0] == '\0')
        {
            std::string err = name;
            err += "  has not been specified";
            throw err.c_str();
        }
    }

    rc_t CC KMain ( int argc, char *argv [] )
    {
        rc_t rc = -1;
        try
        {
            size_t num_runs = 0;
            /* parse command line arguments */
            if ( argc < 3 )
                UsageSummary ( argv[0] );
            for ( int i = 1; i < argc; ++ i )
            {
                const char * arg = argv [ i ];
                if ( arg [ 0 ] != '-' )
                {
                    handle_error ( arg, (void*)"this program accepts options only" );
                }
                else do switch ( ( ++ arg ) [ 0 ] )
                {
                case 's':
                    RefVariation::g_Params.accession = findArg ( arg, i, argc, argv );
                    break;
                case 'r':
                    RefVariation::g_Params.ref_name = findArg ( arg, i, argc, argv );
                    break;
                case 'p':
                    RefVariation::g_Params.ref_pos_ins = AsciiToI64 ( findArg ( arg, i, argc, argv ), 
                        handle_error, ( void * ) "Invalid reference position" );
                    break;
                case 'q':
                    RefVariation::g_Params.query = findArg ( arg, i, argc, argv );
                    break;
                case 'v':
                    ++ RefVariation::g_Params.verbosity;
                    break;
                case 'h':
                case '?':
                    handle_help ( argv [ 0 ] );
                    return 0;
                case '-':
                    ++ arg;
                    if ( strcmp ( arg, "source-accession" ) == 0 )
                    {
                        RefVariation::g_Params.accession = getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "reference-name" ) == 0 )
                    {
                        RefVariation::g_Params.ref_name = getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "position" ) == 0 )
                    {
                        RefVariation::g_Params.ref_pos_ins = AsciiToI64 ( getArg ( i, argc, argv ), 
                            handle_error, ( void * ) "Invalid reference position" );
                    }
                    else if ( strcmp ( arg, "query" ) == 0 )
                    {
                        RefVariation::g_Params.query = getArg ( i, argc, argv );
                    }
                    else if ( strcmp ( arg, "verbose" ) == 0 )
                    {
                        ++ RefVariation::g_Params.verbosity;
                    }
                    else if ( strcmp ( arg, "help" ) == 0 )
                    {
                        handle_help ( argv [ 0 ] );
                        return 0;
                    }
                    else
                    {
                        throw "Invalid Argument";
                    }

                    arg = "\0";

                    break;

                default:
                    throw "Invalid argument";
                }
                while ( arg [ 1 ] != 0 );
            }

            // check required arguments
            check_value ( RefVariation::g_Params.accession, "source-accession" );
            check_value ( RefVariation::g_Params.ref_name,  "reference-name" );
            check_value ( RefVariation::g_Params.query,     "query" );

            RefVariation::run();

            rc = 0;
        }
        catch ( ngs::ErrorMsg const& x )
        {
            std :: cerr
                << "ERROR: "
                << argv [ 0 ]
                << ": "
                << x . what ()
                << '\n'
                ;
        }
        catch ( const char x [] )
        {
            std :: cerr
                << "ERROR: "
                << argv [ 0 ]
                << ": "
                << x
                << '\n'
                ;
        }
        catch ( ... )
        {
            std :: cerr
                << "ERROR: "
                << argv [ 0 ]
                << ": unknown\n"
                ;
        }

        return rc;
    }
}
