if( NOT WIN32 )

    set(CMAKE_CXX_STANDARD 17)
    add_definitions(-msse4.2 -DBMSSE42OPT)

    find_package(ZLIB)
    find_package(BZip2)
    find_package (Threads)

    set(LOCAL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/libs)
    set(LOCAL_INCDIR ${LOCAL_BUILD_DIR}/include)
    set(LOCAL_LIBDIR ${LOCAL_BUILD_DIR}/lib64)

    # Google RE2 release
    set(RE2_TAG 2021-09-01)

    include (ExternalProject)
    ExternalProject_Add(RE2_      
    PREFIX RE2      
    GIT_REPOSITORY https://github.com/google/re2.git
    GIT_TAG ${RE2_TAG}    
    BUILD_IN_SOURCE 1
    UPDATE_COMMAND "" # No update as we use specific tag
    CMAKE_ARGS    
        -DCMAKE_BUILD_TYPE:STRING=Release 
        -DRE2_BUILD_TESTING:BOOL=OFF 
        -DCMAKE_INSTALL_PREFIX:PATH=${LOCAL_BUILD_DIR} 
        -DCMAKE_VERBOSE_MAKEFILE:BOOL=OFF 
        -DCMAKE_POSITION_INDEPENDENT_CODE:BOOL=ON  
        -DCMAKE_INSTALL_MESSAGE:STRING=LAZY
    )
    set(RE2_STATIC_LIBRARIES -L${LOCAL_LIBDIR} -lre2 )
    add_executable(test-sharq-parser test-sharq-parser.cpp )
    add_dependencies(test-sharq-parser RE2_)
    target_include_directories(test-sharq-parser PUBLIC ${LOCAL_INCDIR} ../../tools/sharq)
    target_link_libraries(test-sharq-parser stdc++fs ZLIB::ZLIB ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} ${BZIP2_LIBRARIES} ${RE2_STATIC_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT})

    add_test( NAME Test_sharq_parser COMMAND test-sharq-parser WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} )

endif()