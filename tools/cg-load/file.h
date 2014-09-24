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
#ifndef _tools_cg_load_file_h_
#define _tools_cg_load_file_h_

#include <kfs/file.h>
#include <kfs/directory.h>

#include <klib/container.h>
#include <klib/log.h>
#include <klib/rc.h>

#include "defs.h"
#include "writer-seq.h"
#include "writer-algn.h"
#include "writer-evidence-intervals.h"
#include "writer-evidence-dnbs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <strtol.h>


/* some usefull utils */
/* strchr but in fixed size buffer (not asciiZ!) */
static __inline__ const char* str_chr(const char* str, const size_t len, char sep)
{
    const char* end = str + len;
    while( str < end ) {
        if( *str == sep ) {
            break;
        }
        str++;
    }
    return str == end ? NULL : str;
}

static __inline__
rc_t str2buf(const char* str, const size_t len, char* buf, const size_t buf_sz)
{
    if( buf_sz <= len ) {
        rc_t rc = RC(rcRuntime, rcString, rcCopying, rcBuffer, rcInsufficient);
        if (rc != 0) {
            PLOGERR(klogErr, (klogErr, rc,  "'$(str)': $(sz) <= $(len)",
                "str=%.*s,sz=%lu,len=%lu", len, str, buf_sz, len));
        }
        return rc;
    }
    memcpy(buf, str, len);
    buf[len] = '\0';
    return 0;
}

static __inline__
rc_t str2unsigned(const char* str, const size_t len, uint64_t max, uint64_t* value)
{
    char* end;
    int64_t q;

    if( len == 0 ) {
        return RC(rcRuntime, rcString, rcConverting, rcData, rcTooShort);
    }
    q = strtou64(str, &end, 10);
    if( end - str != len ) {
        return RC(rcRuntime, rcString, rcConverting, rcData, rcInvalid);
    }
    if( q < 0 || ( uint64_t ) q > max ) {
        return RC(rcRuntime, rcString, rcConverting, rcData, rcOutofrange);
    }
    *value = q;
    return 0;
}
static __inline__
rc_t str2u64(const char* str, const size_t len, uint64_t* value)
{
    rc_t rc;
    uint64_t q;

    if( (rc = str2unsigned(str, len, -1, &q)) == 0 ) {
        *value = q;
    }
    return rc;
}

static __inline__
rc_t str2signed(const char* str, const size_t len, int64_t min, int64_t max, int64_t* value)
{
    char* end;
    int64_t q;

    if( len == 0 ) {
        return RC(rcRuntime, rcString, rcConverting, rcData, rcTooShort);
    }
    q = strtoi64(str, &end, 10);
    if( end - str != len ) {
        return RC(rcRuntime, rcString, rcConverting, rcData, rcInvalid);
    }
    if( q < min || q > max ) {
        return RC(rcRuntime, rcString, rcConverting, rcData, rcOutofrange);
    }
    *value = q;
    return 0;
}
static __inline__
rc_t str2i64(const char* str, const size_t len, int64_t* value)
{
    rc_t rc;
    int64_t q;

#if _ARCH_BITS == 32
    if( (rc = str2signed(str, len, -0x7FFFFFFFFFFFFFFFLL, 0x7FFFFFFFFFFFFFFFLL, &q)) == 0 ) {
        *value = q;
    }
#else
    if( (rc = str2signed(str, len, -0x7FFFFFFFFFFFFFFFL, 0x7FFFFFFFFFFFFFFFL, &q)) == 0 ) {
        *value = q;
    }
#endif
    return rc;
}
static __inline__
rc_t str2i32(const char* str, const size_t len, int32_t* value)
{
    rc_t rc;
    int64_t q;

    if( (rc = str2signed(str, len, -0x7FFFFFFF - 1, 0x7FFFFFFF, &q)) == 0 ) {
        *value = ( int32_t ) q;
    }
    return rc;
}
static __inline__
rc_t str2u32(const char* str, const size_t len, uint32_t* value)
{
    rc_t rc;
    uint64_t q;

    if( (rc = str2unsigned(str, len, 0xFFFFFFFF, &q)) == 0 ) {
        *value = ( uint32_t ) q;
    }
    return rc;
}
static __inline__
rc_t str2i16(const char* str, const size_t len, int16_t* value)
{
    rc_t rc;
    int64_t q;

    if( (rc = str2signed(str, len, -0x7FFF - 1, 0x7FFF, &q)) == 0 ) {
        *value = ( int16_t ) q;
    }
    return rc;
}
static __inline__
rc_t str2u16(const char* str, const size_t len, uint16_t* value)
{
    rc_t rc;
    uint64_t q;

    if( (rc = str2unsigned(str, len, 0xFFFF, &q)) == 0 ) {
        *value = ( uint16_t ) q;
    }
    return rc;
}
#define CG_LINE_START(file, buf, len, res) \
    do { \
        const char* buf, *res; \
        size_t len; \
        if( (rc = CGLoaderFile_Readline(file, (const void**)&buf, &len)) != 0 ) { \
            break; \
        } \
        res = buf - 1;

#define CG_LINE_NEXT_FIELD(buf, len, res) \
    if( rc != 0 ) { \
        break; \
    } else { \
        len -= ++res - buf; \
        buf = res; \
        if( (res = str_chr(buf, len, '\t')) == NULL ) { \
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcCorrupt); \
            break; \
        } \
    }

#define CG_LINE_LAST_FIELD(buf, len, res) \
    if( rc != 0 ) { \
        break; \
    } else { \
        len -= ++res - buf; \
        buf = res; \
        res = buf + len; \
        if( str_chr(buf, len, '\t') != NULL ) { \
            rc = RC(rcRuntime, rcFile, rcReading, rcData, rcCorrupt); \
            break; \
        } \
    }

#define CG_LINE_END() \
    } while(false)

#ifndef CGFILETYPE_IMPL
#define CGFILETYPE_IMPL CGFileType
#endif

typedef struct CGFileType CGFileType;

typedef struct CGFileType_vt_struct {
    rc_t ( CC *header ) (const CGFILETYPE_IMPL* self, const char* buf, const size_t len);

    rc_t ( CC *reads ) (const CGFILETYPE_IMPL* cself, TReadsData* data);
    rc_t ( CC *get_start_row ) (const CGFILETYPE_IMPL* cself, int64_t* rowid);
    rc_t ( CC *mappings ) (const CGFILETYPE_IMPL* cself, TMappingsData* data);
    rc_t ( CC *evidence_intervals )(const CGFILETYPE_IMPL* cself, TEvidenceIntervalsData* data);
    rc_t ( CC *evidence_dnbs )(const CGFILETYPE_IMPL* cself, const char* interval_id, TEvidenceDnbsData* data);
    rc_t ( CC *tag_lfr )(const CGFILETYPE_IMPL* cself, TReadsData* data);

    rc_t ( CC *assembly_id) (const CGFILETYPE_IMPL* self, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id);
    rc_t ( CC *slide) (const CGFILETYPE_IMPL* self, const CGFIELD_SLIDE_TYPE** slide);
    rc_t ( CC *lane) (const CGFILETYPE_IMPL* self, const CGFIELD_LANE_TYPE** lane);
    rc_t ( CC *batch_file_number) (const CGFILETYPE_IMPL* self, const CGFIELD_BATCH_FILE_NUMBER_TYPE** batch_file_number);
    rc_t ( CC *sample) (const CGFILETYPE_IMPL* self, const CGFIELD_SAMPLE_TYPE** sample);
    rc_t ( CC *chromosome) (const CGFILETYPE_IMPL* self, const CGFIELD_CHROMOSOME_TYPE** chromosome);

    void ( CC *destroy ) (const CGFILETYPE_IMPL* self, uint64_t* records);
} CGFileType_vt;

struct CGFileType {
    uint32_t format_version;
    CG_EFileType type;
    const CGFileType_vt* vt;
};

typedef struct CGLoaderFile
{
    bool read_ahead;
    const struct KLoaderFile *file;
    const CGFileType* cg_file;
} CGLoaderFile;

typedef struct CGFileTypeFactory {
    const char* name;
    CG_EFileType type;
    rc_t ( CC *make ) (const CGFileType** self, const CGLoaderFile* file);
} CGFileTypeFactory;

rc_t CGLoaderFile_Make(const CGLoaderFile **cself, const KDirectory* dir, const char* filename,
                       const uint8_t* md5_digest, bool read_ahead);

rc_t CGLoaderFile_Release(const CGLoaderFile* cself, bool ignored);

/* returns true if eof is reached and buffer is empty */
rc_t CGLoaderFile_IsEof(const CGLoaderFile* cself, bool* eof);

/* closes the underlying file */
rc_t CGLoaderFile_Close(const CGLoaderFile* cself);

/* returns current 1-based line number in file */
rc_t CGLoaderFile_Line(const CGLoaderFile* cself, uint64_t* line);

rc_t CGLoaderFile_Filename(const CGLoaderFile *cself, const char** name);

rc_t CGLoaderFile_LOG(const CGLoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, ...);

rc_t CGLoaderFile_GetType(const CGLoaderFile* cself, CG_EFileType* type);

rc_t CGLoaderFile_GetRead(const CGLoaderFile* cself, TReadsData* data);
rc_t CGLoaderFile_GetStartRow(const CGLoaderFile* cself, int64_t* rowid);

rc_t CGLoaderFile_GetTagLfr(const CGLoaderFile* cself, TReadsData* data);

rc_t CGLoaderFile_GetMapping(const CGLoaderFile* cself, TMappingsData* data);

rc_t CGLoaderFile_GetEvidenceIntervals(const CGLoaderFile* cself, TEvidenceIntervalsData* data);

rc_t CGLoaderFile_GetEvidenceDnbs(const CGLoaderFile* cself, const char* interval_id, TEvidenceDnbsData* data);

rc_t CGLoaderFile_GetAssemblyId(const CGLoaderFile* cself, const CGFIELD_ASSEMBLY_ID_TYPE** assembly_id);
rc_t CGLoaderFile_GetSlide(const CGLoaderFile* cself, const CGFIELD_SLIDE_TYPE** slide);
rc_t CGLoaderFile_GetLane(const CGLoaderFile* cself, const CGFIELD_LANE_TYPE** lane);
rc_t CGLoaderFile_GetBatchFileNumber(const CGLoaderFile* cself, const CGFIELD_BATCH_FILE_NUMBER_TYPE** batch_file_number);
rc_t CGLoaderFile_GetSample(const CGLoaderFile* cself, const CGFIELD_SAMPLE_TYPE** sample);
rc_t CGLoaderFile_GetChromosome(const CGLoaderFile* cself, const CGFIELD_CHROMOSOME_TYPE** chromosome);


/* Readline
 *  makes next line from a file available in buffer.
 *  eligable EOL symbols are: \n (unix), \r (older mac), \r\n (win)
 *  EOL symbol(s) never included in buffer length.
 *  line is \0 terminated.
 *  if there is no EOL at EOF - not an error.
 *  fails if internal buffer is insufficient.
 *  buffer is NULL on EOF
 *  rc state of (rcString rcTooLong) means line was too long
 *              you may copy line and readline again for the tail of the line
 *
 *  "buffer" [ OUT ] and "length" [ OUT ] - returned line and it's length
 */
rc_t CGLoaderFile_Readline(const CGLoaderFile* cself, const void** buffer, size_t* length);

rc_t CGLoaderFile_CreateCGFile(CGLoaderFile* self,
    uint32_t FORMAT_VERSION, const char* TYPE);

rc_t CGLoaderFileMakeCGFileType(const CGLoaderFile* self, const char* type,
    const CGFileTypeFactory* factory, size_t factories,
    const CGFileType** ftype);

#endif /* _tools_cg_load_file_h_ */
