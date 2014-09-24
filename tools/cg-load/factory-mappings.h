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
#ifndef _tools_cg_load_factory_mappings_h_
#define _tools_cg_load_factory_mappings_h_

#include <klib/defs.h>

struct CGFileType;
struct CGLoaderFile;

rc_t CC CGMappings13_Make(const struct CGFileType** self,
                          const struct CGLoaderFile* file);
rc_t CC CGMappings15_Make(const struct CGFileType** self,
                          const struct CGLoaderFile* file);
rc_t CC CGMappings20_Make(const struct CGFileType** self,
                          const struct CGLoaderFile* file);
rc_t CC CGMappings22_Make(const struct CGFileType** self,
                          const struct CGLoaderFile* file);

#endif /* _tools_cg_load_factory_mappings_h_ */
