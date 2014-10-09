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

#include <sysalloc.h>

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <sstream>

#include <kapp/args.h>
#include <kapp/main.h>

#include <string.h>

#include "args.hpp"

using namespace ngs;

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* AOptDef empementation                                       */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
AOptDef :: AOptDef ()
{
    reset ();
}   /* AOptDef :: AOptDef () */

AOptDef :: AOptDef ( const AOptDef & OptDef )
{
    reset ( OptDef );
}   /* AOptDef :: AOptDef () */

AOptDef :: ~AOptDef ()
{
    reset ();
}   /* AOptDef :: ~AOptDef () */

AOptDef &
AOptDef :: operator = ( const AOptDef & OptDef )
{
    if ( this != & OptDef ) {
        reset ( OptDef );
    }

    return * this;
}   /* AOptDef :: AOptDef () */

void
AOptDef :: reset ()
{
    _M_name . clear ();
    _M_aliases . clear ();
    _M_param . clear ();
    _M_hlp . clear ();
    _M_max_count = 0;
    _M_need_value = true;
    _M_required = true;
}   /* AOptDef :: reset () */

void
AOptDef :: reset ( const AOptDef & OptDef )
{
    _M_name = OptDef . _M_name;
    _M_aliases = OptDef . _M_aliases;
    _M_param = OptDef . _M_param;
    _M_hlp = OptDef . _M_hlp;
    _M_max_count = OptDef . _M_max_count;
    _M_need_value = OptDef . _M_need_value;
    _M_required = OptDef . _M_required;
}   /* AOptDef :: reset () */

bool
AOptDef :: good () const
{
    return ! _M_name . empty ();
}   /* AOptDef :: good () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* AOPBase impementation                                       */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
AOPBase :: AOPBase ()
:   _M_exist ( false )
{
    reset ();
}   /* AOPBase () */

AOPBase :: ~AOPBase ()
{
    reset ();
}   /* AOPBase :: ~AOPBase () */

void
AOPBase :: reset ( const String & , Args * , bool )
{
    throw ErrorMsg ( "reset: Unimpemented method" );
}   /* AOPBase :: reset () */

void
AOPBase :: reset ()
{
    _M_exist = false;
    _M_name . clear ();
    _M_val . clear ();
}   /* AOPBase :: reset () */

void
AOPBase :: reset ( const AOPBase & Bse )
{
    reset ();

    _M_val = Bse . _M_val;
    _M_name = Bse . _M_name;
    _M_exist = Bse . _M_exist;
}   /* AOPBase :: reset () */

uint32_t
AOPBase :: valCount () const
{
/* Not sure about it !!!
    if ( ! exist () ) {
        throw ErrorMsg ( "valCount: Value not exits" );
    }

    return _M_val . size ();
*/

    return exist () ? _M_val . size () : 0;
}   /* AOPBase :: valCount () */

const String &
AOPBase :: val ( uint32_t idx ) const
{
    uint32_t cnt = valCount ();

    if ( cnt <= idx ) {
        throw ErrorMsg ( "val: Invalid index" );
    }

    return _M_val [ idx ];
}   /* AOPBase :: val () */

static
void CC
__handle_error ( const char * arg, void * data )
{
    std :: stringstream str;
    str << "Can not convert \"" << arg << "\" to int for paramter \"" << ( char * ) data << "\"";
    throw ErrorMsg ( str . str () );
}   /* __handle_error () */

uint32_t
AOPBase :: uint32Val ( uint32_t idx ) const
{
    return AsciiToU32 (
                    val ( idx ) . c_str (),
                    __handle_error,
                    ( void * ) _M_name . c_str ()
                    );
}   /* AOPBase :: uint32Val () */

int32_t
AOPBase :: int32Val ( uint32_t idx ) const
{
    return AsciiToI32 (
                    val ( idx ) . c_str (),
                    __handle_error,
                    ( void * ) _M_name . c_str ()
                    );
}   /* AOPBase :: int32Val () */

uint64_t
AOPBase :: uint64Val ( uint32_t idx ) const 
{
    return AsciiToU64 (
                    val ( idx ) . c_str (),
                    __handle_error,
                    ( void * ) _M_name . c_str ()
                    );
}   /* AOPBase :: uint64Val () */

int64_t
AOPBase :: int64Val ( uint32_t idx ) const
{
    return AsciiToI64 (
                    val ( idx ) . c_str (),
                    __handle_error,
                    ( void * ) _M_name . c_str ()
                    );
}   /* AOPBase :: int64Val () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* AOptVal impementation                                       */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
AOptVal :: AOptVal ()
:   AOPBase ()
{
}   /* AOptVal :: AOptVal () */

AOptVal :: AOptVal ( const AOptVal & Val )
:   AOPBase ()
{
    AOPBase :: reset ( Val );
}   /* AOptVal :: AOptVal () */

AOptVal &
AOptVal :: operator = ( const AOptVal & Val )
{
    if ( this != & Val ) {
        AOPBase :: reset ( Val );
    }

    return * this;
}   /* AOptVal :: operator = () */

void
AOptVal :: reset ( const String & Name, Args * TheArgs, bool needValue )
{
    AOPBase :: reset ();

    if ( TheArgs == NULL ) {
        throw ErrorMsg ( "reset: NULL args passed" );
    }

    if ( Name . empty () ) {
        throw ErrorMsg ( "reset: Empty name passed" );
    }

    uint32_t count = 0;
    if ( ArgsOptionCount ( TheArgs, Name . c_str (), & count ) != 0 ) {
        throw ErrorMsg ( String ( "reset: Can not get count for option \"" ) + Name + "\"" );
    }

    if ( count == 0 ) {
        return;
    }

    _M_val . resize ( count );
    if ( needValue ) {
        for ( uint32_t i = 0; i < count; i ++ ) {
            const char * val = NULL;
            if ( ArgsOptionValue ( TheArgs, Name . c_str (), i, & val ) != 0 ) {
                std :: stringstream Vsg;
                Vsg << "reset: Can not get value for option \"" << Name;
                Vsg << "\" in series " << i;
                throw ErrorMsg ( Vsg . str () );
            }

            _M_val [ i ] = val == NULL ? "" : val;
        }
    }

    _M_name = Name;
    _M_exist = true;
}   /* AOptVal :: reset () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* AParVal impementation                                       */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
AParVal :: AParVal ()
:   AOPBase ()
{
}   /* AParVal :: AParVal () */

AParVal :: AParVal ( const AParVal & Val )
:   AOPBase ()
{
    AOPBase :: reset ( Val );
}   /* AParVal :: AParVal () */

AParVal &
AParVal :: operator = ( const AParVal & Val )
{
    if ( this != & Val ) {
        AOPBase :: reset ( Val );
    }

    return * this;
}   /* AParVal :: operator = () */

void
AParVal :: reset ( const String & Name, Args * TheArgs, bool )
{
    AOPBase :: reset ();

    if ( TheArgs == NULL ) {
        throw ErrorMsg ( "reset: NULL args passed" );
    }

    uint32_t count = 0;
    if ( ArgsParamCount ( TheArgs, & count ) != 0 ) {
        throw ErrorMsg ( "reset: Can not get count for parameters" );
    }

    if ( count == 0 ) {
        return;
    }

    _M_val . resize ( count );
    for ( uint32_t i = 0; i < count; i ++ ) {
        const char * val = NULL;
        if ( ArgsParamValue ( TheArgs, i, & val ) != 0 ) {
            std :: stringstream Vsg;
            Vsg << "reset: Can not get parameter in series " << i;
            throw ErrorMsg ( Vsg . str () );
        }

        _M_val [ i ] = val;
    }

    _M_name = Name;
    _M_exist = true;
}   /* AParVal :: reset () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* AArgs impementation                                         */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
AArgs :: MArgs AArgs :: _sM_args;

AArgs :: AArgs ()
:   _M_args ( NULL )
,   _M_standards_added ( false )
,   _M_progName ( "" )
{
}   /* AArgs :: AArgs () */

AArgs :: ~AArgs ()
{
    /* We are not disposing Arguments, it should be done manually */
}   /* AArgs :: ~AArgs () */

void
AArgs :: init ( bool AddStandardOrguments )
{
    if ( good () ) {
        throw ErrorMsg ( "AArgs: We are good" );
    }

    Args * TempArgs;
    if ( ArgsMake ( & TempArgs ) != 0 ) {
        throw ErrorMsg ( "AArgs: Can not make Args" );
    }

    if ( TempArgs == NULL ) {
        throw ErrorMsg ( "AArgs: Can not allocate Args" );
    }

    if ( AddStandardOrguments ) {
        try {
            __addStdOpts ( TempArgs );
            _M_standards_added = true;
        }
        catch ( ... ) {
            __disposeArgs ( TempArgs );

            throw;
        }
    }

    _M_args = TempArgs;

    try {
        __customInit ();
    }
    catch ( ... ) {
        __disposeArgs ( _M_args );

        _M_args = NULL;

        throw;
    }

    __regArgs ( _M_args, this );

}   /* AArgs :: init () */

void
AArgs :: __customInit ()
{
}   /* AArgs :: __customInit () */

void
AArgs :: dispose ()
{
    if ( good () ) {
        __customDispose ();

        Args * TheArgs = _M_args;
        _M_args = NULL;

        _M_optDefs . clear ();
        _M_standards_added = false;

        __deregArgs ( TheArgs );
        __disposeArgs ( TheArgs );

        _M_progName . clear ();
    }
}   /* AArgs :: dispose () */

void 
AArgs :: __customDispose () 
{
}   /* AArgs :: __customDispose () */

void 
AArgs :: __disposeArgs ( Args * TheArgs )
{
    if ( TheArgs != NULL ) {
        ArgsWhack ( TheArgs );
    }
}   /* AArgs :: __disposeArgs () */

void
AArgs :: addStdOpts ()
{
    if ( ! good () ) {
        throw ErrorMsg ( "addStdOpts: Not good" );
    }

    try {
        __addStdOpts ( _M_args );
    }
    catch ( ... ) {
        dispose ();

        throw;
    }
}   /* AArgs :: addStdOpts () */

static
void
__toOpt ( const AOptDef & In, OptDef & Out )
{
    if ( ! In . good () ) {
        throw ErrorMsg ( "__toOpt: IN is not good" );
    }

    memset ( & Out, 0, sizeof ( OptDef ) );

    Out . name = In . getName ();
    Out . aliases = In . getAliases ();
    const char * Hlp = In . getHlp ();
    Out . help = & Hlp;
    Out . max_count = In . maxCount ();
    Out . needs_value = In . needValue ();
    Out . required = In . required ();
}   /* __toOpt () */

void
AArgs :: addOpt ( const AOptDef & Opt )
{
    if ( ! good () ) {
        throw ErrorMsg ( "addOpt: Not good " );
    }

    struct OptDef TheOpt;
    __toOpt ( Opt, TheOpt );

    if ( ArgsAddOptionArray ( _M_args, & TheOpt, 1 ) != 0 ) {
        throw ErrorMsg ( "addOpt: Can not add option" );
    }

    _M_optDefs . insert ( _M_optDefs . end (), Opt );
}   /* AArgs :: addOpt () */

void
AArgs :: __addStdOpts ( Args * TheArgs )
{
    if ( TheArgs == NULL ) {
        throw ErrorMsg ( "__addStdOpts: Very not good" );
    }

    if ( ArgsAddStandardOptions ( TheArgs ) != 0 ) {
        throw ErrorMsg ( "__addStdOpts: Can not add standard options" );
    }
}   /* AArgs :: __addStdOpts () */

void
AArgs :: parse ( int argc, char ** argv )
{
    if ( ! good () ) {
        throw ErrorMsg ( "parseArgs: Not good" );
    }

    if ( ArgsParse ( _M_args, argc, argv ) != 0 ) {
        throw ErrorMsg ( "parseArgs: Can not parse arguments" );
    }

    const char * prog;
    if ( ArgsProgram ( _M_args, NULL, & prog ) != 0 ) {
        throw ErrorMsg ( "parseArgs: Can not extract Progname" );
    }
    _M_progName =  prog;

        /*) Here we should handle standard argumends
            NOTE: help and version are managing authomatically
         (*/
    if ( _M_standards_added ) {
        if ( ArgsHandleStandardOptions ( _M_args ) != 0 ) {
            throw ErrorMsg ( "parseArgs: Can not handle standard options" );
        }
    }

    __customParse ();
}   /* AArgs :: parse () */

void 
AArgs :: __customParse () 
{
    /* nothing to do here */
}   /* AArgs :: __customParse () */

void
AArgs :: __regArgs ( Args * TheArgs, AArgs * TheAArgs )
{
    if ( __getArgs ( TheArgs ) != NULL ) {
        throw ErrorMsg ( "__regArgs: Already registered" );
    }

    _sM_args [ TheArgs ] = TheAArgs;
}   /* AArgs :: __regArgs () */

AArgs *
AArgs :: __getArgs ( Args * TheArgs )
{
    if ( TheArgs != NULL ) {
        MArgsI Iter = _sM_args . find ( TheArgs );
        if ( Iter != _sM_args . end () ) {
            return ( * Iter ) . second;
        }
    }
    return NULL;
}   /* AArgs :: __getArgs () */

void
AArgs :: __deregArgs ( Args * TheArgs )
{
    if ( TheArgs != NULL ) {
        MArgsI Iter = _sM_args . find ( TheArgs );
        if ( Iter != _sM_args . end () ) {
            _sM_args . erase ( Iter );
        }
    }
}   /* AArgs :: __deregArgs () */

AOptVal
AArgs :: optVal ( const char * name ) const
{
    if ( ! good () ) {
        throw ErrorMsg ( "optVal: Not good" );
    }

    if ( name == NULL ) {
        throw ErrorMsg ( "optVal: NULL name passed" );
    }

    AOptVal retVal;
    retVal . reset ( name, _M_args, optDef ( name ) . needValue () );
    return retVal;
}   /* AArgs :: optVal () */

AParVal
AArgs :: parVal () const
{
    if ( ! good () ) {
        throw ErrorMsg ( "parVal: Not good" );
    }

    AParVal retVal;
    retVal . reset ( "parameters", _M_args, true );
    return retVal;
}   /* AArgs :: paramVal () */

const AOptDef &
AArgs :: optDef ( const String & name ) const
{
    if ( ! good () ) {
        throw ErrorMsg ( "optDef: Not good" );
    }

    for (
        VOptI S = _M_optDefs . begin ();
        S != _M_optDefs . end ();
        S ++
    ) {
        if ( ( * S ) . getName () == name ) {
            return * S;
        }
    }

    throw ErrorMsg ( String ( "optDef: Can not find definition for opotion \"" ) + name + "\"" );
}   /* AArgs :: optDef () */

void
AArgs :: usage () const
{
    :: Usage ( _M_args );
}   /* AArgs :: Usage () */
