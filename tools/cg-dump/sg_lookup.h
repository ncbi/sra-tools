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
#ifndef _h_sg_lookup_
#define _h_sg_lookup_

#ifdef __cplusplus
extern "C" {
#endif

#include <klib/rc.h>
#include <klib/text.h>
#include <klib/container.h>

#include <vdb/database.h>

struct sg_lookup;

typedef struct sg
{
    BSTNode node;

    const String name;
    const String field_size;
    const String lib;
    const String sample;
} sg;


rc_t make_sg_lookup( struct sg_lookup ** self );

rc_t parse_sg_lookup( struct sg_lookup * self, const VDatabase * db );

rc_t perform_sg_lookup( struct sg_lookup * self, const String * name, sg ** entry );

rc_t destroy_sg_lookup( struct sg_lookup * self );


#ifdef __cplusplus
}
#endif

#endif
