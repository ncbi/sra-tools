/*=======================================================================================
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
#ifndef _sra_load_loader_fmt_
#define _sra_load_loader_fmt_

#include <kfs/directory.h>
#include <kapp/args.h>
#include <kapp/log-xml.h>

#include "loader-file.h"
#include "experiment-xml.h"

typedef struct ResultXML {
    char *file;
    char *name;
    char *region;
    char *sector;
    char *member_name;
} ResultXML;

typedef struct ExpectedXML {
    uint8_t number_of_results;
    ResultXML **result;
} ExpectedXML;

typedef struct TArgs_struct {
    const char *_runXmlPath;
    const char* _input_path;
    uint32_t _input_unpacked; /* as bool */
    uint32_t _no_read_ahead; /* as bool */

    const char *_experimentXmlPath;
    const char *_target;
    uint32_t _force_target; /* as bool */

    int64_t _spots_to_run;
    int64_t _spots_bad_allowed;
    int _spots_bad_allowed_percentage;

    const char* _expectedXmlPath;
    int _intensities; /* trie state: 0 - default, 1 - on, 2 - off */

    Args* args;
    const XMLLogger* _xml_log;
    RunXML* _run;
    const KDirectory* _input_dir;
    const ExperimentXML* _experiment;
    const ExpectedXML* _expected;
    SRAMgr* _sra_mgr;
} TArgs;

typedef struct SInput_struct {
    struct {
        const SRALoaderFile** files;
        uint32_t count;
    } *blocks;
    uint32_t count;
} SInput;

/* Filter output columns */
enum ESRAColumnFilter {
    efltrINTENSITY = 0x01,
    efltrNOISE     = 0x02,
    efltrSIGNAL    = 0x04,
    efltrDEFAULT   = 0x08 /* if this bit is set FE must decide for itself ignoring other bits!! */
};

typedef struct SRALoaderConfig_struct {
    /* if spots_to_run >= 0 then reader must stop after writing given number of spots */
	int64_t spots_to_run;
    int64_t spots_bad_allowed;
    /* combination of SRAColumnFilter values. If the corresponding bit is set then the column should be skipped */
	uint64_t columnFilter;
    const ExperimentXML* experiment;
    const char* table_path;
    SRAMgr* sra_mgr;
    bool force_table_overwrite;
} SRALoaderConfig;


typedef union SRALoaderFmt_vt SRALoaderFmt_vt;

typedef struct SRALoaderFmt_struct {
    const SRALoaderFmt_vt *vt;
} SRALoaderFmt;

#ifndef SRALOADERFMT_IMPL
#define SRALOADERFMT_IMPL SRALoaderFmt
#endif

typedef struct SRALoaderFmt_vt_v1_struct {
    /* version == 1.x */
    uint32_t maj;
    uint32_t min;

    /* start minor version == 0 */
    rc_t ( * destroy ) ( SRALOADERFMT_IMPL *self, SRATable** table  );
    rc_t ( * version ) ( const SRALOADERFMT_IMPL *self, uint32_t *vers, const char** name );
    rc_t ( * exec_prep ) (const SRALOADERFMT_IMPL *self, const TArgs* args, const SInput* input,
                                                    const char** path, const char* eargs[], size_t max_eargs);
    rc_t ( * write_data ) ( SRALOADERFMT_IMPL *self, uint32_t argc, const SRALoaderFile *const argv [], int64_t* spots_bad_count );
    /* end minor version == 0 */

} SRALoaderFmt_vt_v1;

union SRALoaderFmt_vt {
    SRALoaderFmt_vt_v1 v1;
};

/* Make
 *  allocate object based on SRALoaderFmt type and initialize it both using
 *  SRALoaderFmtInit and SRALoaderConfig data
 */
rc_t SRALoaderFmtMake( SRALoaderFmt **self, const SRALoaderConfig *config );

/* Init
 *  initialize a newly allocated loader object
 */
rc_t SRALoaderFmtInit( SRALoaderFmt *self, const SRALoaderFmt_vt *vt);

rc_t SRALoaderFmtRelease ( const SRALoaderFmt *self, SRATable** table );

/* Version
 *  return 4-part version code: 0xMMmmrrrr, where
 *      MM = major release
 *      mm = minor release
 *    rrrr = bug-fix release
 */
rc_t SRALoaderFmtVersion ( const SRALoaderFmt *self, uint32_t *vers, const char** name );

/* WriteData
 *  reader will parse data from input files and write them to table
 *  a row at a time.
 *
 *  "table" [ IN ] - an open, writable table with no open columns.
 *  the reader should open columns with the appropriate data type
 *  based upon its format and input files as determined during construction
 *
 *  "argc" [ IN ] and "argv" [ IN ] - input files positionally
 *  corresponding to information given in reader configuration
 *  data in XML node.
 */
rc_t SRALoaderFmtWriteData ( SRALoaderFmt *self, uint32_t argc, const SRALoaderFile *const argv [], int64_t* spots_bad_count );

rc_t SRALoaderFmtExecPrep (const SRALoaderFmt *self, const TArgs* args, const SInput* input,
                           const char** path, const char* eargs[], size_t max_eargs);

#endif /* _sra_load_loader_fmt_ */
