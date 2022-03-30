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
#ifndef _h_kapp_loader_file_
#define _h_kapp_loader_file_

#ifndef _h_kapp_extern_
#include <kapp/extern.h>
#endif

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <stdarg.h>

#include <klib/log.h>

#ifdef __cplusplus
extern "C" {
#endif

struct KDirectory;

/*--------------------------------------------------------------------------
* SRA reader buffered input file
*/
typedef struct KLoaderFile KLoaderFile;

/*
    md5_digest - not null forces MD5 verification for the file content
    read_ahead - force reading of the file in a diff thread, ahead of time,
                 usefull on compressed file, speeds up MD5 verify too
*/
KAPP_EXTERN rc_t CC KLoaderFile_Make(const KLoaderFile **file, struct KDirectory const* dir, const char* filename,
                                     const uint8_t* md5_digest, bool read_ahead);

KAPP_EXTERN rc_t CC KLoaderFile_Release(const KLoaderFile* cself, bool exclude_from_progress);

/* temporary close the file to avoid too many open files, but stay on position */
KAPP_EXTERN rc_t CC KLoaderFile_Close(const KLoaderFile* cself);

/* restart reading from beginning of the file */
KAPP_EXTERN rc_t CC KLoaderFile_Reset(const KLoaderFile* cself);

KAPP_EXTERN rc_t CC KLoaderFile_SetReadAhead(const KLoaderFile* cself, bool read_ahead);

/* print error msg file file info and return original!! rc
   if msg is NULL fmt is not used so call with NULL, NULL if no msg needs to be printed */
KAPP_EXTERN rc_t CC KLoaderFile_LOG(const KLoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, ...);
KAPP_EXTERN rc_t CC KLoaderFile_VLOG(const KLoaderFile* cself, KLogLevel lvl, rc_t rc, const char *msg, const char *fmt, va_list args);

/* returns true if eof is reached and buffer is empty */
KAPP_EXTERN rc_t CC KLoaderFile_IsEof(const KLoaderFile* cself, bool* eof);

/* returns current buffer position in file */
KAPP_EXTERN rc_t CC KLoaderFile_Offset(const KLoaderFile* cself, uint64_t* offset);

/* returns current line number in file */
KAPP_EXTERN rc_t CC KLoaderFile_Line(const KLoaderFile* cself, uint64_t* line);

/* file name */
KAPP_EXTERN rc_t CC KLoaderFile_Name(const KLoaderFile *self, const char **name);

/* real file name */
KAPP_EXTERN rc_t CC KLoaderFile_FullName(const KLoaderFile *self, const char **name);

/* file name completly resolved */
KAPP_EXTERN rc_t CC KLoaderFile_ResolveName(const KLoaderFile *self, char *resolved, size_t rsize);

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
KAPP_EXTERN rc_t CC KLoaderFile_Readline(const KLoaderFile* self, const void** buffer, size_t* length);

/* Read
*  reads "size" bytes from file and makes them available through "buffer"
*  if "advance" is > 0 than before reading skips "advance" bytes in file
*  if "size" == 0 then nothing is read and available "length" bytes is returned in "buffer"
*
*  "buffer" [ OUT ] - pointer to read bytes, "buffer" NULL means EOF
*  "length" [ OUT ] - number of read bytes, normally == size,
                      if less than requested size, rc is [rcBuffer,rcInsufficient], advance and read more!
*/
KAPP_EXTERN rc_t CC KLoaderFile_Read(const KLoaderFile* self, size_t advance, size_t size, const void** buffer, size_t* length);

#ifdef __cplusplus
}
#endif

#endif /* _h_kapp_loader_file_ */
