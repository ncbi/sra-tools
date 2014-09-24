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

#include "kqsh-priv.h"
#include "kqsh-tok.h"

#include <klib/container.h>
#include <klib/symbol.h>
#include <klib/symtab.h>
#include <klib/token.h>

#include <klib/log.h>
#include <klib/rc.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


/* alias
 */
static
rc_t kqsh_help_alias ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "create a new name for an existing object\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}


/* alter
 */
static
rc_t kqsh_help_alter_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "alter a cursor\n"
             "\n"
             "usage:\n"
             "  'alter <cursor> add column [ ( TYPEDECL ) ] NAME;'\n"
             "\n"
             "  the 'alter <cursor> add' command allows addition of columns\n"
             "  before the cursor has been opened for use.\n"
        );

    return 0;
}
static
rc_t kqsh_help_alter_schema ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "alter an open schema\n"
             "\n%s%s%s%s",
             "usage:\n"
             "  'alter <schema> add include [ path ] PATH;'\n"
             "  'alter <schema> add path PATH;'\n"
             "  'alter <schema> load PATH;'\n"
             "\n",
             "  the 'alter <schema> add' command allows addition of new\n"
             "  search paths that affect loading and schema include directives.\n"
             "\n",
             "  the 'alter <schema> load' command will load schema from the\n"
             "  indicated file.\n",
             "\n"
             "  the keywords 'alter schema' may be used instead of 'alter' alone.\n"
        );

    return 0;
}

static
rc_t kqsh_help_alter ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        case kw_cursor:
            return kqsh_help_alter_cursor ( tbl, src, t );
        case kw_schema:
            return kqsh_help_alter_schema ( tbl, src, t );

        default:
            return expected ( t, klogWarn, "cursor or schema" );
        }
    }

    printf ( "alter an open object\n"
             "\n%s%s",
             "usage:\n"
             "  alter <vdb manager>...\n"
             "  alter <sra manager> ...\n"
             "  alter <cursor> ...\n"
             "  alter <schema> ...\n"
             "\n",
             "  the alter command is used to modify open objects in some way.\n"
             "\n"
             "  type 'help alter <object>;' for more information about that command.\n"
        );

    return 0;
}


/* close
 */
static
rc_t kqsh_help_close ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "close an open object or cursor row\n"
             "\n"
             "usage:\n"
             "  'close ID;'\n"
             "  'close row [ on ] <cursor>;\n"
        );

    return 0;
}


/* create
 */
static
rc_t kqsh_help_create_column ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "create or open a stand-alone column using kdb manager (OBSCURE)\n"
             "\n"
             "usage:\n"
             "  'create column PATH [ as ID ] [ using KDB-MGR ];'\n"
             "\n"
             "  create a column using implicit or named kdb manager.\n"
             "  type 'help create;' for information on create semantics.\n"
             "\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}

static
rc_t kqsh_help_create_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "create a row cursor upon a table\n"
             "\n"
             "usage:\n"
             "  'create cursor ID on TBL [ for update ];'\n"
             "\n"
             "  create a named cursor on the given table. the default cursor mode is\n"
             "  read-only, but may be used for write 'for update' is specified.\n"
        );

    return 0;
}

static
rc_t kqsh_help_create_database ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "create or open a databse\n"
             "\n%s%s",
             "usage:\n"
             "  'create database PATH [ as ID ] [ using KDB-MGR ];'\n"
             "  'create database PATH [ as ID ]\n"
             "     [ with ] schema ID.ID [ using MGR ];'\n"
             "  'create database ID [ as ID ] [ using KDB-DB ];'\n"
             "  'create database ID [ as ID ]\n"
             "     [ with ] schema ID [ using DB ];'\n"
             "\n",
             "  create a database using implicit or named manager or database.\n"
             "  type 'help create;' for information on create semantics.\n"
             "  when using a database schema, the target object may be any\n"
             "  manager or database above kdb level.\n"
             "\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}

static
rc_t kqsh_help_create_schema ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "create an empty schema object\n"
             "\n"
             "usage:\n"
             "  'create schema as ID [ using MGR ];'\n"
             "\n"
             "  create a new schema populated only by intrinsic definitions.\n"
             "  the manager used must be above kdb level.\n"
        );

    return 0;
}

static
rc_t kqsh_help_create_table ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "create or open a table\n"
             "\n%s%s",
             "usage:\n"
             "  'create table PATH [ as ID ] [ using KDB-MGR ];'\n"
             "  'create table PATH [ as ID ]\n"
             "     [ with ] schema ID.ID [ using MGR ];'\n"
             "  'create table ID [ as ID ] [ using KDB-DB ];'\n"
             "  'create table ID [ as ID ]\n"
             "     [ with ] schema ID [ using DB ];'\n"
             "\n",
             "  create a table using implicit or named manager or database.\n"
             "  type 'help create;' for information on create semantics.\n"
             "  when using a table schema, the target object may be any\n"
             "  manager or database above kdb level.\n"
        );

    return 0;
}

static
rc_t kqsh_help_create ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        case kw_column:
            return kqsh_help_create_column ( tbl, src, t );
        case kw_cursor:
            return kqsh_help_create_cursor ( tbl, src, t );
        case kw_database:
            return kqsh_help_create_database ( tbl, src, t );
        case kw_schema:
            return kqsh_help_create_schema ( tbl, src, t );
        case kw_table:
            return kqsh_help_create_table ( tbl, src, t );

        default:
            return expected ( t, klogWarn, "column, cursor, database, schema, table or ;" );
        }
    }

    printf ( "create a new object or open/reinitialize an existing one\n"
             "\n%s%s%s%s",
             "usage:\n"
             "  create ...            - create a new object or fail if it already exists.\n"
             "  create or replace ... - create a new object or reinitialize existing one.\n"
             "  create or open ...    - create a new object or open existing one for update.\n"
             "\n",
             "  create column ...\n"
             "  create cursor ...\n"
             "  create database ...\n"
             "  create schema ...\n"
             "  create table ...\n"
             "\n",
             "  the create command is used primarily to create new objects, but may\n"
             "  also be used to open and optionally reinitialize certain types of\n"
             "  existing objects, namely database, tables and columns.\n"
             "\n",
             "semantic variations on databases, tables or columns:\n"
             "  create or open - create if not there, open otherwise [ open ( O_CREAT ) ]\n"
             "  create or init - create if not there, reinitialize otherwise [ open ( O_CREAT | O_TRUNC ) ]\n"
             "\n"
             "  type 'help create <object>;' for more information about that command.\n"
        );

    return 0;
}

/* delete
 */
static
rc_t kqsh_help_delete ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "delete rows or nodes\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}


/* drop
 */
static
rc_t kqsh_help_drop ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "remove an object from the file system\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}


/* execute
 */
static
rc_t kqsh_help_execute ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "run a kqsh script in a sub-scope\n"
        );

    return 0;
}


/* help
 */
static
rc_t kqsh_help_help ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "print help\n"
             "\n"
             "usage:\n"
             "  'help <topic>;'\n"
        );

    return 0;
}


/* open
 */
static
rc_t kqsh_help_open_mgr ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "open a library manager\n"
             "\n%s%s%s",
             "usage:\n"
             "  'open kdb [ manager ] [ as ID ];'\n"
             "  'open vdb [ manager ] [ as ID ];'\n"
             "  'open sra [ manager ] [ as ID ];'\n"
             "  'open sra path [ manager ] [ as ID ];'\n"
             "\n",
             "  this command opens a manager object for the indicated library,\n"
             "  dynamically loading the library if required. the library operational\n"
             "  mode (read-only or update) is selected at kqsh launch time by using\n"
             "  the '-u' switch for update (default is read-only).\n"
             "\n",
             "  the sra manager can optionally work with a path manager to convert\n"
             "  table accession strings into runtime paths. type 'help open sra;' for\n"
             "  more information.\n"
        );

    return 0;
}

static
rc_t kqsh_help_open_path_mgr ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        case kw_manager:
            return kqsh_help_open_mgr ( tbl, src, t );

        case kw_path:
            if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
            {
                switch ( next_token ( tbl, src, t ) -> id )
                {
                case eEndOfInput:
                case eSemiColon:
                case kw_manager:
                    break;
                default:
                    return expected ( t, klogWarn, "manager" );
                }
            }
            break;

        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "open a library or path manager\n"
             "\n%s%s%s%s%s",
             "usage:\n"
             "  'open sra [ manager ] [ as ID ];'\n"
             "  'open sra path [ manager ] [ as ID ];'\n"
             "\n",
             "  this command opens a manager object for the indicated library,\n"
             "  dynamically loading the library if required. the library operational\n"
             "  mode (read-only or update) is selected at kqsh launch time by using\n"
             "  the '-u' switch for update (default is read-only).\n"
             "\n",
             "  the path manager may be opened along with its related db manager.\n"
             "  its purpose is to transform object paths given in open commands from\n"
             "  accessions into runtime paths. for example:\n"
             "\n",
             "    > 'open sra manager; open sra path manager;'\n"
             "    > 'open table \"SRR000001\" using sramgr;'\n"
             "\n",
             "  the commands above will open the sra manager under its default name,\n"
             "  then open the installation specific sra path manager and associate the\n"
             "  two, such that the next open table command will convert the given path\n"
             "  from an accession 'SRR000001' to a full file system path.\n"
        );

    return 0;
}

static
rc_t kqsh_help_open_cursor ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "open a cursor\n"
             "\n"
             "usage:\n"
             "  'open ID at ROW;'\n"
             "\n"
             "  this command will cause a cursor to be opened at the given starting row.\n"
        );

    return 0;
}

static
rc_t kqsh_help_open_row ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "open a row on cursor\n"
             "\n"
             "usage:\n"
             "  'open row [ ROW ] [ on ] <cursor>;'\n"
             "\n"
             "  this command will cause a cursor row to be opened.\n"
             "\n"
             "  if no row id is specified, the current cursor row marker will be used.\n"
             "  otherwise, the cursor marker will be preset to given row id before the\n"
             "  row is opened.\n"
             "\n"
             "  if the cursor row is already open, the command will succeed provided\n"
             "  that the current row id matches the requested row id.\n"
        );

    return 0;
}

static
rc_t kqsh_help_open ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        case kw_sra:
            return kqsh_help_open_path_mgr ( tbl, src, t );
        case kw_kdb:
        case kw_vdb:
            if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
            {
                switch ( next_token ( tbl, src, t ) -> id )
                {
                case eEndOfInput:
                case eSemiColon:
                case kw_manager:
                    break;
                    return expected ( t, klogWarn, "manager" );
                }
            }
        case kw_manager:
            return kqsh_help_open_mgr ( tbl, src, t );

        case kw_cursor:
            return kqsh_help_open_cursor ( tbl, src, t );

        case kw_row:
            return kqsh_help_open_row ( tbl, src, t );

        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "open an existing object\n"
             "\n"
             "usage:\n"
             "  'open kdb [ manager ] [ as ID ];'\n"
             "  'open vdb [ manager ] [ as ID ];'\n"
             "  'open sra [ manager ] [ as ID ];'\n"
             "  'open sra path [ manager ] [ as ID ];'\n"
             "  'open column' ...\n"
             "  'open database' ...\n"
             "  'open metadata' ...\n"
             "  'open row' ...\n"
             "  'open schema' ...\n"
             "  'open table' ...\n"
             "  'open cursor' ...\n"
             "\n"
             "  type 'help open manager;' for more information about those commands,\n"
             "  type 'help open <object>;' for more information about remaining command.\n"
        );

    return 0;
}

/* quit aka exit
 */
static
rc_t kqsh_help_quit ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "exit kqsh\n"
             "\n"
             "usage:\n"
             "  'quit;'\n"
             "  'exit;'\n"
        );

    return 0;
}

#define kqsh_help_exit( tbl, src, t ) \
    kqsh_help_quit ( tbl, src, t )


/* rename
 */
static
rc_t kqsh_help_rename ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "rename an object within file system\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}


/* show
 */
static
rc_t kqsh_help_show_mgr ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "show library manager properties\n"
             "\n"
             "usage:\n"
             "  'show <mgr> version;'\n"
        );

    return 0;
}

static
rc_t kqsh_help_show_path_mgr ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
        case kw_manager:
            return kqsh_help_show_mgr ( tbl, src, t );

        case kw_path:
            if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
            {
                switch ( next_token ( tbl, src, t ) -> id )
                {
                case eEndOfInput:
                case eSemiColon:
                case kw_manager:
                    break;
                default:
                    return expected ( t, klogWarn, "manager" );
                }
            }
            break;

        default:
            return expected ( t, klogWarn, ";" );
        }
    }
    else
    {
        return kqsh_help_show_mgr ( tbl, src, t );
    }

    printf ( "show path manager properties\n"
             "\n"
             "usage:\n"
             "  NO USAGE AVAILABLE\n"
        );

    return 0;
}

static
rc_t kqsh_help_show_schema ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;


        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "inspect an open schema\n"
             "\n"
             "usage:\n"
             "  'show <schema>;'\n"
             "  'show <schema> types;'\n"
             "  'show <schema> typesets;'\n"
             "  'show <schema> formats;'\n"
             "  'show <schema> constants;'\n"
             "  'show <schema> functions;'\n"
             "  'show <schema> columns;'\n"
             "  'show <schema> tables;'\n"
             "  'show <schema> databases;'\n"
             "  'show <schema> . NAME;'\n"
             "  'show <schema> . NAME # VERSION;'\n"
             "\n"
             "  this command allows inspection of schema contents by listing object\n"
             "  names or dumping their declarations.\n"
        );

    return 0;
}

static
rc_t kqsh_help_show ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        case kw_sra:
            return kqsh_help_show_path_mgr ( tbl, src, t );

        case kw_kdb:
        case kw_vdb:
            if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
            {
                switch ( next_token ( tbl, src, t ) -> id )
                {
                case eEndOfInput:
                case eSemiColon:
                case kw_manager:
                    break;
                    return expected ( t, klogWarn, "manager" );
                }
            }
        case kw_manager:
            return kqsh_help_show_mgr ( tbl, src, t );

        case kw_schema:
            return kqsh_help_show_schema ( tbl, src, t );

        default:
            return expected ( t, klogWarn, "object or ;" );
        }
    }

    printf ( "display an object\n"
             "\n"
             "usage:\n"
             "  'show manager ...'\n"
             "  'show schema ...'\n"
             "\n"
             "  type 'help show manager;' for more information about those commands,\n"
             "  type 'help show <object>;' for more information about remaining command.\n"
        );

    return 0;
}

/* write
 */
static
rc_t kqsh_help_write ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "write or append row data to column or node\n"
             "%s%s%s%s%s",
             "\n"
             "usage:\n"
             "  'write <cursor>.<column> ROW'\n"
             "  'write <cursor>.IDX ROW'\n"
             "\n",
             "ROW:\n"
             "  STRING              - character data\n"
             "  <bin-elem>          - non-character datum\n"
             "  '[' <bin-elems> ']' - a vector of non-character data\n"
             "\n",
             "bin-elem:\n"
             "  BOOLEAN             - the constants 'true' or 'false'\n"
             "  INTEGER             - signed or unsigned integer value\n"
             "  FLOAT               - floating point value\n"
             "\n",
             "bin-elems:\n"
             "  <bin-elem> [ ',' <bin-elems> ]\n"
             "\n",
             "  specify an open cursor and column by name or ordinal index.\n"
             "  the row data given will be appended to that column.\n"
        );

    return 0;
}


/* use
 */
static
rc_t kqsh_help_use ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "make object the implicit target of 'using' clause\n"
             "  THIS FEATURE IS NOT YET IMPLEMENTED.\n"
        );

    return 0;
}


/* macro for generating switch statements */
#define KQSH_HELP_KEYWORD( topic ) \
    case kw_ ## topic: return kqsh_help_ ## topic ( tbl, src, t )

/* commands
 */
static
rc_t kqsh_help_commands ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        KQSH_HELP_KEYWORD ( alias );
        KQSH_HELP_KEYWORD ( alter );
        KQSH_HELP_KEYWORD ( close );
        KQSH_HELP_KEYWORD ( create );
        KQSH_HELP_KEYWORD ( delete );
        KQSH_HELP_KEYWORD ( drop );
        KQSH_HELP_KEYWORD ( execute );
        KQSH_HELP_KEYWORD ( exit );
        KQSH_HELP_KEYWORD ( help );
        KQSH_HELP_KEYWORD ( open );
        KQSH_HELP_KEYWORD ( quit );
        KQSH_HELP_KEYWORD ( rename );
        KQSH_HELP_KEYWORD ( show );
        KQSH_HELP_KEYWORD ( use );
        KQSH_HELP_KEYWORD ( write );

        default:
            return expected ( t, klogWarn, "command topic" );
        }
    }

    printf ( "kqsh commands:\n"
             "%s%s%s",
             "  alter - alter an open object\n"
             "  close - close an open object\n"
             "  create - create a new object\n"
             "  execute - execute a kqsh script in a new scope\n"
             "  help - print this list\n"
             "  open - open an existing object\n"
             "  quit - exit shell\n"
             "  show - display object data\n"
             "  write - write a row of data to a column\n"
             "\n",
             "unimplemented commands:\n"
             "  alias - create a new name to an file system object\n"
             "  delete - delete rows or nodes\n"
             "  drop - remove an object from the file system\n"
             "  rename - rename an object within file system\n"
             "  use - make object the implicit target of 'using' clause\n"
             "\n",
             "  type 'help <command>;' for more information about that command.\n"
        );

    return 0;
}

/* kdb
 */
static
rc_t kqsh_help_kdb ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "kdb objects:\n"
             "  kdb manager - root object of kdb\n"
             "  kdb database - raw, physical database\n"
             "  kdb table - raw, physical table\n"
             "  kdb column - raw, physical column with opaque data\n"
             "  kdb metadata - associated with manager, database, table and column\n"
             "\n"
             "  NB - in some tables with early version columns, metadata may not be present.\n"
        );

    return 0;
}


/* vdb
 */
static
rc_t kqsh_help_vdb ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "vdb objects:\n"
             "  vdb manager - root object of vdb\n"
             "  vdb schema - describes other objects\n"
             "  vdb database - can contain tables, indices and sub-databases\n"
             "  vdb table - can contain columns and indices\n"
             "  vdb cursor - a group of user-selected columns\n"
             "\n"
             "  the vdb manager builds upon the kdb manager. it allows use\n"
             "  and definition of schema objects which describe the shape and\n"
             "  behavior of other objects that may be created or opened under vdb.\n"
        );

    return 0;
}


/* sra
 */
static
rc_t kqsh_help_sra ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;
        default:
            return expected ( t, klogWarn, ";" );
        }
    }

    printf ( "sra objects:\n"
             "  sra manager - root object of sra\n"
             "  sra path manager - installation specific path translator\n"
             "  sra table - can contain columns and indices\n"
             "\n"
             "  the sra manager builds upon the vdb manager. it has a pre-defined\n"
             "  schema within the sra domain and the objects provide some interface\n"
             "  refinements of their vdb counterparts.\n"
             "\n"
             "  not all installations will have an sra path manager for translating\n"
             "  paths given to the open command. when not available or used, open paths\n"
             "  will be used as given.\n"
        );

    return 0;
}


/* objects
 */
static
rc_t kqsh_help_objects ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        KQSH_HELP_KEYWORD ( kdb );
        KQSH_HELP_KEYWORD ( vdb );
        KQSH_HELP_KEYWORD ( sra );

#if 0
        KQSH_HELP_KEYWORD ( column );
        KQSH_HELP_KEYWORD ( database );
        KQSH_HELP_KEYWORD ( metadata );
        KQSH_HELP_KEYWORD ( schema );
        KQSH_HELP_KEYWORD ( table );
#endif

        default:
            return expected ( t, klogWarn, "object class" );
        }
    }

    printf ( "kqsh objects:\n"
             "  kdb ...\n"
             "  vdb ...\n"
             "  sra ...\n"
             "  column\n"
             "  cursor\n"
             "  database\n"
             "  metadata\n"
             "  schema\n"
             "  table\n"
             "\n"
             "  type 'help <object>;' for more information about that object.\n"
        );

    return 0;
}

/* kqsh
 */
rc_t kqsh_help ( KSymTable *tbl, KTokenSource *src, KToken *t )
{
    /* this is one command where we don't insist on a trailing ';'
       because the user might just be getting it figured out */
    if ( ! interactive || KTokenSourceAvail ( src ) > 1 )
    {
        /* this command could still block */
        switch ( next_token ( tbl, src, t ) -> id )
        {
        case eEndOfInput:
        case eSemiColon:
            break;

        KQSH_HELP_KEYWORD ( commands );
        KQSH_HELP_KEYWORD ( objects );

        KQSH_HELP_KEYWORD ( alias );
        KQSH_HELP_KEYWORD ( alter );
        KQSH_HELP_KEYWORD ( close );
        KQSH_HELP_KEYWORD ( create );
        KQSH_HELP_KEYWORD ( delete );
        KQSH_HELP_KEYWORD ( drop );
        KQSH_HELP_KEYWORD ( execute );
        KQSH_HELP_KEYWORD ( help );
        KQSH_HELP_KEYWORD ( open );
        KQSH_HELP_KEYWORD ( quit );
        KQSH_HELP_KEYWORD ( rename );
        KQSH_HELP_KEYWORD ( show );
        KQSH_HELP_KEYWORD ( use );
        KQSH_HELP_KEYWORD ( write );

        KQSH_HELP_KEYWORD ( kdb );
        KQSH_HELP_KEYWORD ( vdb );
        KQSH_HELP_KEYWORD ( sra );

#if 0
        KQSH_HELP_KEYWORD ( column );
        KQSH_HELP_KEYWORD ( database );
        KQSH_HELP_KEYWORD ( metadata );
        KQSH_HELP_KEYWORD ( schema );
        KQSH_HELP_KEYWORD ( table );
#endif

        default:
            return expected ( t, klogWarn, "help topic" );
        }
    }

    /* general help */
    printf ( "kqsh help:\n"
             "\n"
             "  topics:\n"
             "    commands -- list shell commands\n"
             "    objects -- list objects and their usage\n"
             "\n"
             "  the command processor expects a semi-colon ( ';' ) to process each command.\n"
             "  this is not intended to be irritating, but the shell itself is NOT line oriented\n"
             "  which requires a statement terminator. the 'help' and 'quit' commands are the only\n"
             "  exceptions to this rule.\n"
             "\n"
             "  type 'help <topic>;' for more information about that topic.\n"
        );

    return 0;
}
