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

#ifndef _h_sra_sort_map_file_
#define _h_sra_sort_map_file_

#ifndef _h_sra_sort_defs_
#include "sort-defs.h"
#endif


/*--------------------------------------------------------------------------
 * forwards
 */
struct IdxMapping;


/*--------------------------------------------------------------------------
 * MapFile
 *  a file for storing id mappings
 */
typedef struct MapFile MapFile;


/* Make
 *  creates an id map
 */
MapFile *MapFileMake ( const ctx_t *ctx, const char *name, bool random_access );


/* MakeForPoslen
 *  creates an id map with an additional poslen file
 *  this is specially for PRIMARY_ALIGNMENT_IDS and SECONDARY_ALIGNMENT_IDS
 *  that use *_ALIGNMENT global position and length as a sorting key
 *  we drop it to a file after its generation so that it can be
 *  picked up later when copying the corresponding alignment table.
 */
MapFile *MapFileMakeForPoslen ( const ctx_t *ctx, const char *name );


/* Release
 */
void MapFileRelease ( const MapFile *self, const ctx_t *ctx );

/* Duplicate
 */
MapFile *MapFileDuplicate ( const MapFile *self, const ctx_t *ctx );


/* SetIdRange
 *  required second-stage initialization
 *  must be called before any writes occur
 */
void MapFileSetIdRange ( MapFile *self, const ctx_t *ctx,
    int64_t first_id, uint64_t num_ids );


/* First
 *  return first mapped id
 * Count
 *  return number of ids
 */
int64_t MapFileFirst ( const MapFile *self, const ctx_t *ctx );
uint64_t MapFileCount ( const MapFile *self, const ctx_t *ctx );


/* SetOldToNew
 *  write new=>old id mappings
 */
void MapFileSetOldToNew ( MapFile *self, const ctx_t *ctx,
    struct IdxMapping const *ids, size_t count );


/* SetNewToOld
 *  write new=>old id mappings
 */
void MapFileSetNewToOld ( MapFile *self, const ctx_t *ctx,
    struct IdxMapping const *ids, size_t count );


/* SetPoslen
 *  write global position/length in new-id order
 */
void MapFileSetPoslen ( MapFile *self, const ctx_t *ctx,
    struct IdxMapping const *ids, size_t count );


/* ReadPoslen
 *  read global position/length in new-id order
 *  starts reading at the indicated row
 *  returns the number read
 */
size_t MapFileReadPoslen ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, uint64_t *poslen, size_t max_count );


/* AllocMissingNewIds
 *  it is possible to have written fewer new=>old mappings
 *  than are supposed to be there, e.g. SEQUENCE that gets
 *  initially written by align tables. unaligned sequences
 *  need to fill in the remainder.
 *
 *  returns the first newly allocated id
 */
int64_t MapFileAllocMissingNewIds ( MapFile *self, const ctx_t *ctx );


/* SelectOldToNewPairs
 *  specify the range of new ids to select from
 *  read them in old=>new order
 */
size_t MapFileSelectOldToNewPairs ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, struct IdxMapping *ids, size_t max_count );


/* SelectOldToNewSingle
 *  specify the range of new ids to select from
 *  read them in old=>new order
 */
size_t MapFileSelectOldToNewSingle ( const MapFile *self, const ctx_t *ctx,
    int64_t start_id, int64_t *ids, uint32_t *opt_ord, size_t max_count );


/* MapSingleOldToNew
 *  reads a single old=>new mapping
 *  returns new id or 0 if not found
 *  optionally allocates a new id if "insert" is true
 */
int64_t MapFileMapSingleOldToNew ( MapFile *self, const ctx_t *ctx,
    int64_t old_id, bool insert );


/* MapMultipleOldToNew
 *  reads a single old=>new mapping
 *  returns new id or 0 if not found
 *  optionally allocates a new id if "insert" is true
 */
void MapFileMapMultipleOldToNew ( MapFile *self, const ctx_t *ctx,
    int64_t *ids, size_t count, bool insert );


/* ConsistencyCheck
 *  should be const, but MapFileOldToNew claims to be non-const
 *  and is used within. only non-const if "insert" is true.
 */
void MapFileConsistencyCheck ( const MapFile *self, const ctx_t *ctx );


#endif
