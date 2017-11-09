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

#include "sra-stat.h" /* Ctx */

#include <insdc/sra.h> /* INSDC_coord_len */

#include <kdb/table.h> /* KTable */

#include <klib/container.h> /* BSTNode */
#include <klib/debug.h> /* DBGMSG */
#include <klib/log.h> /* LOGERR */
#include <klib/out.h> /* OUTMSG */
#include <klib/rc.h>

#include <vdb/blob.h> /* VBlob */
#include <vdb/cursor.h> /* VCursor */
#include <vdb/database.h> /* VDatabase */
#include <vdb/table.h> /* VTable */
#include <vdb/vdb-priv.h> /* VTableOpenKTableRead */

typedef struct {
    BSTNode n;
    uint64_t length;
} Contig;

static void CC ContigWhack ( BSTNode * n, void * data ) {
    free ( n );
}

static
int64_t CC ContigSort ( const BSTNode * item, const BSTNode * n )
{
    const Contig * contigl = ( const Contig * ) item;
    const Contig * contigr = ( const Contig * ) n;

    assert ( contigl && contigr);

    return contigl -> length < contigr -> length;
}

typedef struct {
    uint64_t assemblyLength;
    uint64_t contigLength;

    uint64_t count;
    uint64_t length;

    BSTree bt;

    uint64_t l50;
    uint64_t n50;
    uint64_t l90;
    uint64_t n90;
} Contigs;

static void CC ContigNext ( BSTNode * n, void * data ) {
    const Contig * contig = ( const Contig * ) n;
    Contigs * nl = ( Contigs * ) data;

    assert ( contig && nl );

    ++ nl -> count;
    nl -> length += contig -> length;

    DBGMSG ( DBG_APP, DBG_COND_1, ( "Contig %lu/%lu: %lu. Total: %lu/%lu\n",
        nl -> count, nl -> contigLength, contig -> length,
        nl -> length, nl -> assemblyLength ) );

    if ( nl -> l50 == 0 && nl -> length * 2 >= nl -> assemblyLength ) {
        nl -> n50 = contig -> length;
        nl -> l50 = nl -> count;
        DBGMSG ( DBG_APP, DBG_COND_1, ( "L50: %lu, N50: %lu (%lu>=%lu/2)\n",
            nl -> l50, nl -> n50, nl -> length, nl -> assemblyLength ) );
    }

    if ( nl -> l90 == 0 &&
        .9 * nl -> assemblyLength <= nl -> length )
    {
        nl -> n90 = contig -> length;
        nl -> l90 = nl -> count;
        DBGMSG ( DBG_APP, DBG_COND_1, ( "L90: %lu, N90: %lu (%lu*.9>=%lu)\n",
            nl -> l90, nl -> n90, nl -> length, nl -> assemblyLength ) );
    }
}

static void ContigsInit ( Contigs * self ) {
    assert ( self );

    memset ( self, 0, sizeof * self );
}

static rc_t ContigsAdd ( Contigs * self, uint32_t length ) {
    Contig * contig = ( Contig * ) calloc ( 1, sizeof * contig );

    assert ( self );

    if ( contig == NULL )
        return RC ( rcExe, rcStorage, rcAllocating, rcMemory, rcExhausted );

    self -> assemblyLength += length;
    ++ self -> contigLength;

    contig -> length = length;
    return BSTreeInsert ( & self -> bt, ( BSTNode * ) contig, ContigSort );
}

static void ContigsCalculateStatistics ( Contigs * self ) {
    assert ( self );

    BSTreeForEach ( & self -> bt, false, ContigNext, self );
}

static void ContigsFini ( Contigs * self ) {
    assert ( self );

    BSTreeWhack ( & self -> bt, ContigWhack, NULL );
}

/* Calculate N50, L50 statistics:
see https://en.wikipedia.org/wiki/N50,_L50,_and_related_statistics */
rc_t CC CalculateNL ( const VDatabase * db, Ctx * ctx ) {
    rc_t rc = 0;

    const VTable * tbl = NULL;
    const KTable * ktbl = NULL;
    const KIndex * idx = NULL;
    const VCursor * cursor = NULL;

    uint32_t CMP_READ;
    uint32_t READ_LEN;

    Contigs contigs;

    int64_t start = 1;
    uint64_t count = 0;

    /* Statictics is calculated just for VDatabase-s */
    if ( db == NULL ) {
        DBGMSG ( DBG_APP, DBG_COND_1,
            ( "CalculateAssemblyStatistics skipped: not a database\n" ) );
        return 0;
    }

    rc = VDatabaseOpenTableRead ( db, & tbl, "REFERENCE" );
    /* Statictics is calculated just for VDatabase-s with REFERENCE */
    if ( rc != 0 && GetRCState ( rc ) == rcNotFound ) {
        DBGMSG ( DBG_APP, DBG_COND_1,
            ( "CalculateAssemblyStatistics skipped: no REFERENCE table\n" ) );
        return 0;
    }

    ContigsInit ( & contigs );

    rc = VTableOpenKTableRead ( tbl, & ktbl );
    DISP_RC ( rc, "while calling "
        "VTableOpenKTableRead(VDatabaseOpenTableRead(REFERENCE))");
    if ( rc == 0 ) {
        rc = KTableOpenIndexRead ( ktbl, & idx, "i_name" );
        DISP_RC ( rc, "while calling KTableOpenIndexRead"
            "(VTableOpenKTableRead(VDatabaseOpenTableRead(REFERENCE),i_name)");
    }
    if ( rc == 0 ) {
        rc = VTableCreateCursorRead ( tbl, & cursor );
        DISP_RC ( rc, "while calling VTableCreateCursorRead(REFERENCE)");
    }
    if ( rc == 0 ) {
        rc = VCursorAddColumn ( cursor,  & CMP_READ, "CMP_READ" );
        DISP_RC ( rc, "while calling VCursorAddColumn(REFERENCE,CMP_READ)");
    }
    if ( rc == 0 ) {
        rc = VCursorAddColumn ( cursor,  & READ_LEN, "READ_LEN" );
        DISP_RC ( rc, "while calling VCursorAddColumn(REFERENCE,READ_LEN)");
    }
    if ( rc == 0 ) {
        rc = VCursorOpen ( cursor );
        DISP_RC ( rc, "while calling VCursorOpen(REFERENCE)");
    }

    for ( start = 1; rc == 0; start += count ) {
        uint32_t row_len = 0;
        const VBlob * blob = NULL;
        size_t key_size = 0;
        char key [ 4096 ];
        rc = KIndexProjectText ( idx, start, & start, & count,
                                 key, sizeof key, & key_size );
        if ( rc == SILENT_RC
            ( rcDB, rcIndex, rcProjecting, rcId, rcNotFound ) )
        {
            rc = 0;
            break; /* no more references */
        }
        DISP_RC ( rc, "while calling KIndexProjectText(KTableOpenIndexRead"
            "(VTableOpenKTableRead(VDatabaseOpenTableRead(REFERENCE),i_name)");
        if ( rc == 0 ) {
            rc = VCursorGetBlobDirect ( cursor, & blob, start, CMP_READ );
            DISP_RC ( rc, "while calling VCursorGetBlobDirect(CMP_READ)" );
        }
        if ( rc == 0 ) {
            uint32_t elem_bits = 0;
            const void * base = NULL;
            uint32_t boff = 0;
            rc = VBlobCellData ( blob, start,
                                 & elem_bits, & base, & boff, & row_len );
            DISP_RC ( rc, "while calling VBlobCellData(CMP_READ)" );
        }
        if ( rc == 0 ) {
            if ( row_len == 0 ) {
                /* When CMP_READ is not empty - local reference.
                   We calculate statistics just for local references */
                DBGMSG ( DBG_APP, DBG_COND_1, ( "CalculateAssemblyStatistics: "
                    "%s skipped: not a local reference\n", key ) );
            }
            else {
                uint64_t length = 0;
                INSDC_coord_len buffer = 0;
                uint32_t row_len = 0;
                rc = VCursorReadDirect ( cursor, start,
                    READ_LEN, 8, & buffer, sizeof buffer, & row_len );
                DISP_RC ( rc, "while calling VCursorReadDirect(READ_LEN,id)" );
                if ( rc == 0 )
                    length = buffer;
                if ( rc == 0 && count > 1 ) {
                    INSDC_coord_len buffer = 0;
                    uint32_t row_len = 0;
                    rc = VCursorReadDirect ( cursor, start + count - 1,
                        READ_LEN, 8, & buffer, sizeof buffer, & row_len );
                    DISP_RC ( rc,
                        "while calling VCursorReadDirect(READ_LEN,id+count)" );
                    if ( rc == 0 )
                        length = length * ( count - 1) + buffer;
                }
                if ( rc == 0 )
                    rc = ContigsAdd ( & contigs, length );
            }
        }
        RELEASE ( VBlob, blob );
    }

    if ( rc == 0 )
        ContigsCalculateStatistics ( & contigs );

    RELEASE ( VCursor, cursor );
    RELEASE ( KIndex, idx );
    RELEASE ( KTable, ktbl );
    RELEASE ( VTable, tbl );

    if ( rc == 0 ) {
        assert ( ctx );
        assert ( contigs . assemblyLength == contigs . length );
        assert ( contigs . contigLength == contigs . count );
        if ( contigs . n90 > 0 ) {
            ctx -> l50 = contigs . l50;
            ctx -> n50 = contigs . n50;
            ctx -> l90 = contigs . l90;
            ctx -> n90 = contigs . n90;
            ctx -> l = contigs . contigLength;
            ctx -> n = contigs . assemblyLength;
        }
    }

    ContigsFini ( & contigs );

    return rc;
}
