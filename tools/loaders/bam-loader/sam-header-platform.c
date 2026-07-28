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

static int platform_cmp(char const platform[], char const test[])
{
    unsigned i;

    for (i = 0; ; ++i) {
        int const ch1 = toupper(platform[i]);
        int const ch2 = toupper(test[i]);

        if (ch1 < ch2)
            return -1;
        if (ch2 < ch1)
            return 1;
        if (ch1 == '\0')
            return 0;
    }
}

static INSDC_SRA_platform_id get_platform_id(char const *qry)
{
    static char const *names[] = {
        "CAPILLARY",
        "DNBSEQ",
        "ELEMENT",
        "HELICOS",
        "ILLUMINA",
        "IONTORRENT",
        "LS454",
        "ONT",
        "PACBIO",
        "SINGULAR",
        "SOLID",
        "ULTIMA"
    };
    static const INSDC_SRA_platform_id ids[] = {
        /* "CAPILLARY"  */ SRA_PLATFORM_CAPILLARY,
        /* "DNBSEQ"     */ SRA_PLATFORM_DNBSEQ,
        /* "ELEMENT"    */ SRA_PLATFORM_ELEMENT_BIO,
        /* "HELICOS"    */ SRA_PLATFORM_HELICOS,
        /* "ILLUMINA"   */ SRA_PLATFORM_ILLUMINA,
        /* "IONTORRENT" */ SRA_PLATFORM_ION_TORRENT,
        /* "LS454"      */ SRA_PLATFORM_454,
        /* "ONT"        */ SRA_PLATFORM_OXFORD_NANOPORE,
        /* "PACBIO"     */ SRA_PLATFORM_PACBIO_SMRT,
        /* "SINGULAR".  */ SRA_PLATFORM_SINGULAR_GENOMICS,
        /* "SOLID"      */ SRA_PLATFORM_ABSOLID,
        /* "ULTIMA"     */ SRA_PLATFORM_ULTIMA,
    };
    int f = 0;
    int e = (int)(sizeof(names)/sizeof(names[0]));
    
    assert(e == (int)(sizeof(ids)/sizeof(ids[0])));
    while (f < e) {
        int const m = f + ((e - f) >> 1);
        int const diff = platform_cmp(names[m], qry);
        if (diff == 0)
            return ids[m];
        if (diff < 0)
            f = m + 1;
        else
            e = m;
    }
    return SRA_PLATFORM_UNDEFINED;
}
