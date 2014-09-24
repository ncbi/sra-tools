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
#ifndef _h_pl_basecalls_cmn_
#define _h_pl_basecalls_cmn_

#ifdef __cplusplus
extern "C" {
#endif

#include "pl-tools.h"
#include "pl-zmw.h"
#include <klib/rc.h>

typedef struct BaseCalls_cmn
{
    zmw_tab zmw;
    af_data Basecall;
    af_data QualityValue;
    af_data DeletionQV;
    af_data DeletionTag;
    af_data InsertionQV;
    af_data SubstitutionQV;
    af_data SubstitutionTag;
} BaseCalls_cmn;


void init_BaseCalls_cmn( BaseCalls_cmn *tab );
void close_BaseCalls_cmn( BaseCalls_cmn *tab );
rc_t open_BaseCalls_cmn( const KDirectory *hdf5_dir, BaseCalls_cmn *tab,
                         const bool num_passes, const char * path,
                         bool cache_content, bool supress_err_msg );

#ifdef __cplusplus
}
#endif

#endif
