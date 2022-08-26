/* A Bison parser, made by GNU Bison 3.5.1.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2020 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.5.1"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         FASTQ_parse
#define yylex           FASTQ_lex
#define yyerror         FASTQ_error
#define yydebug         FASTQ_debug
#define yynerrs         FASTQ_nerrs

/* First part of user prologue.  */

    #include <sysalloc.h>
    #include <ctype.h>
    #include <stdlib.h>
    #include <string.h>

    #include "fastq-parse.h"

    #define YYSTYPE FASTQToken
    #define YYLEX_PARAM pb->scanner
    #define YYDEBUG 1

    #include "fastq-grammar.h"

    static void SetReadNumber ( FASTQParseBlock * pb, const FASTQToken * token );

    static void SetSpotGroup ( FASTQParseBlock * pb, const FASTQToken * token );

    static void SetRead     ( FASTQParseBlock * pb, const FASTQToken * token);
    static void ExpandRead  ( FASTQParseBlock * pb, const FASTQToken * token);

    static void SetQuality      ( FASTQParseBlock * pb, const FASTQToken * token);
    static void ExpandQuality   ( FASTQParseBlock * pb, const FASTQToken * token);

    static void StartSpotName(FASTQParseBlock* pb, size_t offset);
    static void ExpandSpotName(FASTQParseBlock* pb, const FASTQToken* token);
    static void StopSpotName(FASTQParseBlock* pb);
    static void RestartSpotName(FASTQParseBlock* pb);
    static void SaveSpotName(FASTQParseBlock* pb);
    static void RevertSpotName(FASTQParseBlock* pb);

    #define UNLEX do { if (yychar != YYEMPTY && yychar != fqENDOFTEXT) FASTQ_unlex(pb, & yylval); } while (0)

    #define IS_PACBIO(pb) ((pb)->defaultReadNumber == -1)


# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Use api.header.include to #include this header
   instead of duplicating it here.  */
#ifndef YY_FASTQ_HOME_BOSHKINS_NCBI_DEVEL_SRA_TOOLS_TOOLS_FASTQ_LOADER_ZZ_FASTQ_GRAMMAR_H_INCLUDED
# define YY_FASTQ_HOME_BOSHKINS_NCBI_DEVEL_SRA_TOOLS_TOOLS_FASTQ_LOADER_ZZ_FASTQ_GRAMMAR_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int FASTQ_debug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    fqENDOFTEXT = 0,
    fqRUNDOTSPOT = 258,
    fqSPOTGROUP = 259,
    fqNUMBER = 260,
    fqALPHANUM = 261,
    fqWS = 262,
    fqENDLINE = 263,
    fqBASESEQ = 264,
    fqCOLORSEQ = 265,
    fqTOKEN = 266,
    fqASCQUAL = 267,
    fqCOORDS = 268,
    fqUNRECOGNIZED = 269
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef FASTQToken YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int FASTQ_parse (FASTQParseBlock* pb);

#endif /* !YY_FASTQ_HOME_BOSHKINS_NCBI_DEVEL_SRA_TOOLS_TOOLS_FASTQ_LOADER_ZZ_FASTQ_GRAMMAR_H_INCLUDED  */



#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))

/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && ! defined __ICC && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                            \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  19
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   141

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  24
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  50
/* YYNRULES -- Number of rules.  */
#define YYNRULES  105
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  147

#define YYUNDEFTOK  2
#define YYMAXUTOK   269


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    23,     2,    21,    20,    22,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    15,     2,
       2,    18,    17,     2,    16,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    19,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    90,    90,    92,    97,    98,   100,    96,   103,   105,
     109,   110,   114,   118,   119,   120,   121,   125,   125,   126,
     126,   127,   127,   131,   132,   136,   137,   138,   142,   142,
     144,   144,   149,   150,   155,   156,   158,   160,   160,   161,
     161,   162,   164,   164,   165,   165,   167,   167,   169,   170,
     171,   175,   176,   180,   181,   182,   182,   183,   184,   185,
     189,   199,   201,   200,   207,   207,   208,   208,   209,   209,
     210,   214,   215,   216,   217,   218,   219,   220,   221,   226,
     225,   250,   249,   261,   262,   263,   264,   265,   266,   262,
     271,   271,   272,   276,   277,   278,   282,   283,   284,   290,
     291,   295,   296,   300,   301,   302
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "fqENDOFTEXT", "error", "$undefined", "fqRUNDOTSPOT", "fqSPOTGROUP",
  "fqNUMBER", "fqALPHANUM", "fqWS", "fqENDLINE", "fqBASESEQ", "fqCOLORSEQ",
  "fqTOKEN", "fqASCQUAL", "fqCOORDS", "fqUNRECOGNIZED", "':'", "'@'",
  "'>'", "'='", "'_'", "'.'", "'-'", "'/'", "'+'", "$accept", "sequence",
  "$@1", "$@2", "$@3", "endfile", "endline", "readLines", "header", "$@4",
  "$@5", "$@6", "read", "baseRead", "csRead", "$@7", "$@8", "inlineRead",
  "tagLine", "$@9", "$@10", "$@11", "$@12", "$@13", "opt_fqWS",
  "nameSpotGroup", "$@14", "nameWS", "nameWithCoords", "$@15", "$@16",
  "$@17", "$@18", "name", "readNumber", "$@19", "$@20", "casava1_8",
  "$@21", "$@22", "$@23", "$@24", "$@25", "indexSequence", "$@26", "index",
  "runSpotRead", "qualityLines", "qualityHeader", "quality", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_int16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,    58,    64,    62,    61,    95,
      46,    45,    47,    43
};
# endif

#define YYPACT_NINF (-111)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-85)

#define yytable_value_is_error(Yyn) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      26,  -111,     4,  -111,     3,  -111,    38,  -111,    61,  -111,
      11,    40,     4,    53,    88,     4,  -111,    83,    83,  -111,
    -111,  -111,  -111,    54,    27,  -111,  -111,  -111,  -111,  -111,
    -111,  -111,  -111,  -111,  -111,     4,    57,  -111,    83,    30,
    -111,  -111,     7,    94,    66,    34,  -111,  -111,  -111,    45,
       4,  -111,    60,     4,    73,  -111,  -111,    98,   102,   103,
    -111,     8,    97,   109,    72,  -111,  -111,    64,   110,    13,
     111,     4,  -111,     4,  -111,  -111,  -111,  -111,     4,  -111,
    -111,   112,   113,   116,    96,  -111,  -111,  -111,    -3,  -111,
    -111,  -111,  -111,  -111,  -111,  -111,   104,    95,  -111,   107,
    -111,  -111,  -111,   108,  -111,   106,  -111,   106,   112,   106,
    -111,  -111,  -111,   114,   117,   118,   111,   111,    75,   106,
      75,  -111,    75,  -111,  -111,   119,  -111,  -111,    75,   115,
     120,   123,     4,  -111,   121,   122,  -111,  -111,  -111,    55,
     126,  -111,  -111,  -111,  -111,   121,  -111
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_int8 yydefact[] =
{
       0,    10,     0,    72,     0,    12,    17,    21,     0,     9,
       0,     3,     0,     0,    16,     0,    19,     0,     0,     1,
      11,   101,     2,     0,     0,    78,    77,     4,    76,    73,
      75,    74,    25,    28,    15,    23,    24,     8,     0,    98,
      71,    18,    34,     0,    53,    50,    46,    22,   102,     0,
       0,    13,     0,     0,    27,    30,    20,     0,     0,    42,
      79,    35,    71,    57,     0,    54,    60,    61,     0,    48,
      52,     0,   103,    99,    14,     5,    29,    26,     0,    96,
      97,     0,     0,     0,    41,    81,    59,    58,    70,    62,
      66,    56,    49,    51,    47,   100,   105,     0,    31,    83,
      43,    45,    80,    37,    39,     0,    68,     0,     0,     0,
     104,    32,    33,     0,     0,     0,    52,    52,    82,     0,
      65,    63,    67,     6,    85,     0,    38,    40,    69,     0,
       0,     0,     7,    86,    92,     0,    90,    36,    87,    95,
       0,    94,    93,    91,    88,    92,    89
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
    -111,  -111,  -111,  -111,  -111,   125,    -2,  -111,  -111,  -111,
    -111,  -111,   101,  -111,  -111,  -111,  -111,  -111,   -13,  -111,
    -111,  -111,  -111,  -111,  -110,  -111,  -111,  -111,    90,  -111,
    -111,  -111,  -111,     1,    93,  -111,  -111,    31,  -111,  -111,
    -111,  -111,  -111,    -5,  -111,  -111,  -111,  -111,  -111,    12
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     8,    52,    97,   129,     9,    10,    11,    12,    17,
      38,    18,    34,    35,    36,    53,    78,   113,    41,   116,
     117,    81,    82,    70,    94,    42,    68,    43,    44,   108,
     107,   109,   119,    45,    61,    83,   105,   100,   114,   130,
     135,   140,   145,   137,   139,   143,    46,    22,    23,    73
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      14,    13,   -64,   -64,    15,    47,   126,   127,   -71,   -71,
      24,     1,     5,    37,    59,    84,   -71,   106,   -71,     5,
      92,    49,   -71,   -71,   -71,    56,     1,     2,    50,    60,
      85,     3,     4,    54,     5,    85,    32,    33,   -55,    25,
      26,    66,     6,     7,    64,    16,    71,    67,    74,    28,
      57,    76,    58,    29,    30,    31,    60,    72,    25,    26,
     141,    19,     5,    21,   142,    48,    27,    55,    28,    95,
      65,    96,    29,    30,    31,    75,    98,    25,    26,    88,
      25,    26,    77,    89,    90,    67,    39,    28,     3,    40,
      28,    29,    30,    31,    29,    30,    31,    32,    33,     3,
      62,   103,   104,    79,   111,   112,   118,    80,   120,   -44,
     122,     3,    40,    87,    91,    86,   110,    99,    93,   101,
     128,   102,   -84,   115,   125,    51,   133,    72,   134,   123,
      96,   144,   124,    63,   131,    20,   136,   138,    69,   121,
     146,   132
};

static const yytype_uint8 yycheck[] =
{
       2,     0,     5,     6,     1,    18,   116,   117,     5,     6,
      12,     0,     8,    15,     7,     7,    13,    20,    15,     8,
       7,    23,    19,    20,    21,    38,     0,     1,     1,    22,
      22,     5,     6,    35,     8,    22,     9,    10,     4,     5,
       6,     7,    16,    17,    43,     7,     1,    13,    50,    15,
      20,    53,    22,    19,    20,    21,    22,    12,     5,     6,
       5,     0,     8,    23,     9,    11,    13,    10,    15,    71,
       4,    73,    19,    20,    21,    15,    78,     5,     6,    15,
       5,     6,     9,    19,    20,    13,     3,    15,     5,     6,
      15,    19,    20,    21,    19,    20,    21,     9,    10,     5,
       6,     5,     6,     5,     9,    10,   105,     5,   107,     6,
     109,     5,     6,     4,     4,    18,    12,     5,     7,     6,
     119,     5,    15,    15,     6,    24,     6,    12,     5,    15,
     132,     5,    15,    43,    15,    10,    15,    15,    45,   108,
     145,   129
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,     0,     1,     5,     6,     8,    16,    17,    25,    29,
      30,    31,    32,    57,    30,     1,     7,    33,    35,     0,
      29,    23,    71,    72,    30,     5,     6,    13,    15,    19,
      20,    21,     9,    10,    36,    37,    38,    30,    34,     3,
       6,    42,    49,    51,    52,    57,    70,    42,    11,    30,
       1,    36,    26,    39,    30,    10,    42,    20,    22,     7,
      22,    58,     6,    52,    57,     4,     7,    13,    50,    58,
      47,     1,    12,    73,    30,    15,    30,     9,    40,     5,
       5,    45,    46,    59,     7,    22,    18,     4,    15,    19,
      20,     4,     7,     7,    48,    30,    30,    27,    30,     5,
      61,     6,     5,     5,     6,    60,    20,    54,    53,    55,
      12,     9,    10,    41,    62,    15,    43,    44,    57,    56,
      57,    61,    57,    15,    15,     6,    48,    48,    57,    28,
      63,    15,    73,     6,     5,    64,    15,    67,    15,    68,
      65,     5,     9,    69,     5,    66,    67
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_int8 yyr1[] =
{
       0,    24,    25,    25,    26,    27,    28,    25,    25,    25,
      29,    29,    30,    31,    31,    31,    31,    33,    32,    34,
      32,    35,    32,    36,    36,    37,    37,    37,    39,    38,
      40,    38,    41,    41,    42,    42,    42,    43,    42,    44,
      42,    42,    45,    42,    46,    42,    47,    42,    42,    42,
      42,    48,    48,    49,    49,    50,    49,    49,    49,    49,
      51,    52,    53,    52,    54,    52,    55,    52,    56,    52,
      52,    57,    57,    57,    57,    57,    57,    57,    57,    59,
      58,    60,    58,    61,    62,    63,    64,    65,    66,    61,
      68,    67,    67,    69,    69,    69,    70,    70,    70,    71,
      71,    72,    72,    73,    73,    73
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     2,     1,     0,     0,     0,     9,     3,     1,
       1,     2,     1,     3,     4,     3,     2,     0,     3,     0,
       4,     0,     3,     1,     1,     1,     3,     2,     0,     3,
       0,     4,     1,     1,     1,     2,     9,     0,     6,     0,
       6,     3,     0,     4,     0,     4,     0,     3,     2,     3,
       1,     1,     0,     1,     2,     0,     3,     2,     3,     3,
       2,     2,     0,     5,     0,     5,     0,     5,     0,     6,
       3,     1,     1,     2,     2,     2,     2,     2,     2,     0,
       3,     0,     4,     1,     0,     0,     0,     0,     0,    11,
       0,     3,     0,     1,     1,     0,     3,     3,     1,     3,
       4,     1,     2,     1,     3,     2
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (pb, YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, pb); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, FASTQParseBlock* pb)
{
  FILE *yyoutput = yyo;
  YYUSE (yyoutput);
  YYUSE (pb);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyo, yytoknum[yytype], *yyvaluep);
# endif
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo, int yytype, YYSTYPE const * const yyvaluep, FASTQParseBlock* pb)
{
  YYFPRINTF (yyo, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyo, yytype, yyvaluep, pb);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp, int yyrule, FASTQParseBlock* pb)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[+yyssp[yyi + 1 - yynrhs]],
                       &yyvsp[(yyi + 1) - (yynrhs)]
                                              , pb);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, pb); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen(S) (YY_CAST (YYPTRDIFF_T, strlen (S)))
#  else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T
yystrlen (const char *yystr)
{
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYPTRDIFF_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            else
              goto append;

          append:
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (yyres)
    return yystpcpy (yyres, yystr) - yyres;
  else
    return yystrlen (yystr);
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYPTRDIFF_T *yymsg_alloc, char **yymsg,
                yy_state_t *yyssp, int yytoken)
{
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Actual size of YYARG. */
  int yycount = 0;
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[+*yyssp];
      YYPTRDIFF_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
      yysize = yysize0;
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYPTRDIFF_T yysize1
                    = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
                    yysize = yysize1;
                  else
                    return 2;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    /* Don't count the "%s"s in the final size, but reserve room for
       the terminator.  */
    YYPTRDIFF_T yysize1 = yysize + (yystrlen (yyformat) - 2 * yycount) + 1;
    if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
      yysize = yysize1;
    else
      return 2;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          ++yyp;
          ++yyformat;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, FASTQParseBlock* pb)
{
  YYUSE (yyvaluep);
  YYUSE (pb);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (FASTQParseBlock* pb)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    yy_state_fast_t yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss;
    yy_state_t *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYPTRDIFF_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    goto yyexhaustedlab;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
# undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, pb);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 2:
                                { UNLEX; return 1; }
    break;

  case 3:
                                { UNLEX; return 1; }
    break;

  case 4:
                                { ExpandSpotName(pb, &yyvsp[0]); StopSpotName(pb); }
    break;

  case 5:
                                { FASTQScan_inline_sequence(pb); }
    break;

  case 6:
                                { FASTQScan_inline_quality(pb); }
    break;

  case 7:
                                { UNLEX; return 1; }
    break;

  case 8:
                                { UNLEX; return 1; }
    break;

  case 9:
                                { return 0; }
    break;

  case 17:
          { StartSpotName(pb, 1); }
    break;

  case 19:
               { StartSpotName(pb, 1 + yyvsp[0].tokenLength); }
    break;

  case 21:
          { StartSpotName(pb, 1); }
    break;

  case 23:
                { pb->record->seq.is_colorspace = false; }
    break;

  case 24:
                { pb->record->seq.is_colorspace = true; }
    break;

  case 25:
                { SetRead(pb, & yyvsp[0]); }
    break;

  case 26:
                                 { ExpandRead(pb, & yyvsp[0]); }
    break;

  case 28:
                 { SetRead(pb, & yyvsp[0]); }
    break;

  case 30:
                        { SetRead(pb, & yyvsp[0]); }
    break;

  case 32:
                                  { SetRead(pb, & yyvsp[0]); pb->record->seq.is_colorspace = false; }
    break;

  case 33:
                                  { SetRead(pb, & yyvsp[0]); pb->record->seq.is_colorspace = true; }
    break;

  case 37:
                                             { FASTQScan_skip_to_eol(pb); }
    break;

  case 39:
                                               { FASTQScan_skip_to_eol(pb); }
    break;

  case 41:
                                    { FASTQScan_skip_to_eol(pb); }
    break;

  case 42:
                          { ExpandSpotName(pb, &yyvsp[-1]); StopSpotName(pb); }
    break;

  case 43:
                                                                                   { FASTQScan_skip_to_eol(pb); }
    break;

  case 44:
                          { ExpandSpotName(pb, &yyvsp[-1]); StopSpotName(pb); }
    break;

  case 45:
                                                                                    { FASTQScan_skip_to_eol(pb); }
    break;

  case 46:
                  { FASTQScan_skip_to_eol(pb); }
    break;

  case 49:
                            { FASTQScan_skip_to_eol(pb); }
    break;

  case 54:
                                                { SetSpotGroup(pb, &yyvsp[0]); }
    break;

  case 55:
           { StopSpotName(pb); }
    break;

  case 56:
                                                { SetSpotGroup(pb, &yyvsp[0]); }
    break;

  case 58:
                                                { SetSpotGroup(pb, &yyvsp[0]); }
    break;

  case 59:
                             { RevertSpotName(pb); FASTQScan_skip_to_eol(pb); }
    break;

  case 60:
    {   /* 'name' without coordinates attached will be ignored if followed by a name with coordinates (see the previous production).
           however, if not followed, this will be the spot name, so we need to save the 'name's coordinates in case
           we need to revert to them later (see call to RevertSpotName() above) */
        SaveSpotName(pb);
        ExpandSpotName(pb, &yyvsp[0]); /* need to account for white space but it is not part of the spot name */
        RestartSpotName(pb); /* clean up for the potential nameWithCoords to start here */
    }
    break;

  case 61:
                    { ExpandSpotName(pb, &yyvsp[0]); StopSpotName(pb); }
    break;

  case 62:
                {   /* another variation by Illumina, this time "_" is used as " /" */
                    ExpandSpotName(pb, &yyvsp[-1]);
                    StopSpotName(pb);
                    ExpandSpotName(pb, &yyvsp[0]);
                }
    break;

  case 64:
                            { ExpandSpotName(pb, &yyvsp[-1]); ExpandSpotName(pb, &yyvsp[0]);}
    break;

  case 66:
                            { ExpandSpotName(pb, &yyvsp[-1]); ExpandSpotName(pb, &yyvsp[0]);}
    break;

  case 68:
                            { ExpandSpotName(pb, &yyvsp[-2]); ExpandSpotName(pb, &yyvsp[-1]); ExpandSpotName(pb, &yyvsp[0]);}
    break;

  case 70:
                            { ExpandSpotName(pb, &yyvsp[-1]); ExpandSpotName(pb, &yyvsp[0]); StopSpotName(pb); }
    break;

  case 71:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 72:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 73:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 74:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 75:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 76:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 77:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 78:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 79:
        {
            /* in PACBIO fastq, the first '/' and the following digits are treated as a continuation of the spot name, not a read number */
            if ( IS_PACBIO(pb) )
            {
                ExpandSpotName(pb, &yyvsp[0]);
            }
            else
            {
                StopSpotName(pb);
            }
        }
    break;

  case 80:
        {
            if ( IS_PACBIO(pb) )
            {
                ExpandSpotName(pb, &yyvsp[0]);
            }
            else
            {
                SetReadNumber(pb, &yyvsp[0]);
            }
        }
    break;

  case 81:
        {
            if (IS_PACBIO(pb)) pb->spotNameDone = false;
            ExpandSpotName(pb, &yyvsp[0]);
        }
    break;

  case 82:
        {
            if (IS_PACBIO(pb)) StopSpotName(pb);
        }
    break;

  case 83:
                        { SetReadNumber(pb, &yyvsp[0]); ExpandSpotName(pb, &yyvsp[0]); StopSpotName(pb); }
    break;

  case 84:
                        { SetReadNumber(pb, &yyvsp[0]); ExpandSpotName(pb, &yyvsp[0]); StopSpotName(pb); }
    break;

  case 85:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 86:
                        { ExpandSpotName(pb, &yyvsp[0]); if (yyvsp[0].tokenLength == 1 && TokenTextPtr(pb, &yyvsp[0])[0] == 'Y') pb->record->seq.lowQuality = true; }
    break;

  case 87:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 88:
                        { ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 90:
           { ExpandSpotName(pb, &yyvsp[0]); FASTQScan_inline_sequence(pb); }
    break;

  case 93:
                { SetSpotGroup(pb, &yyvsp[0]); ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 94:
                { SetSpotGroup(pb, &yyvsp[0]); ExpandSpotName(pb, &yyvsp[0]); }
    break;

  case 96:
                                    { ExpandSpotName(pb, &yyvsp[-2]); StopSpotName(pb); SetReadNumber(pb, &yyvsp[0]); }
    break;

  case 97:
                                    { ExpandSpotName(pb, &yyvsp[-2]); StopSpotName(pb); SetReadNumber(pb, &yyvsp[0]); }
    break;

  case 98:
                                    { ExpandSpotName(pb, &yyvsp[0]); StopSpotName(pb); }
    break;

  case 103:
                                { SetQuality(pb, & yyvsp[0]); }
    break;

  case 104:
                                { ExpandQuality(pb, & yyvsp[0]); }
    break;



      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (pb, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = YY_CAST (char *, YYSTACK_ALLOC (YY_CAST (YYSIZE_T, yymsg_alloc)));
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (pb, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, pb);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, pb);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;


#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (pb, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif


/*-----------------------------------------------------.
| yyreturn -- parsing is finished, return the result.  |
`-----------------------------------------------------*/
yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, pb);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[+*yyssp], yyvsp, pb);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}


/* values used in validating quality lines */
#define MIN_PHRED_33    33
#define MAX_PHRED_33    126
#define MIN_PHRED_64    64
#define MAX_PHRED_64    127
#define MIN_LOGODDS     59
#define MAX_LOGODDS     126

/* make sure all qualities fall into the required range */
static bool CheckQualities ( FASTQParseBlock* pb, const FASTQToken* token )
{
    uint8_t floor;
    uint8_t ceiling;
    const char* format;
    switch ( pb->qualityFormat )
    {
    case FASTQphred33:
        floor   = MIN_PHRED_33;
        ceiling = MAX_PHRED_33;
        format = "Phred33";
        pb -> qualityAsciiOffset = 33;
        break;
    case FASTQphred64:
        floor   = MIN_PHRED_64;
        ceiling = MAX_PHRED_64;
        format = "Phred64";
        pb -> qualityAsciiOffset = 64;
        break;
    case FASTQlogodds:
        floor   = MIN_LOGODDS;
        ceiling = MAX_LOGODDS;
        format = "Logodds";
        pb -> qualityAsciiOffset = 64;
        break;
    default:
        /* TODO:
            if qualityAsciiOffset is 0,
                guess based on the raw values on the first quality line:
                    if all values are above MAX_PHRED_33, qualityAsciiOffset = 64
                    if all values are in MIN_PHRED_33..MAX_PHRED_33, qualityAsciiOffset = 33
                    if any value is below MIN_PHRED_33, abort
                if the guess is 33 and proven wrong (a raw quality value >MAX_PHRED_33 is encountered and no values below MIN_PHRED_64 ever seen)
                    reopen the file,
                    qualityAsciiOffset = 64
                    try to parse again
                    if a value below MIN_PHRED_64 seen, abort
        */
        {
            char buf[200];
            sprintf ( buf, "Invalid quality format: %d.", pb->qualityFormat );
            pb->fatalError = true;
            yyerror(pb, buf);
            return false;
        }
    }

    {
        unsigned int i;
        for (i=0; i < token->tokenLength; ++i)
        {
            char ch = TokenTextPtr(pb, token)[i];
            if (ch < floor || ch > ceiling)
            {
                char buf[200];
                sprintf ( buf, "Invalid quality value ('%c'=%d, position %d): for %s, valid range is from %d to %d.",
                                                        ch,
                                                        ch,
                                                        i,
                                                        format,
                                                        floor,
                                                        ceiling);
                pb->fatalError = true;
                yyerror(pb, buf);
                return false;
            }
        }
    }
    return true;
}

void SetQuality ( FASTQParseBlock* pb, const FASTQToken* token)
{
    if ( CheckQualities ( pb, token ) )
    {
        pb->qualityOffset = token->tokenStart;
        pb->qualityLength= token->tokenLength;
    }
}

void SetReadNumber(FASTQParseBlock* pb, const FASTQToken* token)
{   /* token is known to be numeric */
    if (pb->defaultReadNumber != -1)
    {   /* we will only handle 1-digit read numbers for now*/
        if (token->tokenLength == 1)
        {
            switch (TokenTextPtr(pb, token)[0])
            {
            case '1':
                {
                    pb->record->seq.readnumber = 1;
                    break;
                }
            case '0':
                {
                    pb->record->seq.readnumber = pb->defaultReadNumber;
                    break;
                }
            default:
                {   /* all secondary read numbers should be the same across an input file */
                    uint8_t readNum = TokenTextPtr(pb, token)[0] - '0';
                    if (pb->secondaryReadNumber == 0) /* this is the first secondary read observed */
                    {
                        pb->secondaryReadNumber = readNum;
                    }
                    else if (pb->secondaryReadNumber != readNum)
                    {
                        char buf[200];
                        sprintf(buf,
                                "Inconsistent secondary read number: previously used %d, now seen %d",
                                pb->secondaryReadNumber, readNum);
                        pb->fatalError = true;
                        yyerror(pb, buf);
                        return;
                    }
                    /* all secondary read numbers are internally represented as 2 */
                    pb->record->seq.readnumber = 2;

                    break;
                }
            }
        }
        else
            pb->record->seq.readnumber = pb->defaultReadNumber;
    }
}

void StartSpotName(FASTQParseBlock* pb, size_t offset)
{
    pb->spotNameOffset = offset;
    pb->spotNameLength = 0;
}

void SaveSpotName(FASTQParseBlock* pb)
{
    pb->spotNameOffset_saved = pb->spotNameOffset;
    pb->spotNameLength_saved = pb->spotNameLength;
}

void RestartSpotName(FASTQParseBlock* pb)
{
    pb->spotNameOffset += pb->spotNameLength;
    pb->spotNameLength = 0;
}

void RevertSpotName(FASTQParseBlock* pb)
{
    pb->spotNameOffset = pb->spotNameOffset_saved;
    pb->spotNameLength = pb->spotNameLength_saved;
}

void ExpandSpotName(FASTQParseBlock* pb, const FASTQToken* token)
{
    if (!pb->spotNameDone)
    {
        pb->spotNameLength += token->tokenLength;
    }
}

void StopSpotName(FASTQParseBlock* pb)
{   /* there may be more tokens coming, they will not be a part of the spot name */
    pb->spotNameDone = true;
}

void SetSpotGroup(FASTQParseBlock* pb, const FASTQToken* token)
{
    if ( ! pb->ignoreSpotGroups )
    {
        unsigned int nameStart = 0;
        /* skip possible '#' at the start of spot group name */
        if ( TokenTextPtr ( pb, token )[0] == '#' )
        {
            nameStart = 1;
        }

        if ( token->tokenLength != 1+nameStart || TokenTextPtr(pb, token)[nameStart] != '0' ) /* ignore spot group 0 */
        {
            pb->spotGroupOffset = token->tokenStart  + nameStart;
            pb->spotGroupLength = token->tokenLength - nameStart;
        }
    }
}

void SetRead(FASTQParseBlock* pb, const FASTQToken* token)
{
    pb -> readOffset = token->tokenStart;
    pb -> readLength = token->tokenLength;
    pb -> expectedQualityLines = 1;
}

/* Handling of multi-line reads and qualities (VDB-3413) */

void ExpandQuality ( FASTQParseBlock * pb, const FASTQToken * newQualityLine )
{
    /* In the already-processed part of the flex buffer, move newQualityLine's text to be immediately after
       the current qualit string's text. Used to discard end-of-lines in multiline qualities */

    if ( CheckQualities ( pb, newQualityLine ) )
    {
        char * copyTo = ( char * ) ( pb -> record -> source . base ) + pb -> qualityOffset + pb -> qualityLength;
        memmove (  copyTo, TokenTextPtr ( pb, newQualityLine ), newQualityLine -> tokenLength );
        pb -> qualityLength += newQualityLine -> tokenLength;
    }
}

void ExpandRead ( FASTQParseBlock * pb, const FASTQToken * newReadLine )
{
    /* In the already-processed part of the flex buffer, move newReadLine's text to be immediately after
       the current read's text. Used to discard end-of-lines in multiline reads */

    char * copyTo = ( char * ) ( pb -> record -> source . base ) + pb -> readOffset + pb -> readLength;
    memmove (  copyTo, TokenTextPtr ( pb, newReadLine ), newReadLine -> tokenLength );
    pb -> readLength += newReadLine -> tokenLength;
    ++ pb -> expectedQualityLines;
}
