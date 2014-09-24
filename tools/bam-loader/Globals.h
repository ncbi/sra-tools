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

#ifndef BAM_LOAD_GLOBALS_H_
#define BAM_LOAD_GLOBALS_H_ 1

enum LoaderModes {
    mode_Archive,
    mode_Analysis
};

typedef struct globals
{
    char const *inpath;
    char const *outpath;
    char const *tmpfs;
    
    struct KFile *noMatchLog;
    
    char const *schemaPath;
    char const *schemaIncludePath;
    
    char const *refXRefPath;
    
    char const *QualQuantizer;
    
    char const *refFilter;

    char const** refFiles; /* NULL-terminated array pointing to argv */
    
    char const *headerText;
    
    uint64_t maxAlignCount;
    size_t cache_size;

    unsigned errCount;
    unsigned maxErrCount;
    unsigned maxWarnCount_NoMatch;
    unsigned maxWarnCount_DupConflict;
    unsigned pid;
    unsigned minMatchCount; /* minimum number of matches to count as an alignment */
    int minMapQual;
    enum LoaderModes mode;
    uint32_t maxSeqLen;
    bool omit_aligned_reads;
    bool omit_reference_reads;
    bool no_real_output;
    bool expectUnsorted;
    bool requireSorted;
    bool noVerifyReferences;
    bool onlyVerifyReferences;
    bool useQUAL;
    bool limit2config;
    bool editAlignedQual;
    bool keepMismatchQual;
    bool acceptBadDups; /* accept spots with inconsistent PCR duplicate flags */
    bool acceptNoMatch; /* accept without any matching bases */
    bool noSpotAssembly;
    uint8_t alignedQualValue;
    bool allUnaligned; /* treat all records as unaligned */
    bool noColorSpace;
    bool noSecondary;
    bool hasTI;
    bool acceptHardClip;
} Globals;

extern Globals G;
#endif
