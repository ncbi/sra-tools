/* ===========================================================================
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
 */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "expandCIGAR.h"

static void testExpandMatch(struct cFastaFile *file, int refNo)
{
    char const seq[] = "GGACAACGGGACTATCTAAAAGAGCTAAAATTGGAAACATTCTATTATCATTTAGGACGCAAAGTTAAAA";
    struct Event *event = NULL;
    int events = expandCIGAR(&event, 0, "70M", seq, 70 * 3, file, refNo);
    assert(events == 1);
    
    assert(event[0].type == match);
    assert(event[0].length == 70);
    assert(event[0].refPos == 0);
    assert(event[0].seqPos == 0);
    
    free(event);
}

static void testSoftClip(struct cFastaFile *file, int refNo)
{
    char const seq[] = "GGACAACGGGACTATCTAAAAGAGCTAAAATTGGAAACATTCTATTATCATTTAGGACGCAAAGTTAAAA";
    struct Event *event = NULL;
    int events = expandCIGAR(&event, 0, "65M5S", seq, 70 * 3, file, refNo);
    assert(events == 1);
    
    assert(event[0].type == match);
    assert(event[0].length == 65);
    assert(event[0].refPos == 0);
    assert(event[0].seqPos == 0);
    
    free(event);
}

static void testExpandInsert(struct cFastaFile *file, int refNo)
{
    char const seq[] = "GGACAACGGGACTAACTATCTAAAAGAGCTAAAATTGGAAACATTCTATTATCATTTAGGACGCAAAGTT";
    struct Event *event = NULL;
    int events = expandCIGAR(&event, 0, "10M4I56M", seq, 70 * 3, file, refNo);
    assert(events == 3);
    
    assert(event[0].type == match);
    assert(event[0].length == 10);
    assert(event[0].refPos == 0);
    assert(event[0].seqPos == 0);
    
    assert(event[1].type == insertion);
    assert(event[1].length == 4);
    assert(event[1].refPos == 10);
    assert(event[1].seqPos == 10);
    
    assert(event[2].type == match);
    assert(event[2].length == 56);
    assert(event[2].refPos == 10);
    assert(event[2].seqPos == 14);
    
    free(event);
}

static void testExpandDelete(struct cFastaFile *file, int refNo)
{
    char const seq[] = "GGACAACGGGTCTAAAAGAGCTAAAATTGGAAACATTCTATTATCATTTAGGACGCAAAGTTAAAAGAGA";
    struct Event *event = NULL;
    int events = expandCIGAR(&event, 0, "10M4D60M", seq, 70 * 3, file, refNo);
    assert(events == 3);
    
    assert(event[0].type == match);
    assert(event[0].length == 10);
    assert(event[0].refPos == 0);
    assert(event[0].seqPos == 0);
    
    assert(event[1].type == deletion);
    assert(event[1].length == 4);
    assert(event[1].refPos == 10);
    assert(event[1].seqPos == 10);
    
    assert(event[2].type == match);
    assert(event[2].length == 60);
    assert(event[2].refPos == 14);
    assert(event[2].seqPos == 10);
    
    free(event);
}

static void testExpand()
{
    struct cFastaFile *file = loadFastaFile(0, "tiny.fasta");
    assert(file != NULL);
    
    int refNo = FastaFile_getNamedSequence(file, 0, "R");
    assert(refNo == 0);
    
    testExpandMatch(file, refNo);
    testSoftClip(file, refNo);
    testExpandInsert(file, refNo);
    testExpandDelete(file, refNo);
    
    unloadFastaFile(file);
}

static void testReference()
{
    struct cFastaFile *file = loadFastaFile(0, "tiny.fasta");
    assert(file != NULL);
    
    int refNo = FastaFile_getNamedSequence(file, 0, "R");
    assert(refNo == 0);
    
    char const *sequence;
    unsigned length = FastaFile_getSequenceData(file, refNo, &sequence);
    printf("length: %u\n", length);
    printf("%.70s\n", sequence);
    
    unloadFastaFile(file);
}

static void testCIGAR()
{
    unsigned reflen = 0;
    unsigned seqlen = 0;
    assert(validateCIGAR(0, "70M", &reflen, &seqlen) == 0);
    assert(reflen == 70);
    assert(seqlen == 70);
    
    /* not a valid CIGAR */
    assert(validateCIGAR(0, "70Z", NULL, NULL) != 0);
}

int main(int argc, char *argv[])
{
    testCIGAR();
    testExpand();
    testReference();
    return 0;
}
