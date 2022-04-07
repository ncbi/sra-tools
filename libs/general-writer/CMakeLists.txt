include_directories ( ${CMAKE_SOURCE_DIR}/libs/inc/general-writer )

# libgeneral-writer
add_library ( general-writer
                general-writer.cpp
                utf8-like-int-codec.c
             )

# gw-dumper

add_executable( gw-dumper gw-dumper.cpp )

add_dependencies ( gw-dumper general-writer )

target_link_libraries ( gw-dumper
    general-writer
	${COMMON_LIBS_READ}
)
add_subdirectory ( test )
