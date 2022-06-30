include_directories( ../inc )

set( SRC
	busy
	payload
	base64
	except
	string
	atoi
	memset_s
)

GenerateStaticLibs( secure "${SRC}" )
target_compile_definitions(secure PUBLIC "UTF8PROC_STATIC")

include(CheckIncludeFileCXX)
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
    target_compile_definitions( secure PRIVATE HAVE_CXXABI_H )
endif()
if( HAVE_EXECINFO_H )
    target_compile_definitions( secure PRIVATE HAVE_EXECINFO_H )
endif()
if( HAVE_CXXABI_F )
    target_compile_definitions( secure PRIVATE HAVE_CXXABI_F )
endif()
