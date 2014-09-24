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

#include <klib/rc.h>
#include <klib/data-buffer.h>
#include <klib/sort.h>

#include "KFileHelper.h"
#include "analysis.h"
#include "Globals.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static rc_t ParseAnalysis(Analysis *rslt, unsigned lines, char *src) {
    unsigned i;
    
    rslt->N = lines;
    memset(&rslt->line[0], 0, lines * sizeof(rslt->line[0]));
    
    for (i = 0; i != lines; ++i) {
        char *endp;
        char *tab;
        
        endp = strchr(src, 0);
        assert(endp);
        
        rslt->line[i].name = src;
        tab = strchr(src, '\t');
        if (!tab) break;
        src = tab + 1;
        tab[0] = '\0';
        
        rslt->line[i].refID = src;
        tab = strchr(src, '\t');
        if (!tab) break;
        src = tab + 1;
        tab[0] = '\0';
        
        tab = strchr(src, '\t');
        if (!tab) break;
        src = tab + 1;
        
        rslt->line[i].bamFile = src;
        tab = strchr(src, '\t');
        if (!tab) break;
        src = tab + 1;
        tab[0] = '\0';
        
        rslt->line[i].asoFile = src;
        tab = strchr(src, '\t');
        if (!tab) break;
        tab[0] = '\0';
        
        src = endp + 1;
    }
    for (i = 0; i != lines; ++i) {
        if (rslt->line[i].name == NULL ||
            rslt->line[i].refID == NULL ||
            rslt->line[i].bamFile == NULL ||
            rslt->line[i].asoFile == NULL
            )
        {
            return RC(rcApp, rcFile, rcReading, rcData, rcCorrupt);
        }
    }
    return 0;
}

static int CC CompAnalysisLineBamFile(const void *A, const void *B, void *ignore) {
    const AnalysisLine *a = A;
    const AnalysisLine *b = B;
    int rslt = strcmp(a->bamFile, b->bamFile);
    
    if (rslt == 0)
        rslt = strcmp(a->name, b->name);
    return rslt;
}

const char *AnalysisNextBAMFile(const char *last) {
    unsigned i;
    if (last == NULL) {
        return G.analysis->line[0].bamFile;
    }
    for (i = 1; i < G.analysis->N; ++i) {
        if (strcmp(last, G.analysis->line[i].bamFile) > 0) {
            return G.analysis->line[i].bamFile;
        }
    }
    return NULL;
}

AnalysisLine *AnalysisMatch(const char *bamFile, const char *name) {
    unsigned i;
    
    for (i = 0; i != G.analysis->N; ++i) {
        if (strcmp(bamFile, G.analysis->line[i].bamFile) == 0 &&
            strcmp(name, G.analysis->line[i].name) == 0
            )
        {
            return &G.analysis->line[i];
        }
    }
    return NULL;
}

rc_t LoadAnalysis(const char *fileName) {
    rc_t rc;
    KDataBuffer buf;
    uint64_t lines;
    
    memset(&buf, 0, sizeof(buf));
    buf.elem_bits = 8;
    
    rc = LoadFile(&buf, &lines, G.inDir, fileName);
    if (rc == 0) {
        if (lines > 0 && buf.elem_count > 0) {
            G.analysis = malloc(sizeof(*G.analysis) + (lines - 1) * sizeof(G.analysis->line[0]) + buf.elem_count);
            if (G.analysis)
                memcpy(&G.analysis->line[lines], buf.base, buf.elem_count);
            else
                rc = RC(rcApp, rcFile, rcReading, rcMemory, rcExhausted);
        }
        else
            rc = RC(rcApp, rcFile, rcReading, rcData, rcEmpty);
    }
    KDataBufferWhack(&buf);
    if (rc == 0) {
        rc = ParseAnalysis(G.analysis, lines, (char *)&G.analysis->line[lines]);
    }
    return rc;
}

