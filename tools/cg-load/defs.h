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
#ifndef _tools_cg_load_defs_h_
#define _tools_cg_load_defs_h_

/* buffers and sizes */

/*
#define CG_CHROMOSOME_NAME (8) make if fit NC_003977 */
#define CG_CHROMOSOME_NAME (10)

#define CG_SLIDE (32)
#define CG_LANE (8)

#define CG_READS_NREADS (2)
#define CG_READS_SPOT_LEN (70)
#define CG_READS_NGAPS (3)
#define CG_MAPPINGS_MAX (2048)
#define CG_EVDNC_PLOIDY (3)
#define CG_EVDNC_INTERVALID_LEN (32)
#define CG_EVDNC_SPOT_LEN (10 * 1024)
#define CG_TAG_LFR_DATA_LEN (10)

/* CG_EVDNC_ALLELE_NUM is 3 in v.1.5; 4 in v.2 */
#define CG_EVDNC_ALLELE_NUM (4)

#define CG_EVDNC_ALLELE_LEN (10 * 1024)
#define CG_EVDNC_ALLELE_CIGAR_LEN (10 * 1024)

typedef char CGFIELD_LIBRARY_TYPE;
typedef char CGFIELD_SAMPLE_TYPE;
typedef char CGFIELD_SLIDE_TYPE;
typedef char CGFIELD_LANE_TYPE;
typedef char CGFIELD_CHROMOSOME_TYPE;
typedef char CGFIELD_ASSEMBLY_ID_TYPE;
typedef char CGFIELD_SOFTWARE_VERSION_TYPE;
typedef char CGFIELD_DBSNP_BUILD_TYPE;
typedef char CGFIELD_COSMIC_TYPE;
typedef char CGFIELD_PFAM_DATE_TYPE;
typedef char CGFIELD_MIRBASE_VERSION_TYPE;
typedef char CGFIELD_DGV_VERSION_TYPE;
typedef char CGFIELD_GENERATED_AT_TYPE;
typedef char CGFIELD_GENERATED_BY_TYPE;
typedef char CGFIELD_GENE_ANNOTATIONS_TYPE;
typedef char CGFIELD_GENOME_REFERENCE_TYPE;
typedef uint32_t CGFIELD_BATCH_FILE_NUMBER_TYPE;
typedef uint64_t CGFIELD_BATCH_OFFSET_TYPE;
typedef uint32_t CGFIELD_FIELD_SIZE_TYPE;
typedef uint32_t CGFIELD_MAX_PLOIDY_TYPE;
typedef uint16_t CGFIELD_WINDOW_SHIFT_TYPE;
typedef uint16_t CGFIELD_WINDOW_WIDTH_TYPE;
typedef uint16_t CGFIELD_NUMBER_LEVELS_TYPE;
typedef uint16_t CGFIELD_MEAN_LEVEL_X_TYPE;
typedef uint16_t CGFIELD_WELL_ID;

typedef enum CG_EFileType_enum {
    cg_eFileType_Unknown = 0,
    cg_eFileType_READS = 1,
    cg_eFileType_MAPPINGS,
    cg_eFileType_LIB_DNB,
    cg_eFileType_LIB_MATE_GAPS,
    cg_eFileType_LIB_SEQDEP_GAPS,
    cg_eFileType_REFMETRICS,
    cg_eFileType_IDENTIFIER_MAPPING,
    cg_eFileType_DBSNP_TO_CGI,
    cg_eFileType_GENE_ANNOTATION,
    cg_eFileType_SUMMARY_REPORT,
    cg_eFileType_VAR_ANNOTATION,
    cg_eFileType_GENE_VAR_SUMMARY_REPORT,
    cg_eFileType_EVIDENCE_CORRELATION,
    cg_eFileType_EVIDENCE_DNBS,
    cg_eFileType_EVIDENCE_INTERVALS,
    cg_eFileType_COVERAGE_DISTRIBUTION,
    cg_eFileType_COVERAGE_BY_GC,
    cg_eFileType_DEPTH_OF_COVERAGE,
    cg_eFileType_INDEL_LENGTH_CODING,
    cg_eFileType_INDEL_LENGTH,
    cg_eFileType_SUBSTITUTION_LENGTH_CODING,
    cg_eFileType_SUBSTITUTION_LENGTH,
    cg_eFileType_CNV_SEGMENTS,
    cg_eFileType_TUMOR_CNV_SEGMENTS,
    cg_eFileType_CNV_DETAILS_SCORES,
    cg_eFileType_TUMOR_DETAILS_SCORES,
    cg_eFileType_NONDIPLOID_SOMATIC_CNV_SEGMENTS,
    cg_eFileType_NONDIPLOID_SOMATIC_CNV_DETAILS,
    cg_eFileType_JUNCTIONS,
    cg_eFileType_JUNCTION_DNBS,
    cg_eFileType_SV_EVENTS,
    cg_eFileType_VAR_OLPL,
    cg_eFileType_MEI,
    cg_eFileType_TUMOR_CNV_DETAILS,
    cg_eFileType_DIPLOID_SOMATIC_CNV_SEGMENTS,
    cg_eFileType_COVERAGE_DISTRIBUTION_CODING,
    cg_eFileType_COVERAGE_BY_GC_CODING,
    cg_eFileType_LIB_SMALL_GAPS_ROLLUP,
    cg_eFileType_DIPLOID_SOMATIC_CNV_DETAIL_SCORES,

    cg_eFileType_TAG_LFR,
    cg_eFileType_Last
} CG_EFileType;

typedef enum CG_EReadsFlags_enum {
    cg_eLeftHalfDnbNoMatches = 0x01,
    cg_eLeftHalfDnbMapOverflow = 0x02,
    cg_eRightHalfDnbNoMatches = 0x04,
    cg_eRightHalfDnbMapOverflow = 0x08
} CG_EReadsFlags;

typedef enum CG_EMappingsFlags_enum {
    cg_eLastDNBRecord = 0x01,
    cg_eLeftHalfDnbMap = 0x00,
    cg_eRightHalfDnbMap = 0x02,
    cg_eFwdDnbStrand = 0x00,
    cg_eRevDnbStrand = 0x04
} CG_EMappingsFlags;

#endif /* _tools_cg_load_defs_h_ */
