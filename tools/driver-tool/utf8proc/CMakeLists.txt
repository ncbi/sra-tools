set( SRC
  utf8proc.c
  utf8proc.h
)
GenerateStaticLibs( utf8proc "${SRC}" )
target_compile_definitions(utf8proc PUBLIC "UTF8PROC_STATIC")
