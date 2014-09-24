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
#ifndef _sra_load_common_xml_
#define _sra_load_common_xml_

#include <kxml/xml.h>
#include <search/grep.h>
#include <sra/sradb.h>

#define SRALOAD_MAX_READS 32

rc_t XMLNode_get_strnode(const KXMLNode* node, const char* child, bool optional, char** value);

typedef struct PlatformXML_struct {
    SRAPlatforms id;
    union {
        struct {
            char* key_sequence; /* optional */
            char* flow_sequence; /* optional */
            uint32_t flow_count; /* optional */
        } ls454;

        struct {
            char* key_sequence; /* optional */
            char* flow_sequence; /* optional */
            uint32_t flow_count; /* optional */
        } ion_torrent;

        struct {
            char* flow_sequence; /* optional */
            uint32_t flow_count; /* optional */
        } helicos;
    } param;
} PlatformXML;

rc_t PlatformXML_Make(const PlatformXML** cself, const KXMLNode* node, uint32_t* spot_length);

void PlatformXML_Whack(const PlatformXML* cself);

typedef struct ReadSpecXML_read_BASECALL_struct {
    char* basecall;
    char* read_group_tag;
    uint32_t min_match;
    uint32_t max_mismatch;
    enum {
        match_edge_Full = 1,
        match_edge_Start,
        match_edge_End
    } match_edge;
    Agrep* agrep;
} ReadSpecXML_read_BASECALL;

typedef struct ReadSpecXML_read_BASECALL_TABLE_struct {
    uint32_t default_length;
    uint32_t base_coord;
    ReadSpecXML_read_BASECALL* table;
    uint32_t size; /* allocated structures qty */
    uint32_t count; /* used structures qty */
    bool pooled; /* true disables search if member is present from run */
    uint16_t match_start; /* length of longest bc with match_edge="start" */
    uint16_t match_end; /* length of longest bc with match_edge="end" */
} ReadSpecXML_read_BASECALL_TABLE;

typedef struct ReadSpecXML_read_struct {
    char* read_label; /* asciiz */
    SRAReadTypes read_class;
    enum {
        rdsp_Forward_rt = 1,
        rdsp_Reverse_rt,
        rdsp_Adapter_rt,
        rdsp_Primer_rt,
        rdsp_Linker_rt,
        rdsp_BarCode_rt,
        rdsp_Other_rt
    } read_type;
    
    enum {
        /* order is important !!! */
        rdsp_FIXED_BRACKET_ct = 1, /* special fixed size type */
        rdsp_RelativeOrder_ct,
        rdsp_BaseCoord_ct,
        rdsp_CycleCoord_ct,
        rdsp_ExpectedBaseCall_ct,
        rdsp_ExpectedBaseCallTable_ct
    } coord_type;

    union {
        struct {
            int16_t follows;
            int16_t precedes;
        } relative_order;
        /* starting position for *_COORD types */
        int16_t start_coord;
        /* EXPECTED_BASECALL is a table of 1 element, unless IUPAC is used in values */
        ReadSpecXML_read_BASECALL_TABLE expected_basecalls;
    } coord;
} ReadSpecXML_read;

typedef struct ReadSpecXML_struct {
    uint32_t nreads;
    ReadSpecXML_read spec[SRALOAD_MAX_READS + 2];
    ReadSpecXML_read* reads;
} ReadSpecXML;

rc_t ReadSpecXML_Make(const ReadSpecXML** cself, const KXMLNode* node, const char* path, uint32_t* spot_length);

void ReadSpecXML_Whack(const ReadSpecXML* cself);

typedef enum {
    eExperimentQualityType_Undefined = 0,
    eExperimentQualityType_Phred,
    eExperimentQualityType_LogOdds,
    eExperimentQualityType_Other
} ExperimentQualityType;

typedef enum {
    eExperimentQualityEncoding_Undefined = 0,
    eExperimentQualityEncoding_Ascii,
    eExperimentQualityEncoding_Decimal,
    eExperimentQualityEncoding_Hexadecimal
} ExperimentQualityEncoding;

/*

The value 'default' is same as '' or NULL in terms coding and represents default group (member) in SRA.

We use value of these data elements as SPOT_GROUP (barcode, member, etc) column value.
Rules (whichever is present in order of importance):

1.	Run.xml: /RUN/DATA_BLOCK/ attribute member_name. (must have POOL to set read lengths).
2.	Barcode in spot name in file data. (if not explicitly chosen [below] must have pool, otherwise fail)(must have POOL to set read lengths).
3.	Experiment.xml: /EXPERIMENT/DESIGN/SAMPLE_DESCRIPTOR/POOL elements MEMBER. Match subsequences to determine SPOT_GROUP value.
4.	No SPOT_GROUP column

Than <RUN_ATTRIBUTE> with <TAG> case-insensitive value of ‘read_name_barcode_proc_directive’ comes in with case-insensitive <VALUE> values:

1.	RUN_ATTRIBUTE not present at all – use rules above.
2.	'interpret_as_spotgroup' (use_file_spot_name) – use data from file’s spot names. Force rule #2. (new addition)
3.	‘use_table_in_experiment’ – use POOL table from experiment.xml. Force rule #3.
4.	‘ignore’ – do not write anything to SPOT_GROUP column. Force rule #4

*/

typedef enum {
    eBarcodeRule_not_set = 0,
    eBarcodeRule_use_file_spot_name,
    eBarcodeRule_use_table_in_experiment,
    eBarcodeRule_ignore_barcode
} ExperimentBarcodeRule;

typedef struct RunAttributes_struct {
    uint32_t spot_length;
    const PlatformXML* platform;
    const ReadSpecXML* reads;
    ExperimentBarcodeRule barcode_rule;
    ExperimentQualityType quality_type;
    uint8_t quality_offset;
} RunAttributes;

#endif /* _sra_load_common_xml_ */
