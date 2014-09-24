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
#include <klib/rc.h>
#include <klib/log.h>
#include <os-native.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include "run-xml.h"

void DataBlockFile_Whack(DataBlockFile* self)
{
    if( self ) {
        free(self->filename);
        free(self->md5_digest);
        free(self->cc_xml);
        free(self->cc_path);
    }
}

void DataBlock_Whack(DataBlock* self)
{
    if( self ) {
        uint32_t i;
        for(i = 0; i < self->files_count; i++) {
            DataBlockFile_Whack(&self->files[i]);
        }
        free(self->files);
        free(self->name);
        free(self->member_name);
    }
}

rc_t RunXML_Whack(const RunXML *cself)
{
    if( cself ) {
        RunXML* self = (RunXML*)cself;
        uint32_t i = 0;
        for(i = 0; i < self->datablock_count; i++) {
            DataBlock_Whack(&self->datablocks[i]);
        }
        free(self->datablocks);
        PlatformXML_Whack(self->attributes.platform);
        ReadSpecXML_Whack(self->attributes.reads);
        free(self);
    }
    return 0;
}

static
const struct {
    const char* name;
    ERunFileType type;
} file_types[] = {
    /* generic types giving us no useful information about file */
    { "tab", rft_Unknown },
    { "454_native", rft_Unknown },
    { "454_native_seq", rft_Unknown },
    { "454_native_qual", rft_Unknown },
    { "Illumina_native", rft_Unknown },
    { "Illumina_native_fastq", rft_Unknown },
    { "Illumina_native_scarf", rft_Unknown },
    { "SOLiD_native", rft_Unknown },
    { "bam", rft_Unknown },
    { "sra", rft_Unknown },
    { "kar", rft_Unknown },
    /* more specific information about file content */
    { "fastq", rft_Fastq },
    { "srf", rft_SRF },
    { "sff", rft_SFF },
    { "Helicos_native", rft_HelicosNative },
    { "Illumina_native_seq", rft_IlluminaNativeSeq },
    { "Illumina_native_prb", rft_IlluminaNativePrb },
    { "Illumina_native_int", rft_IlluminaNativeInt },
    { "Illumina_native_qseq", rft_IlluminaNativeQseq },
    { "SOLiD_native_csfasta", rft_ABINativeCSFasta },
    { "SOLiD_native_qual", rft_ABINativeQuality },
    { "PacBio_HDF5", rft_PacBio_HDF5 }
};

static
rc_t parse_FILE(const KXMLNode* FILE, DataBlockFile* file)
{
    rc_t rc = 0;
    char* filetype = NULL, *checksum_method = NULL, *checksum = NULL;
    char* quality_scoring_system = NULL, *quality_encoding = NULL, *ascii_offset = NULL;

    if( (rc = KXMLNodeReadAttrCStr(FILE, "filename", &file->filename, NULL)) != 0 ||
        file->filename == NULL || file->filename[0] == '\0' ) {
        LOGERR(klogErr, rc, "FILE @filename");
    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "filetype", &filetype, NULL)) != 0 ||
        filetype == NULL || filetype[0] == '\0' ) {
        rc = rc ? rc : RC(rcExe, rcStorage, rcConstructing, rcTag, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @filetype", PLOG_S(file), file->filename));

/* <!-- custom pipeline attributes, not from schema! */
    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "upload_id", &file->cc_xml, NULL)) != 0 &&
               !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @upload_id", PLOG_S(file), file->filename));
    } else if( file->cc_xml != NULL && file->cc_xml[0] == '\0' ) {
        rc = RC(rcExe, rcStorage, rcConstructing, rcTag, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @upload_id", PLOG_S(file), file->filename));

    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "cc_path", &file->cc_path, NULL)) != 0 &&
                !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @cc_path", PLOG_S(file), file->filename));
    } else if( ((file->cc_path == NULL || file->cc_path[0] == '\0') && file->cc_xml != NULL) || 
                (file->cc_path != NULL && file->cc_xml == NULL) ) {
        rc = RC(rcExe, rcStorage, rcConstructing, rcTag, file->cc_xml ? rcEmpty : rcUnexpected);
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @cc_path", PLOG_S(file), file->filename));
/* --> */

    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "quality_scoring_system", &quality_scoring_system, NULL)) != 0 &&
                !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @quality_scoring_system", PLOG_S(file), file->filename));
    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "quality_encoding", &quality_encoding, NULL)) != 0 &&
                !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @quality_encoding", PLOG_S(file), file->filename));
    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "ascii_offset", &ascii_offset, NULL)) != 0 &&
                !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @ascii_offset", PLOG_S(file), file->filename));

    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "checksum_method", &checksum_method, NULL)) != 0 &&
                !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @checksum_method", PLOG_S(file), file->filename));
    } else if( (rc = KXMLNodeReadAttrCStr(FILE, "checksum", &checksum, NULL)) != 0 &&
                !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @checksum", PLOG_S(file), file->filename));
    } else if( checksum_method != NULL && strcasecmp(checksum_method, "MD5") != 0 ) {
        rc = RC(rcExe, rcStorage, rcConstructing, rcData, rcInvalid);
        PLOGERR(klogErr, (klogErr, rc, "FILE $(file) @checksum_method", PLOG_S(file), file->filename));
    } else {
        rc_t rc1 = 0, rc2 = 0, rc3 = 0, rc4 = 0;
        if( quality_scoring_system != NULL ) {
            if( quality_encoding == NULL ) {
                rc1 = RC(rcExe, rcStorage, rcConstructing, rcParam, rcIncomplete);
                PLOGERR(klogErr, (klogErr, rc1, "FILE $(file) @quality_encoding is required if @quality_scoring_system is specified",
                    PLOG_S(file), file->filename));
            } else if( strcmp(quality_scoring_system, "phred") == 0 ) {
                file->attr.quality_scoring_system = eExperimentQualityType_Phred;
            } else if( strcmp(quality_scoring_system, "log-odds") == 0 ) {
                file->attr.quality_scoring_system = eExperimentQualityType_LogOdds;
            } else {
                rc1 = RC(rcExe, rcStorage, rcConstructing, rcAttr, rcOutofrange);
                PLOGERR(klogErr, (klogErr, rc1, "FILE $(file) @quality_scoring_system", PLOG_S(file), file->filename));
            }
        }
        if( quality_encoding != NULL ) {
            if( quality_scoring_system == NULL ) {
                rc2 = RC(rcExe, rcStorage, rcConstructing, rcParam, rcIncomplete);
                PLOGERR(klogErr, (klogErr, rc2, "FILE $(file) @quality_scoring_system is required if @quality_encoding is specified",
                    PLOG_S(file), file->filename));
            } else if( strcmp(quality_encoding, "ascii") == 0 ) {
                file->attr.quality_encoding = eExperimentQualityEncoding_Ascii;
                if( ascii_offset == NULL ) {
                    rc2 = RC(rcExe, rcStorage, rcConstructing, rcParam, rcIncomplete);
                    PLOGERR(klogErr, (klogErr, rc2, "FILE $(file) @ascii_offset is required if @quality_encoding is 'ascii'",
                        PLOG_S(file), file->filename));
                } else if( strcmp(ascii_offset, "!") == 0 || strcmp(ascii_offset, "@") == 0 ) {
                    file->attr.ascii_offset = ascii_offset[0];
                } else {
                    rc2 = RC(rcExe, rcStorage, rcConstructing, rcAttr, rcOutofrange);
                    PLOGERR(klogErr, (klogErr, rc2, "FILE $(file) @ascii_offset", PLOG_S(file), file->filename));
                }
            } else if( strcmp(quality_encoding, "decimal") == 0 ) {
                file->attr.quality_encoding = eExperimentQualityEncoding_Decimal;
            } else if( strcmp(quality_encoding, "hexadecimal") == 0 ) {
                file->attr.quality_encoding = eExperimentQualityEncoding_Hexadecimal;
            } else {
                rc2 = RC(rcExe, rcStorage, rcConstructing, rcAttr, rcOutofrange);
                PLOGERR(klogErr, (klogErr, rc2, "FILE $(file) @quality_encoding", PLOG_S(file), file->filename));
            }
        }
        if( ascii_offset != NULL ) {
            if( file->attr.quality_encoding != eExperimentQualityEncoding_Ascii ) {
                PLOGMSG(klogWarn, (klogWarn, "FILE $(file) @ascii_offset ignored since @quality_encoding is $(reason)",
                    PLOG_2(PLOG_S(reason),PLOG_S(file)),
                    file->attr.quality_encoding == eExperimentQualityEncoding_Undefined ? "undefined" : "not ascii", file->filename));
            }
        }
        if( checksum_method == NULL && checksum != NULL ) {
            PLOGMSG(klogWarn, (klogWarn, "FILE $(file) @checksum ignored since @checksum_method is not defined", PLOG_S(file), file->filename));
            free(checksum);
            checksum = NULL;
        }
        if( checksum != NULL ) {
            if( strlen(checksum) != (16 * 2) ) {
                /* MD5 sum is 16 bytes in hex format (2 hex digits per byte) */
                rc3 = RC(rcExe, rcStorage, rcConstructing, rcData, rcUnrecognized);
                PLOGERR(klogErr, (klogErr, rc3, "FILE $(file) MD5 @checksum", PLOG_S(file), file->filename));
            } else if( (file->md5_digest = malloc(16 * sizeof(uint8_t))) == NULL ) {
                rc3 = RC(rcExe, rcStorage, rcConstructing, rcMemory, rcExhausted);
                PLOGERR(klogErr, (klogErr, rc3, "FILE $(file) MD5 @checksum allocation", PLOG_S(file), file->filename));
            } else {
                int i = 0;
                char digit[3];
                unsigned int xdig;
                
                digit[2] = '\0';
                for(i = 0; rc3 == 0 && i < 16; i++) {
                    digit[0] = tolower(checksum[2 * i]);
                    digit[1] = tolower(checksum[2 * i + 1]);
                    if(!isxdigit(digit[0]) || !isxdigit(digit[1])) {
                        rc3 = RC(rcExe, rcStorage, rcConstructing, rcData, rcInvalid);
                        PLOGERR(klogErr, (klogErr, rc3, "FILE $(file) MD5 @checksum $(digits)",
                                 PLOG_2(PLOG_S(file),PLOG_S(digits)), file->filename, digit));
                    } else {
                        sscanf(digit, "%x", &xdig);
                        file->md5_digest[i] = xdig;
                    }
                }
            }
        }
        if( filetype != NULL ) {
            int i;
            for(i = 0; i < sizeof(file_types) / sizeof(file_types[0]); i++) {
                if( strcmp(filetype, file_types[i].name) == 0 ) {
                    file->attr.filetype = file_types[i].type;
                    break;
                }
            }
            if( file->attr.filetype == 0 ) {
                rc4 = RC(rcExe, rcStorage, rcConstructing, rcAttr, rcOutofrange);
                PLOGERR(klogErr, (klogErr, rc4, "FILE $(file) @filetype '$(type)'",
                         PLOG_2(PLOG_S(file),PLOG_S(type)), file->filename, filetype));
            }
        }
        rc = rc1 ? rc1 : (rc2 ? rc2 : (rc3 ? rc3 : rc4));
    }
    free(filetype);
    free(checksum_method);
    free(checksum);
    free(quality_scoring_system);
    free(quality_encoding);
    free(ascii_offset);
    return rc;
}

static
rc_t parse_DATA_BLOCK(const KXMLNode* DATA_BLOCK, DataBlock* datablock)
{
    rc_t rc = 0;
    const KXMLNodeset* FILES;

    rc = KXMLNodeReadAttrCStr(DATA_BLOCK, "name", &datablock->name, NULL);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "DATA_BLOCK @name");
        return rc;
    }
    rc = KXMLNodeReadAttrCStr(DATA_BLOCK, "member_name", &datablock->member_name, NULL);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "DATA_BLOCK @member_name");
        return rc;
    }
    if( datablock->member_name != NULL && strcasecmp(datablock->member_name, "default") == 0 ) {
        datablock->member_name[0] = '\0';
    }
    datablock->serial = ~0; /* file w/o serial pushed to end of list */
    rc = KXMLNodeReadAttrAsU32(DATA_BLOCK, "serial", &datablock->serial);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "DATA_BLOCK @serial");
        return rc;
    }
    datablock->sector = -1;
    rc = KXMLNodeReadAttrAsU64(DATA_BLOCK, "sector", (uint64_t*)&datablock->sector);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "DATA_BLOCK @sector");
        return rc;
    }
    datablock->region = -1;
    rc = KXMLNodeReadAttrAsU64(DATA_BLOCK, "region", (uint64_t*)&datablock->region);
    if( rc != 0 && !(GetRCObject(rc) == (enum RCObject)rcAttr && GetRCState(rc) == rcNotFound) ) {
        LOGERR(klogErr, rc, "DATA_BLOCK @region");
        return rc;
    }

    if( (rc = KXMLNodeOpenNodesetRead(DATA_BLOCK, &FILES, "FILES/FILE")) == 0) {
        if( (rc = KXMLNodesetCount(FILES, &datablock->files_count)) != 0) {
            LOGERR(klogErr, rc, "reading FILES/FILE node(s)");
        } else if( datablock->files_count < 1 ) {
            rc = RC(rcExe, rcStorage, rcConstructing, rcData, rcIncomplete);
            LOGERR(klogErr, rc, "FILES/FILE at least one node is required");
        } else if( (datablock->files = calloc(datablock->files_count, sizeof(*datablock->files))) == NULL ) {
                rc = RC(rcExe, rcStorage, rcConstructing, rcMemory, rcExhausted);
                LOGERR(klogErr, rc, "allocating FILES/FILE");
        } else {
            uint32_t i = 0;
            for(i = 0; rc == 0 && i < datablock->files_count; i++) {
                const KXMLNode *FILE = NULL;
                if( (rc = KXMLNodesetGetNodeRead(FILES, &FILE, i)) == 0 ) {
                    rc = parse_FILE(FILE, &datablock->files[i]);
                    KXMLNodeRelease(FILE);
                }
                if( rc != 0 ) {
                    PLOGERR(klogErr, (klogErr, rc, "reading FILES/FILE $(i)", PLOG_U32(i), i));
                }
            }
        }
        KXMLNodesetRelease(FILES);
    }
    return rc;
}

rc_t RunXML_Make(RunXML** rslt, const KXMLNode* RUN)
{
    rc_t rc = 0;
    const KXMLNodeset* nset = NULL;
    RunXML* obj = NULL;
    uint32_t p_spot_length = 0, r_spot_length = 0;

    if(rslt == NULL || RUN == NULL) {
        return RC(rcExe, rcStorage, rcConstructing, rcParam, rcNull);
    }
    *rslt = NULL;
    obj = calloc(1, sizeof(*obj));
    if( obj == NULL ) {
        rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
    } else {
        if( (rc = PlatformXML_Make(&obj->attributes.platform, RUN, &p_spot_length)) != 0 ) {
            if( GetRCState(rc) == rcNotFound ) {
                rc = 0;
            } else {
                LOGERR(klogErr, rc, "RUN/PLATFORM");
            }
        }
        if( rc == 0 && (rc = ReadSpecXML_Make(&obj->attributes.reads, RUN, "SPOT_DESCRIPTOR", &r_spot_length)) != 0 ) {
            if( GetRCState(rc) == rcNotFound ) {
                rc = 0;
            } else {
                LOGERR(klogErr, rc, "RUN/.../READ_SPEC");
            }
        }
        obj->attributes.spot_length = r_spot_length ? r_spot_length : p_spot_length;
        if( rc == 0 && p_spot_length != 0 && r_spot_length != 0 && p_spot_length != r_spot_length ) {
            PLOGMSG(klogWarn, (klogWarn,
                "in RUN sequence length (cycle count) in PLATFORM $(p) differ"
                " from SPOT_LENGTH $(l) in SPOT_DECODE_SPEC",
                PLOG_2(PLOG_U32(p),PLOG_U32(l)), p_spot_length, r_spot_length));
        }
    }
    if( rc == 0 ) {
        if( (rc = KXMLNodeOpenNodesetRead(RUN, &nset, "DATA_BLOCK")) == 0) {
            if( (rc = KXMLNodesetCount(nset, &obj->datablock_count)) == 0 && obj->datablock_count > 0 ) {
                obj->datablocks = calloc(obj->datablock_count, sizeof(*obj->datablocks));
                if( obj->datablocks == NULL ) {
                    rc = RC(rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted);
                } else {
                    uint32_t i = 0;
                    for(i = 0; rc == 0 && i < obj->datablock_count; i++) {
                        const KXMLNode *DATA_BLOCK = NULL;
                        if( (rc = KXMLNodesetGetNodeRead(nset, &DATA_BLOCK, i)) == 0 ) {
                            rc = parse_DATA_BLOCK(DATA_BLOCK, &obj->datablocks[i]);
                            KXMLNodeRelease(DATA_BLOCK);
                        }
                        if( rc != 0 ) {
                            PLOGERR(klogErr, (klogErr, rc, "reading DATA_BLOCK $(i)", PLOG_U32(i), i));
                        }
                    }
                    if( rc == 0 ) {
                        /* serial ascending sort */
                        bool swapped = true;
                        while(swapped) {
                            swapped = false;
                            for(i = 1; i < obj->datablock_count; i++) {
                                if( obj->datablocks[i].serial < obj->datablocks[i - 1].serial ) {
                                    DataBlock db;
                                    memcpy(&db, &obj->datablocks[i], sizeof(db));
                                    memcpy(&obj->datablocks[i], &obj->datablocks[i - 1], sizeof(db));
                                    memcpy(&obj->datablocks[i - 1], &db, sizeof(db));
                                    swapped = true;
                                }
                            }
                        }
                    }
                }
            }
            KXMLNodesetRelease(nset);
        } else {
            LOGERR(klogErr, rc, "DATA_BLOCK node(s)");
        }
    }

    if( rc == 0 ) {
        if( (rc = KXMLNodeOpenNodesetRead(RUN, &nset, "RUN_ATTRIBUTES/RUN_ATTRIBUTE")) == 0) {
            uint32_t num_attr = 0;
            if( (rc = KXMLNodesetCount(nset, &num_attr)) == 0) {
                uint32_t i = 0;
                for(i = 0; rc == 0 && i < num_attr; i++) {
                    const KXMLNode* RUN_ATTRIBUTE = NULL;
                    if( (rc = KXMLNodesetGetNodeRead(nset, &RUN_ATTRIBUTE, i)) == 0 ) {
                        const KXMLNode* TAG = NULL, *VALUE = NULL;
                        if( (rc = KXMLNodeGetFirstChildNodeRead(RUN_ATTRIBUTE, &TAG, "TAG")) == 0 && 
                            (rc = KXMLNodeGetFirstChildNodeRead(RUN_ATTRIBUTE, &VALUE, "VALUE")) == 0 ) {
                            char* tag = NULL, *value = NULL;
                            if( (rc = KXMLNodeReadCStr(TAG, &tag, NULL)) == 0 &&
                                (rc = KXMLNodeReadCStr(VALUE, &value, NULL)) == 0 ) {
                                if( strcasecmp(tag, "read_name_barcode_proc_directive") == 0 ) {
                                    if( value == NULL || value[0] == '\0' ) {
                                        obj->attributes.barcode_rule = eBarcodeRule_not_set;
                                    } else if (strcasecmp(value, "interpret_as_spotgroup") == 0 || strcasecmp(value, "use_file_spot_name") == 0) {
                                        obj->attributes.barcode_rule = eBarcodeRule_use_file_spot_name;
                                    } else if (strcasecmp(value, "use_table_in_experiment") == 0) {
                                        obj->attributes.barcode_rule = eBarcodeRule_use_table_in_experiment;
                                    } else if(strcasecmp(value, "ignore") == 0) {
                                        obj->attributes.barcode_rule = eBarcodeRule_ignore_barcode;
                                    } else {
                                        rc = RC(rcSRA, rcFormatter, rcParsing, rcAttr, rcUnrecognized);
                                        PLOGERR(klogErr, (klogErr, rc, "RUN_ATTRIBUTE['$(t)'] VALUE='$(v)'", PLOG_2(PLOG_S(t),PLOG_S(v)), tag, value));
                                    }
                                } else if( strcasecmp(tag, "quality_scoring_system") == 0 ) {
                                    if( value != NULL )  {
                                        if( strcasecmp(value, "phred") == 0 ) {
                                            obj->attributes.quality_type = eExperimentQualityType_Phred;
                                        } else if( strcasecmp(value, "logodds") == 0 || strcasecmp(value, "log odds") == 0 ) {
                                            obj->attributes.quality_type = eExperimentQualityType_LogOdds;
                                        } else if( strcasecmp(value, "other") == 0 ) {
                                            obj->attributes.quality_type = eExperimentQualityType_Other;
                                        }
                                    }
                                    if( obj->attributes.quality_type == eExperimentQualityType_Undefined ) {
                                        rc = RC(rcSRA, rcFormatter, rcParsing, rcAttr, rcUnrecognized);
                                        PLOGERR(klogErr, (klogErr, rc, "RUN_ATTRIBUTE['$(t)'] VALUE='$(v)'", PLOG_2(PLOG_S(t),PLOG_S(v)), tag, value));
                                    }
                                } else if( strcasecmp(tag, "quality_book_char") == 0 ) {
                                    if( value != NULL && value[0] != '\0' ) {
                                        obj->attributes.quality_offset = value[0];
                                    } else {
                                        PLOGMSG(klogWarn, (klogWarn, "RUN_ATTRIBUTE['$(t)'] VALUE='$(v)' ignored", PLOG_2(PLOG_S(t),PLOG_S(v)), tag, value));
                                    }
                                }
                            } else {
                                PLOGERR(klogErr, (klogErr, rc, "RUN_ATTRIBUTE[$(i)]/VALUE", PLOG_U32(i), i));
                            }
                            free(value);
                            free(tag);
                        } else {
                            PLOGERR(klogErr, (klogErr, rc, "RUN_ATTRIBUTE[$(i)]", PLOG_U32(i), i));
                        }
                        KXMLNodeRelease(VALUE);
                        KXMLNodeRelease(TAG);
                        KXMLNodeRelease(RUN_ATTRIBUTE);
                    } else {
                        PLOGERR(klogErr, (klogErr, rc, "RUN_ATTRIBUTE $(i)", PLOG_U32(i), i));
                    }
                }
            }
            KXMLNodesetRelease(nset);
        } else if( GetRCState(rc) == rcNotFound ) {
            rc = 0;
        } else {
            LOGERR(klogErr, rc, "RUN_ATTRIBUTES/RUN_ATTRIBUTE node(s)");
        }
    }
    if (rc == 0) {
        *rslt = obj;
    } else {
        RunXML_Whack(obj);
    }
    return rc;
}
