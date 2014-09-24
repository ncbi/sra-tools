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
#ifndef _sra_load_file_
#define _sra_load_file_

#include <klib/defs.h>
#include <klib/log.h>
#include <kfs/file.h>
#include <kfs/directory.h>
#include <kapp/loader-file.h>

#include "run-xml.h"

/*--------------------------------------------------------------------------
* SRA reader buffered input file
*/
typedef struct SRALoaderFile SRALoaderFile;

rc_t SRALoaderFile_Make(const SRALoaderFile **cself, const KDirectory* dir, const char* filename,
                        const DataBlock* block, const DataBlockFileAttr* fileattr, const uint8_t* md5_digest, bool read_ahead);

rc_t SRALoaderFile_Release(const SRALoaderFile* cself);

/* returns true if eof is reached and buffer is empty */
rc_t SRALoaderFile_IsEof(const SRALoaderFile* cself, bool* eof);

/* print error msg file file info and return original!! rc
   if msg is NULL fmt is not used so call with NULL, NULL if no msg needs to be printed */
rc_t SRALoaderFile_LOG(const SRALoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, ...);

/* returns current buffer position in file */
rc_t SRALoaderFile_Offset(const SRALoaderFile* cself, uint64_t* offset);

/* file name */
rc_t SRALoaderFileName ( const SRALoaderFile *cself, const char **name );

rc_t SRALoaderFileFullName(const SRALoaderFile *cself, const char **name);

rc_t SRALoaderFileResolveName(const SRALoaderFile  *self, char *resolved, size_t rsize);

/* DATA_BLOCK name attribute */
rc_t SRALoaderFileBlockName ( const SRALoaderFile *cself, const char **block_name );

/* DATA_BLOCK member_name attribute */
rc_t SRALoaderFileMemberName ( const SRALoaderFile *cself, const char **member_name );

/* DATA_BLOCK sector attribute */
rc_t SRALoaderFileSector( const SRALoaderFile *cself, int64_t* sector);

/* DATA_BLOCK region attribute */
rc_t SRALoaderFileRegion( const SRALoaderFile *cself, int64_t* region);

/* FILE @quality_scoring_system */
rc_t SRALoaderFile_QualityScoringSystem( const SRALoaderFile *scelf, ExperimentQualityType* quality_scoring_system );

/* FILE @quality_encoding */
rc_t SRALoaderFile_QualityEncoding( const SRALoaderFile *cself, ExperimentQualityEncoding* quality_encoding );

/* FILE @ascii_offset */
rc_t SRALoaderFile_AsciiOffset( const SRALoaderFile *cself, uint8_t* ascii_offset );

/* FILE @filetype */
rc_t SRALoaderFile_FileType( const SRALoaderFile *cself, ERunFileType* filetype );

/* Readline
 *  makes next line from a file available in buffer.
 *  eligable EOL symbols are: \n (unix), \r (older mac), \r\n (win)
 *  EOL symbol(s) never included in buffer length.
 *  if there is no EOL at EOF - not an error.
 *  fails if internal buffer is insufficient.
 *  buffer is NULL on EOF
 *  rc state of (rcString rcTooLong) means line was too long
 *              you may copy line and readline again for the tail of the line
 *
 *  "buffer" [ OUT ] and "length" [ OUT ] - returned line and it's length
 */
rc_t SRALoaderFileReadline(const SRALoaderFile* cself, const void** buffer, size_t* length);

/* Read
*  reads "size" bytes from file and makes them available through "buffer"
*  if "advance" is > 0 than before reading skips "advance" bytes in file
*  if "size" == 0 then nothing is read and available "length" bytes is returned in "buffer"
*
*  "buffer" [ OUT ] - pointer to read bytes, "buffer" NULL means EOF
*  "length" [ OUT ] - number of read bytes, normally == size,
                      if less than requested size, rc is [rcBuffer,rcInsufficient], advance and read more!
*/
rc_t SRALoaderFileRead(const SRALoaderFile* cself, size_t advance, size_t size, const void** buffer, size_t* length);

#endif /* _sra_load_file_ */
