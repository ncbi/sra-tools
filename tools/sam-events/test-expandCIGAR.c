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

int main(int argc, char *argv[])
{
    static char const *const eventTypes[] = {
        "none", "match", "mismatch", "insert", "delete"
    };
    struct Event *event = NULL;
    int events = 0;
    
    struct cFastaFile *file = loadFastaFile(0, "tiny.fasta");
    assert(file != NULL);
    assert(validateCIGAR(0, "70M", NULL, NULL) == 0);
    {
        char const seq[] = "GGACAACGGGACTATCTAAAAGAGCTAAAATTGGAAACATTCTATTATCATTTAGGACGCAAAGTTAAAA";
        events = expandCIGAR(&event, 0, "70M", seq, 70 * 3, file, 0);
        assert(events == 1);

        assert(event[0].type == match);
        assert(event[0].length == 70);
        assert(event[0].refPos == 0);
        assert(event[0].seqPos == 0);
        
        free(event);
    }
    {
        char const seq[] = "GGACAACGGGACTAACTATCTAAAAGAGCTAAAATTGGAAACATTCTATTATCATTTAGGACGCAAAGTT";
        events = expandCIGAR(&event, 0, "10M4I56M", seq, 70 * 3, file, 0);
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
    unloadFastaFile(file);
    return 0;
}
