/*==============================================================================
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


#include "shared.h"

#include <krypto/wgaencrypt.h>
#include <krypto/encfile.h>
#include <kfs/file.h>
#include <kfs/cacheteefile.h>
#include <klib/rc.h>
#include <klib/defs.h>
#include <klib/log.h>
#include <klib/status.h>
#include <kapp/main.h>

#include <assert.h>
#include <ctype.h> /* isdigit */
#include <string.h>

/* Usage
 */
const char UsageDefaultName [] = "vdb-decrypt";
const char * UsageSra []       = { "decrypt sra archives - [NOT RECOMMENDED]",
                                   NULL };
const char De[]             = "De";
const char de[]             = "de";
const char OptionSra[] = OPTION_DEC_SRA;


static
OptDef Options[] =
{
    /* name            alias max times oparam required fmtfunc help text loc */
    { OPTION_DEC_SRA, ALIAS_DEC_SRA, NULL, UsageSra,      0, false, false },
    { OPTION_FORCE,   ALIAS_FORCE,   NULL, ForceUsage,    0, false, false },
    { OPTION_NGC,     ALIAS_NGC,     NULL, NgcUsage,      0, true,  false },
};


static
bool DecryptSraFlag = false;


const bool Decrypting = true;

void CryptOptionLines ()
{
    HelpOptionLine (ALIAS_DEC_SRA, OPTION_DEC_SRA, NULL, UsageSra);
}

bool DoThisFile (const KFile * infile, EncScheme enc, ArcScheme * parc)
{
    const KFile * Infile;
    ArcScheme arc;
    rc_t rc;

    *parc = arcNone;

    switch (enc)
    {
    default:
        STSMSG (1, ("not encrypted"));
        return false;

    case encEncFile:
        /*
         * this will apply to KEncFiles versions 1 and 2, maybe not 3
         * but will hopefully become obsolete eventually.
         */
        rc = KEncFileMakeRead (&Infile, infile, &Key); /* replace with VFSManagerOpenFileReadDirectoryRelativeInt */
        if (rc)
            return false;
        break;

    case encSraEncFile:
        /* these are NCBIsenc instead of NCBInenc */
        goto sra_enc_file;

    case encWGAEncFile:
        rc = KFileMakeWGAEncRead (&Infile, infile, Password, PasswordSize);
        if (rc)
            return false;
        break;
    }
    arc = ArchiveTypeCheck (Infile);
    KFileRelease (Infile);
    switch (arc)
    {
    default:
        return false;
    case arcNone:
        return true;
    case arcSRAFile:
        break;
    }
sra_enc_file:
    *parc = arcSRAFile;
    STSMSG (1, ("encrypted sra archive\ndecryption%s requested",
                DecryptSraFlag ? "" : " not"));
    return DecryptSraFlag;
}

bool NameFixUp (char * name)
{
    bool r = false;

    char * pc = strrchr (name, '.');
    if (pc != NULL)
    {
        if (strcmp (pc, EncExt) == 0)
        {
            pc[0] = '\0';
            r = true;
        }
    }

    pc = strrchr(name, '_');
    if (pc != NULL) {
        const char dbGaP[] = "_dbGaP-";
        if (strlen(pc) > sizeof dbGaP &&
            strncmp(pc, dbGaP, sizeof dbGaP - 1) == 0)
        {
            bool encrypted = true;
            bool dot = false;
            const char * p = pc + sizeof dbGaP - 1;
            for (; encrypted && *p; ++p) {
                if (*p == '.') {
                    dot = true;
                    break;
                }
                else if (!isdigit(*p))
                    encrypted = false;
            }
            if (encrypted && dot) {
                if (strncmp(p, ".sra", 4) == 0) {
                    strcpy(pc, ".sra");
                    r |= true;
                }
            }
        }
    }

    return r;
}

rc_t CryptFile (const KFile * in, const KFile ** new_in,
                KFile * out, KFile ** new_out, EncScheme scheme)
{
    const KFile * dec;
    rc_t rc;

    assert (in);
    assert (out);
    assert (new_in);
    assert (new_out);


    rc = KFileAddRef (out);
    if (rc)
        return rc;

    switch (scheme)
    {
    default:
    case encError:
        rc = RC (rcExe, rcFile, rcClassifying, rcFile, rcInvalid);
        break;

    case encNone:
    copy:
        rc = KFileAddRef (in);
        if (rc)
            goto fail;
        *new_in = in;
        *new_out = out;
        STSMSG (1, ("not encrypted just copying"));
        return 0;

    case encEncFile:
        rc = KEncFileMakeRead (&dec, in, &Key);
    made_enc:
        if (rc)
            goto fail;

        switch (ArchiveTypeCheck (dec))
        {
        default:
        case arcError:
            rc = RC (rcExe, rcFile, rcClassifying, rcFile, rcInvalid);
            break;

        case arcSRAFile:
            if (!DecryptSraFlag)
            {
                rc = KFileRelease (dec);
                if (rc)
                {
                    KFileRelease (dec);
                    KFileRelease (in);
                    goto fail;
                }
                goto copy;
            }
            /* fall through */
        case arcNone:
            *new_out = out;
            *new_in = dec;
            return 0;
        }
        break;

    case encWGAEncFile:
        rc = KFileMakeWGAEncRead (&dec, in, Password, PasswordSize);
        goto made_enc;
        break;
    }
    fail:
        KFileRelease (out);
        *new_in = *new_out = NULL;
        return rc;
}


MAIN_DECL( argc, argv )
{
    VDB_INITIALIZE(argc, argv, VDB_INIT_FAILED);

    Args * args;
    rc_t rc;

    SetUsage( Usage );
    SetUsageSummary( UsageSummary );

    KStsLevelSet (1);

    rc = ArgsMakeAndHandle (&args, argc, argv, 1, Options,
                            sizeof (Options) / sizeof (Options[0]));
    if (rc)
        LOGERR (klogInt, rc, "failed to parse command line parameters");

    else
    {
        uint32_t ocount;

        rc = ArgsOptionCount (args, OPTION_DEC_SRA, &ocount);
        if (rc)
            LOGERR (klogInt, rc, "failed to examine decrypt "
                    "sra option");
        else
        {
            DecryptSraFlag = (ocount > 0);

            rc = CommonMain (args);
        }
        ArgsWhack (args);
    }

    if (rc)
        STSMSG (1, ("exiting: %R (%u)", rc, rc));
    else
        STSMSG (1, ("exiting: success"));
    return VDB_TERMINATE( rc );
}


/* EOF */
