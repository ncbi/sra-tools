# ===========================================================================
#
#                            PUBLIC DOMAIN NOTICE
#               National Center for Biotechnology Information
#
#  This software/database is a "United States Government Work" under the
#  terms of the United States Copyright Act.  It was written as part of
#  the author's official duties as a United States Government employee and
#  thus cannot be copyrighted.  This software/database is freely available
#  to the public for use. The National Library of Medicine and the U.S.
#  Government have not placed any restriction on its use or reproduction.
#
#  Although all reasonable efforts have been taken to ensure the accuracy
#  and reliability of the software and data, the NLM and the U.S.
#  Government do not and cannot warrant the performance or results that
#  may be obtained by using this software or data. The NLM and the U.S.
#  Government disclaim all warranties, express or implied, including
#  warranties of performance, merchantability or fitness for any particular
#  purpose.
#
#  Please cite the author in any work or product based on this material.
#
# ===========================================================================

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINDIR} )

add_subdirectory( driver-tool )
add_subdirectory( util )
add_subdirectory( align-cache )
add_subdirectory( align-info )
add_subdirectory( bam-loader )
add_subdirectory( rd-filter-redact )
add_subdirectory( prefetch )
add_subdirectory( vdb-config )
add_subdirectory( kar )
add_subdirectory( copycat )
add_subdirectory( sra-dump )
add_subdirectory( sra-sort )
add_subdirectory( sra-pileup )
add_subdirectory( fastq-loader )
add_subdirectory( fasterq-dump )
add_subdirectory( vdb-dump )
add_subdirectory( vdb-copy )
add_subdirectory( vdb-validate )
add_subdirectory( vdb-decrypt )
add_subdirectory( srapath )
add_subdirectory( sra-stat )
add_subdirectory( sharq )
add_subdirectory( cache-mgr )
add_subdirectory( sra-load )
add_subdirectory( ccextract )
add_subdirectory( pacbio-load )
add_subdirectory( vdb-diff )
add_subdirectory( ngs-pileup )
add_subdirectory( vdb-sql )
add_subdirectory( cg-load )
add_subdirectory( ref-variation )
add_subdirectory( sra-search )

if( NOT WIN32 )
	add_subdirectory( general-loader )
    add_subdirectory( pileup-stats )
endif()
