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

struct cSRAPair;
#define DBPAIR_IMPL struct cSRAPair

#include "csra-pair.h"
#include "csra-tbl.h"
#include "meta-pair.h"
#include "dir-pair.h"
#include "sra-sort.h"
#include "ctx.h"
#include "caps.h"
#include "except.h"
#include "status.h"
#include "mem.h"
#include "map-file.h"

#include <vdb/database.h>
#include <vdb/table.h>
#include <vdb/cursor.h>
#include <klib/printf.h>
#include <klib/text.h>
#include <klib/rc.h>

#include <string.h>


FILE_ENTRY ( csra-pair );


/*--------------------------------------------------------------------------
 * cSRAPair
 */

static
void cSRAPairWhack ( cSRAPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    /* destroy me */
    MapFileRelease ( self -> seq_idx, ctx );
    MapFileRelease ( self -> sa_idx, ctx );
    MapFileRelease ( self -> pa_idx, ctx );

    DbPairDestroy ( & self -> dad, ctx );

    MemFree ( ctx, self, sizeof * self );
}

static
TablePair *cSRAPairMakeTablePair ( cSRAPair *self, const ctx_t *ctx,
     const char *member, const char *name, uint32_t align_idx, bool required, bool reorder,
     TablePair* ( * make ) ( DbPair *self, const ctx_t *ctx, const VTable *src, VTable *dst, const char *name, bool reorder ) )
{
    FUNC_ENTRY ( ctx );

    TablePair *tbl;

    if ( name == NULL )
        name = member;

    TRY ( tbl = DbPairMakeTablePair ( & self -> dad, ctx, member, name, required, reorder, make ) )
    {
        if ( tbl != NULL )
        {
            TRY ( DbPairAddTablePair ( & self -> dad, ctx, tbl ) )
            {
                ( ( cSRATblPair* ) tbl ) -> align_idx = align_idx;
                return tbl;
            }

            TablePairRelease ( tbl, ctx );
        }
    }

    return NULL;
}

static
void cSRAPairExplode ( cSRAPair *self, const ctx_t *ctx )
{
    FUNC_ENTRY ( ctx );

    TablePair *tbl;

    TRY ( tbl = cSRAPairMakeTablePair ( self, ctx, "REFERENCE", NULL, 0, true, false, cSRATblPairMakeRef ) )
    {
        self -> reference = tbl;

        TRY ( tbl = cSRAPairMakeTablePair ( self, ctx, "PRIMARY_ALIGNMENT", NULL, 1, true, true, cSRATblPairMakeAlign ) )
        {
            self -> prim_align = tbl;

            TRY ( tbl = cSRAPairMakeTablePair ( self, ctx, "SECONDARY_ALIGNMENT", NULL, 2, false, true, cSRATblPairMakeAlign ) )
            {
                self -> sec_align = tbl;

                TRY ( tbl = cSRAPairMakeTablePair ( self, ctx, NULL, "EVIDENCE_ALIGNMENT", 3, false, false, cSRATblPairMakeAlign ) )
                {
                    self -> evidence_align = tbl;

                    TRY ( tbl = cSRAPairMakeTablePair ( self, ctx, NULL, "SEQUENCE", 0, false, true, cSRATblPairMakeSeq ) )
                    {
                        DirPair *dir;

                        self -> sequence = tbl;

                        TRY ( dir = DbPairMakeDirPair ( & self -> dad, ctx, "extra", false, DbPairMakeStdDirPair ) )
                        {
                            ON_FAIL ( DbPairAddDirPair ( & self -> dad, ctx, dir ) )
                                DirPairRelease ( dir, ctx );
                        }
                    }
                }
            }
        }
    }
}

static
const char *cSRAPairGetTblMember ( const cSRAPair *self, const ctx_t *ctx, const VTable *src, const char *name )
{
    FUNC_ENTRY ( ctx );

    /* special kludge for "SEQUENCE" table */
    if ( strcmp ( name, "SEQUENCE" ) == 0 )
    {
        /* determine whether table has CMP_CSREAD */
        rc_t rc;
        const VCursor *curs;

        STATUS ( 4, "determining member name of SEQUENCE table" );

        rc = VTableCreateCursorRead ( src, & curs );
        if ( rc != 0 )
            ERROR ( rc, "VTableOpenCursorRead failed on '%s.%s'", self -> dad . full_spec, name );
        else
        {
            uint32_t idx;
            rc = VCursorAddColumn ( curs, & idx, "CMP_CSREAD" );
            if ( rc == 0 )
            {
                rc = VCursorOpen ( curs );
                if ( rc == 0 )
                    name = "CS_SEQUENCE";
            }

            VCursorRelease ( curs );
        }

        STATUS ( 4, "SEQUENCE table member name determined to be '%s'", name );
    }

    return name;
}

static DbPair_vt cSRAPair_vt =
{
    cSRAPairWhack,
    cSRAPairExplode,
    cSRAPairGetTblMember
};

DbPair *cSRAPairMake ( const ctx_t *ctx,
    const VDatabase *src, VDatabase *dst, const char *name )
{
    FUNC_ENTRY ( ctx );

    cSRAPair *db;

    TRY ( db = MemAlloc ( ctx, sizeof * db, true ) )
    {
        TRY ( DbPairInit ( & db -> dad, ctx, & cSRAPair_vt, src, dst, name, NULL ) )
        {
            static const char *exclude_tbls [] =
            {
                "EVIDENCE_ALIGNMENT",
                "PRIMARY_ALIGNMENT",
                "REFERENCE",
                "SECONDARY_ALIGNMENT",
                "SEQUENCE"
            };
            db -> dad . exclude_tbls = exclude_tbls;

            return & db -> dad;
        }

        MemFree ( ctx, db, sizeof * db );
    }

    return NULL;
}
