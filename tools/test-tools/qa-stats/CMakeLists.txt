cmake_minimum_required (VERSION 3.8)

project(qa-stats)

set(SOURCES
    main.cpp
    input.cpp
)

set(HEADERS
    input.hpp
    parameters.hpp
    stats.hpp
)

if (NOT WIN32)
    find_package(Threads REQUIRED)

    add_executable(qa-stats ${SOURCES} ${HEADERS})
    target_include_directories(qa-stats PRIVATE ../../../libs/inc)
    target_compile_features(qa-stats PRIVATE cxx_std_17)
    target_link_libraries(qa-stats Threads::Threads)

    if (NOT CMAKE_GENERATOR STREQUAL "Xcode")
        MakeLinksExe ( qa-stats false )
    endif()

    if (RUN_SANITIZER_TESTS)
        target_compile_options( qa-stats PRIVATE -fsanitize=undefined )
        target_compile_options( qa-stats PRIVATE -fsanitize=address )
        target_link_options( qa-stats PRIVATE -fsanitize=undefined )
        target_link_options( qa-stats PRIVATE -fsanitize=address )

        add_executable(qa-stats-tsan ${SOURCES} ${HEADERS})
        target_compile_features(qa-stats-tsan PRIVATE cxx_std_17)
        target_link_libraries(qa-stats-tsan Threads::Threads)
        target_compile_options( qa-stats-tsan PRIVATE -fsanitize=thread )
        target_link_options( qa-stats-tsan PRIVATE -fsanitize=thread )
    endif()
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Source Files" FILES ${SOURCES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "Header Files" FILES ${HEADERS})
