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
#ifndef _sra_load_experiment_xml_
#define _sra_load_experiment_xml_

#include "common-xml.h"

typedef struct ProcessingXML_struct {
    char* quality_scorer;
    ExperimentQualityType quality_type;
    uint8_t quality_offset;
    ExperimentBarcodeRule barcode_rule;

} ProcessingXML;

typedef struct ExperimentXML ExperimentXML;

/* Make spot descriptor information structure based on XML file
 */
rc_t Experiment_Make(const ExperimentXML** self, const KXMLDoc* doc, RunAttributes* attr);

rc_t Experiment_GetPlatform(const ExperimentXML* cself, const PlatformXML** platform);

rc_t Experiment_GetProcessing(const ExperimentXML* cself, const ProcessingXML** processing);

rc_t Experiment_GetReadNumber(const ExperimentXML* cself, uint8_t* nreads);

rc_t Experiment_GetSpotLength(const ExperimentXML* cself, uint32_t* spot_length);

/* ZERO-based read_id here! */
rc_t Experiment_GetRead(const ExperimentXML* cself, uint8_t read_id, const ReadSpecXML_read** read_spec);

/* find in read (EXPECTED_BASECALL_TABLE type ONLY) basecall and/or member_name by bases or mamber_name
 * on of basecall or member_name must be not NULL
 */
rc_t Experiment_FindReadInTable(const ExperimentXML* cself, uint8_t read_id, const char* key, const char** basecall, const char** member_name);

rc_t Experiment_HasPool(const ExperimentXML* cself, bool* has_pool);

rc_t Experiment_ReadSegDefault(const ExperimentXML* self, SRASegment* seg);

rc_t Experiment_MemberSeg(const ExperimentXML* self,
                          const char* const file_member_name, const char* const data_block_member_name,
                          uint32_t nbases, const char* bases,
                          SRASegment* seg, const char** const new_member_name);

rc_t Experiment_MemberSegSimple(const ExperimentXML* self,
                                const char* const file_member_name, const char* const data_block_member_name,
                                const char** const new_member_name);


void Experiment_Whack(const ExperimentXML* cself);

#endif /* _sra_load_experiment_xml_ */
