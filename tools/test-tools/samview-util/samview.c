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
 *
 */

#include <kapp/args.h>
#include <kapp/main.h>
#include <klib/log.h>

#include <stdlib.h>
#include <stdio.h>

#include <align/bam.h>

static
void samview(char const file[])
{
    char buffer[64*1024];
    BAMFile const *bam;
    rc_t rc = BAMFileMake(&bam, file);

    if (rc == 0) {
        char const *header;
        size_t hsize;
        BAMAlignment const *rec;

        BAMFileGetHeaderText(bam, &header, &hsize);
        fwrite(header, 1, hsize, stdout);

        while ((rc = BAMFileRead2(bam, &rec)) == 0) {
            size_t actsize = 0;

            if (BAMAlignmentFormatSAM(rec, &actsize, sizeof(buffer), buffer) != 0)
	        break;
            fwrite(buffer, 1, actsize, stdout);
            BAMAlignmentRelease(rec);
        }
        BAMFileRelease(bam);
    }
    LOGERR(klogWarn, rc, "Final RC");
}

const char UsageDefaultName[] = "samview-util";

MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    while (--argc) {
        const char * arg = *++argv;
        if ( arg [ 0 ] == '-' ) {
            if (arg [ 1 ] == 'V') {
                HelpVersion ( UsageDefaultName, KAppVersion () );
                return 0;
            }
        }
        samview(*argv);
    }
    return VDB_TERMINATE( 0 );
}
