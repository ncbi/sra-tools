// align_test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <fmtdef.h>
#include <align/align-access.h>

static void testEnum(const AlignAccessDB *db) {
    AlignAccessRefSeqEnumerator *e;
    rc_t rc;
    char buf[1024];
    
    rc = AlignAccessDBEnumerateRefSequences(db, &e);
    if (rc)
        return;
    do {
        AlignAccessRefSeqEnumeratorGetID(e, buf, sizeof(buf), NULL);
        printf("ID: %s\n", buf);
    } while (AlignAccessRefSeqEnumeratorNext(e) == 0);
    AlignAccessRefSeqEnumeratorRelease(e);
}

static void print1(AlignAccessAlignmentEnumerator *e) {
    uint64_t start;
    char buf[4 * 1024];
    
    AlignAccessAlignmentEnumeratorGetRefSeqPos(e, &start);
    printf("Ref. Seq. Position: %" LU64 "\n", start);
    AlignAccessAlignmentEnumeratorGetShortSeqID(e, buf, sizeof(buf), NULL);
    printf("spot name: %s\n", buf);
    AlignAccessAlignmentEnumeratorGetCIGAR(e, &start, buf, sizeof(buf), NULL);
    printf("Aligned Seq. Pos.: %" LU64 "\n", start);
    printf("CIGAR: %s\n", buf);
    AlignAccessAlignmentEnumeratorGetShortSequence(e, buf, sizeof(buf), NULL);
    printf("sequence: %s\n\n", buf);
}

static void print2(AlignAccessAlignmentEnumerator *e) {
    print1(e);
    AlignAccessAlignmentEnumeratorNext(e);
    print1(e);
}

static
void test(const char *bam, const char *idx, const char *refSeqID) {
    rc_t rc;
    const AlignAccessMgr *mgr;
    const AlignAccessDB *db;
    AlignAccessAlignmentEnumerator *e;
    
    rc = AlignAccessMgrMake(&mgr);
    if (rc)
        return;
    if (idx)
        rc = AlignAccessMgrMakeIndexBAMDB(mgr, &db, bam, idx);
    else
        rc = AlignAccessMgrMakeBAMDB(mgr, &db, bam);
    AlignAccessMgrRelease(mgr);
    if (rc)
        return;
    testEnum(db);
    if (idx) {
        rc = AlignAccessDBWindowedAlignments(db, &e, refSeqID, 60509765, 0);
        if (rc == 0) {
            print2(e);
            AlignAccessAlignmentEnumeratorRelease(e);
        }
        rc = AlignAccessDBWindowedAlignments(db, &e, refSeqID, 60449500, 0);
        if (rc == 0) {
            print2(e);
            AlignAccessAlignmentEnumeratorRelease(e);
        }
        rc = AlignAccessDBWindowedAlignments(db, &e, refSeqID, 8, 2);
        if (rc == 0) {
            do {
                print1(e);
            } while (AlignAccessAlignmentEnumeratorNext(e) == 0);
            AlignAccessAlignmentEnumeratorRelease(e);
        }
    }
    else {
        rc = AlignAccessDBEnumerateAlignments(db, &e);
        if (rc == 0) {
            print2(e);
            AlignAccessAlignmentEnumeratorRelease(e);
        }
    }
    AlignAccessDBRelease(db);
}

static
void testGetCIGAR(const char *bam) {
    const AlignAccessMgr *mgr;
    const AlignAccessDB *db;
    AlignAccessAlignmentEnumerator *e;
    uint64_t pos;
    char buffer[4096];
    size_t in_buf;
    size_t buflen;
    rc_t rc;
    uint64_t i = 0;
    
    rc = AlignAccessMgrMake(&mgr);
    if (rc)
        return;
    rc = AlignAccessMgrMakeBAMDB(mgr, &db, bam);
    AlignAccessMgrRelease(mgr);
    if (rc)
        return;
    
    rc = AlignAccessDBEnumerateAlignments(db, &e);
    if (rc == 0) {
        do {
        	++i;
            AlignAccessAlignmentEnumeratorGetRefSeqPos(e, &pos);
            AlignAccessAlignmentEnumeratorGetCIGAR(e, &pos, buffer, sizeof(buffer), &in_buf);
            buflen = strlen(buffer);
            if (!(in_buf == 0 || buflen + 1 == in_buf)) {
            	printf("failed at position %lu, record number %lu\n", pos, i);
            	printf("in_buf: %lu\tbuflen: %lu\n", in_buf, buflen);
            	abort();
            }
        } while (AlignAccessAlignmentEnumeratorNext(e) == 0);
        AlignAccessAlignmentEnumeratorRelease(e);
    }
    AlignAccessDBRelease(db);
}

int main_test(int argc, char *argv[])
{
    const char *path, *ipath, *refSeq;

    if ( argc < 2 )
    {
		path = "z:/experiments/NA12878.chromMT.454.ssaha.SRP000032.2009_07.bam";
		ipath = "z:/experiments/NA12878.chromMT.454.ssaha.SRP000032.2009_07.bam.bai";
        refSeq = "2";
    }
    else
    {
        path = argv [ 1 ];
        ipath = ( argc > 2 ) ? argv [ 2 ] : NULL;
        refSeq = (argc > 3) ? argv[3] : "2";
    }

//	testGetCIGAR(path);

    test(path, ipath, refSeq);

	return 0;
}

int __cdecl _tmain(int argc, _TCHAR* argv[])
{
	return main_test(argc, (char **)argv);
}

