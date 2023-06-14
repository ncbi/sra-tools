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

#include "vdb-dump-helper.h"
#include "vdb-dump-inspect.h"

static rc_t inspect_file( const KFile * f ) {
    rc_t rc = 0;
    rc = KOutMsg( "...inspecting the file!\n" );
    return rc;
}

static rc_t vpath_to_file( const KDirectory * dir, const VPath * vpath, const KFile ** f ) {
    rc_t rc = 0;
    if ( VPathIsFSCompatible( vpath ) ) {
        KOutMsg( "...is fs compatible!\n" );
        rc = vdh_open_vpath_as_file( dir, vpath, f );
    } else {
        KOutMsg( "...is something else!\n" );
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcParam, rcInvalid );
    }
    return rc;
}

rc_t vdb_dump_inspect( const KDirectory * dir, const char * object ) {
    rc_t rc = KOutMsg( "inspecting: >%s<\n", object );
    if ( 0 == rc ) {
        VPath * vpath;
        rc = vdh_path_to_vpath( object, &vpath );
        if ( 0 == rc ) {
            const KFile * f = NULL;
            rc = vpath_to_file( dir, vpath, &f );
            if ( 0 == rc ) {
                rc = inspect_file( f );
                rc = vdh_kfile_release( rc, f );
            }
            rc = vdh_vpath_release( rc, vpath );
        }
    }
    return rc;
}
