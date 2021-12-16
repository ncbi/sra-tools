add_subdirectory( adapter )
add_subdirectory( dispatch )
add_subdirectory( language )
add_subdirectory( examples )

set( NGS_CPP_TARGETS
    ngs-bind-c++
    ngs-disp
)

set( NGS_SDK_TARGETS
    ngs-disp
    ngs-bind-python
)
if ( Java_FOUND )
    set( NGS_SDK_TARGETS ${NGS_SDK_TARGETS} ngs-bind-java )
endif()

set( NCBI_NGS_TARGETS
    ngs
    ngs-py
)
if ( Java_FOUND )
    set( NCBI_NGS_TARGETS ${NCBI_NGS_TARGETS} ngs-jni )
endif()

if( SINGLE_CONFIG OR WIN32 )
    include( CMakeLists.posix.txt )
else() # XCode
    include( CMakeLists.xcode.txt )
endif()

install( DIRECTORY ngs
         DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)

