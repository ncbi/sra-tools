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

enum SplitMode { split3, split_spot, split_file, concat, unknown_split_mode };
enum Format { fastq, fasta, unknown_format };

struct Options {

    std::string accession;
    uint32_t threads;
    SplitMode split_mode;
    Format format;
    bool show_opts;
        
    Options() {
        accession = "";
        threads = 6;
        split_mode = split3;
        format = fastq;
        show_opts = false;
        show_report = false;
    }
    
    rc_t print( void ) const {
        rc_t rc = KOutMsg( "Options:\n" );
        if ( 0 == rc ) { rc = KOutMsg( "acc         : %s\n", accession.c_str() ); }
        if ( 0 == rc ) { rc = KOutMsg( "split-mode  : %s\n", Options::SplitMode2String( split_mode ).c_str() ); }
        if ( 0 == rc ) { rc = KOutMsg( "format      : %s\n", Options::Format2String( format ).c_str() ); }        
        if ( 0 == rc ) { rc = KOutMsg( "threads     : %u\n", threads ); }
        if ( 0 == rc ) { rc = KOutMsg( "show report : %s\n", show_report ? "yes" : "no" ); }
        return rc;
    }
    
    static std::string SplitMode2String( SplitMode mode ) {
        switch( mode ) {
            case split3     : return ENUM_TO_STR( split3 ); break;
            case split_spot : return ENUM_TO_STR( split_spot ); break;
            case split_file : return ENUM_TO_STR( split_file ); break;
            case concat     : return ENUM_TO_STR( concat ); break;
            case unknown_split_mode : return ENUM_TO_STR( unknown_split_mode ); break;
        }
        return ENUM_TO_STR( unknown_split_mode );
    }
    
    static SplitMode String2SplitMode( const std::string& mode ) {
        if ( 0 == mode.compare( ENUM_TO_STR( split3 ) ) ) {
            return split3;
        } else if ( 0 == mode.compare( ENUM_TO_STR( split_spot ) ) ) {
            return split_spot;
        } else if ( 0 == mode.compare( ENUM_TO_STR( split_file ) ) ) {
            return split_file;
        } else if ( 0 == mode.compare( ENUM_TO_STR( concat ) ) ) {
            return concat;
        } else {
            return unknown_split_mode;
        }
    }

    static std::string Format2String( Format format ) {
        switch( format ) {
            case fastq : return ENUM_TO_STR( fastq ); break;
            case fasta : return ENUM_TO_STR( fasta ); break;
            case unknown_format : return ENUM_TO_STR( unknown_format ); break;
        }
        return ENUM_TO_STR( unknown_format );
    }
    
    static Format String2Format( const std::string& format ) {
        if ( 0 == format.compare( ENUM_TO_STR( fastq ) ) ) {
            return fastq;
        } else if ( 0 == format.compare( ENUM_TO_STR( fasta ) ) ) {
            return fasta;
        } else {
            return unknown_format;
        }
    }
    
};

typedef std::shared_ptr< Options > OptionsPtr;
