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

#pragma once

/**
 * @file istreambuf_holder.hpp
 * @brief RAII wrapper for streambuf with MD5 observer support
 * 
 * This class ensures proper cleanup of streambuf and associated MD5 observers.
 */

#include <string>
#include <variant>
#include <stdexcept>

#include "bxzstr/bxzstr.hpp"
#include <kfile_stream/kfile_stream.hpp>

namespace sharq {

// bxz::istream does not delete its streambuf;
// this class makes sure it is deleted on destruction
class istreambuf_holder : public bxz::istream
{
public:
    istreambuf_holder( std::streambuf * sb)
    : bxz::istream(sb), held_streambuf( sb )
    {
    }

    istreambuf_holder( std::streambuf * sb, const vdb::KFileMD5ReadObserver * obs )
    : bxz::istream(sb), held_streambuf( sb ), md5( obs )
    {
    }
    istreambuf_holder( std::streambuf * sb, const vdb::KStreamMD5ReadObserver * obs )
    : bxz::istream(sb), held_streambuf( sb ), md5( obs )
    {
    }
    virtual ~istreambuf_holder()
    {
        if ( std::holds_alternative<const vdb::KFileMD5ReadObserver *>( md5 ) )
        {
            KFileMD5ReadObserverRelease( std::get<const vdb::KFileMD5ReadObserver *>( md5 ) );
        }
        else if ( std::holds_alternative<const vdb::KStreamMD5ReadObserver *>( md5 ) )
        {
            KStreamMD5ReadObserverRelease( std::get<const vdb::KStreamMD5ReadObserver *>( md5 ) );
        }
        delete held_streambuf;
    }

    virtual void get_md5( uint8_t digest [ 16 ] ) const
    {
        rc_t rc = 0;
        const char * error = nullptr;
        if ( std::holds_alternative<const vdb::KFileMD5ReadObserver *>( md5 ) )
        {
            rc = KFileMD5ReadObserverGetDigest ( std::get<const vdb::KFileMD5ReadObserver *>( md5 ), digest, & error);
        }
        else if ( std::holds_alternative<const vdb::KStreamMD5ReadObserver *>( md5 ) )
        {
            rc = KStreamMD5ReadObserverGetDigest ( std::get<const vdb::KStreamMD5ReadObserver *>( md5 ), digest, & error);
        }
        if ( rc != 0 )
        {
            throw std::runtime_error(std::string("Failure reading MD5 from input: '") + error + "'");
        }
    }

private:
    std::streambuf * held_streambuf;

    std::variant< const vdb::KFileMD5ReadObserver *, const vdb::KStreamMD5ReadObserver * > md5;
};

} // namespace sharq

