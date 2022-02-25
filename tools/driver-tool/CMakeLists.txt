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

add_compile_definitions( __mod__="tools/driver-tool" )
#add_compile_definitions( HAVE_CXXABI_H="1" )

add_subdirectory( json )
add_subdirectory( secure )
add_subdirectory( utf8proc )

include_directories( inc )

set( SRATOOLS_SRC
	sratools
	sratools2
	imp_fasterq_dump
	imp_fastq_dump
	imp_prefetch
	imp_sam_dump
	imp_sra_pileup
	imp_srapath
	imp_vdb_dump
	cmdline
	config
	proc
	run-source
	uuid
	service
    support2
)

if( WIN32 )
    set( ADDITIONAL_LIBS
            libcmt$<$<CONFIG:Debug>:d>
            libcpmt$<$<CONFIG:Debug>:d>
            libucrt$<$<CONFIG:Debug>:d>
            libvcruntime$<$<CONFIG:Debug>:d>
    )
else()
	set( ADDITIONAL_LIBS m )
endif()

add_executable( sratools "${SRATOOLS_SRC}" )
target_link_libraries( sratools ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} json secure utf8proc ${ADDITIONAL_LIBS} )

include(CheckIncludeFileCXX)
# unset(HAVE_CXXABI_H CACHE)
# unset(HAVE_EXECINFO_H CACHE)
# unset(HAVE_CXXABI_F CACHE)
check_include_file_cxx(cxxabi.h HAVE_CXXABI_H)
check_include_file_cxx(execinfo.h HAVE_EXECINFO_H)

if ( HAVE_CXXABI_H AND HAVE_EXECINFO_H )
    include(CheckCXXSourceRuns)
    check_cxx_source_runs("
#include <stdio.h>
#include <cxxabi.h>
#include <execinfo.h>

int main(int argc, char *argv[]) {
    void * callstack [ 128 ];
    int stack_frames;
    stack_frames = backtrace ( callstack, sizeof callstack / sizeof callstack [ 0 ] );
    printf ( \"stack_frames: %d\", stack_frames );

    int status;
    char* res = abi::__cxa_demangle ( \"main\", NULL, NULL, &status );
    printf ( \"res=%p, status=%d\", res, status );

    return 0;
}
" HAVE_CXXABI_F)
endif()


if( HAVE_CXXABI_H )
	message("HAVE_CXXABI_H: ${HAVE_CXXABI_H}")
    target_compile_definitions( sratools PRIVATE HAVE_CXXABI_H )
endif()
if( HAVE_EXECINFO_H )
	message("HAVE_EXECINFO_H: ${HAVE_EXECINFO_H}")
    target_compile_definitions( sratools PRIVATE HAVE_EXECINFO_H )
endif()
if( HAVE_CXXABI_F )
	message("HAVE_CXXABI_F: ${HAVE_CXXABI_F}")
    target_compile_definitions( sratools PRIVATE HAVE_CXXABI_F )
endif()

# add_executable( cxxabi_test cxxabi_test )
# target_link_libraries( cxxabi_test ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} json secure utf8proc ${ADDITIONAL_LIBS} )

MakeLinksExe( sratools false )
