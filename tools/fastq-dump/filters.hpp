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

#ifndef _h_outpost_filters_
#define _h_outpost_filters_

#include <vector>

#ifndef _h_klib_defs_
#include <klib/defs.h>
#endif

#include <ngs/ErrorMsg.hpp>
#include <ngs/StringRef.hpp>

/*)))   Namespace
 (((*/
namespace ngs {

/*))
 // Some forwards
((*/
class ReadIterator;

class AFilter {
public :
    AFilter ();
    virtual ~AFilter ();

    virtual bool checkIt ( const ReadIterator & pos ) const = 0;

    virtual String report () const;

protected :
        /* That method should be called from 'checkIt()' for stat
         */
    inline void reject () const { _M_rejected ++; };
    inline uint64_t rejected () const { return _M_rejected; };

    virtual String reason () const;

private :
    mutable uint64_t _M_rejected;

};  /* class AFilter */

class AFilters {
public :
    typedef std :: vector < AFilter * > TVec;
    typedef TVec :: const_iterator TVecCI;
    typedef TVec :: iterator TVecI;

public :
    AFilters ( const String & source );
    virtual ~AFilters ();

    bool checkIt ( const ReadIterator & pos ) const;

        /* Adds new user_defined filter ...
         */
    void addFilter ( AFilter * pFilter );

        /* There are some standard predefined filters to add
         */
    void addLengthFilter ( uint64_t minLength );

        /* Misc stuff
         */
    inline const String & source () const { return _M_source ; }

    String report ( bool legacyStyle = false ) const;

private :
    void init ();
    void dispose ();

    bool __checkIt ( const ReadIterator & pos ) const;

private :
    String _M_source;

    TVec _M_filters;

    mutable uint64_t _M_confirmed;
};  /* class AFilters */

/*)))   Namespace
 (((*/
}; /* namespace ngs */

#endif /* _h_outpost_filters_ */
