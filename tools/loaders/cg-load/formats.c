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


#include "defs.h" /* CG_FORMAT_2_5 */
#include "formats.h" /* get_cg_reads_ngaps */

#include <assert.h>


/* CG native format files have some changes since version 2.5 */


uint32_t get_cg_reads_ngaps(uint32_t reads_format) {
    assert(reads_format);

    return reads_format < CG_FORMAT_2_5 ? 3 : 2;
}

/* IN FORMAT VERSION 2.5 SPOT LENGTH WAS CHANGES FROM 70 to 60 */
uint32_t get_cg_read_len(uint32_t reads_format) {
    uint32_t spot_len = CG_READS15_SPOT_LEN;

    assert(reads_format);

    if (reads_format >= CG_FORMAT_2_5) {
        spot_len = CG_READS25_SPOT_LEN;
    }

    return spot_len / 2;
}
