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

#include "vdb-dump-repo.h"
#include "vdb-dump-helper.h"

#include <klib/text.h>
#include <klib/out.h>
#include <kfg/config.h>
#include <kfg/repository.h>

/* ------------------------------------------------------------------------------------------------- */

static const KRepCategory id_to_repo_cat[ 4 ] = { krepUserCategory, krepSiteCategory, krepRemoteCategory, krepBadCategory };

static const char * s_BadCategory       = "BadCategory";
static const char * s_UserCategory      = "UserCategory";
static const char * s_SiteCategory      = "SiteCategory";
static const char * s_RemoteCategory    = "RemoteCategory";
static const char * s_UnknownCategory   = "unknow category";

static const char * KRepCategory_to_str( const KRepCategory c )
{
    switch( c )
    {
        case krepBadCategory        : return s_BadCategory; break;
        case krepUserCategory       : return s_UserCategory; break;
        case krepSiteCategory       : return s_SiteCategory; break;
        case krepRemoteCategory     : return s_RemoteCategory; break;
    };
    return s_UnknownCategory;
}

static const char * s_Prefix_bad        = "bad";
static const char * s_Prefix_user       = "user";
static const char * s_Prefix_site       = "site";
static const char * s_Prefix_remote     = "remote";
static const char * s_Prefix_unknown    = "unknow";

static const char * KRepCategory_to_prefix( const KRepCategory c )
{
    switch( c )
    {
        case krepBadCategory        : return s_Prefix_bad; break;
        case krepUserCategory       : return s_Prefix_user; break;
        case krepSiteCategory       : return s_Prefix_site; break;
        case krepRemoteCategory     : return s_Prefix_remote; break;
    };
    return s_Prefix_unknown;
}


static const char * s_BadSubCategory            = "BadSubCategory";
static const char * s_MainSubCategory           = "MainSubCategory";
static const char * s_AuxSubCategory            = "AuxSubCategory";
static const char * s_ProtectedSubCategory      = "ProtectedSubCategory";

static const char * KRepSubCategory_to_str( const KRepSubCategory c )
{
    switch( c )
    {
        case krepBadSubCategory         : return s_BadSubCategory; break;
        case krepMainSubCategory        : return s_MainSubCategory; break;
        case krepAuxSubCategory         : return s_AuxSubCategory; break;
        case krepProtectedSubCategory   : return s_ProtectedSubCategory; break;
    }
    return s_UnknownCategory;
}

static const char * s_yes = "yes";
static const char * s_no  = "no";

static const char * yes_or_no( const bool flag )
{
    return flag ? s_yes : s_no;
}

typedef rc_t ( CC * repofunc )( const KRepository *self, char *buffer, size_t bsize, size_t * size );

static rc_t vdi_report_repo_str( const KRepository * repo, const char * elem_name, repofunc f )
{
    if ( f != NULL )
    {
        char buffer[ 4096 ];
        rc_t rc = f( repo, buffer, sizeof buffer, NULL );
        if ( rc == 0 ) return KOutMsg( "     - %s : %s\n", elem_name, buffer );
    }
    return 0;
}

static rc_t vdi_report_repository( const KRepository * repo, const char * prefix, int idx, bool full )
{
    KRepCategory cat = KRepositoryCategory( repo );
    KRepSubCategory subcat = KRepositorySubCategory( repo );

    rc_t rc = KOutMsg( "  repo.%s #%d:\n", prefix, idx );
    if ( rc == 0 )
        rc = KOutMsg( "     - category : %s.%s\n", KRepCategory_to_str( cat ), KRepSubCategory_to_str( subcat ) );
    if ( rc == 0 )
        rc = vdi_report_repo_str( repo, "name", KRepositoryName );
    if ( rc == 0 )
        rc = KOutMsg( "     - disabled : %s\n", yes_or_no( KRepositoryDisabled( repo ) ) );
    
    if ( full )
    {
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "displayname", KRepositoryDisplayName );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "root", KRepositoryRoot );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "resolver", KRepositoryResolver );
        
        if ( rc == 0 )
            rc = KOutMsg( "     - cached : %s\n", yes_or_no( KRepositoryCacheEnabled( repo ) ) );
            
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "ticket", KRepositoryDownloadTicket );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "key", KRepositoryEncryptionKey );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "keyfile", KRepositoryEncryptionKeyFile );
        if ( rc == 0 ) rc = vdi_report_repo_str( repo, "desc", KRepositoryDescription );    
        
        if ( rc == 0 )
        {
            uint32_t prj_id;
            rc_t rc1 = KRepositoryProjectId( repo, &prj_id );
            if ( rc1 == 0 ) rc = KOutMsg( "     - prj-id : %d\n", prj_id );
        }
    }
    
    if ( rc == 0 ) rc = KOutMsg( "\n" );
    return rc;
}

typedef rc_t ( CC * catfunc )( const KRepositoryMgr *self, KRepositoryVector *user_repositories );

static catfunc vdi_get_catfunc( const KRepCategory cat )
{
    switch( cat )
    {
        case krepUserCategory       : return KRepositoryMgrUserRepositories; break;
        case krepSiteCategory       : return KRepositoryMgrSiteRepositories; break;
        case krepRemoteCategory     : return KRepositoryMgrRemoteRepositories; break;
    };
    return NULL;
}

static rc_t vdi_report_repo_vector( const KRepositoryMgr * repomgr, const KRepCategory cat,
                    int32_t select, bool full )
                                    
{
    rc_t rc = 0;
    catfunc f = vdi_get_catfunc( cat );
    if ( f != NULL )
    {
        KRepositoryVector v;
        rc = f( repomgr, &v );
        if ( rc == 0 )
        {
            const char * prefix = KRepCategory_to_prefix( cat );
            uint32_t idx, len = VectorLength( &v );
            bool disabled = KRepositoryMgrCategoryDisabled( repomgr, cat );
            rc = KOutMsg( "repo.%s --> disabled: %s, %d subrepositories )\n", prefix, yes_or_no( disabled ), len );
            for ( idx = 0; rc == 0 && idx < len; ++idx )
            {
                if ( select == idx || !full )
                    rc = vdi_report_repository( VectorGet( &v, idx ), prefix, idx, full );
            }
            
            KRepositoryVectorWhack( &v );
        }
    }
    return rc;
}


static rc_t vdi_repo_all( const KRepositoryMgr * repomgr, bool full )
{
    rc_t rc = vdi_report_repo_vector( repomgr, krepUserCategory, -1, full );
    if ( rc == 0 )
        rc = vdi_report_repo_vector( repomgr, krepSiteCategory, -1, full );
    if ( rc == 0 )
        rc = vdi_report_repo_vector( repomgr, krepRemoteCategory, -1, full );
    return rc;
}


static rc_t vdi_repo_switch( const KRepositoryMgr * repomgr, const KRepCategory cat, bool disabled )
{
    const char * s_cat = KRepCategory_to_prefix( cat );
    rc_t rc = KRepositoryMgrCategorySetDisabled( repomgr, cat, disabled );
    if ( rc == 0 )
        rc = KOutMsg( "repository '%s' successfully %s", s_cat, ( disabled ? "disabled" : "enabled" ) );
    else
        rc = KOutMsg( "repository '%s' not %s: '%R'", s_cat, ( disabled ? "disabled" : "enabled" ), rc );
    return rc;
}


static rc_t vdi_sub_repo( const KRepositoryMgr * repomgr, const Vector * v, const String * which_repo, int32_t repo_id )
{
    rc_t rc = 0;
    const KRepCategory cat = id_to_repo_cat[ repo_id ];
    
    if ( VectorLength( v ) > 2 )
    {
        const String * which_sub = VectorGet( v, 2 );
        int32_t repo_func = index_of_match( which_sub, 2, "on", "off" );
        switch( repo_func )
        {
            case 0 : rc = vdi_repo_switch( repomgr, cat, false ); break;
            case 1 : rc = vdi_repo_switch( repomgr, cat, true ); break;
            case -1 :  {
                            int32_t select = ( int32_t )string_to_I64( which_sub->addr, which_sub->len, NULL );
                            rc = vdi_report_repo_vector( repomgr, cat, select, true );
                        }
                        break; 
        }
    }
    else
    {
        if ( repo_id < 3 )
            rc = vdi_report_repo_vector( repomgr, cat, -1, false );
        else
            rc = vdi_repo_all( repomgr, false );
    }
    
    return rc;
}


rc_t vdi_repo( const Vector * v )
{
    KConfig * cfg;
    rc_t rc = KConfigMake( &cfg, NULL );
    if ( rc == 0 )
    {
        const KRepositoryMgr * repomgr;
        rc = KConfigMakeRepositoryMgrRead( cfg, &repomgr );
        {
            if ( VectorLength( v ) < 2 )
            {
                rc = vdi_repo_all( repomgr, true );
            }
            else
            {
                const String * which_repo = VectorGet( v, 1 );
                if ( which_repo != NULL )
                {
                    int32_t repo_id = index_of_match( which_repo, 4, "user", "site", "remote", "all" );
                    if ( repo_id < 0 || repo_id > 3 )
                        rc = KOutMsg( "unknow repository '%S'", which_repo );
                    else
                        rc = vdi_sub_repo( repomgr, v, which_repo, repo_id );
                }
            }
            
            KRepositoryMgrRelease( repomgr );
        }
        KConfigRelease ( cfg );
    }
    return rc;
}
