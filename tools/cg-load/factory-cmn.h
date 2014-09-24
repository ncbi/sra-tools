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
#ifndef _tools_cg_load_factory_cmh_h_
#define _tools_cg_load_factory_cmh_h_

#include <klib/defs.h>
#include "defs.h"

typedef CGFIELD_LIBRARY_TYPE CGFIELD15_LIBRARY[32];
typedef CGFIELD_SAMPLE_TYPE CGFIELD15_SAMPLE[32];
typedef CGFIELD_SLIDE_TYPE CGFIELD15_SLIDE[CG_SLIDE];
typedef CGFIELD_LANE_TYPE CGFIELD15_LANE[CG_LANE];
typedef CGFIELD_CHROMOSOME_TYPE CGFIELD15_CHROMOSOME[CG_CHROMOSOME_NAME];
typedef CGFIELD_ASSEMBLY_ID_TYPE CGFIELD15_ASSEMBLY_ID[44];

/* VDB-951: make it process invalid #SOFTWARE_VERSION value
typedef CGFIELD_SOFTWARE_VERSION_TYPE CGFIELD15_SOFTWARE_VERSION[16];*/
typedef CGFIELD_SOFTWARE_VERSION_TYPE CGFIELD15_SOFTWARE_VERSION[38];

typedef CGFIELD_DBSNP_BUILD_TYPE CGFIELD15_DBSNP_BUILD[8];
typedef CGFIELD_COSMIC_TYPE CGFIELD15_COSMIC[8];
typedef CGFIELD_PFAM_DATE_TYPE CGFIELD15_PFAM_DATE[16];
typedef CGFIELD_MIRBASE_VERSION_TYPE CGFIELD15_MIRBASE_VERSION[8];
typedef CGFIELD_DGV_VERSION_TYPE CGFIELD15_DGV_VERSION[16];
typedef CGFIELD_GENERATED_AT_TYPE CGFIELD15_GENERATED_AT[32];
typedef CGFIELD_GENERATED_BY_TYPE CGFIELD15_GENERATED_BY[32];
typedef CGFIELD_GENE_ANNOTATIONS_TYPE CGFIELD15_GENE_ANNOTATIONS[16];
typedef CGFIELD_GENOME_REFERENCE_TYPE CGFIELD15_GENOME_REFERENCE[16];
typedef CGFIELD_BATCH_FILE_NUMBER_TYPE CGFIELD15_BATCH_FILE_NUMBER;
typedef CGFIELD_BATCH_OFFSET_TYPE CGFIELD15_BATCH_OFFSET;
typedef CGFIELD_FIELD_SIZE_TYPE CGFIELD15_FIELD_SIZE;
typedef CGFIELD_MAX_PLOIDY_TYPE CGFIELD15_MAX_PLOIDY;
typedef CGFIELD_WINDOW_SHIFT_TYPE CGFIELD15_WINDOW_SHIFT;
typedef CGFIELD_WINDOW_WIDTH_TYPE CGFIELD15_WINDOW_WIDTH;
typedef CGFIELD_NUMBER_LEVELS_TYPE CGFIELD15_NUMBER_LEVELS;
typedef CGFIELD_MEAN_LEVEL_X_TYPE CGFIELD15_MEAN_LEVEL_X;

#endif /* _tools_cg_load_factory_cmh_h_ */
