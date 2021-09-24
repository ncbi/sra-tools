add_subdirectory( adapter )
add_subdirectory( dispatch )
add_subdirectory( language )
add_subdirectory( examples )


add_library( ngs-c++ STATIC
    $<TARGET_OBJECTS:ngs-bind-c++>
    $<TARGET_OBJECTS:ngs-disp>
)
ExportStatic( ngs-c++ true )

add_library( ngs-c++-shared SHARED
    $<TARGET_OBJECTS:ngs-bind-c++>
    $<TARGET_OBJECTS:ngs-disp>
)
set_target_properties( ngs-c++-shared PROPERTIES OUTPUT_NAME ngs-c++ )
MakeLinksShared( ngs-c++-shared ngs-c++ true )


add_library( ngs-sdk STATIC
    $<TARGET_OBJECTS:ngs-disp>
    $<TARGET_OBJECTS:ngs-bind-java>
    $<TARGET_OBJECTS:ngs-bind-python>
)
ExportStatic( ngs-sdk true )

add_library( ngs-sdk-shared SHARED
    $<TARGET_OBJECTS:ngs-disp>
    $<TARGET_OBJECTS:ngs-bind-java>
    $<TARGET_OBJECTS:ngs-bind-python>
)
set_target_properties( ngs-sdk-shared PROPERTIES OUTPUT_NAME ngs-sdk )
MakeLinksShared( ngs-sdk-shared ngs-sdk true )

if( WIN32 )
    link_libraries( Ws2_32 Crypt32 )
endif()

##################################################################################
# C++/Jni/Py wrappers for libncbi-vdb as a static library
add_library( ncbi-ngs STATIC
    $<TARGET_OBJECTS:ngs>
    $<TARGET_OBJECTS:ngs-jni>
    $<TARGET_OBJECTS:ngs-py>
)
ExportStatic( ncbi-ngs true )

##################################################################################
# libncbi-ngs is a shared library that includes libncbi-vdb and its C++/Jni/Py wrappers
#
# NGS-based C++/Java/Python apps will now link against this library instead of libncbi-vdb
#
add_library( ncbi-ngs-shared SHARED
    $<TARGET_OBJECTS:ngs>
    $<TARGET_OBJECTS:ngs-jni>
    $<TARGET_OBJECTS:ngs-py>
)
target_link_libraries( ncbi-ngs-shared ncbi-vdb )
set_target_properties( ncbi-ngs-shared PROPERTIES OUTPUT_NAME ncbi-ngs )
MakeLinksShared( ncbi-ngs-shared ncbi-ngs true )
