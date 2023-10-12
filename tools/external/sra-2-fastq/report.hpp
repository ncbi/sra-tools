/*==============================================================================
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

#include <klib/out.h>
#include <string>
#include <memory>
#include "helper.hpp"

enum Layout { cSRA, table, unknown_layout };

struct Report {
    Layout layout;
    uint64_t seq_rows;
    uint64_t file_size;
    // etc.

    Report() {
        layout = unknown_layout;
        seq_rows = 0;
        file_size = 0;
    }
    
    rc_t print( void ) const {
        rc_t rc = KOutMsg( "Report:\n" );
        if ( 0 == rc ) { rc = KOutMsg( "layout:\t%s\n", Report::Layout2String( layout ).c_str() ); }
        if ( 0 == rc ) { rc = KOutMsg( "SEQ-rows:\t%u\n", seq_rows ); }
        if ( 0 == rc ) { rc = KOutMsg( "size:\t%u\n", file_size ); }
        return rc;
    }

    static std::string Layout2String( Layout layout ) {
        switch( layout ) {
            case cSRA     : return ENUM_TO_STR( cSRA ); break;
            case table    : return ENUM_TO_STR( table ); break;
            case unknown_layout : return ENUM_TO_STR( unknown_layout ); break;
        }
        return ENUM_TO_STR( unknown_layout );
    }
    
};

typedef std::shared_ptr< Report > ReportPtr;
