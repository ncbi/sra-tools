add_compile_definitions( __mod__="shared" )

set( SRC toolkit.vers.c )
GenerateStaticLibs( tk-version "${SRC}" )