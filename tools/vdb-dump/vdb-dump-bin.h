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

#ifndef _h_vdb_dump_bin_
#define _h_vdb_dump_bin_

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

rc_t vdi_dump_opened_table( const p_dump_context ctx, const VTable *my_table );
rc_t vdi_bin_phase( const p_dump_context ctx, Args * args );

rc_t find_slice( const p_dump_context ctx, Args * args );

#ifdef __cplusplus
}
#endif

#endif