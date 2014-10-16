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
#ifndef _h_sra_fuse_formats_
#define _h_sra_fuse_formats_

#include <kfs/file.h>
#include <kdb/meta.h>
#include <sra/sradb.h>

#define FILEOPTIONS_BUFFER 32

struct SRAListNode;

/* DO NOT CHANGE SIZE OR ELEMENT ORDER OF THIS STRUCTURE
   CACHE MAY FAIL! see SRAList_Init */
typedef struct FileOptionsOld {
    enum {
        eOldSRAFuseFmtArc         = 0x0001,
        eOldSRAFuseFmtArcLite     = 0x0002,
        eOldSRAFuseFmtFastq       = 0x0004,
        eOldSRAFuseFmtFastqGz     = 0x0008,
        eOldSRAFuseFmtFastqMD5    = 0x0010,
        eOldSRAFuseFmtSFF         = 0x0020,
        eOldSRAFuseFmtSFFGz       = 0x0040,
        eOldSRAFuseFmtSFFMD5      = 0x0080,
        eOldSRAFuseFmtArcMD5      = 0x0100,
        eOldSRAFuseFmtArcLiteMD5  = 0x0200
    } type;
    char suffix[FILEOPTIONS_BUFFER];
    uint64_t file_sz;
    KTime_t obsolete; /* not removed because cache is binary */
    char index[FILEOPTIONS_BUFFER];
    uint32_t buffer_sz;
    
    union {
        char txt64b[FILEOPTIONS_BUFFER * 2];
        struct {
            bool lite;
        } sra;
        struct {
            char accession[FILEOPTIONS_BUFFER];
            uint64_t minSpotId;
            uint64_t maxSpotId;
            uint8_t colorSpace;
            char colorSpaceKey;
            uint8_t origFormat;
            uint8_t printLabel;
            uint8_t printReadId;
            uint8_t clipQuality;
            uint32_t minReadLen;
            uint16_t qualityOffset;
            bool gzip;
        } fastq;
        struct {
            char accession[FILEOPTIONS_BUFFER];
            uint64_t minSpotId;
            uint64_t maxSpotId;
            bool gzip;
        } sff;
    } f;
} FileOptionsOld;

typedef struct FileOptions {
    enum {
        eSRAFuseFmtArc         = 0x0001,
        eSRAFuseFmtArcLite     = 0x0002,
        eSRAFuseFmtFastq       = 0x0004,
        eSRAFuseFmtFastqGz     = 0x0008,
        eSRAFuseFmtFastqMD5    = 0x0010,
        eSRAFuseFmtSFF         = 0x0020,
        eSRAFuseFmtSFFGz       = 0x0040,
        eSRAFuseFmtSFFMD5      = 0x0080,
        eSRAFuseFmtArcMD5      = 0x0100,
        eSRAFuseFmtArcLiteMD5  = 0x0200
    } type;
    char suffix[FILEOPTIONS_BUFFER];
    uint64_t file_sz;
    KTime_t obsolete; /* not removed because cache is binary */
    char index[FILEOPTIONS_BUFFER];
    uint32_t buffer_sz;
    
    union {
        char txt64b[FILEOPTIONS_BUFFER * 2];
        struct {
            bool lite;
        } sra;
        struct {
            char accession[FILEOPTIONS_BUFFER];
            uint64_t minSpotId;
            uint64_t maxSpotId;
            uint8_t colorSpace;
            char colorSpaceKey;
            uint8_t origFormat;
            uint8_t printLabel;
            uint8_t printReadId;
            uint8_t clipQuality;
            uint32_t minReadLen;
            uint16_t qualityOffset;
            bool gzip;
        } fastq;
        struct {
            char accession[FILEOPTIONS_BUFFER];
            uint64_t minSpotId;
            uint64_t maxSpotId;
            bool gzip;
        } sff;
    } f;
    /* added ver this struct 2: */
    char md5[32];
    int8_t md5_file; /* index relative to self with array of structs, 0 - no md5 */
} FileOptions;

rc_t FileOptions_Make(FileOptions** self, uint32_t count);
void FileOptions_Release(FileOptions* self);

rc_t FileOptions_Clone(FileOptions** self, const FileOptions* src, uint32_t count);

rc_t FileOptions_SRAArchive(FileOptions* self, const SRATable* tbl, KTime_t ts, bool lite);
rc_t FileOptions_SRAArchiveInstant(FileOptions* self, FileOptions* fmd5,
                                   const SRAMgr* mgr, const char* accession, const char* path,
                                   const bool lite, KTime_t ts, uint64_t size, char md5[32]);
rc_t FileOptions_SRAArchiveUpdate(FileOptions* self, const char* name,
                                  KTime_t ts, uint64_t size, char md5[32]);


rc_t FileOptions_ParseMeta(FileOptions* self, const KMDataNode* file, const SRATable* tbl, KTime_t ts, const char* suffix);

rc_t FileOptions_AttachMD5(FileOptions* self, const char* name, FileOptions* md5);
rc_t FileOptions_CalcMD5(FileOptions* self, const char* name, const struct SRAListNode* sra);
rc_t FileOptions_UpdateMD5(FileOptions* self, const char* name);

rc_t FileOptions_OpenFile(const FileOptions* cself, const struct SRAListNode* sra, const KFile** kfile);

#endif /* _h_sra_fuse_formats_ */
