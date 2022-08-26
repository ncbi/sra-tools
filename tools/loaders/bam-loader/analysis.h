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

#ifndef BAM_LOAD_ANALYSIS_H_
#define BAM_LOAD_ANALYSIS_H_ 1
#include <klib/rc.h>

typedef struct s_analysis_line {
    const char *name;
    const char *refID;
    const char *bamFile;
    const char *asoFile;
    uint64_t    startPos;
} AnalysisLine;

typedef struct s_analysis {
    unsigned N;
    AnalysisLine line[1];
} Analysis;

const char *AnalysisNextBAMFile(const char *last);

AnalysisLine *AnalysisMatch(const char *bamFile, const char *name);

rc_t LoadAnalysis(const char *fileName);
#endif
