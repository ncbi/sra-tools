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

#ifndef _h_bam_reader_
#define _h_bam_reader_

#include <klib/rc.h>
#include <kfs/directory.h>

#include <align/extern.h>
#include <align/bam.h>

#include <loader/common-reader-priv.h>

#ifdef __cplusplus
extern "C" {
#endif

rc_t CC BamReaderFileMake( const ReaderFile **self, char const headerText[], char const path[] );

/* temporary crutches for refactoring: */
const BAMFile* ToBam(const ReaderFile *self); 
const BAMAlignment *ToBamAlignment(const Record* record);

#if 0
#ifdef TENTATIVE
/*--------------------------------------------------------------------------
 * BAMReader, a parsing thread adapter for BAMFile.
 * Creates a thread that does reading and parsing of BAM files, provides access to parsed data one record at a time. 
 */
typedef struct BAMReader BAMReader;

rc_t BAMReaderMake( const BAMReader **result,
                    char const headerText[],
                    char const path[] );

/* AddRef
 * Release
 */
rc_t BAMReaderAddRef ( const BAMReader *self );
rc_t AMReaderRelease ( const BAMReader *self );

/* GetBAMFile
 */
const BAMFile* BAMReaderGetBAMFile ( const BAMReader *self );

/* Read
 *  read an aligment
 *
 *  "result" [ OUT ] - return param for BAMAlignment object
 *   must be released with BAMAlignmentRelease
 *
 *  returns RC(..., ..., ..., rcRow, rcNotFound) at end
 */
rc_t BAMReaderRead ( const BAMReader *self, const BAMAlignment **result );

#else /*TENTATIVE*/

/* use BAMFile directly, as in the earlier version */
typedef struct BAMFile BAMReader;

#define BAMReaderMake 			BAMFileMakeWithHeader
#define BAMReaderAddRef 		BAMFileAddRef
#define BAMReaderRelease 		BAMFileRelease
#define BAMReaderGetBAMFile(p) 	(p)
#define BAMReaderRead 			BAMFileRead

#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* _h_bam_reader_ */
