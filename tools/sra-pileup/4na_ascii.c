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

#include <os-native.h>
#include <insdc/sra.h>

/***************************************
    N (0x4E)  n (0x6E)  <--> 0x0
    A (0x41)  a (0x61)  <--> 0x1
    C (0x43)  c (0x63)  <--> 0x2
    M (0x4D)  m (0x6D)  <--> 0x3
    G (0x47)  g (0x67)  <--> 0x4
    R (0x52)  r (0x72)  <--> 0x5
    S (0x53)  s (0x73)  <--> 0x6
    V (0x56)  v (0x76)  <--> 0x7
    T (0x54)  t (0x74)  <--> 0x8
    W (0x57)  w (0x77)  <--> 0x9
    Y (0x59)  y (0x79)  <--> 0xA
    H (0x48)  h (0x68)  <--> 0xB
    K (0x4B)  k (0x6B)  <--> 0xC
    D (0x44)  d (0x64)  <--> 0xD
    B (0x42)  b (0x62)  <--> 0xE
    N (0x4E)  n (0x6E)  <--> 0xF
***************************************/


static char _4na_2_ascii_tab[] =
{
/*  0x0  0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
    'N', 'A', 'C', 'M', 'G', 'R', 'S', 'V', 'T', 'W', 'Y', 'H', 'K', 'D', 'B', 'N',
    'n', 'a', 'c', 'm', 'g', 'r', 's', 'v', 't', 'w', 'y', 'h', 'k', 'd', 'b', 'n'
};


/***************************************
    N (0x4E)  n (0x6E)  <--> 0x0
    A (0x41)  a (0x61)  <--> 0x0
    C (0x43)  c (0x63)  <--> 0x1
    M (0x4D)  m (0x6D)  <--> 0x0
    G (0x47)  g (0x67)  <--> 0x2
    R (0x52)  r (0x72)  <--> 0x0
    S (0x53)  s (0x73)  <--> 0x0
    V (0x56)  v (0x76)  <--> 0x0
    T (0x54)  t (0x74)  <--> 0x3
    W (0x57)  w (0x77)  <--> 0x0
    Y (0x59)  y (0x79)  <--> 0x0
    H (0x48)  h (0x68)  <--> 0x0
    K (0x4B)  k (0x6B)  <--> 0x0
    D (0x44)  d (0x64)  <--> 0x0
    B (0x42)  b (0x62)  <--> 0x0
    N (0x4E)  n (0x6E)  <--> 0x0
***************************************/


static uint32_t _4na_2_index_tab[] =
{
/*  0x0  0x01 0x02 0x03 0x04 0x05 0x06 0x07 0x08 0x09 0x0A 0x0B 0x0C 0x0D 0x0E 0x0F */
     0,   0,   1,   0,   2,   0,  0,   0,   3,   0,   0,   0,   0,   0,   0,   0 
};


/* ------------------------------------------------------------------------------------- */

char _4na_to_ascii( INSDC_4na_bin c, bool reverse )
{
    return _4na_2_ascii_tab[ ( c & 0x0F ) | ( reverse ? 0x10 : 0 ) ];
}

uint32_t _4na_to_index( INSDC_4na_bin c )
{
    return _4na_2_index_tab[ ( c & 0x0F ) ];
}
