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

%{
    #include "token.h"
    #include "prefs-yacc.h"
    #include <sysalloc.h>

    #define YYVAL_TYPE( code )            \
        yylval -> t . var = code

    #define YYVAL_NONE()                  \
        YYVAL_TYPE ( val_none )

    #define YY_RETURN( tok )              \
        YYVAL_NONE ();                    \
        return tok

    #define YYVAL_RETURN( code, tok )     \
        YYVAL_TYPE ( code );              \
        return tok

    #define YYTXT_RETURN( code, tok )     \
        yylval -> t . val . c = yytext;   \
        yylval -> t . len = yyleng;       \
        YYVAL_RETURN ( code, tok )

%}

%option never-interactive nounistd yylineno

%option prefix="prefs_yy"

/* non-re-entrant scanner
%option reentrant
 */

/* bison options */
%option bison-bridge

 /* maintain state stack */
%option stack

 /* let parser handle EOF */
%option noyywrap

 /* comment states */
%x SLASH_STAR
%x LINE_CMT

 /* common patterns */
year                                                            [0-9][0-9][0-9][0-9]
month                                                           [0-9][0-9]
day                                                             [0-9][0-9]
date                                                            {year}-{month}-{day}
hour                                                            [0-9][0-9]
minute                                                          [0-9][0-9]
second                                                          [0-9][0-9]
time                                                            {hour}:{minute}:{second}
zone                                                            {hour}:{minute}

%%

 /* multi-line comments */
\/\*                                                            { yy_push_state ( SLASH_STAR );             }
<SLASH_STAR>[^*\n]+                                             { /* ignore */                              }
<SLASH_STAR>\*+[^*/\n]+                                         { /* ignore */                              }
<SLASH_STAR>\**\n                                               { ++ yylineno;                              }
<SLASH_STAR>\*+\/                                               { yy_pop_state ();                          }

 /* line comments */
"#"                                                             { yy_push_state ( LINE_CMT );               }
<LINE_CMT>[^\\\n]+                                              { /* ignore */                              }
<LINE_CMT>\\[^\r\n]                                             { /* ignore */                              }
<LINE_CMT>\\\r[^\n]                                             { /* ignore */                              }
<LINE_CMT>\\\r\n                                                { ++ yylineno;                              }
<LINE_CMT>\\\n                                                  { ++ yylineno;                              }
<LINE_CMT>\n                                                    {
                                                                    ++ yylineno;
                                                                    yy_pop_state ();
                                                                    YY_RETURN ( EOLN );
                                                                }

 /* whitespace characters */
[ \t\f\v\r]                                                     { /* ignore */                              }
\\\r\n                                                          { ++ yylineno;                              }
\\\n                                                            { ++ yylineno;                              }
\n                                                              { ++ yylineno; YY_RETURN ( EOLN );          }

 /* prefs members */
"download-date"                                                 { YY_RETURN ( PD_DOWNLOAD_DATE );           }
"last-check-date"                                               { YY_RETURN ( PD_LAST_CHECK_DATE );         }
"latest-version"                                                { YY_RETURN ( PD_LATEST_VERS );             }
"current-version"                                               { YY_RETURN ( PD_CURRENT_VERS );            }
"download-url"                                                  { YY_RETURN ( PD_DOWNLOAD_URL );            }
"latest-vers-url"                                               { YY_RETURN ( PD_LATEST_VERS_URL );         }
"path-to-installation"                                          { YY_RETURN ( PD_PATH_TO_INSTALLATION );    }
"auto-download-enabled"                                         { YY_RETURN ( PD_AUTO_DOWNLOAD_ENABLED );   }

 /* symbolic values */
"true"                                                          { YY_RETURN ( KW_TRUE );                    }
"TRUE"                                                          { YY_RETURN ( KW_TRUE );                    }
"false"                                                         { YY_RETURN ( KW_FALSE );                   }
"FALSE"                                                         { YY_RETURN ( KW_FALSE );                   }

 /* version tokens */
[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+                                  {
                                                                    if ( toolkit_atov ( & yylval -> t . val . v, yytext, yyleng ) )
                                                                    { YYVAL_RETURN ( val_vers, MAJMINRELBUILD ); }
                                                                    YYTXT_RETURN ( val_txt, OVER_VERSION );
                                                                }
[0-9]+\.[0-9]+\.[0-9]+                                          {
                                                                    if ( toolkit_atov ( & yylval -> t . val . v, yytext, yyleng ) )
                                                                    { YYVAL_RETURN ( val_vers, MAJMINREL ); }
                                                                    YYTXT_RETURN ( val_txt, OVER_VERSION );
                                                                }

 /* real numbers or two-part versions */
[0-9]+\.[0-9]+                                                  { YYTXT_RETURN ( val_txt, REAL );           }

 /* unsigned integers or one-part versions */
[0-9]+                                                          {
                                                                    if ( toolkit_dtoi ( & yylval -> t . val . u, yytext, yyleng ) )
                                                                    { YYVAL_RETURN ( val_u64, DECIMAL ); }
                                                                    YYTXT_RETURN ( val_txt, OVER_DECIMAL );
                                                                }
 /* a string */
\"[^\\"\f\r\n]*\"                                               { YYTXT_RETURN ( val_quot, STRING );        }
\"(\\.|[^\\"\f\r\n])+\"                                         { YYTXT_RETURN ( val_esc, STRING );         }


 /* a current date */
{date}T{time}Z                                                  {
                                                                    if ( toolkit_atotm ( & yylval -> t . val . t, yytext, yyleng ) )
                                                                    { YYVAL_RETURN ( val_time, DATETIME ); }
                                                                    YYTXT_RETURN ( val_txt, BAD_DATETIME );
                                                                }
{date}T{time}[+-]{zone}                                         { YYTXT_RETURN ( val_txt, TZ_DATETIME );   }

 /* random punctuation */
[-!%^&*+=|~/;:?,.@(){}<>[\]]                                    { YY_RETURN ( yytext [ 0 ] );               }


 /* unrecognized input */
.                                                               { YYTXT_RETURN ( val_txt, UNRECOGNIZED );   }

%%

rc_t PrefsInitFlex ( FILE * prefs )
{
    yyin = prefs;
    return 0;
}

void PrefsDestroyFlex ( void )
{
    yyin = NULL;
}
