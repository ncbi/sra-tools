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

#ifndef _h_kar_
#define _h_kar_

#ifndef _h_kfs_defs_
#include <kfs/defs.h>
#endif

#include <klib/container.h>


/********************************************************************
 *  TOC entries
 */
typedef struct KARDir KARDir;

typedef struct KAREntry KAREntry;
struct KAREntry
{
    BSTNode dad;

    KTime_t mod_time;

    const char *name;
    KARDir *parentDir;
    
    uint32_t access_mode;

    uint8_t type;
    uint8_t eff_type;

    bool the_flag;
};

struct KARDir
{
    KAREntry dad;
    
    BSTree contents;
};

typedef struct KARFile KARFile;
struct KARFile
{
    KAREntry dad;

    uint64_t byte_size;
    uint64_t byte_offset;

        /* Chunked entries */
    KTocChunk * chunks;
    uint32_t nun_chunks;
};

typedef KARFile **KARFilePtrArray;

typedef struct KARArchiveFile KARArchiveFile;
struct KARArchiveFile
{
    uint64_t starting_pos;
    uint64_t pos;
    KFile * archive;
};

typedef struct KARAlias KARAlias;
struct KARAlias
{
    KAREntry dad;

    KAREntry * resolved;
    const char *link;
};


enum
{
    ktocentrytype_unknown = -1,
    ktocentrytype_notfound,
    ktocentrytype_dir,
    ktocentrytype_file,
    ktocentrytype_chunked,
    ktocentrytype_softlink,
    ktocentrytype_hardlink,
    ktocentrytype_emptyfile,
    ktocentrytype_zombiefile
};

/********************************************************************
 *  TOC misc functions
 */
size_t kar_entry_full_path (
                            const KAREntry * entry,
                            const char * root_dir,
                            char * buffer,
                            size_t bsize
                            );

/********************************************************************
 *  TOC diagnostics, printing and whatever
 */
enum PrintMode
{
    pm_normal,
    pm_longlist
};

typedef struct KARPrintMode KARPrintMode;
struct KARPrintMode
{
    enum PrintMode pm;
    uint32_t indent;
};

void kar_print ( BSTNode *node, void *data );
void kar_print_set_max_size_fw ( uint64_t num );
void kar_print_set_max_offset_fw ( uint64_t num );

/********************************************************************
 *  Usefuls
 */

/**  allocating and copying null terminated string, dangerous.
 **/
rc_t CC kar_stdp ( const char ** Dst, const char * Src );

/**  allocating and copying 'n' characters of string
 **/
rc_t CC kar_stndp ( const char ** Dst, const char * Src, size_t n );

/**  composes path from top to node, allocates, result should be freed
 **/
rc_t CC kar_entry_path ( const char ** Ret, KAREntry * Entry );

/**  will cal 'Cb' function for node and all it's subnodes in following
 **  order : first will call it for children, and last for node itself.
 **  NOTE : it does not follow soft/hard links to directory
 **/
void CC kar_entry_for_each (
                const KAREntry * self,
                void ( CC * Cb ) ( const KAREntry * Entry, void * Data),
                void * Data
                );

/**  lookup of KAREntry by path, needed in some filtering operations
 **
 **/
    /*  if Entry in not null, kar_p2e_scan will be executed for
     *  that entry. Otherwise just BSTreeInit will be executed
     */
rc_t CC kar_p2e_init ( BSTree * Tree, KAREntry * Entry );
    /*  User are responsible for freeing Tree instance
     */
rc_t CC kar_p2e_whack ( BSTree * Tree );
    /*  Will scan and add to Tree all subentries with etry itself
     */
rc_t CC kar_p2e_scan ( BSTree * Tree, KAREntry * Entry );
    /*  Will add Entry only to a Tree
     */
rc_t CC kar_p2e_add ( BSTree * Tree, KAREntry * Entry );
    /*  Will return KAREntry for path, or null if there is no such
     */
KAREntry * CC kar_p2e_find ( BSTree * Tree, const char * Path );

/********************************************************************
 *  Simple vector pretty useful here
 */
typedef struct KARWek KARWek;
struct KARWek;

rc_t CC kar_wek_clean ( KARWek * self );
rc_t CC kar_wek_dispose ( KARWek * self );

    /* Parameters initial_size, inc_size, and destructor are optional
     * You may use 0 or NULL for initialisation
     */
rc_t CC kar_wek_make (
                    KARWek ** Wek,
                    size_t initial_capacity,
                    size_t increment_size,
                    void ( CC * destructor ) ( void * i )
                    );
/* calls kar_wek_make ( self, 0, 0, NULL ); */
rc_t CC kar_wek_make_def ( KARWek ** Wek );

rc_t CC kar_wek_append ( KARWek * self, void * Item );

size_t CC kar_wek_size ( KARWek * self );
void ** CC kar_wek_data ( KARWek * self );
void * CC kar_wek_get ( KARWek * self, size_t Idx );

/********************************************************************
 *  Some misk methods
 */

/**  will scan object children and return KARWek object with all 
 **  storeable files
 **/
rc_t CC kar_entry_list_files ( const KAREntry * self, KARWek ** Files );

/**  will scan object children and return KARWek object whth objects
 **  which satisfy filter
 **/
rc_t CC kar_entry_filter_files (
                const KAREntry * self,
                KARWek ** Files,
                bool ( CC * Fl ) ( const KAREntry * Entry, void * Data),
                void * Data
                );

/**  will scan object children and return KARWek object whth objects
 **  which has flag set in some value
 **/
rc_t CC kar_entry_filter_flag (
                                const KAREntry * self,
                                KARWek ** Files,
                                bool FlagValue
                                );

/**  will set the_flag value for entry
 **/
rc_t CC kar_entry_set_flag ( const KAREntry * self, bool Value );

#ifdef JOJOBA
Temporary removing from here
/**  General KAREntry functions
 **
 **/
void kar_entry_whack ( BSTNode *node, void *data );

rc_t CC car_entry_create ( KAREntry ** rtn, size_t entry_size,
    const KDirectory * dir, const char * name, uint32_t type,
    uint8_t eff_type
);
#endif /* JOJOBA */

#endif /*_h_kar_args_*/
