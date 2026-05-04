#ifndef __SHARQ_VERSION_H__
#define __SHARQ_VERSION_H__

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

/**
 * @file version.h
 * @brief SharQ versioning
 *
 */

// VDB-4764: use the toolkit-wide version

// #define SHARQ_VERSION_MAJOR 1
// #define SHARQ_VERSION_MINOR 0
// #define SHARQ_VERSION_PATCH 3 /// Doxygen comments; defline_matcher groupSize refectoring

// #define SHARQ_VERSION to_string(SHARQ_VERSION_MAJOR) + "." + to_string(SHARQ_VERSION_MINOR) + "." + to_string(SHARQ_VERSION_PATCH)
#include <shared/toolkit.vers.h>
#define SHARQ_VERSION ( to_string(TOOLKIT_VERS >> 24) + "." + to_string((TOOLKIT_VERS >> 16) & 0x00ff) + "." + to_string(TOOLKIT_VERS & 0xffff) )

#include <klib/sra-release-version.h>
#include <kapp/version-hash.h> /* HASH_NCBI_VDB */
#include <kapp/sra-version-hash.h> /* HASH_SRA_TOOLS */

#endif
