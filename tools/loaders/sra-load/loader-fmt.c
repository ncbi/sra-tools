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
#include <klib/log.h>
#include <klib/rc.h>

#include "loader-fmt.h"

rc_t SRALoaderFmtInit( SRALoaderFmt *self, const SRALoaderFmt_vt *vt )
{
    rc_t rc = 0;

    if( self == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcSelf, rcNull);
    } else if( vt == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcInterface, rcNull);
    } else {
        switch( vt->v1.maj ) {
            case 0:
                rc = RC(rcSRA, rcFormatter, rcConstructing, rcInterface, rcInvalid);
                break;
            case 1:
                if( vt->v1.destroy == NULL || vt->v1.version == NULL ||
                    (vt->v1.write_data == NULL && vt->v1.exec_prep == NULL) ) {
                    rc = RC(rcSRA, rcFormatter, rcConstructing, rcInterface, rcInvalid);
                }
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcConstructing, rcInterface, rcBadVersion);
                break;
        }
    }
    if( rc == 0 ) {
        self->vt = vt;
    } else if( KLogLastErrorCode() != rc ) {
        PLOGERR( klogInt, (klogInt, rc, "failed to initialize loader formatter version $(maj).$(min)",
                              "maj=%u,min=%u", vt ? vt->v1.maj : 0, vt ? vt->v1.min : 0));
    }
    return rc;
}

rc_t SRALoaderFmtRelease ( const SRALoaderFmt *cself, SRATable** table )
{
    rc_t rc = 0;

    if( cself != NULL ) {
        SRALoaderFmt *self = (SRALoaderFmt*)cself;
        switch( self->vt->v1.maj ) {
            case 1:
                rc = self->vt ->v1.destroy(self, table);
                break;

            default:
                rc = RC(rcSRA, rcFormatter, rcDestroying, rcInterface, rcBadVersion);
                PLOGERR( klogInt, (klogInt, rc, "failed to release loader formatter with interface version $(maj).$(min)",
                         "maj=%u,min=%u", self->vt->v1.maj, self->vt->v1.min));
                break;
        }
    }
    return rc;
}

rc_t SRALoaderFmtVersion ( const SRALoaderFmt *self, uint32_t *vers, const char** name )
{
    rc_t rc = 0;

    if( self == NULL || vers == NULL || name == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcAccessing, rcParam, rcNull);
    } else {
        *vers = 0;
        *name = NULL;
        switch( self->vt->v1.maj ) {
            case 1:
                rc = self->vt->v1.version(self, vers, name);
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcAccessing, rcInterface, rcBadVersion);
                PLOGERR( klogInt, (klogInt, rc, "failed to read loader formatter version with interface version $(maj).$(min)",
                    "maj=%u,min=%u", self ? self->vt->v1.maj : 0, self ? self->vt->v1.min : 0));
                break;
        }
    }
    return rc;
}

rc_t SRALoaderFmtWriteData ( SRALoaderFmt *self, uint32_t argc, const SRALoaderFile *const argv [], int64_t* spots_bad_count )
{
    rc_t rc = 0;

    if( self == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcWriting, rcSelf, rcNull);
    } else if( argv == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcArgv, rcNull);
    } else if( argc == 0 ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcArgv, rcEmpty);
    } else if( spots_bad_count == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcWriting, rcParam, rcNull);
    } else {
        switch( self->vt->v1.maj ) {
            case 1:
                if( self->vt->v1.write_data == NULL ) {
                    rc = RC(rcSRA, rcFormatter, rcCreating, rcInterface, rcNull);
                } else {
                    rc = self->vt->v1.write_data(self, argc, argv, spots_bad_count);
                }
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcWriting, rcInterface, rcBadVersion);
                PLOGERR ( klogInt, ( klogInt, rc, "failed to load data with interface version $(maj).$(min)",
                    "maj=%u,min=%u", self ? self->vt->v1.maj : 0, self ? self->vt->v1.min : 0));
                break;
        }
    }
    return rc;
}

rc_t SRALoaderFmtExecPrep(const SRALoaderFmt *self, const TArgs* args, const SInput* input,
                          const char** path, const char* eargs[], size_t max_eargs)
{
    rc_t rc = 0;

    if( self == NULL ) {
        rc = RC( rcSRA, rcFormatter, rcCreating, rcSelf, rcNull);
    } else if( args == NULL || input == NULL || path == NULL || eargs == NULL || max_eargs == 0 ) {
        rc = RC(rcSRA, rcFormatter, rcCreating, rcParam, rcInvalid);
    } else {
        switch( self->vt->v1.maj ) {
            case 1:
                if( self->vt->v1.exec_prep == NULL ) {
                    rc = RC(rcSRA, rcFormatter, rcCreating, rcInterface, rcNull);
                } else {
                    rc = self->vt->v1.exec_prep(self, args, input, path, eargs, max_eargs);
                }
                break;
            default:
                rc = RC(rcSRA, rcFormatter, rcCreating, rcInterface, rcBadVersion);
                PLOGERR ( klogInt, ( klogInt, rc, "failed to load data with interface version $(maj).$(min)",
                    "maj=%u,min=%u", self ? self->vt->v1.maj : 0, self ? self->vt->v1.min : 0));
                break;
        }
    }
    return rc;
}
