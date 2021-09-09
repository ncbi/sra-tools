if( WIN32 )
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG})
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE})
	
	message( "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG}" )
	message( "CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE}" )

    #include( CMakeLists.win.txt )
elseif( SINGLE_CONFIG )
    #include( CMakeLists.posix.txt )
else() # XCode
    #include( CMakeLists.xcode.txt )
endif()


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

#link_libraries( ncbi-vdb ngs )

add_library( ncbi-ngs STATIC
    $<TARGET_OBJECTS:ngs>
    $<TARGET_OBJECTS:ngs-jni>
    $<TARGET_OBJECTS:ngs-py>
)
ExportStatic( ncbi-ngs true )

add_library( ncbi-ngs-shared SHARED
    $<TARGET_OBJECTS:ngs>
    $<TARGET_OBJECTS:ngs-jni>
    $<TARGET_OBJECTS:ngs-py>
)
set_target_properties( ncbi-ngs-shared PROPERTIES OUTPUT_NAME ncbi-ngs )
MakeLinksShared( ncbi-ngs-shared ncbi-ngs true )
