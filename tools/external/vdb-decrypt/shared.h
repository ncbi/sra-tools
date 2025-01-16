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

#ifndef _tools_vdb_decrypt_shared_h_
#define _tools_vdb_decrypt_shared_h_

#define DIRECTORY_TO_DIRECTORY_SUPPORTED 0

#include <klib/defs.h>
#include <krypto/key.h>
#include <kapp/args.h>

#define OPTION_FORCE   "force"
#define OPTION_DEC_SRA "decrypt-sra-files"
#define OPTION_NGC     "ngc"

#define ALIAS_FORCE    "f"
#define ALIAS_DEC_SRA  NULL
#define ALIAS_NGC      NULL

extern const bool Decrypting;

extern char Password [];
extern size_t PasswordSize;

extern const char De [];
extern const char de [];
extern const char SraOption[];

extern const char EncExt[];
bool NameFixUp (char * name);
extern const char * ForceUsage[];
extern const char * NgcUsage[];

/* for encfile encrypt/decrypt */
extern KKey Key;


typedef enum ArcScheme
{
    arcError,
    arcNone,
    arcSRAFile
} ArcScheme;

extern bool IsArchive; /* this approach makes threading fail */

struct KFile;

ArcScheme ArchiveTypeCheck (const struct KFile * f);

typedef enum EncScheme
{
    encError,
    encNone,
    encEncFile,
    encSraEncFile,
    encWGAEncFile
} EncScheme;

void CryptOptionLines ();

bool DoThisFile (const struct KFile * infile, EncScheme enc, ArcScheme * arc);

struct Args;
rc_t CommonMain (struct Args * args);

rc_t CryptFile (const struct KFile * in, const struct KFile ** new_in,
                struct KFile * out, struct KFile ** new_out, EncScheme scheme);

extern const char UsageDefaultName[];
extern rc_t CC UsageSummary (const char * progname);
extern rc_t CC Usage (const Args * args);

#endif

/* EOF */
