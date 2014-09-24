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
*
*/
#ifndef _sra_tools_run_xml_
#define _sra_tools_run_xml_

#ifndef _h_klib_container_
#include <klib/container.h>
#endif

#include "common-xml.h"

typedef enum ERunFileType_enum {
    rft_Unknown = 1,
    rft_Fastq,
    rft_SRF,
    rft_SFF,
    rft_HelicosNative,
    rft_IlluminaNativeSeq,
    rft_IlluminaNativePrb,
    rft_IlluminaNativeInt,
    rft_IlluminaNativeQseq,
    rft_ABINativeCSFasta,
    rft_ABINativeQuality,
    rft_PacBio_HDF5
} ERunFileType;

typedef struct DataBlockFileAttr_struct {
    ERunFileType filetype;
    ExperimentQualityType quality_scoring_system;
    ExperimentQualityEncoding quality_encoding;
    uint8_t ascii_offset;
} DataBlockFileAttr;

typedef struct DataBlockFile_struct {
    DataBlockFileAttr attr;
    char* filename;
    uint8_t* md5_digest;
    char* cc_xml;
    char* cc_path;
} DataBlockFile;

typedef struct DataBlock_struct {
    char* name;
    char* member_name;
    int64_t sector;
    int64_t region;
    uint32_t serial;
    uint32_t files_count;
    DataBlockFile* files;
} DataBlock;

typedef struct RunXML {
    uint32_t datablock_count;
    DataBlock* datablocks;
    /* known/used run attribuites */
    RunAttributes attributes;
} RunXML;

void DataBlockFile_Whack(DataBlockFile* self);

void DataBlock_Whack(DataBlock* self);

rc_t RunXML_Make(RunXML **rslt, const KXMLNode *RUN);

rc_t RunXML_Whack(const RunXML *cself);

#endif /* _sra_tools_run_xml_ */
