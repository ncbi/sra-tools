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

#include <search/extern.h>
#include <search/nucstrstr.h>
#include <arch-impl.h>
#include <sysalloc.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <os-native.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <byteswap.h>
#include <endian.h>

#define TRACE_OPERATIONS 0
#define TRACE_PARSE 1
#define TRACE_HEADER 1
#define TRACE_RESULT 1
#define TRACE_PATMASK 1

#if __INTEL_COMPILER || defined __SSE2__

#include <emmintrin.h>
#define INTEL_INTRINSICS 1
#define NEVER_MATCH 0
#define RETURN_LOCATION 1
#define USE_MEMALIGN 0

#endif

#if INTEL_INTRINSICS

#if USE_MEMALIGN
#include <malloc.h>
#endif

#define RELOAD_BUFFER 1
#define NA2_LIMIT 61
#define NA4_LIMIT 29
typedef __m128i nucreg_t;
typedef union
{
    uint8_t b [ 16 ];
    uint16_t w [ 8 ];
    uint32_t i [ 4 ];
    uint64_t l [ 2 ];
    uint128_t s;
} nucpat_t;

#else

#define NA2_LIMIT 29
#define NA4_LIMIT 13
typedef uint64_t nucreg_t;
typedef union
{
    uint8_t b [ 8 ];
    uint16_t w [ 4 ];
    uint32_t i [ 2 ];
    uint64_t l;
} nucpat_t;

#endif


static int8_t fasta_2na_map [ 128 ];
static int8_t fasta_4na_map [ 128 ];
static uint16_t expand_2na [ 256 ] =
   /* AAAA    AAAC    AAAG    AAAT    AACA    AACC    AACG    AACT */
{   0x1111, 0x1112, 0x1114, 0x1118, 0x1121, 0x1122, 0x1124, 0x1128,
   /* AAGA    AAGC    AAGG    AAGT    AATA    AATC    AATG    AATT */
    0x1141, 0x1142, 0x1144, 0x1148, 0x1181, 0x1182, 0x1184, 0x1188,
   /* ACAA    ACAC    ACAG    ACAT    ACCA    ACCC    ACCG    ACCT */
    0x1211, 0x1212, 0x1214, 0x1218, 0x1221, 0x1222, 0x1224, 0x1228,
   /* ACGA    ACGC    ACGG    ACGT    ACTA    ACTC    ACTG    ACTT */
    0x1241, 0x1242, 0x1244, 0x1248, 0x1281, 0x1282, 0x1284, 0x1288,
   /* AGAA    AGAC    AGAG    AGAT    AGCA    AGCC    AGCG    AGCT */
    0x1411, 0x1412, 0x1414, 0x1418, 0x1421, 0x1422, 0x1424, 0x1428,
   /* AGGA    AGGC    AGGG    AGGT    AGTA    AGTC    AGTG    AGTT */
    0x1441, 0x1442, 0x1444, 0x1448, 0x1481, 0x1482, 0x1484, 0x1488,
   /* ATAA    ATAC    ATAG    ATAT    ATCA    ATCC    ATCG    ATCT */
    0x1811, 0x1812, 0x1814, 0x1818, 0x1821, 0x1822, 0x1824, 0x1828,
   /* ATGA    ATGC    ATGG    ATGT    ATTA    ATTC    ATTG    ATTT */
    0x1841, 0x1842, 0x1844, 0x1848, 0x1881, 0x1882, 0x1884, 0x1888,
   /* CAAA    CAAC    CAAG    CAAT    CACA    CACC    CACG    CACT */
    0x2111, 0x2112, 0x2114, 0x2118, 0x2121, 0x2122, 0x2124, 0x2128,
   /* CAGA    CAGC    CAGG    CAGT    CATA    CATC    CATG    CATT */
    0x2141, 0x2142, 0x2144, 0x2148, 0x2181, 0x2182, 0x2184, 0x2188,
   /* CCAA    CCAC    CCAG    CCAT    CCCA    CCCC    CCCG    CCCT */
    0x2211, 0x2212, 0x2214, 0x2218, 0x2221, 0x2222, 0x2224, 0x2228,
   /* CCGA    CCGC    CCGG    CCGT    CCTA    CCTC    CCTG    CCTT */
    0x2241, 0x2242, 0x2244, 0x2248, 0x2281, 0x2282, 0x2284, 0x2288,
   /* CGAA    CGAC    CGAG    CGAT    CGCA    CGCC    CGCG    CGCT */
    0x2411, 0x2412, 0x2414, 0x2418, 0x2421, 0x2422, 0x2424, 0x2428,
   /* CGGA    CGGC    CGGG    CGGT    CGTA    CGTC    CGTG    CGTT */
    0x2441, 0x2442, 0x2444, 0x2448, 0x2481, 0x2482, 0x2484, 0x2488,
   /* CTAA    CTAC    CTAG    CTAT    CTCA    CTCC    CTCG    CTCT */
    0x2811, 0x2812, 0x2814, 0x2818, 0x2821, 0x2822, 0x2824, 0x2828,
   /* CTGA    CTGC    CTGG    CTGT    CTTA    CTTC    CTTG    CTTT */
    0x2841, 0x2842, 0x2844, 0x2848, 0x2881, 0x2882, 0x2884, 0x2888,
   /* GAAA    GAAC    GAAG    GAAT    GACA    GACC    GACG    GACT */
    0x4111, 0x4112, 0x4114, 0x4118, 0x4121, 0x4122, 0x4124, 0x4128,
   /* GAGA    GAGC    GAGG    GAGT    GATA    GATC    GATG    GATT */
    0x4141, 0x4142, 0x4144, 0x4148, 0x4181, 0x4182, 0x4184, 0x4188,
   /* GCAA    GCAC    GCAG    GCAT    GCCA    GCCC    GCCG    GCCT */
    0x4211, 0x4212, 0x4214, 0x4218, 0x4221, 0x4222, 0x4224, 0x4228,
   /* GCGA    GCGC    GCGG    GCGT    GCTA    GCTC    GCTG    GCTT */
    0x4241, 0x4242, 0x4244, 0x4248, 0x4281, 0x4282, 0x4284, 0x4288,
   /* GGAA    GGAC    GGAG    GGAT    GGCA    GGCC    GGCG    GGCT */
    0x4411, 0x4412, 0x4414, 0x4418, 0x4421, 0x4422, 0x4424, 0x4428,
   /* GGGA    GGGC    GGGG    GGGT    GGTA    GGTC    GGTG    GGTT */
    0x4441, 0x4442, 0x4444, 0x4448, 0x4481, 0x4482, 0x4484, 0x4488,
   /* GTAA    GTAC    GTAG    GTAT    GTCA    GTCC    GTCG    GTCT */
    0x4811, 0x4812, 0x4814, 0x4818, 0x4821, 0x4822, 0x4824, 0x4828,
   /* GTGA    GTGC    GTGG    GTGT    GTTA    GTTC    GTTG    GTTT */
    0x4841, 0x4842, 0x4844, 0x4848, 0x4881, 0x4882, 0x4884, 0x4888,
   /* TAAA    TAAC    TAAG    TAAT    TACA    TACC    TACG    TACT */
    0x8111, 0x8112, 0x8114, 0x8118, 0x8121, 0x8122, 0x8124, 0x8128,
   /* TAGA    TAGC    TAGG    TAGT    TATA    TATC    TATG    TATT */
    0x8141, 0x8142, 0x8144, 0x8148, 0x8181, 0x8182, 0x8184, 0x8188,
   /* TCAA    TCAC    TCAG    TCAT    TCCA    TCCC    TCCG    TCCT */
    0x8211, 0x8212, 0x8214, 0x8218, 0x8221, 0x8222, 0x8224, 0x8228,
   /* TCGA    TCGC    TCGG    TCGT    TCTA    TCTC    TCTG    TCTT */
    0x8241, 0x8242, 0x8244, 0x8248, 0x8281, 0x8282, 0x8284, 0x8288,
   /* TGAA    TGAC    TGAG    TGAT    TGCA    TGCC    TGCG    TGCT */
    0x8411, 0x8412, 0x8414, 0x8418, 0x8421, 0x8422, 0x8424, 0x8428,
   /* TGGA    TGGC    TGGG    TGGT    TGTA    TGTC    TGTG    TGTT */
    0x8441, 0x8442, 0x8444, 0x8448, 0x8481, 0x8482, 0x8484, 0x8488,
   /* TTAA    TTAC    TTAG    TTAT    TTCA    TTCC    TTCG    TTCT */
    0x8811, 0x8812, 0x8814, 0x8818, 0x8821, 0x8822, 0x8824, 0x8828,
   /* TTGA    TTGC    TTGG    TTGT    TTTA    TTTC    TTTG    TTTT */
    0x8841, 0x8842, 0x8844, 0x8848, 0x8881, 0x8882, 0x8884, 0x8888
};


/*--------------------------------------------------------------------------
 * debugging printf
 */
#if TRACE_OPERATIONS

#if INTEL_INTRINSICS

#define COPY_NUCREG( to, from ) \
    _mm_storeu_si128 ( ( __m128i* ) ( to ) . b, ( from ) )

#else

#define COPY_NUCREG( to, from ) \
    ( ( to ) . b [ 0 ] = ( uint8_t ) ( ( from ) >> 56 ), \
      ( to ) . b [ 1 ] = ( uint8_t ) ( ( from ) >> 48 ), \
      ( to ) . b [ 2 ] = ( uint8_t ) ( ( from ) >> 40 ), \
      ( to ) . b [ 3 ] = ( uint8_t ) ( ( from ) >> 32 ), \
      ( to ) . b [ 4 ] = ( uint8_t ) ( ( from ) >> 24 ), \
      ( to ) . b [ 5 ] = ( uint8_t ) ( ( from ) >> 16 ), \
      ( to ) . b [ 6 ] = ( uint8_t ) ( ( from ) >>  8 ), \
      ( to ) . b [ 7 ] = ( uint8_t ) ( ( from ) >>  0 ) )

#endif

/* sprintf_2na
 *  print 2na sequence
 */
static
const char *sprintf_2na ( char *str, size_t ssize, nucreg_t nr )
{
    int b, i, j;
    nucpat_t np;
    const char *ncbi2na = "ACGT";

    if ( ssize <= sizeof np * 4 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( np, nr );

    for ( i = j = 0; i < sizeof np . b; j += 4, ++ i )
    {
        b = np . b [ i ];
        str [ j + 0 ] = ncbi2na [ ( b >> 6 ) & 3 ];
        str [ j + 1 ] = ncbi2na [ ( b >> 4 ) & 3 ];
        str [ j + 2 ] = ncbi2na [ ( b >> 2 ) & 3 ];
        str [ j + 3 ] = ncbi2na [ ( b >> 0 ) & 3 ];
    }

    str [ sizeof np * 4 ] = 0;

    return str;
}

/* sprintf_4na
 *  print 4na sequence
 */
static
const char *sprintf_4na ( char *str, size_t ssize, nucreg_t nr )
{
    int b, i, j;
    nucpat_t np;
    const char *ncbi4na = "-ACMGRSVTWYHKDBN";

    if ( ssize <= sizeof np * 2 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( np, nr );

    for ( i = j = 0; i < sizeof np . b; j += 2, ++ i )
    {
        b = np . b [ i ];
        str [ j + 0 ] = ncbi4na [ ( b >> 4 ) & 15 ];
        str [ j + 1 ] = ncbi4na [ ( b >> 0 ) & 15 ];
    }

    str [ sizeof np * 2 ] = 0;

    return str;
}

/* sprintf_m2na
 *  print 2na mask
 */
static
const char *sprintf_m2na ( char *str, size_t ssize, nucreg_t mask )
{
    int b, i, j;
    nucpat_t mp;
    const char *amask = " ??x";

    if ( ssize <= sizeof mp * 4 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( mp, mask );

    for ( i = j = 0; i < mp . b; j += 4, ++ i )
    {
        b = mp . b [ i ];
        str [ j + 0 ] = amask [ ( b >> 6 ) & 3 ];
        str [ j + 1 ] = amask [ ( b >> 4 ) & 3 ];
        str [ j + 2 ] = amask [ ( b >> 2 ) & 3 ];
        str [ j + 3 ] = amask [ ( b >> 0 ) & 3 ];
    }

    str [ sizeof mp * 4 ] = 0;

    return str;
}

/* sprintf_m4na
 *  print 4na mask
 */
static
const char *sprintf_m4na ( char *str, size_t ssize, nucreg_t mask )
{
    int b, i, j;
    nucpat_t mp;
    const char *amask = " ??????????????x";

    if ( ssize <= sizeof mp * 2 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( mp, mask );

    for ( i = j = 0; i < sizeof mp . b; j += 2, ++ i )
    {
        b = mp . b [ i ];
        str [ j + 0 ] = amask [ ( b >> 4 ) & 15 ];
        str [ j + 1 ] = amask [ ( b >> 0 ) & 15 ];
    }

    str [ sizeof mp * 2 ] = 0;

    return str;
}

/* sprintf_a2na
 *  print 2na alignment, where buffer has source,
 *  pattern has some set of valid pattern bases,
 *  and mask defines which parts are valid
 */
static
const char *sprintf_a2na ( char *str, size_t ssize,
    nucreg_t buffer, nucreg_t pattern, nucreg_t mask )
{
    nucpat_t bp, pp, mp;
    int s, b, m, i, j, k, l, s_ch, b_ch;
    const char *ncbi2na = "    ????????ACGT";

    if ( ssize <= sizeof bp * 4 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( bp, buffer );
    COPY_NUCREG ( pp, pattern );
    COPY_NUCREG ( mp, mask );

    for ( i = j = 0; i < sizeof bp . b; j += 4, ++ i )
    {
        s = bp . b [ i ];
        b = pp . b [ i ];
        m = ( int ) mp . b [ i ] << 2;

        for ( k = 0, l = 6; k < 4; l -= 2, ++ k )
        {
            s_ch = ncbi2na [ 12 | ( ( s >> l ) & 3 ) ];
            b_ch = ncbi2na [ ( ( m >> l ) & 12 ) | ( ( b >> l ) & 3 ) ];
            str [ j + k ] = ( s_ch == b_ch ) ? ( char ) b_ch : ( char ) tolower ( b_ch );
        }
    }

    str [ sizeof bp * 4 ] = 0;

    return str;
}

/* sprintf_a4na
 *  print 4na alignment, where buffer has source,
 *  pattern has some set of valid pattern bases,
 *  and mask defines which parts are valid
 */
static
const char *sprintf_a4na ( char *str, size_t ssize,
    nucreg_t buffer, nucreg_t pattern, nucreg_t mask )
{
    nucpat_t bp, pp, mp;
    int s, b, m, i, j, k, l, s_ch, b_ch;
    const char *ncbi4na =
        "                ????????????????????????????????????????????????"
        "????????????????????????????????????????????????????????????????"
        "????????????????????????????????????????????????????????????????"
        "????????????????????????????????????????????????""-ACMGRSVTWYHKDBN";
    /* the odd ??""- is to prevent a ??- trigraph for ~ */

    if ( ssize <= sizeof bp * 2 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( bp, buffer );
    COPY_NUCREG ( pp, pattern );
    COPY_NUCREG ( mp, mask );

    for ( i = j = 0; i < sizeof bp . b; j += 2, ++ i )
    {
        s = bp . b [ i ];
        b = pp . b [ i ];
        m = ( int ) mp . b [ i ] << 4;

        for ( k = 0, l = 4; k < 2; l -= 4, ++ k )
        {
#if INTEL_INTRINSICS
            s_ch = ncbi4na [ ( ( m >> l ) & 0xF0 ) | ( ( s >> l ) & 15 ) ];
#else
            s_ch = ncbi4na [ 0xF0 | ( ( s >> l ) & 15 ) ];
#endif
            b_ch = ncbi4na [ ( ( m >> l ) & 0xF0 ) | ( ( b >> l ) & 15 ) ];
            str [ j + k ] =
                ( ( fasta_4na_map [ s_ch ] & fasta_4na_map [ b_ch ] ) != 0 ) ?
                ( char ) s_ch : ( char ) tolower ( b_ch );
        }
    }

    str [ sizeof bp * 2 ] = 0;

    return str;
}

/* sprintf_r2na
 *  print results of 2na comparison
 */
static
const char *sprintf_r2na ( char *str, size_t ssize, nucreg_t result, nucreg_t mask )
{
    nucpat_t rp;
    int b, i, j;
    const char *amask = " ??x";

    if ( ssize <= sizeof rp * 4 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( rp, result );

    for ( i = j = 0; i < sizeof rp . b; j += 4, ++ i )
    {
        b = rp . b [ i ];
        str [ j + 0 ] = amask [ ( b >> 6 ) & 3 ];
        str [ j + 1 ] = amask [ ( b >> 4 ) & 3 ];
        str [ j + 2 ] = amask [ ( b >> 2 ) & 3 ];
        str [ j + 3 ] = amask [ ( b >> 0 ) & 3 ];
    }

    str [ sizeof rp * 4 ] = 0;

    return str;
}

static
const char *sprintf_r4na ( char *str, size_t ssize, nucreg_t result, nucreg_t mask )
{
    int b, i, j;
    nucpat_t rp;
    const char *amask = " ??????????????x";

    if ( ssize <= sizeof rp * 2 )
        return "** BUFFER TOO SMALL **";

    COPY_NUCREG ( rp, result );

    for ( i = j = 0; i < sizeof rp . b; j += 2, ++ i )
    {
        b = rp . b [ i ];
        str [ j + 0 ] = amask [ ( b >> 4 ) & 15 ];
        str [ j + 1 ] = amask [ ( b >> 0 ) & 15 ];
    }

    str [ sizeof rp * 2 ] = 0;

    return str;
}

#endif

#if TRACE_OPERATIONS && TRACE_PARSE
static
void PARSE_2NA_PATTERN ( const char *fasta, size_t size,
    nucpat_t pattern, nucpat_t mask )
{
#if INTEL_INTRINSICS
    char str [ 65 ];
    nucreg_t nr = _mm_loadu_si128 ( ( const __m128i* ) pattern . b );
    nucreg_t nm = _mm_loadu_si128 ( ( const __m128i* ) mask . b );
#else
    char str [ 33 ];
    nucreg_t nr = pattern;
    nucreg_t nm = mask;
#endif

    assert ( size < sizeof str );
    memmove ( str, fasta, size );
    str [ size ] = 0;

    printf ( "  %s - original FASTA\n", str );
    printf ( "  %s - pattern\n", sprintf_2na ( str, sizeof str, nr ) );
    printf ( "  %s - mask\n", sprintf_m2na ( str, sizeof str, nm ) );
}

static
void PARSE_4NA_PATTERN ( const char *fasta, size_t size,
    nucpat_t pattern, nucpat_t mask )
{
#if INTEL_INTRINSICS
    char str [ 33 ];
    nucreg_t nr = _mm_loadu_si128 ( ( const __m128i* ) pattern . b );
    nucreg_t nm = _mm_loadu_si128 ( ( const __m128i* ) mask . b );
#else
    char str [ 17 ];
    nucreg_t nr = pattern;
    nucreg_t nm = mask;
#endif

    assert ( size < sizeof str );
    memmove ( str, fasta, size );
    str [ size ] = 0;

    printf ( "  %s - original FASTA\n", str );
    printf ( "  %s - pattern\n", sprintf_4na ( str, sizeof str, nr ) );
    printf ( "  %s - mask\n", sprintf_m4na ( str, sizeof str, nm ) );
}

static
void PARSE_2NA_SHIFT ( unsigned int idx, nucpat_t pattern, nucpat_t mask )
{
#if INTEL_INTRINSICS
    char str [ 65 ];
    nucreg_t nr = _mm_loadu_si128 ( ( const __m128i* ) pattern . b );
    nucreg_t nm = _mm_loadu_si128 ( ( const __m128i* ) mask . b );
#else
    char str [ 33 ];
    nucreg_t nr = pattern;
    nucreg_t nm = mask;
#endif

    printf ( "  %s - pattern [ %u ]\n", sprintf_2na ( str, sizeof str, nr ), idx );
    printf ( "  %s - mask [ %u ]\n", sprintf_m2na ( str, sizeof str, nm ), idx );
}

static
void PARSE_4NA_SHIFT ( unsigned int idx, nucpat_t pattern, nucpat_t mask )
{
#if INTEL_INTRINSICS
    char str [ 33 ];
    nucreg_t nr = _mm_loadu_si128 ( ( const __m128i* ) pattern . b );
    nucreg_t nm = _mm_loadu_si128 ( ( const __m128i* ) mask . b );
#else
    char str [ 17 ];
    nucreg_t nr = pattern;
    nucreg_t nm = mask;
#endif

    printf ( "  %s - pattern [ %u ]\n", sprintf_4na ( str, sizeof str, nr ), idx );
    printf ( "  %s - mask [ %u ]\n", sprintf_m4na ( str, sizeof str, nm ), idx );
}
#else

#define PARSE_2NA_PATTERN( fasta, size, pattern, mask ) \
    ( void ) 0

#define PARSE_4NA_PATTERN( fasta, size, pattern, mask ) \
    ( void ) 0

#define PARSE_2NA_SHIFT( idx, pattern, mask ) \
    ( void ) 0

#define PARSE_4NA_SHIFT( idx, pattern, mask ) \
    ( void ) 0

#endif

#if TRACE_OPERATIONS && TRACE_HEADER

static
void ALIGN_2NA_HEADER ( nucreg_t buffer, unsigned int pos, unsigned int len )
{
    unsigned int i, j;
#if INTEL_INTRINSICS
    char str [ 65 ];
    unsigned int end = pos + 63;
    str [ 64 ] = 0;
#else
    char str [ 33 ];
    unsigned int end = pos + 31;
    str [ 32 ] = 0;
#endif

    /* line separator */
    putchar ( '\n' );

    /* print a position with vertical numerals */
    if ( end >= 1000 )
    {
        for ( i = pos, j = 0; i <= end; ++ j, ++ i )
            str [ j ] = '0' + ( char) ( i / 1000 );
        printf ( "  %s\n", str );
    }
    if ( end >= 100 )
    {
        for ( i = pos, j = 0; i <= end; ++ j, ++ i )
            str [ j ] = '0' + ( char) ( ( i / 100 ) % 10 );
        printf ( "  %s\n", str );
    }
    if ( end >= 10 )
    {
        for ( i = pos, j = 0; i <= end; ++ j, ++ i )
            str [ j ] = '0' + ( char) ( ( i / 10 ) % 10 );
        printf ( "  %s\n", str );
    }

    for ( i = pos, j = 0; i <= end; ++ j, ++ i )
        str [ j ] = '0' + ( char) ( i % 10 );
    printf ( "  %s\n", str );

    /* now dump the buffer */
    printf ( "  %s - buffer\n", sprintf_2na ( str, sizeof str, buffer ) );
}

static
void ALIGN_4NA_HEADER ( nucreg_t buffer, unsigned int pos, unsigned int len )
{
    unsigned int i, j;
#if INTEL_INTRINSICS
    char str [ 33 ];
    unsigned int end = pos + 31;
    str [ 32 ] = 0;
#else
    char str [ 17 ];
    unsigned int end = pos + 15;
    str [ 16 ] = 0;
#endif

    /* line separator */
    putchar ( '\n' );

    /* print a position with vertical numerals */
    if ( end >= 1000 )
    {
        for ( i = pos, j = 0; i <= end; ++ j, ++ i )
            str [ j ] = '0' + ( char) ( i / 1000 );
        printf ( "  %s\n", str );
    }
    if ( end >= 100 )
    {
        for ( i = pos, j = 0; i <= end; ++ j, ++ i )
            str [ j ] = '0' + ( char) ( ( i / 100 ) % 10 );
        printf ( "  %s\n", str );
    }
    if ( end >= 10 )
    {
        for ( i = pos, j = 0; i <= end; ++ j, ++ i )
            str [ j ] = '0' + ( char) ( ( i / 10 ) % 10 );
        printf ( "  %s\n", str );
    }

    for ( i = pos, j = 0; i <= end; ++ j, ++ i )
        str [ j ] = '0' + ( char) ( i % 10 );
    printf ( "  %s\n", str );

    /* now dump the buffer */
    printf ( "  %s - buffer\n", sprintf_4na ( str, sizeof str, buffer ) );
}

#else

#define ALIGN_2NA_HEADER( buffer, pos, len ) \
    ( void ) 0

#define ALIGN_4NA_HEADER( buffer, pos, len ) \
    ( void ) 0

#endif

#if TRACE_OPERATIONS && TRACE_RESULT

#if INTEL_INTRINSICS
static
void ALIGN_2NA_RESULT ( nucreg_t buffer, nucreg_t pat, nucreg_t mask,
    nucreg_t cmp, int bits )
{
    unsigned int i;
    char str [ 65 ];

    char bstr [ 17 ];
    for ( i = 0; i < 16; bits >>= 1, ++ i )
        bstr [ i ] = '0' + ( bits & 1 );
    bstr [ 16 ] = 0;

#if TRACE_PATMASK
    printf ( "  %s - pattern\n", sprintf_2na ( str, sizeof str, pat ) );
    printf ( "  %s - mask\n", sprintf_m2na ( str, sizeof str, mask ) );
#endif

    /* dump alignment and re-compare */
    printf ( "  %s - alignment\n", sprintf_a2na ( str, sizeof str, buffer, pat, mask ) );
    printf ( "  %s ( 0b%s )\n", sprintf_r2na ( str, sizeof str, cmp, mask ), bstr );
}

static
void ALIGN_4NA_RESULT ( nucreg_t buffer, nucreg_t pat, nucreg_t mask,
    nucreg_t cmp, int bits )
{
    unsigned int i;
    char str [ 33 ];

    char bstr [ 17 ];
    for ( i = 0; i < 16; bits >>= 1, ++ i )
        bstr [ i ] = '0' + ( bits & 1 );
    bstr [ 16 ] = 0;

#if TRACE_PATMASK
    printf ( "  %s - pattern\n", sprintf_4na ( str, sizeof str, pat ) );
    printf ( "  %s - mask\n", sprintf_m4na ( str, sizeof str, mask ) );
#endif

    /* dump alignment and re-compare */
    printf ( "  %s - alignment\n", sprintf_a4na ( str, sizeof str, buffer, pat, mask ) );
    printf ( "  %s ( 0b%s )\n", sprintf_r4na ( str, sizeof str, cmp, mask ), bstr );
}
#else
static
void ALIGN_2NA_RESULT ( nucreg_t buffer, nucreg_t pat, nucreg_t mask, nucreg_t bits )
{
    char str [ 33 ];

#if TRACE_PATMASK
    printf ( "  %s - pattern\n", sprintf_2na ( str, sizeof str, pat ) );
    printf ( "  %s - mask\n", sprintf_m2na ( str, sizeof str, mask ) );
#endif

    printf ( "  %s\n", sprintf_a2na ( str, sizeof str, buffer, pat, mask ) );
    printf ( "  %s\n", sprintf_r2na ( str, sizeof str, bits, mask ) );
}

static
void ALIGN_4NA_RESULT ( nucreg_t buffer, nucreg_t pat, nucreg_t mask, nucreg_t bits )
{
    char str [ 17 ];

#if TRACE_PATMASK
    printf ( "  %s - pattern\n", sprintf_4na ( str, sizeof str, pat ) );
    printf ( "  %s - mask\n", sprintf_m4na ( str, sizeof str, mask ) );
#endif

    printf ( "  %s\n", sprintf_a4na ( str, sizeof str, buffer, pat, mask ) );
    printf ( "  %s\n", sprintf_r4na ( str, sizeof str, bits, mask ) );
}
#endif

#else

#if INTEL_INTRINSICS
#define ALIGN_2NA_RESULT( buffer, pat, mask, cmp, bits ) \
    ( void ) 0
#define ALIGN_4NA_RESULT( buffer, pat, mask, cmp, bits ) \
    ( void ) 0
#else
#define ALIGN_2NA_RESULT( buffer, pat, mask, bits ) \
    ( void ) 0
#define ALIGN_4NA_RESULT( buffer, pat, mask, bits ) \
    ( void ) 0
#endif

#endif


/*--------------------------------------------------------------------------
 * NucStrExpr
 *  an expression
 */
typedef NucStrstr NucStrExpr;

enum
{
    type_2na_64,
    type_4na_64,
#if INTEL_INTRINSICS
    type_2na_8,
    type_2na_16,
    type_2na_32,
    type_2na_128,
    type_4na_16,
    type_4na_32,
    type_4na_128,
#endif
    type_2na_pos,
    type_4na_pos,
    type_OP,
    type_EXPR,

    op_NOT,
    op_HEAD,
    op_TAIL,
    op_AND,
    op_OR
};

typedef struct NucStrFastaExpr NucStrFastaExpr;
struct NucStrFastaExpr
{
    int32_t type;
    uint32_t size;
#if INTEL_INTRINSICS && ! USE_MEMALIGN
    union
    {
        /* actual allocation for freeing
           since struct pointer will be
           16 byte aligned by code */
        void *outer;
        uint64_t align;
    } u;
#endif
    struct
    {
        nucpat_t pattern;
        nucpat_t mask;
    } query [ 4 ];
};

#if USE_MEMALIGN
#define NucStrFastaExprAlloc( sz ) \
    memalign ( 16, sz )
#elif INTEL_INTRINSICS
static
void *NucStrFastaExprAlloc ( size_t sz )
{
    void *outer = malloc ( sz + 16 );
    if ( outer != NULL )
    {
        NucStrFastaExpr *e = ( void* )
            ( ( ( size_t ) outer + 15 ) & ~ ( size_t ) 15 );
        e -> u . outer = outer;
        assert ( ( ( size_t ) & e -> query [ 0 ] . pattern & 15 ) == 0 );
        assert ( ( ( size_t ) & e -> query [ 0 ] . mask & 15 ) == 0 );
        return e;
    }
    return NULL;
}
#else
#define NucStrFastaExprAlloc( sz ) \
    malloc ( sz )
#endif

typedef struct NucStrOpExpr NucStrOpExpr;
struct NucStrOpExpr
{
    int32_t type;
    int32_t op;
    NucStrExpr *left;
    NucStrExpr *right;
};

typedef struct NucStrSubExpr NucStrSubExpr;
struct NucStrSubExpr
{
    int32_t type;
    int32_t op;
    NucStrExpr *expr;
};

union  NucStrstr
{
    NucStrFastaExpr fasta;
    NucStrOpExpr boolean;
    NucStrSubExpr sub;
};

/* NucStrFastaExprMake
 *  initializes for comparison based upon type
 */
static
int NucStrFastaExprMake2 ( NucStrExpr **expr, int positional,
    const char *fasta, size_t size )
{
    size_t i;
    NucStrExpr *e;
    nucpat_t pattern, mask;

    /* still limiting bases */
    if ( size > NA2_LIMIT )
        return E2BIG;

    e = NucStrFastaExprAlloc ( sizeof * e );
    if ( e == NULL )
        return errno;

    * expr = e;
    e -> fasta . size = ( uint32_t ) size;

#if INTEL_INTRINSICS
    /* translate FASTA to 2na */
    for ( i = 0; i < size; ++ i )
    {
        uint8_t base = fasta_2na_map [ ( int ) fasta [ i ] ];
        switch ( i & 3 )
        {
        case 0:
            pattern . b [ i >> 2 ] = base << 6;
            mask . b [ i >> 2 ] = 3 << 6;
            break;
        case 1:
            pattern . b [ i >> 2 ] |= base << 4;
            mask . b [ i >> 2 ] |= 3 << 4;
            break;
        case 2:
            pattern . b [ i >> 2 ] |= base << 2;
            mask . b [ i >> 2 ] |= 3 << 2;
            break;
        case 3:
            pattern . b [ i >> 2 ] |= base;
            mask . b [ i >> 2 ] |= 3;
            break;
        }
    }

    /* fill trailing with zeros */
    for ( i = ( i + 3 ) >> 2; i < 16; ++ i )
    {
        pattern . b [ i ] = 0;
        mask . b [ i ] = 0;
    }

    PARSE_2NA_PATTERN ( fasta, size, pattern, mask );

    /* treat positional types specially */
    if ( positional )
    {
        e -> fasta . type = type_2na_pos;
    }

    /* replicate for shorter queries */
    else if ( size < 2 )
    {
        pattern . b [ 1 ] = pattern . b [ 0 ];
        pattern . w [ 1 ] = pattern . w [ 0 ];
        pattern . i [ 1 ] = pattern . i [ 0 ];
        pattern . l [ 1 ] = pattern . l [ 0 ];

        mask . b [ 1 ] = mask . b [ 0 ];
        mask . w [ 1 ] = mask . w [ 0 ];
        mask . i [ 1 ] = mask . i [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_2na_8;
    }
    else if ( size < 6 )
    {
        pattern . w [ 1 ] = pattern . w [ 0 ];
        pattern . i [ 1 ] = pattern . i [ 0 ];
        pattern . l [ 1 ] = pattern . l [ 0 ];

        mask . w [ 1 ] = mask . w [ 0 ];
        mask . i [ 1 ] = mask . i [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_2na_16;
    }
    else if ( size < 14 )
    {
        pattern . i [ 1 ] = pattern . i [ 0 ];
        pattern . l [ 1 ] = pattern . l [ 0 ];

        mask . i [ 1 ] = mask . i [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_2na_32;
    }
    else if ( size < 30 )
    {
        pattern . l [ 1 ] = pattern . l [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_2na_64;
    }
    else
    {
        e -> fasta . type = type_2na_128;
    }

    e -> fasta . query [ 0 ] . pattern = pattern;
    e -> fasta . query [ 0 ] . mask = mask;

    /* byte swap the pattern and mask */
    uint128_bswap ( & pattern . s );
    uint128_bswap ( & mask . s );

    /* now shifts should work as imagined */
    uint128_shr ( & pattern . s, 2 );
    uint128_shr ( & mask . s, 2 );

    /* restore the byte order for sse */
    uint128_bswap_copy ( & e -> fasta . query [ 1 ] . pattern . s, & pattern . s );
    uint128_bswap_copy ( & e -> fasta . query [ 1 ] . mask . s, & mask . s );

    uint128_shr ( & pattern . s, 2 );
    uint128_shr ( & mask . s, 2 );

    uint128_bswap_copy ( & e -> fasta . query [ 2 ] . pattern . s, & pattern . s );
    uint128_bswap_copy ( & e -> fasta . query [ 2 ] . mask . s, & mask . s );

    uint128_shr ( & pattern . s, 2 );
    uint128_shr ( & mask . s, 2 );

    uint128_bswap_copy ( & e -> fasta . query [ 3 ] . pattern . s, & pattern . s );
    uint128_bswap_copy ( & e -> fasta . query [ 3 ] . mask . s, & mask . s );

#else
    e -> fasta . type = positional ? type_2na_pos : type_2na_64;

    for ( pattern . l = 0, i = 0; i < size; ++ i )
    {
        pattern . l <<= 2;

        assert ( fasta [ i ] >= 0 );
        assert ( fasta_2na_map [ ( int ) fasta [ i ] ] >= 0 );

        pattern . l |= fasta_2na_map [ ( int ) fasta [ i ] ];
    }

    mask . l = ~ 0;
    pattern . l <<= 64 - size - size;
    mask . l <<= 64 - size - size;

    PARSE_2NA_PATTERN ( fasta, size, pattern, mask );

    e -> fasta . query [ 0 ] . pattern = pattern;
    e -> fasta . query [ 1 ] . pattern . l = pattern . l >> 2;
    e -> fasta . query [ 2 ] . pattern . l = pattern . l >> 4;
    e -> fasta . query [ 3 ] . pattern . l = pattern . l >> 6;

    e -> fasta . query [ 0 ] . mask = mask;
    e -> fasta . query [ 1 ] . mask . l = mask . l >> 2;
    e -> fasta . query [ 2 ] . mask . l = mask . l >> 4;
    e -> fasta . query [ 3 ] . mask . l = mask . l >> 6;

#endif

    PARSE_2NA_SHIFT ( 0, e -> fasta . query [ 0 ] . pattern,
        e -> fasta . query [ 0 ] . mask );
    PARSE_2NA_SHIFT ( 1, e -> fasta . query [ 1 ] . pattern,
        e -> fasta . query [ 1 ] . mask );
    PARSE_2NA_SHIFT ( 2, e -> fasta . query [ 2 ] . pattern,
        e -> fasta . query [ 2 ] . mask );
    PARSE_2NA_SHIFT ( 3, e -> fasta . query [ 3 ] . pattern,
        e -> fasta . query [ 3 ] . mask );

    return 0;
}

static
int NucStrFastaExprMake4 ( NucStrExpr **expr, int positional,
    const char *fasta, size_t size )
{
    size_t i;
    NucStrExpr *e;
    nucpat_t pattern, mask;

    /* still limiting bases */
    if ( size > NA4_LIMIT )
        return E2BIG;

    e = NucStrFastaExprAlloc ( sizeof * e );
    if ( e == NULL )
        return errno;

    * expr = e;
    e -> fasta . size = ( uint32_t ) size;

#if INTEL_INTRINSICS
    /* translate FASTA to 4na */
    for ( i = 0; i < size; ++ i )
    {
        uint16_t base = fasta_4na_map [ ( int ) fasta [ i ] ];
        switch ( i & 3 )
        {
        case 0:
            pattern . w [ i >> 2 ] = base << 4;
            mask . w [ i >> 2 ] = 15 << 4;
            break;
        case 1:
            pattern . w [ i >> 2 ] |= base << 0;
            mask . w [ i >> 2 ] |= 15 << 0;
            break;
        case 2:
            pattern . w [ i >> 2 ] |= base << 12;
            mask . w [ i >> 2 ] |= 15 << 12;
            break;
        case 3:
            pattern . w [ i >> 2 ] |= base << 8;
            mask . w [ i >> 2 ] |= 15 << 8;
            break;
        }
    }

    /* fill trailing with zeros */
    for ( i = ( i + 3 ) >> 2; i < 8; ++ i )
    {
        pattern . w [ i ] = 0;
        mask . w [ i ] = 0;
    }

    PARSE_4NA_PATTERN ( fasta, size, pattern, mask );

    if ( positional )
    {
        e -> fasta . type = type_4na_pos;
    }
    else if ( size < 2 )
    {
        pattern . w [ 1 ] = pattern . w [ 0 ];
        pattern . i [ 1 ] = pattern . i [ 0 ];
        pattern . l [ 1 ] = pattern . l [ 0 ];

        mask . w [ 1 ] = mask . w [ 0 ];
        mask . i [ 1 ] = mask . i [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_4na_16;
    }
    else if ( size < 6 )
    {
        pattern . i [ 1 ] = pattern . i [ 0 ];
        pattern . l [ 1 ] = pattern . l [ 0 ];

        mask . i [ 1 ] = mask . i [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_4na_32;
    }
    else if ( size < 14 )
    {
        pattern . l [ 1 ] = pattern . l [ 0 ];
        mask . l [ 1 ] = mask . l [ 0 ];

        e -> fasta . type = type_4na_64;
    }
    else
    {
        e -> fasta . type = type_4na_128;
    }

    e -> fasta . query [ 0 ] . pattern = pattern;
    e -> fasta . query [ 0 ] . mask = mask;

    /* byte swap the pattern and mask */
    uint128_bswap ( & pattern . s );
    uint128_bswap ( & mask . s );

    /* now shifts should work as imagined */
    uint128_shr ( & pattern . s, 4 );
    uint128_shr ( & mask . s, 4 );

    /* restore the byte order for sse */
    uint128_bswap_copy ( & e -> fasta . query [ 1 ] . pattern . s, & pattern . s );
    uint128_bswap_copy ( & e -> fasta . query [ 1 ] . mask . s, & mask . s );

    uint128_shr ( & pattern . s, 4 );
    uint128_shr ( & mask . s, 4 );

    uint128_bswap_copy ( & e -> fasta . query [ 2 ] . pattern . s, & pattern . s );
    uint128_bswap_copy ( & e -> fasta . query [ 2 ] . mask . s, & mask . s );

    uint128_shr ( & pattern . s, 4 );
    uint128_shr ( & mask . s, 4 );

    uint128_bswap_copy ( & e -> fasta . query [ 3 ] . pattern . s, & pattern . s );
    uint128_bswap_copy ( & e -> fasta . query [ 3 ] . mask . s, & mask . s );

#else
    e -> fasta . type = positional ? type_4na_pos : type_4na_64;

    for ( pattern . l = 0, i = 0; i < size; ++ i )
    {
        pattern . l <<= 4;

        assert ( fasta [ i ] >= 0 );
        assert ( fasta_4na_map [ ( int ) fasta [ i ] ] >= 0 );

        pattern . l |= fasta_4na_map [ ( int ) fasta [ i ] ];
    }

    mask . l = ~ 0;
    pattern . l <<= 64 - ( size << 2 );
    mask . l <<= 64 - ( size << 2 );

    PARSE_4NA_PATTERN ( fasta, size, pattern, mask );

    e -> fasta . query [ 0 ] . pattern = pattern;
    e -> fasta . query [ 1 ] . pattern . l = pattern . l >> 4;
    e -> fasta . query [ 2 ] . pattern . l = pattern . l >> 8;
    e -> fasta . query [ 3 ] . pattern . l = pattern . l >> 12;

    e -> fasta . query [ 0 ] . mask = mask;
    e -> fasta . query [ 1 ] . mask . l = mask . l >> 4;
    e -> fasta . query [ 2 ] . mask . l = mask . l >> 8;
    e -> fasta . query [ 3 ] . mask . l = mask . l >> 12;

#endif

    PARSE_4NA_SHIFT ( 0, e -> fasta . query [ 0 ] . pattern,
        e -> fasta . query [ 0 ] . mask );
    PARSE_4NA_SHIFT ( 1, e -> fasta . query [ 1 ] . pattern,
        e -> fasta . query [ 1 ] . mask );
    PARSE_4NA_SHIFT ( 2, e -> fasta . query [ 2 ] . pattern,
        e -> fasta . query [ 2 ] . mask );
    PARSE_4NA_SHIFT ( 3, e -> fasta . query [ 3 ] . pattern,
        e -> fasta . query [ 3 ] . mask );

    return 0;
}


/* nss_sob
 */
static
const char *nss_sob ( const char *p, const char *end )
{
    while ( p < end && isspace ( * ( const uint8_t* ) p ) )
        ++ p;
    return p;
}

/* nss_FASTA_expr
 */
static
const char *nss_FASTA_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional )
{
    if ( p >= end )
        * status = EINVAL;
    else
    {
        const char *start = p;

        int32_t type = type_2na_64;
        const int8_t *map = fasta_2na_map;
        do
        {
            if ( * p < 0 )
                break;

            if ( map [ * ( const uint8_t* ) p ] < 0 )
            {
                if ( map == fasta_4na_map )
                    break;
                if ( fasta_4na_map [ * ( const uint8_t* ) p ] < 0 )
                    break;
                type = type_4na_64;
                map = fasta_4na_map;
            }
        }
        while ( ++ p < end );

        if ( p <= start )
            * status = EINVAL;
        else if ( type == type_2na_64 )
            * status = NucStrFastaExprMake2 ( expr, positional, start, p - start );
        else
            * status = NucStrFastaExprMake4 ( expr, positional, start, p - start );
    }

    return p;
}

/* nss_fasta_expr
 */
static
const char *nss_fasta_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional )
{
    assert ( p < end );
    switch ( * p )
    {
    case '\'':
        p = nss_FASTA_expr ( p + 1, end, expr, status, positional );
        if ( * status == 0 && ( p == end || * p ++ != '\'' ) )
            * status = EINVAL;
        break;
    case '"':
        p = nss_FASTA_expr ( p + 1, end, expr, status, positional );
        if ( * status == 0 && ( p == end || * p ++ != '"' ) )
            * status = EINVAL;
        break;
    default:
        return nss_FASTA_expr ( p, end, expr, status, positional );
    }

    return p;
}

/* nss_position_expr
 */
#if ENABLE_AT_EXPR
static
const char *nss_position_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional )
{
    assert ( p < end );
    if ( * p == '@' )
    {
        p = nss_sob ( p + 1, end );
        if ( p == end )
        {
            * status = EINVAL;
            return p;
        }
        positional = 1;
    }

    return nss_fasta_expr ( p, end, expr, status, positional );
}
#else
#define nss_position_expr( p, end, expr, status, positional ) \
    nss_fasta_expr ( p, end, expr, status, positional )
#endif

/* forward */
static
const char *nss_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional );

/* nss_primary_expr
 */
static
const char *nss_primary_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional )
{
    NucStrExpr *e;

    assert ( p < end );
    switch ( * p )
    {
    case '^':
        e = malloc ( sizeof e -> sub );
        if ( e == NULL )
            * status = errno;
        else
        {
            e -> sub . type = type_EXPR;
            e -> sub . op = op_HEAD;
            e -> sub . expr = NULL;
            * expr = e;

            p = nss_sob ( p + 1, end );
            p = nss_position_expr ( p, end, & e -> sub . expr, status, positional );
        }
        return p;
    case '(':
        e = malloc ( sizeof e -> sub );
        if ( e == NULL )
            * status = errno;
        else
        {
            e -> sub . type = type_EXPR;
            e -> sub . op = 0;
            * expr = e;

            p = nss_expr ( p + 1, end, & e -> sub . expr, status, positional );
            if ( * status == 0 )
            {
                if ( e -> sub . expr == NULL || p == end || * p ++ != ')' )
                    * status = EINVAL;
            }
        }
        return p;
    }

    p = nss_position_expr ( p, end, expr, status, positional );
    if ( * status == 0 && p < end )
    {
        p = nss_sob ( p, end );
        if ( p < end && * p == '$' )
        {
            ++ p;

            e = malloc ( sizeof e -> sub );
            if ( e == NULL )
                * status = errno;
            else
            {
                e -> sub . type = type_EXPR;
                e -> sub . op = op_TAIL;
                e -> sub . expr = * expr;
                * expr = e;
            }
        }
    }

    return p;
}

/* nss_unary_expr
 */
static
const char *nss_unary_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional )
{
    assert ( p < end );
    if ( * p != '!' )
        return nss_primary_expr ( p, end, expr, status, positional );

#if ! ALLOW_POSITIONAL_OPERATOR_MIX
    if ( positional )
    {
        * status = EINVAL;
        return p;
    }
#endif

    p = nss_sob ( p + 1, end );
    if ( p == end )
        * status = EINVAL;
    else
    {
        NucStrExpr *e = malloc ( sizeof e -> sub );
        if ( e == NULL )
            * status = errno;
        else
        {
            e -> sub . type = type_EXPR;
            e -> sub . op = op_NOT;
            e -> sub . expr = NULL;
            * expr = e;

            p = nss_unary_expr ( p, end, & e -> sub . expr, status, positional );
            assert ( * status != 0 || e -> sub . expr != NULL );
        }
    }
    return p;
}

/* nss_expr
 *
 *   expr        : <unary_expr>
 *               | <unary_expr> <boolean_op> <expr>
 *
 *   boolean_op  : '&', '|', '&&', '||'
 */
static
const char *nss_expr ( const char *p, const char *end,
    NucStrExpr **expr, int *status, int positional )
{
    * expr = NULL;

    p = nss_sob ( p, end );
    if ( p != end )
    {
        p = nss_unary_expr ( p, end, expr, status, positional );
        if ( * status == 0 )
        {
            p = nss_sob ( p, end );
            if ( p != end )
            {
                int32_t op;
                NucStrExpr *e;
                assert ( * expr != NULL );

                switch ( * p ++ )
                {
                case ')':
                    return p - 1;
                case '&':
                    if ( p < end && * p == '&' )
                        ++ p;
                    op = op_AND;
                    break;
                case '|':
                    if ( p < end && * p == '|' )
                        ++ p;
                    op = op_OR;
                    break;
                default:
                    /* unrecognized or missing operator */
                    * status = EINVAL;
                    return p - 1;
                }

#if ! ALLOW_POSITIONAL_OPERATOR_MIX
                /* boolean operators do not work in positional mode */
                if ( positional )
                {
                    * status = EINVAL;
                    return p - 1;
                }
#endif

                e = malloc ( sizeof e -> boolean );
                if ( e == NULL )
                {
                    * status = errno;
                    return p;
                }

                e -> boolean . type = type_OP;
                e -> boolean . op = op;
                e -> boolean . left = * expr;
                * expr = e;

                /* evaluate right-hand */
                p = nss_expr ( p, end, & e -> boolean . right, status, positional );
                assert ( * status != 0 || e -> boolean . right != NULL );
            }
        }
    }

    return p;
}



/*--------------------------------------------------------------------------
 * NucStrstr
 *  prepared handle for nucleotide k-mer strstr expression
 */

/* NucStrstrInit
 */
static
void NucStrstrInit ( void )
{
    int ch;
    unsigned int i;
    const char *p, *ncbi2na = "ACGT";
    const char *ncbi4na = "-ACMGRSVTWYHKDBN";

    /* illegal under most conditions */
    memset ( fasta_2na_map, -1, sizeof fasta_2na_map );
    memset ( fasta_4na_map, -1, sizeof fasta_4na_map );

    /* legal ncbi2na alphabet */
    for ( i = 0, p = ncbi2na; p [ 0 ] != 0; ++ i, ++ p )
    {
        ch = p [ 0 ];
        fasta_2na_map [ ch ] = fasta_2na_map [ tolower ( ch ) ] = ( int8_t ) i;
    }

    /* legal ncbi4na alphabet */
    for ( i = 0, p = ncbi4na; p [ 0 ] != 0; ++ i, ++ p )
    {
        ch = p [ 0 ];
        fasta_4na_map [ ch ] = fasta_4na_map [ tolower ( ch ) ] = ( int8_t ) i;
    }

#if INTEL_INTRINSICS
    /* byte swap the 2na expand map */
    for ( i = 0; i < 256; ++ i )
        expand_2na [ i ] = bswap_16 ( expand_2na [ i ] );
#endif
}

/* NucStrstrMake
 *  prepares search by parsing expression query string
 *  returns error if conversion was not possible.
 *
 *  "nss" [ OUT ] - return parameter for one-time search handle
 *
 *  "positional" [ IN ] - if non-zero, build an expression tree
 *  to return found position rather than simply a Boolean found.
 *  see NucStrstrSearch.
 *
 *  "query" [ IN ] and "len" [ IN ] - query string expression, such that:
 *       expr           : <primary_expr>
 *                      | <primary_expr> <boolean_op> <expr>
 *       primary_expr   : FASTA
 *                      | "'" FASTA "'"
 *                      | '"' FASTA '"'
 *                      | '(' <expr> ')'
 *       boolean_op     : '&', '|', '&&', '||'
 *
 *  return values:
 *    EINVAL - invalid parameter or invalid expression
 */
LIB_EXPORT int CC NucStrstrMake ( NucStrstr **nss, int positional,
    const char *query, unsigned int len )
{
    if ( nss != NULL )
    {
        if ( query != NULL && len != 0 )
        {
            int status = 0;
            const char *end;

            if ( fasta_2na_map [ 0 ] == 0 )
                NucStrstrInit ();

            end = query + len;
            query = nss_expr ( query, end, nss, & status, positional );
            if ( status == 0 )
            {
                if ( query == end )
                    return 0;

                status = EINVAL;
            }

            NucStrstrWhack ( * nss );
            * nss = NULL;
            return status;
        }

        * nss = NULL;
    }
    return EINVAL;
}

/* NucStrstrWhack
 *  discard structure when no longer needed
 */
LIB_EXPORT void CC NucStrstrWhack ( NucStrstr *self )
{
    if ( self != NULL )
    {
        switch ( self -> fasta . type )
        {
        case type_2na_64:
        case type_4na_64:
#if INTEL_INTRINSICS
        case type_2na_8:
        case type_2na_16:
        case type_2na_32:
        case type_2na_128:
        case type_4na_16:
        case type_4na_32:
        case type_4na_128:
#endif
        case type_2na_pos:
        case type_4na_pos:
#if INTEL_INTRINSICS && ! USE_MEMALIGN
            self = self -> fasta . u . outer;
#endif
            break;
        case type_OP:
            NucStrstrWhack ( self -> boolean . left );
            NucStrstrWhack ( self -> boolean . right );
            break;
        case type_EXPR:
            NucStrstrWhack ( self -> sub . expr );
        }

        free ( self );
    }
}

/*--------------------------------------------------------------------------
 * expression evaluation
 */
#if __BYTE_ORDER == __LITTLE_ENDIAN
    #define LOAD64( ptr ) bswap_64 ( * ( const uint64_t* ) ptr )
#else
    #if __BYTE_ORDER == __BIG_ENDIAN
        #define LOAD64( ptr ) ( * ( const uint64_t* ) ptr )
    #else
        #error "Endian-ness is not defined"
    #endif
#endif

#if ! INTEL_INTRINSICS
static
int eval_2na_64 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
    int count;
    uint64_t ra, rb, rc, rd;

#if ENDLESS_BUFFER
    uint64_t buffer;
#else
    nucpat_t bp;
#define buffer bp.l
    const uint8_t *end;
#endif
    const uint8_t *src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

#if TRACE_OPERATIONS
    unsigned int tpos = pos & ~ 3;
    unsigned int align_len = len + ( pos & 3 );
#endif

    if ( len < self -> size )
        return 0;

#if ENDLESS_BUFFER
    /* prime source buffer */
    buffer = LOAD64( src );
#else
    /* accumulate entry position into length */
    end = src + ( ( len + ( pos & 3 ) + 3 ) >> 2 );

    /* prime source buffer */
    bp . l = 0; /* defined above as "buffer" */
    switch ( end - src )
    {
    default:
        bp . b [ 0 ] = src [ 7 ];
    case 7:
        bp . b [ 1 ] = src [ 6 ];
    case 6:
        bp . b [ 2 ] = src [ 5 ];
    case 5:
        bp . b [ 3 ] = src [ 4 ];
    case 4:
        bp . b [ 4 ] = src [ 3 ];
    case 3:
        bp . b [ 5 ] = src [ 2 ];
    case 2:
        bp . b [ 6 ] = src [ 1 ];
    case 1:
        bp . b [ 7 ] = src [ 0 ];
    }
#endif

    /* prime compare results */
    ra = rb = rc = ~ 0;

    /* position src at buffer end for loop */
    src += 8;

    /* evaluate at initial position */
    ALIGN_2NA_HEADER ( buffer, tpos, align_len );
    switch ( pos & 3 )
    {
    case 0:
        ra = ( buffer ^ self -> query [ 0 ] . pattern . l ) & self -> query [ 0 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
    case 1:
        rb = ( buffer ^ self -> query [ 1 ] . pattern . l ) & self -> query [ 1 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
    case 2:
        rc = ( buffer ^ self -> query [ 2 ] . pattern . l ) & self -> query [ 2 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
    case 3:
        rd = ( buffer ^ self -> query [ 3 ] . pattern . l ) & self -> query [ 3 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    /* loop on the number of shifts */
    for ( count = ( int ) ( len - self -> size ); count >= 3; count -= 4 )
    {
        /* exit condition within sequence */
        if ( ra == 0 || rb == 0 || rc == 0 || rd == 0 )
            return 1;

        /* get next 2na byte */
        buffer <<= 8;
#if ENDLESS_BUFFER
        buffer |= * src ++;
#else
        if ( src < end )
            bp . b [ 0 ] = * src ++;
#endif

#if TRACE_OPERATIONS
        tpos = ( tpos + 4 ) & ~ 3;
        ALIGN_2NA_HEADER ( buffer, tpos, align_len );
#endif

        /* test at this position */
        ra = ( buffer ^ self -> query [ 0 ] . pattern . l ) & self -> query [ 0 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
        rb = ( buffer ^ self -> query [ 1 ] . pattern . l ) & self -> query [ 1 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
        rc = ( buffer ^ self -> query [ 2 ] . pattern . l ) & self -> query [ 2 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
        rd = ( buffer ^ self -> query [ 3 ] . pattern . l ) & self -> query [ 3 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    switch ( ( pos + count ) & 3 )
    {
    case 2:
        if ( ! rc ) return 1;
    case 1:
        if ( ! rb ) return 1;
    case 0:
        if ( ! ra ) return 1;
    }

    return 0;

#undef buffer
}

static
int eval_2na_pos ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
    int i, count;
    uint64_t ra, rb, rc, rd;

#if ENDLESS_BUFFER
    uint64_t buffer;
#else
    nucpat_t bp;
#define buffer bp.l
    const uint8_t *end;
#endif
    const uint8_t *src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

#if TRACE_OPERATIONS
    unsigned int tpos = pos & ~ 3;
    unsigned int align_len = len + ( pos & 3 );
#endif

    if ( len < self -> size )
        return 0;

#if ENDLESS_BUFFER
    /* prime source buffer */
    buffer = LOAD64( src );
#else
    /* accumulate entry position into length */
    end = src + ( ( len + ( pos & 3 ) + 3 ) >> 2 );

    /* prime source buffer */
    bp . l = 0; /* defined above as "buffer" */
    switch ( end - src )
    {
    default:
        bp . b [ 0 ] = src [ 7 ];
    case 7:
        bp . b [ 1 ] = src [ 6 ];
    case 6:
        bp . b [ 2 ] = src [ 5 ];
    case 5:
        bp . b [ 3 ] = src [ 4 ];
    case 4:
        bp . b [ 4 ] = src [ 3 ];
    case 3:
        bp . b [ 5 ] = src [ 2 ];
    case 2:
        bp . b [ 6 ] = src [ 1 ];
    case 1:
        bp . b [ 7 ] = src [ 0 ];
    }
#endif

    /* prime compare results */
    ra = rb = rc = ~ 0;

    /* position src at buffer end for loop */
    src += 8;

    /* evaluate at initial position */
    ALIGN_2NA_HEADER ( buffer, tpos, align_len );
    i = 0 - ( int ) ( pos & 3 );
    switch ( pos & 3 )
    {
    case 0:
        ra = ( buffer ^ self -> query [ 0 ] . pattern . l ) & self -> query [ 0 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
    case 1:
        rb = ( buffer ^ self -> query [ 1 ] . pattern . l ) & self -> query [ 1 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
    case 2:
        rc = ( buffer ^ self -> query [ 2 ] . pattern . l ) & self -> query [ 2 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
    case 3:
        rd = ( buffer ^ self -> query [ 3 ] . pattern . l ) & self -> query [ 3 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    /* loop on the number of shifts */
    for ( count = ( int ) ( len - self -> size ); count >= 3; i += 4, count -= 4 )
    {
        /* exit condition within sequence */
        if ( ! ra )
            return i + 1;
        if ( ! rb )
            return i + 2;
        if ( ! rc )
            return i + 3;
        if ( ! rd )
            return i + 4;

        /* get next 2na byte */
        buffer <<= 8;
#if ENDLESS_BUFFER
        buffer |= * src ++;
#else
        if ( src < end )
            bp . b [ 0 ] = * src ++;
#endif

#if TRACE_OPERATIONS
        tpos = ( tpos + 4 ) & ~ 3;
        ALIGN_2NA_HEADER ( buffer, tpos, align_len );
#endif

        /* test at this position */
        ra = ( buffer ^ self -> query [ 0 ] . pattern . l ) & self -> query [ 0 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
        rb = ( buffer ^ self -> query [ 1 ] . pattern . l ) & self -> query [ 1 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
        rc = ( buffer ^ self -> query [ 2 ] . pattern . l ) & self -> query [ 2 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
        rd = ( buffer ^ self -> query [ 3 ] . pattern . l ) & self -> query [ 3 ] . mask . l;
        ALIGN_2NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    switch ( ( pos + count ) & 3 )
    {
    case 0:
        if ( ! ra ) return i + 1;
        break;
    case 1:
        if ( ! ra ) return i + 1;
        if ( ! rb ) return i + 2;
        break;
    case 2:
        if ( ! ra ) return i + 1;
        if ( ! rb ) return i + 2;
        if ( ! rc ) return i + 3;
    }

    return 0;

#undef buffer
}

static
int eval_4na_64 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
    int count;
    uint64_t buffer;
    uint64_t ra, rb, rc, rd;
#if ! ENDLESS_BUFFER
    const uint8_t *end;
#endif
    const uint8_t *src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

#if TRACE_OPERATIONS
    unsigned int tpos = pos & ~ 3;
    unsigned int align_len = len + ( pos & 3 );
#endif

    if ( len < self -> size )
        return 0;

#if ENDLESS_BUFFER
    buffer = expand_2na [ src [ 3 ] ];
    buffer |= ( uint32_t ) expand_2na [ src [ 2 ] ] << 16;
    buffer |= ( uint64_t ) expand_2na [ src [ 1 ] ] << 32;
    buffer |= ( uint64_t ) expand_2na [ src [ 0 ] ] << 48;
#else
    /* accumulate entry position into length */
    end = src + ( ( len + ( pos & 3 ) + 3 ) >> 2 );

    /* prime source buffer */
    buffer = 0;
    switch ( end - src )
    {
    default:
        buffer |= expand_2na [ src [ 3 ] ];
    case 3:
        buffer |= ( uint32_t ) expand_2na [ src [ 2 ] ] << 16;
    case 2:
        buffer |= ( uint64_t ) expand_2na [ src [ 1 ] ] << 32;
    case 1:
        buffer |= ( uint64_t ) expand_2na [ src [ 0 ] ] << 48;
    }
#endif

    /* prime compare results */
    ra = rb = rc = rd = ~ 0;

    /* position src at buffer end for loop */
    src += 4;

    /* evaluate at initial position */
    ALIGN_4NA_HEADER ( buffer, tpos, align_len );
    switch ( pos & 3 )
    {
    case 0:
        ra = ( buffer & self -> query [ 0 ] . pattern . l )
            ^ ( buffer & self -> query [ 0 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
    case 1:
        rb = ( buffer & self -> query [ 1 ] . pattern . l )
            ^ ( buffer & self -> query [ 1 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
    case 2:
        rc = ( buffer & self -> query [ 2 ] . pattern . l )
            ^ ( buffer & self -> query [ 2 ] . mask . l ) ;
        ALIGN_4NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
    case 3:
        rd = ( buffer & self -> query [ 3 ] . pattern . l )
            ^ ( buffer & self -> query [ 3 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    for ( count = ( int ) ( len - self -> size ); count >= 3; count -= 4 )
    {
        /* exit condition within sequence */
        if ( ra == 0 || rb == 0 || rc == 0 || rd == 0 )
            return 1;

        /* shuffle in next byte in 4na */
        buffer <<= 16;
#if ! ENDLESS_BUFFER
        if ( src < end )
#endif
            buffer |= expand_2na [ * src ++ ];

#if TRACE_OPERATIONS
        tpos = ( tpos + 4 ) & ~ 3;
        ALIGN_4NA_HEADER ( buffer, tpos, align_len );
#endif

        /* test at this position */
        ra = ( buffer & self -> query [ 0 ] . pattern . l )
            ^ ( buffer & self -> query [ 0 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
        rb = ( buffer & self -> query [ 1 ] . pattern . l )
            ^ ( buffer & self -> query [ 1 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
        rc = ( buffer & self -> query [ 2 ] . pattern . l )
            ^ ( buffer & self -> query [ 2 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
        rd = ( buffer & self -> query [ 3 ] . pattern . l )
            ^ ( buffer & self -> query [ 3 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    switch ( ( pos + count ) & 3 )
    {
    case 2:
        if ( ! rc ) return 1;
    case 1:
        if ( ! rb ) return 1;
    case 0:
        if ( ! ra ) return 1;
    }

    return 0;
}

static
int eval_4na_pos ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
    int i, count;
    uint64_t buffer;
    uint64_t ra, rb, rc, rd;
#if ! ENDLESS_BUFFER
    const uint8_t *end;
#endif
    const uint8_t *src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

#if TRACE_OPERATIONS
    unsigned int tpos = pos & ~ 3;
    unsigned int align_len = len + ( pos & 3 );
#endif

    if ( len < self -> size )
        return 0;

#if ENDLESS_BUFFER
    buffer = expand_2na [ src [ 3 ] ];
    buffer |= ( uint32_t ) expand_2na [ src [ 2 ] ] << 16;
    buffer |= ( uint64_t ) expand_2na [ src [ 1 ] ] << 32;
    buffer |= ( uint64_t ) expand_2na [ src [ 0 ] ] << 48;
#else
    /* accumulate entry position into length */
    end = src + ( ( len + ( pos & 3 ) + 3 ) >> 2 );

    /* prime source buffer */
    buffer = 0;
    switch ( end - src )
    {
    default:
        buffer |= expand_2na [ src [ 3 ] ];
    case 3:
        buffer |= ( uint32_t ) expand_2na [ src [ 2 ] ] << 16;
    case 2:
        buffer |= ( uint64_t ) expand_2na [ src [ 1 ] ] << 32;
    case 1:
        buffer |= ( uint64_t ) expand_2na [ src [ 0 ] ] << 48;
    }
#endif

    /* prime compare results */
    ra = rb = rc = rd = ~ 0;

    /* position src at buffer end for loop */
    src += 4;

    /* evaluate at initial position */
    ALIGN_4NA_HEADER ( buffer, tpos, align_len );
    i = 0 - ( int ) ( pos & 3 );
    switch ( pos & 3 )
    {
    case 0:
        ra = ( buffer & self -> query [ 0 ] . pattern . l )
            ^ ( buffer & self -> query [ 0 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
    case 1:
        rb = ( buffer & self -> query [ 1 ] . pattern . l )
            ^ ( buffer & self -> query [ 1 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
    case 2:
        rc = ( buffer & self -> query [ 2 ] . pattern . l )
            ^ ( buffer & self -> query [ 2 ] . mask . l ) ;
        ALIGN_4NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
    case 3:
        rd = ( buffer & self -> query [ 3 ] . pattern . l )
            ^ ( buffer & self -> query [ 3 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    for ( count = ( int ) ( len - self -> size ); count >= 3; i += 4, count -= 4 )
    {
        /* exit condition within sequence */
        if ( ! ra )
            return i + 1;
        if ( ! rb )
            return i + 2;
        if ( ! rc )
            return i + 3;
        if ( ! rd )
            return i + 4;

        /* shuffle in next byte in 4na */
        buffer <<= 16;
#if ! ENDLESS_BUFFER
        if ( src < end )
#endif
            buffer |= expand_2na [ * src ++ ];

#if TRACE_OPERATIONS
        tpos = ( tpos + 4 ) & ~ 3;
        ALIGN_4NA_HEADER ( buffer, tpos, align_len );
#endif

        /* test at this position */
        ra = ( buffer & self -> query [ 0 ] . pattern . l )
            ^ ( buffer & self -> query [ 0 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 0 ] . pattern . l, self -> query [ 0 ] . mask . l, ra );
        rb = ( buffer & self -> query [ 1 ] . pattern . l )
            ^ ( buffer & self -> query [ 1 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 1 ] . pattern . l, self -> query [ 1 ] . mask . l, rb );
        rc = ( buffer & self -> query [ 2 ] . pattern . l )
            ^ ( buffer & self -> query [ 2 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 2 ] . pattern . l, self -> query [ 2 ] . mask . l, rc );
        rd = ( buffer & self -> query [ 3 ] . pattern . l )
            ^ ( buffer & self -> query [ 3 ] . mask . l );
        ALIGN_4NA_RESULT ( buffer, self -> query [ 3 ] . pattern . l, self -> query [ 3 ] . mask . l, rd );
    }

    switch ( ( pos + count ) & 3 )
    {
    case 0:
        if ( ! ra ) return i + 1;
        break;
    case 1:
        if ( ! ra ) return i + 1;
        if ( ! rb ) return i + 2;
        break;
    case 2:
        if ( ! ra ) return i + 1;
        if ( ! rb ) return i + 2;
        if ( ! rc ) return i + 3;
    }

    return 0;
}

#else /* INTEL_INTRINSICS */

#if ENDLESS_BUFFER
static __inline__
__m128i prime_buffer_2na ( const uint8_t *src, const uint8_t *ignore )
{
    __m128i buffer;
    ( void ) ignore;
    if ( ( ( size_t ) src & 15 ) == 0 )
        buffer = _mm_load_si128 ( ( const __m128i* ) src );
    else
        buffer = _mm_loadu_si128 ( ( const __m128i* ) src );
    return buffer;
}
#else
static __inline__
__m128i prime_buffer_2na ( const uint8_t *src, const uint8_t *end )
{
    size_t bytes;
    __m128i buffer;

    if ( ( bytes = end - src ) >= 16 )
    {
        if ( ( ( size_t ) src & 15 ) == 0 )
            buffer = _mm_load_si128 ( ( const __m128i* ) src );
        else
            buffer = _mm_loadu_si128 ( ( const __m128i* ) src );
    }
    else
    {
        nucpat_t tmp;

        /* common for Solexa */
        if ( bytes == 7 )
        {
            tmp . b [ 0 ] = src [ 0 ];
            tmp . b [ 1 ] = src [ 1 ];
            tmp . b [ 2 ] = src [ 2 ];
            tmp . b [ 3 ] = src [ 3 ];
            tmp . b [ 4 ] = src [ 4 ];
            tmp . b [ 5 ] = src [ 5 ];
            tmp . b [ 6 ] = src [ 6 ];
            tmp . b [ 7 ] = 0;
            tmp . l [ 1 ] = 0;
        }

        /* currently only seen for 454 */
        else
        {
            memmove ( tmp . b, src, bytes );
            memset ( & tmp . b [ bytes ], 0, sizeof tmp . b - bytes );
        }

        buffer = _mm_loadu_si128 ( ( const __m128i* ) tmp . b );
    }
    return buffer;
}
#endif

#define prime_registers( self ) \
    p0 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 0 ] . pattern . b ); \
    m0 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 0 ] . mask . b ); \
    p1 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 1 ] . pattern . b ); \
    m1 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 1 ] . mask . b ); \
    p2 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 2 ] . pattern . b ); \
    m2 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 2 ] . mask . b ); \
    p3 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 3 ] . pattern . b ); \
    m3 = _mm_load_si128 ( ( const __m128i* ) self -> query [ 3 ] . mask . b )

static
int eval_2na_8 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 1
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi8 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( void ) 0 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* kludge for streaming in a byte at a time
       only needed when qbytes > 1 */
#if qbytes > 1
    int slam;
    const uint8_t *p;
#endif

    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_2na ( src, end );
    src += 16;
#if qbytes > 1
    p = src;

    /* pre-load slam for single byte streaming */
    if ( src < end )
        slam = ( int ) src [ -1 ] << 8;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#else
    num_passes = qbytes;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_2NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_2NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, p0 );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_2NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, p1 );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_2NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, p2 );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_2NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, p3 );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_2NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 2 ) + 0;
                    fb = ( fb << 2 ) + 1;
                    fc = ( fc << 2 ) + 2;
                    fd = ( fd << 2 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 1
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 1 );

                /* bring in new byte */
                if ( p < end )
                {
                    slam >>= 8;
                    slam |= ( int ) p [ 0 ] << 8;
                    buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                }

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 16 - qbytes ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if RELOAD_BUFFER || qbytes == 1
            buffer = prime_buffer_2na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_2na ( src, end );
            else
            {
                if ( ( ( size_t ) ( p - src ) & 1 ) != 0 )
                {
                    buffer = _mm_srli_si128 ( buffer, 1 );
                    if ( p < end )
                    {
                        slam >>= 8;
                        slam |= ( int ) p [ 0 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                    ++ p;
                }

                if ( src + 16 <= end ) switch ( p - src )
                {
                case 4:
                    buffer = _mm_srli_si128 ( buffer, 12 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 5 ], 7 );
                    break;
                case 6:
                    buffer = _mm_srli_si128 ( buffer, 10 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 7 );
                    break;
                case 8:
                    buffer = _mm_srli_si128 ( buffer, 8 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 7 );
                    break;
                case 10:
                    buffer = _mm_srli_si128 ( buffer, 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 7 );
                    break;
                case 12:
                    buffer = _mm_srli_si128 ( buffer, 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 7 );
                    break;
                case 14:
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 7 );
                    break;
                }

                else for ( ; p - src < 16; p += 2 )
                {
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    if ( p < end )
                    {
                        slam = p [ 0 ];
                        if ( p + 1 < end )
                            slam |= ( int ) p [ 1 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                }
            }
#endif

            /* adjust pointers */
            src += 16;
#if qbytes > 1
            p = src;
            if ( src < end )
                slam = ( int ) src [ -1 ] << 8;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_2na_16 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 2
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi16 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( void ) 0 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* kludge for streaming in a byte at a time
       only needed when qbytes > 1 */
#if qbytes > 1
    int slam = 0;
    const uint8_t *p;
#endif

    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_2na ( src, end );
    src += 16;
#if qbytes > 1
    p = src;

    /* pre-load slam for single byte streaming */
    if ( src < end )
        slam = ( int ) src [ -1 ] << 8;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#else
    num_passes = qbytes;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_2NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_2NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, p0 );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_2NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, p1 );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_2NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, p2 );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_2NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, p3 );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_2NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 2 ) + 0;
                    fb = ( fb << 2 ) + 1;
                    fc = ( fc << 2 ) + 2;
                    fd = ( fd << 2 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 1
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 1 );

                /* bring in new byte */
                if ( p < end )
                {
                    slam >>= 8;
                    slam |= ( int ) p [ 0 ] << 8;
                    buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                }

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 16 - qbytes ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if RELOAD_BUFFER || qbytes == 1
            buffer = prime_buffer_2na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_2na ( src, end );
            else
            {
                if ( ( ( size_t ) ( p - src ) & 1 ) != 0 )
                {
                    buffer = _mm_srli_si128 ( buffer, 1 );
                    if ( p < end )
                    {
                        slam >>= 8;
                        slam |= ( int ) p [ 0 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                    ++ p;
                }

                if ( src + 16 <= end ) switch ( p - src )
                {
                case 4:
                    buffer = _mm_srli_si128 ( buffer, 12 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 5 ], 7 );
                    break;
                case 6:
                    buffer = _mm_srli_si128 ( buffer, 10 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 7 );
                    break;
                case 8:
                    buffer = _mm_srli_si128 ( buffer, 8 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 7 );
                    break;
                case 10:
                    buffer = _mm_srli_si128 ( buffer, 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 7 );
                    break;
                case 12:
                    buffer = _mm_srli_si128 ( buffer, 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 7 );
                    break;
                case 14:
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 7 );
                    break;
                }

                else for ( ; p - src < 16; p += 2 )
                {
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    if ( p < end )
                    {
                        slam = p [ 0 ];
                        if ( p + 1 < end )
                            slam |= ( int ) p [ 1 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                }
            }
#endif

            /* adjust pointers */
            src += 16;
#if qbytes > 1
            p = src;
            if ( src < end )
                slam = ( int ) src [ -1 ] << 8;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_2na_32 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 4
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( void ) 0 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* kludge for streaming in a byte at a time
       only needed when qbytes > 1 */
#if qbytes > 1
    int slam = 0;
    const uint8_t *p;
#endif

    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_2na ( src, end );
    src += 16;
#if qbytes > 1
    p = src;

    /* pre-load slam for single byte streaming */
    if ( src < end )
        slam = ( int ) src [ -1 ] << 8;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#else
    num_passes = qbytes;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_2NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_2NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, p0 );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_2NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, p1 );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_2NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, p2 );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_2NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, p3 );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_2NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 2 ) + 0;
                    fb = ( fb << 2 ) + 1;
                    fc = ( fc << 2 ) + 2;
                    fd = ( fd << 2 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 1
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 1 );

                /* bring in new byte */
                if ( p < end )
                {
                    slam >>= 8;
                    slam |= ( int ) p [ 0 ] << 8;
                    buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                }

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 16 - qbytes ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if RELOAD_BUFFER || qbytes == 1
            buffer = prime_buffer_2na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_2na ( src, end );
            else
            {
                if ( ( ( size_t ) ( p - src ) & 1 ) != 0 )
                {
                    buffer = _mm_srli_si128 ( buffer, 1 );
                    if ( p < end )
                    {
                        slam >>= 8;
                        slam |= ( int ) p [ 0 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                    ++ p;
                }

                if ( src + 16 <= end ) switch ( p - src )
                {
                case 4:
                    buffer = _mm_srli_si128 ( buffer, 12 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 5 ], 7 );
                    break;
                case 6:
                    buffer = _mm_srli_si128 ( buffer, 10 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 7 );
                    break;
                case 8:
                    buffer = _mm_srli_si128 ( buffer, 8 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 7 );
                    break;
                case 10:
                    buffer = _mm_srli_si128 ( buffer, 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 7 );
                    break;
                case 12:
                    buffer = _mm_srli_si128 ( buffer, 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 7 );
                    break;
                case 14:
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 7 );
                    break;
                }

                else for ( ; p - src < 16; p += 2 )
                {
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    if ( p < end )
                    {
                        slam = p [ 0 ];
                        if ( p + 1 < end )
                            slam |= ( int ) p [ 1 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                }
            }
#endif

            /* adjust pointers */
            src += 16;
#if qbytes > 1
            p = src;
            if ( src < end )
                slam = ( int ) src [ -1 ] << 8;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_2na_64 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 8
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( res ) &= ( ( res ) & 0x0F0F ) << 4, \
      ( res ) |= ( res ) >> 4 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* kludge for streaming in a byte at a time
       only needed when qbytes > 1 */
#if qbytes > 1
    int slam = 0;
    const uint8_t *p;
#endif

    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_2na ( src, end );
    src += 16;
#if qbytes > 1
    p = src;

    /* pre-load slam for single byte streaming */
    if ( src < end )
        slam = ( int ) src [ -1 ] << 8;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#else
    num_passes = qbytes;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_2NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_2NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, p0 );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_2NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, p1 );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_2NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, p2 );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_2NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, p3 );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_2NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 2 ) + 0;
                    fb = ( fb << 2 ) + 1;
                    fc = ( fc << 2 ) + 2;
                    fd = ( fd << 2 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 1
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 1 );

                /* bring in new byte */
                if ( p < end )
                {
                    slam >>= 8;
                    slam |= ( int ) p [ 0 ] << 8;
                    buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                }

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 16 - qbytes ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if RELOAD_BUFFER || qbytes == 1
            buffer = prime_buffer_2na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_2na ( src, end );
            else
            {
                if ( ( ( size_t ) ( p - src ) & 1 ) != 0 )
                {
                    buffer = _mm_srli_si128 ( buffer, 1 );
                    if ( p < end )
                    {
                        slam >>= 8;
                        slam |= ( int ) p [ 0 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                    ++ p;
                }

                if ( src + 16 <= end ) switch ( p - src )
                {
                case 4:
                    buffer = _mm_srli_si128 ( buffer, 12 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 5 ], 7 );
                    break;
                case 6:
                    buffer = _mm_srli_si128 ( buffer, 10 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 7 );
                    break;
                case 8:
                    buffer = _mm_srli_si128 ( buffer, 8 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 7 );
                    break;
                case 10:
                    buffer = _mm_srli_si128 ( buffer, 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 7 );
                    break;
                case 12:
                    buffer = _mm_srli_si128 ( buffer, 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 7 );
                    break;
                case 14:
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 7 );
                    break;
                }

                else for ( ; p - src < 16; p += 2 )
                {
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    if ( p < end )
                    {
                        slam = p [ 0 ];
                        if ( p + 1 < end )
                            slam |= ( int ) p [ 1 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                }
            }
#endif

            /* adjust pointers */
            src += 16;
#if qbytes > 1
            p = src;
            if ( src < end )
                slam = ( int ) src [ -1 ] << 8;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_2na_128 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 16
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( res ) = - ( ( ( res ) + 1 ) >> 16 ) )
#endif

    /* SSE registers */
    nucreg_t buffer, ri;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* kludge for streaming in a byte at a time
       only needed when qbytes > 1 */
#if qbytes > 1
    int slam = 0;
    const uint8_t *p;
#endif

    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_2na ( src, end );
    src += 16;
#if qbytes > 1
    p = src;

    /* pre-load slam for single byte streaming */
    if ( src < end )
        slam = ( int ) src [ -1 ] << 8;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#else
    num_passes = qbytes;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_2NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_2NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, p0 );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_2NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, p1 );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_2NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, p2 );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_2NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, p3 );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_2NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 2 ) + 0;
                    fb = ( fb << 2 ) + 1;
                    fc = ( fc << 2 ) + 2;
                    fd = ( fd << 2 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 1
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 1 );

                /* bring in new byte */
                if ( p < end )
                {
                    slam >>= 8;
                    slam |= ( int ) p [ 0 ] << 8;
                    buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                }

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 16 - qbytes ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if RELOAD_BUFFER || qbytes == 1
            buffer = prime_buffer_2na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_2na ( src, end );
            else
            {
                if ( ( ( size_t ) ( p - src ) & 1 ) != 0 )
                {
                    buffer = _mm_srli_si128 ( buffer, 1 );
                    if ( p < end )
                    {
                        slam >>= 8;
                        slam |= ( int ) p [ 0 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                    ++ p;
                }

                if ( src + 16 <= end ) switch ( p - src )
                {
                case 4:
                    buffer = _mm_srli_si128 ( buffer, 12 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 5 ], 7 );
                    break;
                case 6:
                    buffer = _mm_srli_si128 ( buffer, 10 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 7 );
                    break;
                case 8:
                    buffer = _mm_srli_si128 ( buffer, 8 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 7 );
                    break;
                case 10:
                    buffer = _mm_srli_si128 ( buffer, 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 7 );
                    break;
                case 12:
                    buffer = _mm_srli_si128 ( buffer, 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 7 );
                    break;
                case 14:
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 7 );
                    break;
                }

                else for ( ; p - src < 16; p += 2 )
                {
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    if ( p < end )
                    {
                        slam = p [ 0 ];
                        if ( p + 1 < end )
                            slam |= ( int ) p [ 1 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                }
            }
#endif

            /* adjust pointers */
            src += 16;
#if qbytes > 1
            p = src;
            if ( src < end )
                slam = ( int ) src [ -1 ] << 8;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_2na_pos ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define positional 1
#define qbytes 16
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( res ) = - ( ( ( res ) + 1 ) >> 16 ) )
#endif

    /* SSE registers */
    nucreg_t buffer, ri;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* used to hold entry position */
#if positional
    unsigned int start;
#endif

    /* kludge for streaming in a byte at a time
       only needed when qbytes > 1 */
#if qbytes > 1
    int slam = 0;
    const uint8_t *p;
#endif

    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* hold starting position */
#if positional
    start = pos;
#endif

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_2na ( src, end );
    src += 16;
#if qbytes > 1
    p = src;

    /* pre-load slam for single byte streaming */
    if ( src < end )
        slam = ( int ) src [ -1 ] << 8;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#else
    num_passes = qbytes;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_2NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_2NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, p0 );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_2NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, p1 );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_2NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, p2 );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_2NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, p3 );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_2NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if positional
                    switch ( stop - pos )
                    {
                    case 0:
                        if ( ra != 0 ) return pos - start + 1;
                        break;
                    case 1:
                        if ( ra != 0 ) return pos - start + 1;
                        if ( rb != 0 ) return pos - start + 2;
                        break;
                    case 2:
                        if ( ra != 0 ) return pos - start + 1;
                        if ( rb != 0 ) return pos - start + 2;
                        if ( rc != 0 ) return pos - start + 3;
                        break;
                    default:
                        if ( ra != 0 ) return pos - start + 1;
                        if ( rb != 0 ) return pos - start + 2;
                        if ( rc != 0 ) return pos - start + 3;
                        if ( rd != 0 ) return pos - start + 4;
                    }
                    return 0;
#elif qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 2 ) + 0;
                    fb = ( fb << 2 ) + 1;
                    fc = ( fc << 2 ) + 2;
                    fd = ( fd << 2 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 1
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 1 );

                /* bring in new byte */
                if ( p < end )
                {
                    slam >>= 8;
                    slam |= ( int ) p [ 0 ] << 8;
                    buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                }

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 16 - qbytes ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if RELOAD_BUFFER || qbytes == 1
            buffer = prime_buffer_2na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_2na ( src, end );
            else
            {
                if ( ( ( size_t ) ( p - src ) & 1 ) != 0 )
                {
                    buffer = _mm_srli_si128 ( buffer, 1 );
                    if ( p < end )
                    {
                        slam >>= 8;
                        slam |= ( int ) p [ 0 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                    ++ p;
                }

                if ( src + 16 <= end ) switch ( p - src )
                {
                case 4:
                    buffer = _mm_srli_si128 ( buffer, 12 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 5 ], 7 );
                    break;
                case 6:
                    buffer = _mm_srli_si128 ( buffer, 10 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 3 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 4 ], 7 );
                    break;
                case 8:
                    buffer = _mm_srli_si128 ( buffer, 8 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 3 ], 7 );
                    break;
                case 10:
                    buffer = _mm_srli_si128 ( buffer, 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 5 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 2 ], 7 );
                    break;
                case 12:
                    buffer = _mm_srli_si128 ( buffer, 4 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 6 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 1 ], 7 );
                    break;
                case 14:
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    buffer = _mm_insert_epi16 ( buffer, ( ( const uint16_t* ) p ) [ 0 ], 7 );
                    break;
                }

                else for ( ; p - src < 16; p += 2 )
                {
                    buffer = _mm_srli_si128 ( buffer, 2 );
                    if ( p < end )
                    {
                        slam = p [ 0 ];
                        if ( p + 1 < end )
                            slam |= ( int ) p [ 1 ] << 8;
                        buffer = _mm_insert_epi16 ( buffer, slam, 7 );
                    }
                }
            }
#endif

            /* adjust pointers */
            src += 16;
#if qbytes > 1
            p = src;
            if ( src < end )
                slam = ( int ) src [ -1 ] << 8;
#endif
        }
    }

#undef positional
#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}


#if ENDLESS_BUFFER
static __inline__
__m128i prime_buffer_4na ( const uint8_t *src, const uint8_t *ignore )
{
    nucpat_t tmp;
    __m128i buffer;

    ( void ) ignore;

    tmp . w [ 0 ] = expand_2na [ src [ 0 ] ];
    tmp . w [ 1 ] = expand_2na [ src [ 1 ] ];
    tmp . w [ 2 ] = expand_2na [ src [ 2 ] ];
    tmp . w [ 3 ] = expand_2na [ src [ 3 ] ];
    tmp . w [ 4 ] = expand_2na [ src [ 4 ] ];
    tmp . w [ 5 ] = expand_2na [ src [ 5 ] ];
    tmp . w [ 6 ] = expand_2na [ src [ 6 ] ];
    tmp . w [ 7 ] = expand_2na [ src [ 7 ] ];

    buffer = _mm_loadu_si128 ( ( const __m128i* ) tmp . b );
    return buffer;
}
#else
static __inline__
__m128i prime_buffer_4na ( const uint8_t *src, const uint8_t *end )
{
    nucpat_t tmp;
    __m128i buffer;

    if ( end - src >= 8 )
    {
        tmp . w [ 0 ] = expand_2na [ src [ 0 ] ];
        tmp . w [ 1 ] = expand_2na [ src [ 1 ] ];
        tmp . w [ 2 ] = expand_2na [ src [ 2 ] ];
        tmp . w [ 3 ] = expand_2na [ src [ 3 ] ];
        tmp . w [ 4 ] = expand_2na [ src [ 4 ] ];
        tmp . w [ 5 ] = expand_2na [ src [ 5 ] ];
        tmp . w [ 6 ] = expand_2na [ src [ 6 ] ];
        tmp . w [ 7 ] = expand_2na [ src [ 7 ] ];
    }
    else
    {
        tmp . l [ 0 ] = tmp . l [ 1 ] = 0;
        switch ( end - src )
        {
        default:
            tmp . w [ 6 ] = expand_2na [ src [ 6 ] ];
        case 6:
            tmp . w [ 5 ] = expand_2na [ src [ 5 ] ];
        case 5:
            tmp . w [ 4 ] = expand_2na [ src [ 4 ] ];
        case 4:
            tmp . w [ 3 ] = expand_2na [ src [ 3 ] ];
        case 3:
            tmp . w [ 2 ] = expand_2na [ src [ 2 ] ];
        case 2:
            tmp . w [ 1 ] = expand_2na [ src [ 1 ] ];
        case 1:
            tmp . w [ 0 ] = expand_2na [ src [ 0 ] ];
            break;
        }
    }

    buffer = _mm_loadu_si128 ( ( const __m128i* ) tmp . b );
    return buffer;
}
#endif

static
int eval_4na_16 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 2
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi16 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( void ) 0 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri, rj;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

#if qbytes > 2
    const uint8_t *p;
#endif
    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_4na ( src, end );
    src += 8;
#if qbytes > 2
    p = src;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#elif qbytes == 1
#error "4na requires at least 2 qbytes"
#else
    num_passes = qbytes / 2;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_4NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes / 2;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_4NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, p0 );
                rj = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, rj );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_4NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, p1 );
                rj = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_4NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, p2 );
                rj = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_4NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, p3 );
                rj = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_4NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 1 ) + 0;
                    fb = ( fb << 1 ) + 1;
                    fc = ( fc << 1 ) + 2;
                    fd = ( fd << 1 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 2
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 2 );

                /* bring in new byte */
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 8 - qbytes / 2 ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if ENDLESS_BUFFER || qbytes == 2
            buffer = prime_buffer_4na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_4na ( src, end );
            else for ( ; p - src < 8; ++ p )
            {
                buffer = _mm_srli_si128 ( buffer, 2 );
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );
            }
#endif

            /* adjust pointers */
            src += 8;
#if qbytes > 2
            p = src;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_4na_32 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 4
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( void ) 0 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri, rj;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

#if qbytes > 2
    const uint8_t *p;
#endif
    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_4na ( src, end );
    src += 8;
#if qbytes > 2
    p = src;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#elif qbytes == 1
#error "4na requires at least 2 qbytes"
#else
    num_passes = qbytes / 2;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_4NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes / 2;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_4NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, p0 );
                rj = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, rj );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_4NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, p1 );
                rj = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_4NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, p2 );
                rj = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_4NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, p3 );
                rj = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_4NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 1 ) + 0;
                    fb = ( fb << 1 ) + 1;
                    fc = ( fc << 1 ) + 2;
                    fd = ( fd << 1 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 2
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 2 );

                /* bring in new byte */
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 8 - qbytes / 2 ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if ENDLESS_BUFFER || qbytes == 2
            buffer = prime_buffer_4na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_4na ( src, end );
            else for ( ; p - src < 8; ++ p )
            {
                buffer = _mm_srli_si128 ( buffer, 2 );
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );
            }
#endif

            /* adjust pointers */
            src += 8;
#if qbytes > 2
            p = src;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_4na_64 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 8
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( res ) &= ( ( res ) & 0x0F0F ) << 4, \
      ( res ) |= ( res ) >> 4 )
#endif

    /* SSE registers */
    nucreg_t buffer, ri, rj;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

#if qbytes > 2
    const uint8_t *p;
#endif
    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_4na ( src, end );
    src += 8;
#if qbytes > 2
    p = src;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#elif qbytes == 1
#error "4na requires at least 2 qbytes"
#else
    num_passes = qbytes / 2;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_4NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes / 2;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_4NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, p0 );
                rj = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, rj );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_4NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, p1 );
                rj = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_4NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, p2 );
                rj = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_4NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, p3 );
                rj = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_4NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 1 ) + 0;
                    fb = ( fb << 1 ) + 1;
                    fc = ( fc << 1 ) + 2;
                    fd = ( fd << 1 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 2
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 2 );

                /* bring in new byte */
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 8 - qbytes / 2 ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if ENDLESS_BUFFER || qbytes == 2
            buffer = prime_buffer_4na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_4na ( src, end );
            else for ( ; p - src < 8; ++ p )
            {
                buffer = _mm_srli_si128 ( buffer, 2 );
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );
            }
#endif

            /* adjust pointers */
            src += 8;
#if qbytes > 2
            p = src;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_4na_128 ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define qbytes 16
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( res ) = - ( ( ( res ) + 1 ) >> 16 ) )
#endif

    /* SSE registers */
    nucreg_t buffer, ri, rj;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

#if qbytes > 2
    const uint8_t *p;
#endif
    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_4na ( src, end );
    src += 8;
#if qbytes > 2
    p = src;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#elif qbytes == 1
#error "4na requires at least 2 qbytes"
#else
    num_passes = qbytes / 2;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_4NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes / 2;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_4NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, p0 );
                rj = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, rj );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_4NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, p1 );
                rj = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_4NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, p2 );
                rj = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_4NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, p3 );
                rj = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_4NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 1 ) + 0;
                    fb = ( fb << 1 ) + 1;
                    fc = ( fc << 1 ) + 2;
                    fd = ( fd << 1 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 2
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 2 );

                /* bring in new byte */
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 8 - qbytes / 2 ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if ENDLESS_BUFFER || qbytes == 2
            buffer = prime_buffer_4na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_4na ( src, end );
            else for ( ; p - src < 8; ++ p )
            {
                buffer = _mm_srli_si128 ( buffer, 2 );
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );
            }
#endif

            /* adjust pointers */
            src += 8;
#if qbytes > 2
            p = src;
#endif
        }
    }

#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}

static
int eval_4na_pos ( const NucStrFastaExpr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len )
{
#define positional 1
#define qbytes 16
#define _mm_cmpeq_epi( a, b ) \
    _mm_cmpeq_epi32 ( a, b )
#if NEVER_MATCH
#define res_adj( res ) \
    res = 0
#else
#define res_adj( res ) \
    ( ( res ) = - ( ( ( res ) + 1 ) >> 16 ) )
#endif

    /* SSE registers */
    nucreg_t buffer, ri, rj;
    nucreg_t p0, p1, p2, p3, m0, m1, m2, m3;

    /* result registers */
    int ra, rb, rc, rd;

    /* used for shifting buffer, testing exit */
    unsigned int num_passes, stop;

    /* used to hold entry position */
#if positional
    unsigned int start;
#endif

#if qbytes > 2
    const uint8_t *p;
#endif
    const uint8_t *end, *src;
    unsigned int qlen = self -> size;

    /* this test is performed outside */
    assert ( len >= qlen );

    /* the effective length of the sequence,
       including leading bases we'll ignore */
    len += pos;

    /* hold starting position */
#if positional
    start = pos;
#endif

    /* convert source pointer */
    src = ( const uint8_t* ) ncbi2na + ( pos >> 2 );

    /* stop testing when position passes this point */
    stop = len - qlen;

    /* the sequence end pointer */
    end = ( const uint8_t* ) ncbi2na + ( ( len + 3 ) >> 2 );

    /* prime the buffer */
    buffer = prime_buffer_4na ( src, end );
    src += 8;
#if qbytes > 2
    p = src;
#endif

    /* prime the registers */
    prime_registers ( self );

    /* enter the loop at an appropriate offset */
    ra = rb = rc = 0;
#if qbytes == 16
    num_passes = ( stop - pos + 7 ) >> 2;
#elif qbytes == 1
#error "4na requires at least 2 qbytes"
#else
    num_passes = qbytes / 2;
#endif

    /* for reporting - give a buffer alignment */
    ALIGN_4NA_HEADER ( buffer, pos & ~ 3, len );
    switch ( pos & 3 )
    {
    default:

        /* outer loop - performs whole buffer loads */
        while ( 1 )
        {
            num_passes = qbytes / 2;

            /* inner loop - shifts in one byte at a time */
            while ( 1 )
            {
                /* for reporting - give a buffer alignment */
                ALIGN_4NA_HEADER ( buffer, pos, len );

                /* perform comparisons */
    case 0:
                ri = _mm_and_si128 ( buffer, p0 );
                rj = _mm_and_si128 ( buffer, m0 );
                ri = _mm_cmpeq_epi ( ri, rj );
                ra = _mm_movemask_epi8 ( ri );
                res_adj ( ra );
                ALIGN_4NA_RESULT ( buffer, p0, m0, ri, ra );
    case 1:
                ri = _mm_and_si128 ( buffer, p1 );
                rj = _mm_and_si128 ( buffer, m1 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rb = _mm_movemask_epi8 ( ri );
                res_adj ( rb );
                ALIGN_4NA_RESULT ( buffer, p1, m1, ri, rb );
    case 2:
                ri = _mm_and_si128 ( buffer, p2 );
                rj = _mm_and_si128 ( buffer, m2 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rc = _mm_movemask_epi8 ( ri );
                res_adj ( rc );
                ALIGN_4NA_RESULT ( buffer, p2, m2, ri, rc );
    case 3:
                ri = _mm_and_si128 ( buffer, p3 );
                rj = _mm_and_si128 ( buffer, m3 );
                ri = _mm_cmpeq_epi ( ri, rj );
                rd = _mm_movemask_epi8 ( ri );
                res_adj ( rd );
                ALIGN_4NA_RESULT ( buffer, p3, m3, ri, rd );

                /* adjust pos */
                pos &= ~ 3;

                /* test for any promising results */
                if ( ( ra | rb | rc | rd ) != 0 )
                {
#if positional
                    switch ( stop - pos )
                    {
                    case 0:
                        if ( ra != 0 ) return pos - start + 1;
                        break;
                    case 1:
                        if ( ra != 0 ) return pos - start + 1;
                        if ( rb != 0 ) return pos - start + 2;
                        break;
                    case 2:
                        if ( ra != 0 ) return pos - start + 1;
                        if ( rb != 0 ) return pos - start + 2;
                        if ( rc != 0 ) return pos - start + 3;
                        break;
                    default:
                        if ( ra != 0 ) return pos - start + 1;
                        if ( rb != 0 ) return pos - start + 2;
                        if ( rc != 0 ) return pos - start + 3;
                        if ( rd != 0 ) return pos - start + 4;
                    }
                    return 0;
#elif qbytes == 16
                    switch ( stop - pos )
                    {
                    default:
                        return 1;
                    case 2:
                        if ( rc != 0 ) return 1;
                    case 1:
                        if ( rb != 0 ) return 1;
                    case 0:
                        if ( ra != 0 ) return 1;
                    }
                    return 0;
#else
                    /* extract first non-zero bit from results
                       where result is all zeros, bit will be -1 */
                    int fa = uint16_lsbit ( ( uint16_t ) ra );
                    int fb = uint16_lsbit ( ( uint16_t ) rb );
                    int fc = uint16_lsbit ( ( uint16_t ) rc );
                    int fd = uint16_lsbit ( ( uint16_t ) rd );

                    /* convert bit number into number of bases
                       from current position. undefined where
                       bit is negative, but unimportant also */
                    fa = ( fa << 1 ) + 0;
                    fb = ( fb << 1 ) + 1;
                    fc = ( fc << 1 ) + 2;
                    fd = ( fd << 1 ) + 3;

                    /* test for any case where result was non-zero
                       and the resultant base index is within range */
                    if ( ra != 0 && pos + fa <= stop ) return 1;
                    if ( rb != 0 && pos + fb <= stop ) return 1;
                    if ( rc != 0 && pos + fc <= stop ) return 1;
                    if ( rd != 0 && pos + fd <= stop ) return 1;
#endif
                }

                /* advance pos */
                pos += 4;

                /* if no further comparisons are possible */
                if ( pos > stop )
                    return 0;

                /* if all shifting passes are complete */
                if ( -- num_passes == 0 )
                    break;

#if qbytes > 2
                /* shift buffer */
                buffer = _mm_srli_si128 ( buffer, 2 );

                /* bring in new byte */
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );

                /* always increment source */
                ++ p;
#endif
            }

            /* want to reload buffer */
            if ( src >= end )
                break;

            /* advance position */
            pos += ( 8 - qbytes / 2 ) << 2;
            if ( pos > stop )
                break;

            /* reload buffer */
#if ENDLESS_BUFFER || qbytes == 2
            buffer = prime_buffer_4na ( src, end );
#elif qbytes == 16
            assert ( 0 );
#else
            if ( p - src < 3 )
                buffer = prime_buffer_4na ( src, end );
            else for ( ; p - src < 8; ++ p )
            {
                buffer = _mm_srli_si128 ( buffer, 2 );
                if ( p < end )
                    buffer = _mm_insert_epi16 ( buffer, expand_2na [ * p ], 7 );
            }
#endif

            /* adjust pointers */
            src += 8;
#if qbytes > 2
            p = src;
#endif
        }
    }

#undef positional
#undef qbytes
#undef _mm_cmpeq_epi
#undef res_adj

    return 0;
}
#endif /* INTEL_INTRINSICS */


/* NucStrstrSearch
 *  search buffer from starting position
 *
 *  "ncbi2na" [ IN ] - pointer to 2na data
 *
 *  "pos" [ IN ] - starting base position for search,
 *  relative to "ncbi2na". may be >= 4.
 *
 *  "len" [ IN ] - the number of bases to include in
 *  the search, relative to "pos".
 *
 *  return values:
 *    ! 0 if the pattern was found
 */
LIB_EXPORT int CC NucStrstrSearch ( const NucStrstr *self,
    const void *ncbi2na, unsigned int pos, unsigned int len , unsigned int * selflen )
{
    if ( self != NULL && ncbi2na != NULL && len != 0 )
    {
        int found;
        unsigned int fasta_len;

        switch ( self -> fasta . type )
        {
        case type_2na_64:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_2na_64 ( & self -> fasta, ncbi2na, pos, len );
        case type_4na_64:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_4na_64 ( & self -> fasta, ncbi2na, pos, len );
#if INTEL_INTRINSICS
        case type_2na_8:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_2na_8 ( & self -> fasta, ncbi2na, pos, len );
        case type_2na_16:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_2na_16 ( & self -> fasta, ncbi2na, pos, len );
        case type_2na_32:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_2na_32 ( & self -> fasta, ncbi2na, pos, len );
        case type_2na_128:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_2na_128 ( & self -> fasta, ncbi2na, pos, len );
        case type_4na_16:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_4na_16 ( & self -> fasta, ncbi2na, pos, len );
        case type_4na_32:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_4na_32 ( & self -> fasta, ncbi2na, pos, len );
        case type_4na_128:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_4na_128 ( & self -> fasta, ncbi2na, pos, len );
#endif
        case type_2na_pos:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_2na_pos ( & self -> fasta, ncbi2na, pos, len );
        case type_4na_pos:
            if ( len < self -> fasta . size ) return 0;
	    if(selflen) *selflen=self -> fasta . size;
            return eval_4na_pos ( & self -> fasta, ncbi2na, pos, len );
        case type_OP:
            found = NucStrstrSearch ( self -> boolean . left, ncbi2na, pos, len, selflen);
            switch ( self -> boolean . op )
            {
            case op_OR:
                if ( found != 0 )
                    return found;
                break;
            case op_AND:
                if ( found == 0 )
                    return found;
                break;
            }
            return NucStrstrSearch ( self -> boolean . right, ncbi2na, pos, len, selflen);
        case type_EXPR:
            switch ( self -> sub . op )
            {
            case 0:
            case op_NOT:
                found = NucStrstrSearch ( self -> sub . expr, ncbi2na, pos, len, selflen);
                if ( self -> sub . op == 0 )
                    return found;
                if ( found == 0 )
                    return 1;
                break;
            case op_HEAD:
                fasta_len = self -> sub . expr -> fasta . size;
                if ( fasta_len > len )
                    return 0;
                return NucStrstrSearch ( self -> sub . expr, ncbi2na, pos, fasta_len, selflen );
            case op_TAIL:
                fasta_len = self -> sub . expr -> fasta . size;
                if ( fasta_len > len )
                    return 0;
                found = NucStrstrSearch ( self -> sub . expr, ncbi2na,
                    pos + len - fasta_len, fasta_len, selflen );
                if ( found != 0 )
                    found += pos + len - fasta_len;
                return found;
            }
            break;
        }
    }
    return 0;
}

