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

%pure-parser

%defines
%debug
%error-verbose
%name-prefix="prefs_"

%{

    /* general prelude for auto-generated C code */
    #include "token.h"
    #include "prefs-priv.h"
    #include <sysalloc.h>

%}

/* token and production values */
%union
{
    /* token from flex */
    Token t;

    /* a boolean */
    bool b;
}

%token < t > UNRECOGNIZED

/* punctuation */
%token < t > EOLN

/* value tokens */
%token < t > DECIMAL REAL MAJMINREL MAJMINRELBUILD DATETIME STRING
%token < t > OVER_DECIMAL OVER_VERSION BAD_DATETIME TZ_DATETIME

/* keywords */
%token < t > KW_TRUE KW_FALSE
%token < t > PD_DOWNLOAD_DATE PD_LAST_CHECK_DATE
%token < t > PD_LATEST_VERS PD_CURRENT_VERS
%token < t > PD_DOWNLOAD_URL PD_LATEST_VERS_URL
%token < t > PD_PATH_TO_INSTALLATION
%token < t > PD_AUTO_DOWNLOAD_ENABLED

/* productions */
%type < t > version version_value
%type < t > timestamp
%type < t > url
%type < t > path
%type < b > auto boolean

%start prefs

%%

prefs
    : /* empty */
    | prefsline_seq
    ;

prefsline_seq
    : prefsline
    | prefsline_seq prefsline
    ;

prefsline
    : EOLN                                                      /* ignore empty lines */
    | PD_DOWNLOAD_DATE download_date
    | PD_LAST_CHECK_DATE last_check_date
    | PD_LATEST_VERS latest_version
    | PD_CURRENT_VERS current_version
    | PD_DOWNLOAD_URL download_url
    | PD_LATEST_VERS_URL latest_version_url
    | PD_PATH_TO_INSTALLATION path_to_installation
    | PD_AUTO_DOWNLOAD_ENABLED auto_download
    ;

download_date
    : timestamp                                                 { PrefsDataSetDownloadDate ( & $1 ); }
    | malformed_timestamp EOLN
    ;

last_check_date
    : timestamp                                                 { PrefsDataSetLastCheckDate ( & $1 ); }
    | malformed_timestamp EOLN
    ;

latest_version
    : version                                                   { PrefsDataSetLatestVersion ( & $1 ); }
    | bad_version EOLN
    ;

current_version
    : version                                                   { PrefsDataSetCurrentVersion ( & $1 ); }
    | bad_version EOLN
    ;

download_url
    : url                                                       { PrefsDataSetDownloadURL ( & $1 ); }
    ;

latest_version_url
    : url                                                       { PrefsDataSetLatestVersionURL ( & $1 ); }
    ;

path_to_installation
    : path                                                      { PrefsDataSetPathToInstallation ( & $1 ); }
    ;

auto_download
    : auto                                                      { PrefsDataSetAutoDownloadEnabled ( & $1 ); }
    ;

auto
    : boolean                                                   { $$ = $1; }
    | '=' boolean                                               { $$ = $2; }
    ;


timestamp
    : DATETIME EOLN                                             { $$ = $1; }
    | '=' DATETIME EOLN                                         { $$ = $2; }
    ;

malformed_timestamp
    : BAD_DATETIME                                              { prefs_token_error ( & $1, "badly formed timestamp" ); }
    | TZ_DATETIME                                               { prefs_token_error ( & $1,  "timestamp must use GMT" ); }
    ;

version
    : version_value EOLN                                        { $$ = $1; }
    | '=' version_value EOLN                                    { $$ = $2; }
    ;

version_value
    : MAJMINRELBUILD                                            { $$ = $1;                    }
    | MAJMINREL                                                 { $$ = $1;                    }
    | REAL                                                      { $$ = toolkit_rtov ( & $1 ); }
    | DECIMAL                                                   { $$ = toolkit_itov ( & $1 ); }
    ;

bad_version
    : OVER_VERSION                                              { prefs_token_error ( & $1, "version component overflow" ); }
    ;

url
    : STRING
    ;

path
    : STRING
    ;

boolean
    : KW_TRUE                                                   { $$ = true;  }
    | KW_FALSE                                                  { $$ = false; }
    ;

%%

void prefs_set_debug ( int enabled )
{
    prefs_debug = enabled;
}
