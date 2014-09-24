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

#ifndef _h_4na_ascii_
#define _h_4na_ascii_

#ifdef __cplusplus
extern "C" {
#endif

#include <os-native.h>
#include <insdc/sra.h>

char _4na_to_ascii( INSDC_4na_bin c, bool reverse );
uint32_t _4na_to_index( INSDC_4na_bin c );

#ifdef __cplusplus
}
#endif

#endif /*  _h_4na_ascii_ */
