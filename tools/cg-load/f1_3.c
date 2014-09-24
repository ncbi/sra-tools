/*==============================================================================
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
*/

#include "factory-reads.h"
#include "factory-mappings.h"
#include "factory-evidence-intervals.h"
#include "factory-evidence-dnbs.h"

#include "file.h"

static const CGFileTypeFactory cg_ETypeXX_names[] = {
    { "READS", cg_eFileType_READS, CGReads13_Make },
    { "MAPPINGS", cg_eFileType_MAPPINGS, CGMappings13_Make },
    { "LIB-DNB", cg_eFileType_LIB_DNB, NULL },
    { "LIB-MATE-GAPS", cg_eFileType_LIB_MATE_GAPS, NULL },
    { "LIB-SEQDEP-GAPS", cg_eFileType_LIB_SEQDEP_GAPS, NULL },
    { "REFMETRICS", cg_eFileType_REFMETRICS, NULL },
    { "DBSNP-TO-CGI", cg_eFileType_DBSNP_TO_CGI, NULL },
    { "GENE-ANNOTATION", cg_eFileType_GENE_ANNOTATION, NULL },
    { "SUMMARY-REPORT", cg_eFileType_SUMMARY_REPORT, NULL },
    { "VAR-ANNOTATION", cg_eFileType_VAR_ANNOTATION, NULL },
    { "GENE-VAR-SUMMARY-REPORT", cg_eFileType_GENE_VAR_SUMMARY_REPORT, NULL },
    { "EVIDENCE-CORRELATION", cg_eFileType_EVIDENCE_CORRELATION, NULL },
    { "EVIDENCE-DNBS", cg_eFileType_EVIDENCE_DNBS, CGEvidenceDnbs13_Make },
    { "EVIDENCE-INTERVALS",
                cg_eFileType_EVIDENCE_INTERVALS, CGEvidenceIntervals13_Make },
    { "COVERAGE-DISTRIBUTION", cg_eFileType_COVERAGE_DISTRIBUTION, NULL },
    { "COVERAGE-BY-GC", cg_eFileType_COVERAGE_BY_GC, NULL },
    { "DEPTH-OF-COVERAGE", cg_eFileType_DEPTH_OF_COVERAGE, NULL },
    { "INDEL-LENGTH-CODING", cg_eFileType_INDEL_LENGTH_CODING, NULL },
    { "INDEL-LENGTH", cg_eFileType_INDEL_LENGTH, NULL },
    { "SUBSTITUTION-LENGTH-CODING",
                cg_eFileType_SUBSTITUTION_LENGTH_CODING, NULL },
    { "SUBSTITUTION-LENGTH", cg_eFileType_SUBSTITUTION_LENGTH, NULL },
    { "CNV_SEGMENTS", cg_eFileType_CNV_SEGMENTS, NULL },
    { "CNV-SEGMENTS", cg_eFileType_CNV_SEGMENTS, NULL },
    { "TUMOR_CNV_SEGMENTS", cg_eFileType_TUMOR_CNV_SEGMENTS, NULL },
    { "CNV_DETAILS_SCORES", cg_eFileType_CNV_DETAILS_SCORES, NULL },
    { "CNV-DETAIL-SCORES", cg_eFileType_CNV_DETAILS_SCORES, NULL },
    { "TUMOR_DETAILS_SCORES", cg_eFileType_TUMOR_DETAILS_SCORES, NULL },
    { "JUNCTIONS", cg_eFileType_JUNCTIONS, NULL },
    { "JUNCTION-DNBS", cg_eFileType_JUNCTION_DNBS, NULL },
    /* undocumented */
    { "VAR-OLPL", cg_eFileType_VAR_OLPL, NULL }
    
};

rc_t CGFile13_Make(const CGFileType** self,
    const char* type, const CGLoaderFile* file)
{
    return CGLoaderFileMakeCGFileType(file, type,
        cg_ETypeXX_names, sizeof cg_ETypeXX_names / sizeof cg_ETypeXX_names[0],
        self);
}
