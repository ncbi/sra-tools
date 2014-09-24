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


#ifndef _h_copycat_priv_
#define _h_copycat_priv_

#ifndef _h_cctree_priv_
#include "cctree-priv.h"
#endif

#ifndef _h_kfs_directory_
#include <kfs/directory.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * forwards
 */
    struct KFile;
struct KTocChunk;

/*--------------------------------------------------------------------------
 * globals
 */
extern uint32_t in_block;
extern uint32_t out_block;
extern int verbose;             /* program-wide access to verbosity level */
extern CCTree *ctree;           /* tree of nodes as seen while cataloging the input */
extern CCTree *etree;           /* tree of nodes as extracted */
extern KDirectory *cdir;        /* here we 'cache' XML files that get tweaked and passed to laoders */
extern KDirectory *edir;        /* here we extract non container/archive files for use if we can't
                                 * load without unpacking */ 
extern bool extract_dir;        /* if set we are adding directories on extraction to match our 
                                 * normal output XML */
extern bool xml_dir;            /* if set we will output XML to match our extracted files not
                                 * the original packed submission */
extern bool no_bzip2;           /* if true, don't try to decompress bzipped files */
extern bool no_md5;             /* if true, don't calculate md5 sums */
extern char epath [8192];       /* we build a path down through containes/archives */
extern char * ehere;            /* the pointer to the next character in epath during descent */
extern KCreateMode cm;          
extern struct KFile *fnull;     /* global reference to "/dev/null" or bit bucket KFile */

extern void * dump_out;
extern char ncbi_encryption_extension[];
extern char wga_encryption_extension[];

rc_t CC copycat_log_writer (void * self, const char * buffer, size_t buffer_size,
                            size_t * num_writ);
rc_t CC copycat_log_lib_writer  (void * self, const char * buffer, size_t buffer_size,
                                 size_t * num_writ);

/*--------------------------------------------------------------------------
 * copycat
 */
typedef struct ccat_pb
{
    CCTree * tree;
    const struct KFile * sf;
    KTime_t mtime;
    enum CCType ntype;
    CCFileNode * node;
    const char * name;
} ccat_pb;

/* ccat
 *  non-buffered recursive entrypoint
 *
 *  "tree" [ IN ] - immediate parent of node
 *
 *  "src" [ IN ] - file to be analyzed
 *
 *  "mtime" [ IN ] - modification time of "src"
 *
 *  "ntype" [ IN ] and "node" [ IN ] - file node and type
 *
 *  "name" [ IN ] - file leaf name
 *
 * use this call if buffering of the parent provides buffering of the child
 * such as archive formats with no compression such as kar or tar
 */
rc_t ccat_md5 ( CCTree *tree, const struct KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name );

/* ccat_buf
 *  buffered recursive entrypoint
 *
 *  "bsize" [ IN ] - requested buffer size
 *
 * use this call when recursing on a type where buffering of the parent won't 
 * help such as decompression or decryption
 */
rc_t ccat_buf ( CCTree *tree, const struct KFile *sf, KTime_t mtime,
                enum CCType ntype, CCFileNode *node, const char *name);


/* -----
 * copycat
 *
 * The copycat function is the actual copy and catalog function.
 * All before this function is called is building toward this.
 */
struct VPath;
rc_t copycat (CCTree *tree, KTime_t mtime, KDirectory * cwd,
              const struct VPath * src, const struct KFile *sf, 
              const struct VPath * dst, struct KFile *df,
              const char *spath, const char *name, 
              uint64_t expected_size, bool do_decrypt, bool do_encrypt);

/*--------------------------------------------------------------------------
 * CCFileFormat
 */
typedef struct CCFileFormat CCFileFormat;
extern CCFileFormat *filefmt;

typedef enum CCFileFormatClass
{
    ccffcError = -1,
    ccffcUnknown,
    ccffcCompressed,
    ccffcArchive,
    ccffcCached,
    ccffcEncoded
} CCFileFormatClass;

typedef enum CCFileFormatTypeCompressed
{
    ccfftcError = -1,
    ccfftcUnknown,
    ccfftcGzip,
    ccfftcBzip2,
    ccfftcZip
} CCFileFormatTypeCompressed;

typedef enum CCFileFormatTypeArchive
{
    ccfftaError = -1,
    ccfftaUnknown,
    ccfftaTar,
    ccfftaSra,
    ccfftaHD5
} CCFileFormatTypeArchive;

typedef enum CCFileFormatTypeXML
{
    ccfftxError = -1,
    ccfftxUnknown,
    ccfftxXML
} CCFileFormatTypeXML;

typedef enum CCFileFormatTypeEncoded
{
    ccffteError = -1,
    ccffteUnknown,
    ccffteNCBIErrored,
    ccffteNCBI,
    ccffteWGAErrored,
    ccffteWGA
} CCFileFormatTypeEncoded;

rc_t CCFileFormatMake ( CCFileFormat ** p );
rc_t CCFileFormatRelease ( const CCFileFormat *self );
rc_t CCFileFormatGetType ( const CCFileFormat *self, struct KFile const *file,
    const char *path, char *buffer, size_t buffsize,
    uint32_t *type, uint32_t *class );


rc_t ccat_tar ( CCContainerNode *np, const struct KFile *sf, const char *name );
rc_t ccat_sra ( CCContainerNode *np, const struct KFile *sf, const char *name );

typedef struct KSubChunkFile KSubChunkFile;

rc_t KFileMakeChunkRead (const struct KFile ** pself,
			 const struct KFile * original,
			 uint64_t size,
			 uint32_t num_chunks,
			 struct KTocChunk * chunks);

bool CCFileFormatIsNCBIEncrypted ( void  * buffer );
bool CCFileFormatIsWGAEncrypted ( void  * buffer );
/*
 * Use as DEBUG_STATUS(("format",arg,...))
 */
#define DEBUG_STATUS(msg)  DBGMSG(DBG_APP,1,msg)


rc_t copycat_log_set (void * new, void ** prev);


struct KFile;
rc_t CC CCFileMakeRead (const struct KFile ** self,
                        const struct KFile * original, rc_t * prc);
rc_t CC CCFileMakeUpdate (struct KFile ** self,
                          struct KFile * original, rc_t * prc);
rc_t CC CCFileMakeWrite (struct KFile ** self,
                         struct KFile * original, rc_t * prc);

#ifdef __cplusplus
}
#endif

#endif /* _h_copycat_priv_ */
