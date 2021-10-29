add_compile_definitions( __mod__="tools/copycat" )

# External
if( ${OS} STREQUAL "linux" )
    add_executable( copycat
        copycat
        ccfileformat
        cccat
        cctree
        cctree-dump
        cctar
        ccsra
        ccsubchunk
        ccfile
    )
    target_link_libraries( copycat ${COMMON_LINK_LIBRARIES} ${COMMON_LIBS_READ} kff magic-static )
    MakeLinksExe( copycat true )
endif()
