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

#include <ngs/ReadCollection.hpp>

#include <sstream>

#include "filters.hpp"

using namespace std;
using namespace ngs;

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*))
 //     AFilter
((*/
AFilter :: AFilter ()
:   _M_rejected ( 0 )
{
}   /* AFilter :: AFilter () */

AFilter :: ~AFilter ()
{
    _M_rejected = 0;
}   /* AFilter :: ~AFilter () */

bool
AFilter :: checkIt ( const ReadIterator & pos ) const
{
    throw ErrorMsg ( ":: checkIt() - is not implemented for class" );
}   /* AFilter :: checkIt () */

String
AFilter :: report () const
{
    if ( _M_rejected != 0 ) {
        stringstream __s;
        __s << "Rejected "
            << _M_rejected
            << ( _M_rejected == 1 ? " SPOT" : " SPOTS" )
            ;
        if ( reason () . empty () ) {
            __s << " reason unknown";
        }
        else {
            __s << " because " << reason ();
        }

        return __s . str ();
    }
    return "";
}   /* AFilter :: report () */

String
AFilter :: reason () const
{
    return "";
}   /* AFilter :: reason () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/* Place for some predefined filters                             */
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*))
 //     Num spots readed filter ... needed as counter only
((*/
class __NReadFilter : public AFilter {
public :
    __NReadFilter ( const String & source );

    bool checkIt ( const ReadIterator & Rit ) const;

    String report () const;

private :
    String _M_source;
};

__NReadFilter :: __NReadFilter ( const string & source )
:   _M_source ( source )
{
}   /* __NReadFilter :: __NReadFilter () */

bool
__NReadFilter :: checkIt ( const ReadIterator & ) const
{
    reject ();

    return true;
}   /* __NReadFilter :: checkIt () */ 

String
__NReadFilter :: report () const
{
    stringstream __s;

    __s << "Read "
        << rejected ()
        << ( rejected () == 1 ? " spot" : " spots" )
        ;

    if ( ! _M_source . empty () ) {
        __s << " for " << _M_source;
    }

    return __s . str ();
}   /* __NReadFilter :: report () */

/*))
 //     Num spots readed filter ... needed as counter only
((*/
class __SpotLengthFilter : public AFilter {
public :
    __SpotLengthFilter ( uint64_t MinLength );

    bool checkIt ( const ReadIterator & Rit ) const;

protected :
    String reason () const;

private :
    uint64_t _M_minLength;
};

__SpotLengthFilter :: __SpotLengthFilter ( uint64_t MinLength )
:   _M_minLength ( MinLength )
{
}   /* __SpotLengthFilter :: __SpotLengthFilter () */

bool
__SpotLengthFilter :: checkIt ( const ReadIterator & Rit ) const
{
    uint64_t __l = Rit . getReadBases () . size ();
    if ( __l < _M_minLength ) {
        reject ();

        return false;
    }

    return true;
}   /* __SpotLengthFilter :: checkIt () */ 

String
__SpotLengthFilter :: reason () const
{
    stringstream __s;

    __s << "SPOTLEN < " << _M_minLength;

    return __s . str ();
}   /* __SpotLengthFilter :: reason () */

/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/
/*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*_*/

/*))
 //     AFilters
((*/
AFilters :: AFilters ( const String & source )
:   _M_source ( source )
,   _M_confirmed ( 0 )
{
    init ();
}   /* AFilters :: AFilters () */

AFilters :: ~AFilters ()
{
    try {
        dispose ();
    }
    catch ( ... ) {
        /* Ha! */
    }
}   /* AFitlers :: ~AFilters () */

void
AFilters :: init ()
{
        /* Just dispose all previous content
         */
    dispose ();

        /* Here we should add some mondaytory filters
         */
    addFilter ( new __NReadFilter ( _M_source ) );
}   /* AFilters :: init () */

void
AFilters :: dispose ()
{
    for ( TVecI __b = _M_filters . begin (); __b != _M_filters . end (); __b ++ ) {
        AFilter * __f = * __b;
        if ( __f != NULL ) {
            delete __f;
        }
        * __b = NULL;
    }
    _M_filters . clear ();

    _M_confirmed = 0;
}   /* AFilters :: dispose () */

bool
AFilters :: checkIt ( const ReadIterator & Rit ) const
{
    bool __r = __checkIt ( Rit );

    if ( __r ) {
        _M_confirmed ++;
    }

    return __r;
}   /* AFilters :: checkIt () */

bool
AFilters :: __checkIt ( const ReadIterator & Rit ) const
{
    for ( TVecCI __b = _M_filters . begin (); __b != _M_filters . end (); __b ++ ) {
        AFilter * __f = * __b;

        if ( __f != NULL ) {
            if ( ! __f -> checkIt ( Rit ) ) {
                return false;
            }
        }
    }

    return true;
}   /* AFilters :: __checkIt () */

void
AFilters :: addFilter ( AFilter * Flt )
{
    if ( Flt != NULL ) {
        _M_filters . insert ( _M_filters . end (), Flt );
    }
}   /* AFilters :: addFilter () */

void
AFilters :: addLengthFilter ( uint64_t minLength )
{
    addFilter ( new __SpotLengthFilter ( minLength ) );
}   /* AFilters :: addLengthFilter () */

String
AFilters :: report ( bool legacyStyle ) const
{
    stringstream __s;

    if ( ! legacyStyle ) {
        for ( TVecCI __b = _M_filters . begin (); __b != _M_filters . end (); __b ++ ) {
            AFilter * __f = * __b;
            if ( __f != NULL ) {
                String __r = __f -> report ();
                if ( ! __r . empty () ) {
                    __s << __r << "\n";
                }
            }
        }
    }

    __s << "Written " << _M_confirmed << ( _M_confirmed == 1 ? " spot" : " spots" ) << " for " << _M_source << "\n";

    if ( legacyStyle ) {
        __s << "Written " << _M_confirmed << ( _M_confirmed == 1 ? " spot" : " spots" ) << " total\n";
    }

    return __s . str ();
}   /* AFilters :: report () */
