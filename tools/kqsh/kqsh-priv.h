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

#ifndef _h_kqsh_priv_
#define _h_kqsh_priv_

#ifndef _h_klib_token_
#include <klib/token.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif



/*--------------------------------------------------------------------------
 * forwards
 */
struct Vector;
struct BSTNode;
struct KDylib;
struct KSymTable;
struct KDirectory;


/*--------------------------------------------------------------------------
 * library data
 */
typedef struct kqsh_libdata kqsh_libdata;
struct kqsh_libdata
{
    /* library */
    struct KDylib *lib;

    /* default manager */
    struct KSymbol *dflt;

    /* const messages */
    const char **msg;
    fptr_t *cvt;

    /* update messages */
    const char **wmsg;
    fptr_t *wvt;
};

/*--------------------------------------------------------------------------
 * kqsh
 */

/* globals
 *  "read_only" - true if update operations are not available
 *
 *  "interactive" - true if (apparently) using console interface
 *
 *  "*_data" - data for the individual libraries
 */
extern bool read_only;
extern bool interactive;
extern kqsh_libdata kdb_data;
extern kqsh_libdata vdb_data;
extern kqsh_libdata sra_data;

/* stack frame
 *  for recursive kqsh'ing
 */
typedef struct kqsh_stack kqsh_stack;
struct kqsh_stack
{
    const kqsh_stack *prev;
    uint32_t cmd_num;
};

/* kqsh
 *  main shell
 */
rc_t kqsh ( struct KSymTable *tbl, KTokenSource *src );
rc_t kqsh_exec_file ( struct KSymTable *tbl, const String *path );

/* kqsh-print
 *  understands printing KSymbol
 */
int kqsh_printf ( const char *format, ... );

/* kqsh-alter
 */
rc_t kqsh_alter ( struct KSymTable *tbl, KTokenSource *src, KToken *t );

/* kqsh-close
 */
void CC kqsh_whackobj ( struct BSTNode *n, void *ignore );
rc_t kqsh_close ( struct KSymTable *tbl, KTokenSource *src, KToken *t );

/* kqsh-help
 *  help for commands and objects
 */
rc_t kqsh_help ( struct KSymTable *tbl, KTokenSource *src, KToken *t );

/* kqsh-load
 *  dylib loading and manager open
 */
rc_t kqsh_init_libpath ( void );
void kqsh_whack_libpath ( void );
rc_t kqsh_update_libpath ( const char *path );
rc_t kqsh_system_libpath ( void );
rc_t kqsh_load ( struct KSymTable *tbl, KTokenSource *src, KToken *t );

/* kqsh-open
 *  create and open described objects
 */
rc_t kqsh_create ( struct KSymTable *tbl, KTokenSource *src, KToken *t );
rc_t kqsh_open ( struct KSymTable *tbl, KTokenSource *src, KToken *t );

/* kqsh-show
 */
rc_t kqsh_show ( struct KSymTable *tbl, KTokenSource *src, KToken *t );

/* kqsh-write
 */
rc_t kqsh_write ( struct KSymTable *tbl, KTokenSource *src, KToken *t );


#ifdef __cplusplus
}
#endif

#endif /* _h_kqsh_priv_ */
