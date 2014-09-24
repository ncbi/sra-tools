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
#include <klib/text.h>
#include <os-native.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

typedef struct PacBioLoaderXmlFmt PacBioLoaderXmlFmt;
#define SRALOADERFMT_IMPL PacBioLoaderXmlFmt
#include "loader-fmt.h"

#include "pacbio-loadxml.vers.h"
#include "debug.h"

struct PacBioLoaderXmlFmt {
    SRALoaderFmt dad;
};

static
rc_t PacBioLoaderXmlFmt_ExecPrep(const PacBioLoaderXmlFmt *self, const TArgs* args, const SInput* input,
                                 const char** path, const char* eargs[], size_t max_eargs)
{
    rc_t rc = 0;
    uint32_t i = 0;
    ERunFileType type = rft_Unknown;

    *path = "pacbio-load";

    /* accept only single file submissions */
    if( input->count != 1 ) {
        rc = RC(rcSRA, rcFormatter, rcExecuting, rcParam, input->count ? rcExcessive : rcInsufficient);
    } else if( input->blocks[0].count != 1 ) {
        rc = RC(rcSRA, rcFormatter, rcExecuting, rcParam, input->blocks[0].count ? rcExcessive : rcInsufficient);
    } else if( (rc = SRALoaderFile_FileType(input->blocks[0].files[0], &type)) == 0 && type != rft_PacBio_HDF5 ) {
        rc = RC(rcSRA, rcFormatter, rcExecuting, rcFormat, rcUnsupported);
    } else {
        char resolved[4096];
        size_t il = string_copy_measure(resolved, sizeof(resolved), args->_input_path);
        resolved[il++] = '/';
        if( (rc = SRALoaderFileResolveName(input->blocks[0].files[0], &resolved[il], sizeof(resolved) - il)) == 0 ) {
            eargs[i] = strdup(resolved);
            if( eargs[i++] == NULL ) {
                rc = RC(rcSRA, rcFormatter, rcExecuting, rcMemory, rcExhausted);
            }
        }
        if( rc == 0 ) {
            eargs[i++] = "-o";
            eargs[i++] = args->_target;

            if( args->_force_target ) {
                eargs[i++] = "-f";
            }
            eargs[i] = NULL;
        }
    }
    return rc;
}

static
rc_t PacBioLoaderXmlFmt_Whack(PacBioLoaderXmlFmt *self, SRATable** table)
{
    free(self);
    return 0;
}

const char UsageDefaultName[] = "pacbio-loadxml";

uint32_t KAppVersion(void)
{
    return PACBIO_LOADXML_VERS;
}

static
rc_t PacBioLoaderXmlFmt_Version (const PacBioLoaderXmlFmt* self, uint32_t *vers, const char** name )
{
    *vers = PACBIO_LOADXML_VERS;
    *name = "PacBioXml";
    return 0;
}

static SRALoaderFmt_vt_v1 vtPacBioLoaderXmlFmt =
{
    1, 0,
    PacBioLoaderXmlFmt_Whack,
    PacBioLoaderXmlFmt_Version,
    PacBioLoaderXmlFmt_ExecPrep,
    NULL
};

rc_t SRALoaderFmtMake(SRALoaderFmt **self, const SRALoaderConfig *config)
{
    rc_t rc = 0;
    const PlatformXML* platform;

    if( self == NULL || config == NULL ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcNull);
    } else if( (rc = Experiment_GetPlatform(config->experiment, &platform)) != 0 ) {
    } else if( platform->id != SRA_PLATFORM_PACBIO_SMRT ) {
        rc = RC(rcSRA, rcFormatter, rcConstructing, rcParam, rcInvalid);
        LOGERR(klogInt, rc, "platform type");
    } else {
        PacBioLoaderXmlFmt* fmt;

        *self = NULL;
        fmt = calloc(1, sizeof(*fmt));
        if(fmt == NULL) {
            rc = RC(rcSRA, rcFormatter, rcConstructing, rcMemory, rcExhausted);
        } else if( (rc = SRALoaderFmtInit(&fmt->dad, (const SRALoaderFmt_vt*)&vtPacBioLoaderXmlFmt)) != 0 ) {
            LOGERR(klogInt, rc, "failed to initialize parent object");
            PacBioLoaderXmlFmt_Whack(fmt, NULL);
        } else {
            *self = &fmt->dad;
        }
    }
    return rc;
}
