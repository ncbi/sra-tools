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

#ifndef _h_KVPFile_
#define _h_KVPFile_

#ifndef _h_kdb_btree_
#include <kdb/btree.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* this can be made to work as we go forward
 */
typedef struct KVPFile KVPFile;


/* generic comparison function
 *  size_t indicates bytes
 */
typedef KBTreeCompareFunc KVPFileCompareFunc;


/* going to make an index with:                                                                        
 *  a) backing file                                                                                     
 *  b) page size is fixed at 32K, otherwise the page size would have to be stored somewhere             
 *  c) page cache - need cache limit in bytes                                                           
 *  d) 2 chunked value streams - need key chunking and value chunking                                   
 *  e) B-Tree-ish index, requiring a comparison function. this interface uses opaque keys.              
 *     the "matched-count" parameter is useful for trie traversal, but this will be built-in            
 *                                                                                                      
 *  "rslt" [ OUT ] - return of Ken's object                                                             
 *                                                                                                      
 *  "file" [ IN ] - open file with read/write permissions.                                              
 *   NB - a reference will be attached to this file, so you still own it                                
 *                                                                                                      
 *  "climit" [ IN ] - cache limit in bytes. the internal page cache will                                
 *   retain UP TO ( but not exceeding ) the limit specified. for 32K pages,                             
 *   this means than the byte values 0..0x7FFF will result in NO CACHE.                                 
 *   internally, this limit is converted to a page count by simple division,                            
 *   i.e. cpage-limit = climit / PGSIZE.                                                                
 *                                                                                                      
 *  "kchunk_size" [ IN ] and "vchunk_size" [ IN ] - the "chunking" ( alignment )                        
 *   factor for storing keys and values, respectively. rounded up to the nearest                        
 *   power of 2 that will hold the bytes specified.                                                     
 *                                                                                                      
 *  "min_ksize" [ IN ] and "max_ksize" [ IN ] - specifies the allowed key sizes. if                     
 *   unknown, a min of 1 and a max of ~ ( size_t ) 0 will allow for any size. if fixed,                 
 *   min_ksize == max_ksize will indicate this, and the size will be used exactly as                    
 *   chunking factor ( see "kchunk_size" above ).                                                       
 *                                                                                                      
 *  "min_vsize" [ IN ] and "max_vsize" [ IN ] - specifies allowed value sizes. see above.               
 *                                                                                                      
 *  "cmp" [ IN ] - comparison callback function for opaque keys. later, we'll want to                   
 *   make use of key type knowledge to improve traversal ( callbacks are expensive ), but               
 *   for now, we can treat them as opaque.                                                              
 */
rc_t KVPFileMake ( KVPFile **rslt, struct KFile *file, size_t climit,
                  size_t kchunk_size, size_t vchunk_size,
                  size_t min_ksize, size_t max_ksize, size_t min_vsize, size_t max_vsize,
                  KVPFileCompareFunc cmp );



/* this whacker works fine
 *
 * "commit" [ IN ] - if false, tells the underlying pagefile
 *  to forget its backing before whacking the cache.
 */
rc_t KVPFileWhack ( KVPFile *self, bool commit );


/* results point directly into pages, and thus require a reference
 * I'm going to invent a structure to use and some functions - the
 * structure itself may change, but we should be able to make it work
 *
 * it will be an defined structure, although it may be defined simply as
 * an array of 3 size_t to make it opaque. if it were made an allocation,
 * then we'd have to have a special allocator in order to keep the noise
 * down. maybe that's the way to go, but it means more code. for now,...
 */
typedef struct KBTreeValue KVPValue;


/* whacker */
void KVPValueWhack ( KVPValue *self );

/* return the pointer to thingie */
rc_t KVPValueAccessRead ( const KVPValue *self, const void **mem, size_t *bytes );
rc_t KVPValueAccessUpdate ( KVPValue *self, void **mem, size_t *bytes );


/* find will not modify the index in any way
 *  returns a pointer into a page, meaning that the values cannot exceed 32K in size
 *  this is done
 *
 *  "val" [ OUT ] - pointer to an uninitialized block that will contain
 *   information on how to access data within a page. note that the value
 *   is "live", and if accessed for update will cause the page to be modified.
 *   it is not clear whether this is desired or side-effect behavior, so I'm
 *   leaving it alone for now.
 *
 *  "key" [ IN ] and "ksize" [ IN ] - opaque key description
 */
rc_t KVPFileFind ( const KVPFile *self, KVPValue *val, const void *key, size_t ksize );



/* find or insert will return an existing or create a new entry
 * note that the new entry is initially zeroed
 *
 *  "val" [ OUT ] - pointer to uninitialized block that will contain value data.
 *   since the intention is to modify the value, access to val will most certainly
 *   be via "KVPValueAccessUpdate" which will dirty the page regardless.
 *
 *  "was_inserted" [ OUT ] - if true, the returned value was the result of an
 *   insertion and can be guaranteed to be all 0 bits. otherwise, the returned
 *   value will be whatever was there previously.
 *
 *  "alloc_size" [ IN ] - the number of value bytes to allocate upon insertion,
 *   i.e. if the key was not found. this value must agree with the limits specified
 *   in Make.
 *
 *  "key" [ IN ] and "ksize" [ IN ] - opaque key description
 */
rc_t KVPFileFindOrInsert ( KVPFile *self, KVPValue *val,
                          bool *was_inserted, size_t alloc_size,
                          const void *key, size_t ksize );

rc_t KVPFileForEach ( const KVPFile *self,
    void ( CC * f )( const void *key, size_t ksize, KVPValue *val, void *ctx ),
    void *ctx);
    
#ifdef __cplusplus
}
#endif

#endif /* _h_KVPFile_ */
