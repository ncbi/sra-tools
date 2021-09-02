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

#include <klib/rc.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/time.h>

#include <sysalloc.h>

#include <kfs/file.h>
#include <kfs/toc.h>

#include <stdio.h>
#include <stdlib.h>

#include "kar+args.h"
#include "kar+.h"


/********************************************************************
 * Globals + Forwards + Declarations + Definitions
 */
static uint32_t max_size_fw;
static uint32_t max_offset_fw;


/********************************************************************
 * Reporting functions
 */
static
void printEntry ( const KAREntry *entry, KARPrintMode *kpm )
{
    char buffer [ 4096 ];
    size_t bsize = sizeof buffer;

    size_t num_writ = kar_entry_full_path ( entry, NULL, buffer, bsize );
    if ( num_writ >= bsize )
    {
        rc_t rc = RC (rcExe, rcPath, rcWriting, rcBuffer, rcInsufficient);
        pLogErr ( klogErr, rc, "Failed to write path for entry '$(name)'",
                  "name=%s", entry -> name );
        exit ( 3 );
    }        

    switch ( kpm -> pm )
    {
    case pm_normal:
        KOutMsg ( "%s", buffer );
        break;
    case pm_longlist:
    {
        KTime tm;
        KTimeLocal ( &tm, entry -> mod_time );

        KOutMsg ( "%04u-%02u-%02u %02u:%02u:%02u %s", 
                  
                  tm . year, tm . month + 1, tm . day + 1, 
                  tm . hour, tm . minute, tm . second, buffer);
        break;
    }
    default:
        break;
    }
}

static
uint32_t get_field_width ( uint64_t num )
{
    uint64_t count;
    if ( num == 0 )
        return 3;

    for ( count = 0; num != 0; ++ count )
        num /= 10;

    return count;
}

void
kar_print_set_max_size_fw ( uint64_t num )
{
    max_size_fw = get_field_width ( num );
}

void
kar_print_set_max_offset_fw ( uint64_t num )
{
    max_offset_fw = get_field_width ( num );
}

static
void printFile ( const KARFile *file, KARPrintMode *kpm )
{
    if ( kpm -> pm == pm_longlist )
    {
        if ( file -> byte_size == 0 )
            KOutMsg ( "%*c ", max_size_fw, '-' );
        else
            KOutMsg ( "%*lu ", max_size_fw, file -> byte_size );
            
        KOutMsg ( "%*lu ", max_offset_fw, file -> byte_offset );
    }

    printEntry ( ( KAREntry * ) file, kpm );
    KOutMsg ( "\n" );
}

static
void printDir ( const KARDir *dir, KARPrintMode *kpm )
{
    if ( kpm -> pm == pm_longlist )
    {
        KOutMsg ( "%*c ", max_size_fw, '-' );
        KOutMsg ( "%*c ", max_offset_fw, '-' );
    }

    printEntry ( & dir -> dad, kpm );
    KOutMsg ( "\n" );
    BSTreeForEach ( &dir -> contents, false, kar_print, kpm );
}

static
void printAlias ( const KARAlias *alias, KARPrintMode *kpm, uint8_t type )
{
    if ( kpm -> pm == pm_longlist )
    {
        KOutMsg ( "% *u ", max_size_fw, string_size ( alias -> link ) );
        KOutMsg ( "% *c ", max_offset_fw, '-' );
        printEntry ( ( KAREntry * ) alias, kpm );
        KOutMsg ( " -> %s", alias -> link );
    }
    else
        printEntry ( ( KAREntry * ) alias, kpm );
    
    KOutMsg ( "\n" );
}

/*
static
void printFile_tree ( const KARFile * file, uint32_t *indent )
{
    KOutMsg ( "%*s%s [ %lu, %lu ]\n", *indent, "", file -> dad . name, file -> byte_offset, file -> byte_size );
}

static
void printDir_tree ( const KARDir *dir, uint32_t *indent )
{
    KOutMsg ( "%*s%s\n", *indent, "", dir -> dad . name );
    *indent += 4;
    BSTreeForEach ( &dir -> contents, false, kar_print, indent );
    *indent -= 4;
}
*/
static
void print_mode ( uint32_t grp )
{
    char r = ( grp & 4 ) ? 'r' : '-';
    char w = ( grp & 2 ) ? 'w' : '-';
    char x = ( grp & 1 ) ? 'x' : '-';

    KOutMsg ( "%c%c%c", r, w, x );
}

void kar_print ( BSTNode *node, void *data )
{
    const KAREntry *entry = ( KAREntry * ) node;


    KARPrintMode *kpm = ( KARPrintMode * ) data;

    switch ( kpm -> pm )
    {
    case pm_normal:
    {
        switch ( entry -> type )
        {
        case kptFile:
            printFile ( ( KARFile * ) entry, kpm );
            break;
        case kptDir:
            printDir ( ( KARDir * ) entry, kpm );
            break;
        case kptAlias:
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            printAlias ( ( KARAlias * ) entry, kpm, entry -> type );
            break;
        default:
            break;
        }
        break;
    }
    case pm_longlist:
    {
        switch ( entry -> type )
        {
        case kptFile:
            KOutMsg ( "-" );
            print_mode ( ( entry -> access_mode >> 6 ) & 7 );
            print_mode ( ( entry -> access_mode >> 3 ) & 7 );
            print_mode ( entry -> access_mode & 7 );
            KOutMsg ( " " );
            printFile ( ( KARFile * ) entry, kpm );
            break;
        case kptDir:
            KOutMsg ( "d" );
            print_mode ( ( entry -> access_mode >> 6 ) & 7 );
            print_mode ( ( entry -> access_mode >> 3 ) & 7 );
            print_mode ( entry -> access_mode & 7 );
            KOutMsg ( " " );
            printDir ( ( KARDir * ) entry, kpm );
            break;
        case kptAlias:
        case kptFile | kptAlias:
        case kptDir | kptAlias:
            KOutMsg ( "l" );
            print_mode ( ( entry -> access_mode >> 6 ) & 7 );
            print_mode ( ( entry -> access_mode >> 3 ) & 7 );
            print_mode ( entry -> access_mode & 7 );
            KOutMsg ( " " );
            printAlias ( ( KARAlias * ) entry, kpm, entry -> type );
            break;
        default:
            break;
        }
        break;
    }
    default:
        break;
    }

}


