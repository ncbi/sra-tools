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

#include <klib/sort.h>
#include <sysalloc.h>
#include <stdlib.h>
#include "mod_reads_defs.h"

uint8_t CompressedBasePos_Lookup[ MAX_BASE_POS + 1 ] =
{
   /*        0   1   2   3   4   5   6   7   8   9 */
   /*  0 */   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
   /*  1 */  10, 10, 10, 10, 10, 11, 11, 11, 11, 11,
   /*  2 */  12, 12, 12, 12, 12, 13, 13, 13, 13, 13,
   /*  3 */  14, 14, 14, 14, 14, 15, 15, 15, 15, 15,
   /*  4 */  16, 16, 16, 16, 16, 17, 17, 17, 17, 17,
   /*  5 */  18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
   /*  6 */  19, 19, 19, 19, 19, 19, 19, 19, 19, 19,
   /*  7 */  20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
   /*  8 */  21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
   /*  9 */  22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
   /* 10 */  23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   /* 11 */  23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   /* 12 */  23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   /* 13 */  23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   /* 14 */  23, 23, 23, 23, 23, 23, 23, 23, 23, 23,
   /* 15 */  24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   /* 16 */  24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   /* 17 */  24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   /* 18 */  24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   /* 19 */  24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
   /* 20 */  25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
   /* 21 */  25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
   /* 22 */  25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
   /* 23 */  25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
   /* 24 */  25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
   /* 25 */  26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
   /* 26 */  26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
   /* 27 */  26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
   /* 28 */  26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
   /* 29 */  26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
   /* 30 */  27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
   /* 31 */  27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
   /* 32 */  27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
   /* 33 */  27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
   /* 34 */  27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
   /* 35 */  28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
   /* 36 */  28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
   /* 37 */  28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
   /* 38 */  28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
   /* 39 */  28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
   /* 40 */  29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
   /* 41 */  29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
   /* 42 */  29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
   /* 43 */  29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
   /* 44 */  29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
   /* 45 */  30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
   /* 46 */  30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
   /* 47 */  30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
   /* 48 */  30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
   /* 49 */  30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
   /* 50 */  31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   /* 51 */  31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   /* 52 */  31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   /* 53 */  31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   /* 54 */  31, 31, 31, 31, 31, 31, 31, 31, 31, 31,
   /* 55 */  32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   /* 56 */  32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   /* 57 */  32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   /* 58 */  32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   /* 59 */  32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
   /* 60 */  33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
   /* 61 */  33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
   /* 62 */  33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
   /* 63 */  33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
   /* 64 */  33, 33, 33, 33, 33, 33, 33, 33, 33, 33,
   /* 65 */  34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
   /* 66 */  34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
   /* 67 */  34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
   /* 68 */  34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
   /* 69 */  34, 34, 34, 34, 34, 34, 34, 34, 34, 34,
   /* 70 */  35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
   /* 71 */  35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
   /* 72 */  35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
   /* 73 */  35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
   /* 74 */  35, 35, 35, 35, 35, 35, 35, 35, 35, 35,
   /* 75 */  36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
   /* 76 */  36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
   /* 77 */  36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
   /* 78 */  36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
   /* 79 */  36, 36, 36, 36, 36, 36, 36, 36, 36, 36,
   /* 80 */  37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
   /* 81 */  37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
   /* 82 */  37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
   /* 83 */  37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
   /* 84 */  37, 37, 37, 37, 37, 37, 37, 37, 37, 37,
   /* 85 */  38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
   /* 86 */  38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
   /* 87 */  38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
   /* 88 */  38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
   /* 89 */  38, 38, 38, 38, 38, 38, 38, 38, 38, 38,
   /* 90 */  39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
   /* 91 */  39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
   /* 92 */  39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
   /* 93 */  39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
   /* 94 */  39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
   /* 95 */  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
   /* 96 */  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
   /* 97 */  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
   /* 98 */  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
   /* 99 */  40, 40, 40, 40, 40, 40, 40, 40, 40, 40,
   /*100 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*101 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*102 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*103 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*104 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*105 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*106 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*107 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*108 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*109 */  41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
   /*110 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*111 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*112 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*113 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*114 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*115 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*116 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*117 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*118 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*119 */  42, 42, 42, 42, 42, 42, 42, 42, 42, 42,
   /*120 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*121 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*122 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*123 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*124 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*125 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*126 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*127 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*128 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*129 */  43, 43, 43, 43, 43, 43, 43, 43, 43, 43,
   /*130 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*131 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*132 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*133 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*134 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*135 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*136 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*137 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*138 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*139 */  44, 44, 44, 44, 44, 44, 44, 44, 44, 44,
   /*140 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*141 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*142 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*143 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*144 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*145 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*146 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*147 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*148 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*149 */  45, 45, 45, 45, 45, 45, 45, 45, 45, 45,
   /*150 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*151 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*152 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*153 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*154 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*155 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*156 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*157 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*158 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*159 */  46, 46, 46, 46, 46, 46, 46, 46, 46, 46,
   /*160 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*161 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*162 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*163 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*164 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*165 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*166 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*167 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*168 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*169 */  47, 47, 47, 47, 47, 47, 47, 47, 47, 47,
   /*170 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*171 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*172 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*173 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*174 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*175 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*176 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*177 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*178 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*179 */  48, 48, 48, 48, 48, 48, 48, 48, 48, 48,
   /*180 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*181 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*182 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*183 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*184 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*185 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*186 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*187 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*188 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49,
   /*189 */  49, 49, 49, 49, 49, 49, 49, 49, 49, 49
};

uint32_t CompressedStart_Lookup[ MAX_COMPRESSED_BASE_POS + 1 ] =
{
   /*          0     1     2     3     4     5     6     7     8     9 */
   /*  0 */    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,
   /*  1 */   10,   15,   20,   25,   30,   35,   40,   45,   50,   60,
   /*  2 */   70,   80,   90,  100,  150,  200,  250,  300,  350,  400,
   /*  3 */  450,  500,  550,  600,  650,  700,  750,  800,  850,  900,
   /*  4 */  950, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800
};

uint32_t CompressedEnd_Lookup[ MAX_COMPRESSED_BASE_POS + 1 ] =
{
   /*          0     1     2     3     4     5     6     7     8     9 */
   /*  0 */    0,    1,    2,    3,    4,    5,    6,    7,    8,    9,
   /*  1 */   14,   19,   24,   29,   34,   39,   44,   49,   59,   69,
   /*  2 */   79,   89,   99,  149,  199,  249,  299,  349,  399,  449,
   /*  3 */  499,  549,  599,  649,  699,  749,  799,  849,  899,  949,
   /*  4 */ 1000, 1099, 1199, 1299, 1399, 1499, 1599, 1699, 1799, 0xFFFFFFFF
};

uint8_t compress_base_pos( const uint32_t src )
{
    if ( src > MAX_BASE_POS )
        return MAX_COMPRESSED_BASE_POS;
    else
        return CompressedBasePos_Lookup[ src ];
}

uint32_t compress_start_lookup( const uint8_t idx )
{
    if ( idx <= MAX_COMPRESSED_BASE_POS )
        return CompressedStart_Lookup[ idx ];
    else
        return CompressedStart_Lookup[ MAX_COMPRESSED_BASE_POS ];
}

uint32_t compress_end_lookup( const uint8_t idx )
{
    if ( idx <= MAX_COMPRESSED_BASE_POS )
        return CompressedEnd_Lookup[ idx ];
    else
        return CompressedEnd_Lookup[ MAX_COMPRESSED_BASE_POS ];
}


static uint8_t Kmer_Ascii_Lookup[ 26 ] =
{
   /*  A   B   C   D   E   F   G   H   I   J   K   L   M  */
       0,  0,  1,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,
   /*  N   O   P   Q   R   S   T   U   V   W   X   Y   Z */
       4,  0,  0,  0,  0,  0,  3,  0,  0,  0,  0,  0,  0
};

uint16_t kmer_ascii_2_int( char * s )
{
    uint16_t res = Kmer_Ascii_Lookup[ s[ 0 ] - 'A' ];
    res <<= 2;
    res |= Kmer_Ascii_Lookup[ s[ 1 ] - 'A' ];
    res <<= 2;
    res |= Kmer_Ascii_Lookup[ s[ 2 ] - 'A' ];
    res <<= 2;
    res |= Kmer_Ascii_Lookup[ s[ 3 ] - 'A' ];
    res <<= 2;
    res |= Kmer_Ascii_Lookup[ s[ 4 ] - 'A' ];
    return res;
}

static char Kmer_int_Lookup[ 4 ] = { 'A', 'C', 'G', 'T' };

void kmer_int_2_ascii( const uint16_t kmer, char * s )
{
    uint16_t value = kmer;
    s[ 5 ] = 0;
    s[ 4 ] = Kmer_int_Lookup[ value & 3 ];
    value >>= 2;
    s[ 3 ] = Kmer_int_Lookup[ value & 3 ];
    value >>= 2;
    s[ 2 ] = Kmer_int_Lookup[ value & 3 ];
    value >>= 2;
    s[ 1 ] = Kmer_int_Lookup[ value & 3 ];
    value >>= 2;
    s[ 0 ] = Kmer_int_Lookup[ value & 3 ];
}


uint16_t kmer_add_base( const uint16_t kmer, const char c )
{
    uint16_t res = ( ( kmer << 2 ) & 0x3FC );
    return ( res | ( Kmer_Ascii_Lookup[ c - 'A' ] & 0x03 ) );
}


static double calc_mean_qual( uint64_t * qual, uint64_t count )
{
    uint8_t idx;
    double sum = 0.0;
    for ( idx = 1; idx <= MAX_QUALITY; ++ idx )
    {
        sum += ( qual[ idx ] * idx );
    }
    sum /= count;
    return sum;
}


static uint8_t get_qual_at_flat_pos( uint64_t * qual, uint64_t flat_pos )
{
    uint8_t idx, res = 0;
    uint64_t pos = 0;
    bool found = false;
    for ( idx = 0; idx <= MAX_QUALITY && !found; ++ idx )
    {
        found = ( ( pos <= flat_pos )  && ( flat_pos < ( pos + qual[ idx ] ) ) );
        if ( found )
            res = idx;
        else
            pos += qual[ idx ];
    }
    return res;
}


static double calc_median_qual_between( uint64_t * qual, uint64_t from, uint64_t to )
{
    double res;
    uint64_t count = ( to - from );
    if ( ( count & 1 ) == 0 )
    {
        uint64_t median_pos = ( from + ( count >> 1 ) ) - 1; /* even */
        res = get_qual_at_flat_pos( qual, median_pos );
        res += get_qual_at_flat_pos( qual, median_pos + 1 );
        res /= 2;
    }
    else
    {
        uint64_t median_pos = ( from + ( ( count + 1 ) >> 1 ) ) - 1; /* odd */
        res = get_qual_at_flat_pos( qual, median_pos );
    }
    return res;
}


static double calc_median_qual( uint64_t * qual, uint64_t count )
{
    return calc_median_qual_between( qual, 0, count );
}


static uint8_t calc_mode( uint64_t * qual, uint64_t count )
{
    uint8_t idx, max = 0, res = 0;
    for ( idx = 1; idx <= MAX_QUALITY; ++ idx )
    {
        if ( max < qual[ idx ] )
        {
            max = qual[ idx ];
            res = idx;
        }
    }
    return res;
}


static double calc_upper_quart( uint64_t * qual, uint64_t count )
{
    uint64_t half = count >> 1;
    return calc_median_qual_between( qual, half, count );
}


static double calc_lower_quart( uint64_t * qual, uint64_t count )
{
    uint64_t half = count >> 1;
    return calc_median_qual_between( qual, 0, half );
}


static double calc_upper_centile( uint64_t * qual, uint64_t count )
{
    uint64_t tenth = count / 10;
    return get_qual_at_flat_pos( qual, count - tenth );
}


static double calc_lower_centile( uint64_t * qual, uint64_t count )
{
    uint64_t tenth = count / 10;
    return get_qual_at_flat_pos( qual, tenth );
}


void setup_bp_array( p_base_pos bp, const uint8_t count )
{
    uint8_t i;
    for ( i = 0; i <= count; ++i )
    {
        bp[ i ].from = compress_start_lookup( i );
        bp[ i ].to   = compress_end_lookup( i );
    }
}


/* base_prob points to a array of base-probabilities in the order A,C,G,T */
double kmer_probability( const uint16_t kmer, double * base_prob )
{
    double res = 1.0;
    uint16_t temp = kmer;
    
    res *= base_prob[ temp & 3 ];
    temp >>= 2;
    res *= base_prob[ temp & 3 ];
    temp >>= 2;
    res *= base_prob[ temp & 3 ];
    temp >>= 2;
    res *= base_prob[ temp & 3 ];
    temp >>= 2;
    res *= base_prob[ temp & 3 ];
    return res;
}


void count_base( p_base_pos bp, const char base, const uint8_t quality )
{
    bp->count++;
    switch( base )
    {
    case 'A' : bp->base_sum[ IDX_A ]++; break;
    case 'C' : bp->base_sum[ IDX_C ]++; break;
    case 'G' : bp->base_sum[ IDX_G ]++; break;
    case 'T' : bp->base_sum[ IDX_T ]++; break;
    case 'N' : bp->base_sum[ IDX_N ]++; break;
    }
    if ( quality <= MAX_QUALITY )
        bp->qual[ quality ]++;
    else
        bp->qual[ MAX_QUALITY ]++;
}


uint16_t count_kmer( p_reads_data data,
                     const uint16_t kmer,
                     const uint8_t base_pos, 
                     const char base )
{
    uint16_t res = kmer_add_base( kmer, base );
    if ( base_pos > 3 )
    {
        /* pointer to the conter for this K-mer */
        p_kmer5_count cnt = &data->kmer5[ res ];
        /* count how often this K-mer occures at all */
        cnt->total_count++;
        /* count how often this K-mer occures at the given base-position */
        cnt->count[ base_pos ]++;
        /* count how many K-mers at all we have at this base-position */
        data->bp_total_kmers[ base_pos ]++;
    }
    return res;
}


void calculate_quality_mean_median_quart_centile( p_base_pos bp )
{
    bp->mean = calc_mean_qual( bp->qual, bp->count );
    bp->median = calc_median_qual( bp->qual, bp->count );
    bp->mode = calc_mode( bp->qual, bp->count );
    bp->upper_quart = calc_upper_quart( bp->qual, bp->count );
    bp->lower_quart = calc_lower_quart( bp->qual, bp->count );
    bp->upper_centile = calc_upper_centile( bp->qual, bp->count );
    bp->lower_centile = calc_lower_centile( bp->qual, bp->count );
}


void calculate_bases_percentage( p_base_pos bp )
{
    uint8_t i;
    for ( i = IDX_A; i <= IDX_N; ++i )
        bp->base_percent[ i ] = percent( bp->base_sum[ i ], bp->count );
}


void calculate_base_probability( p_reads_data data )
{
    uint8_t i;
    uint64_t value, total_acgt = 0;
    for ( i = IDX_A; i <= IDX_T; ++i )
    {
        value = data->total.base_sum[ i ];
        total_acgt += value;
        data->base_prob[ i ] = value;
    }
    /* do not merge the 2 loops, in the first one we calculate total_acgt 
       int the second loop we use it on every item */
    for ( i = IDX_A; i <= IDX_T; ++i )
        data->base_prob[ i ] /= total_acgt;
}


int CC kmer_sort_callback( const void *p1, const void *p2, void * data )
{
    p_reads_data rdata = data;
    int16_t idx1 = *( int16_t * ) p1;
    int16_t idx2 = *( int16_t * ) p2;
    p_kmer5_count k1 = &( rdata->kmer5[ idx1 ] );
    p_kmer5_count k2 = &( rdata->kmer5[ idx2 ] );
    if ( k1->observed_vs_expected < k2->observed_vs_expected )
        return 1;
    if ( k1->observed_vs_expected > k2->observed_vs_expected )
        return -1;
    return ( idx1 - idx2 );
}


void calculate_kmer_observed_vs_expected( p_reads_data data )
{
    uint16_t idx;
    for ( idx = 0; idx < NKMER5; ++idx )
    {
        data->kmer5[ idx ].probability = kmer_probability( idx, data->base_prob );
        data->total_kmers += data->kmer5[ idx ].total_count;
        data->kmer5_idx[ idx ] = idx;
    }
    /* do not merge the 2 loops, we need to calculate total_kmers first ! */
    for ( idx = 0; idx < NKMER5; ++idx )
    {
        uint16_t i;
        p_kmer5_count k = &( data->kmer5[ idx ] );

        k->expected = data->total_kmers;
        k->expected *= k->probability;
        k->observed_vs_expected = k->total_count;
        k->observed_vs_expected /= k->expected;
        for ( i = 0; i < MAX_COMPRESSED_BASE_POS; ++i )
        {
            k->bp_expected[ i ] = data->bp_total_kmers[ i ];
            k->bp_expected[ i ] *= k->probability;
            k->bp_obs_vs_exp[ i ] = k->count[ i ];
            k->bp_obs_vs_exp[ i ] /= k->bp_expected[ i ];
            if ( k->bp_obs_vs_exp[ i ] > k->max_bp_obs_vs_exp )
            {
                k->max_bp_obs_vs_exp = k->bp_obs_vs_exp[ i ];
                k->max_bp_obs_vs_exp_at = i;
            }
        }
    }

    /* sort the kmer-idx-array by the observed-vs-expected value of the kmer */
    ksort ( data->kmer5_idx, NKMER5, sizeof( uint16_t ),
            kmer_sort_callback, data );
}
