/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

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


/* Copy the first part of user declarations.  */


    #include <sysalloc.h>
    #include <ctype.h>
    #include <stdlib.h>
    #include <string.h>

    #include "fastq-parse.h"

    #define YYSTYPE FASTQToken
    #define YYLEX_PARAM pb->scanner
    #define YYDEBUG 1

    #include "fastq-tokens.h"

    static void AddQuality(FASTQParseBlock* pb, const FASTQToken* token);
    static void SetReadNumber(FASTQParseBlock* pb, const FASTQToken* token);
    static void SetSpotGroup(FASTQParseBlock* pb, const FASTQToken* token);
    static void SetRead(FASTQParseBlock* pb, const FASTQToken* token);

    static void StartSpotName(FASTQParseBlock* pb, size_t offset);
    static void GrowSpotName(FASTQParseBlock* pb, const FASTQToken* token);
    static void StopSpotName(FASTQParseBlock* pb);
    static void RestartSpotName(FASTQParseBlock* pb);
    static void SaveSpotName(FASTQParseBlock* pb);
    static void RevertSpotName(FASTQParseBlock* pb);

    #define UNLEX do { if (yychar != YYEMPTY && yychar != YYEOF) FASTQ_unlex(pb, & yylval); } while (0)

    #define IS_PACBIO(pb) ((pb)->defaultReadNumber == -1)



# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "fastq-tokens.h".  */
#ifndef YY_FASTQ_HOME_BOSHKINA_DEVEL_SRA_TOOLS_TOOLS_FASTQ_LOADER_FASTQ_TOKENS_H_INCLUDED
# define YY_FASTQ_HOME_BOSHKINA_DEVEL_SRA_TOOLS_TOOLS_FASTQ_LOADER_FASTQ_TOKENS_H_INCLUDED
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
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int FASTQ_parse (FASTQParseBlock* pb);

#endif /* !YY_FASTQ_HOME_BOSHKINA_DEVEL_SRA_TOOLS_TOOLS_FASTQ_LOADER_FASTQ_TOKENS_H_INCLUDED  */

/* Copy the second part of user declarations.  */



#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

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

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
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
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
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
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
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
#define YYLAST   146

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  24
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  49
/* YYNRULES -- Number of rules.  */
#define YYNRULES  103
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  149

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   269

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
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
static const yytype_uint16 yyrline[] =
{
       0,    82,    82,    84,    89,    90,    92,    88,    95,    97,
     101,   102,   106,   110,   111,   112,   113,   117,   117,   118,
     118,   119,   119,   123,   124,   128,   128,   130,   130,   135,
     135,   137,   137,   142,   143,   148,   149,   151,   153,   154,
     156,   156,   157,   157,   158,   159,   160,   161,   162,   163,
     167,   168,   169,   169,   170,   171,   172,   176,   186,   188,
     187,   194,   194,   195,   195,   196,   196,   197,   201,   202,
     203,   204,   205,   206,   207,   208,   213,   212,   232,   243,
     252,   264,   265,   266,   267,   268,   269,   265,   274,   274,
     275,   279,   280,   281,   285,   286,   287,   293,   294,   298,
     299,   303,   304,   307
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
  "$@5", "$@6", "read", "baseRead", "$@7", "$@8", "csRead", "$@9", "$@10",
  "inlineRead", "tagLine", "$@11", "$@12", "nameSpotGroup", "$@13",
  "nameWS", "nameWithCoords", "$@14", "$@15", "$@16", "$@17", "name",
  "readNumber", "$@18", "pacbioSpotName", "casava1_8", "$@19", "$@20",
  "$@21", "$@22", "$@23", "indexSequence", "$@24", "index", "runSpotRead",
  "qualityLines", "qualityHeader", "quality", "qualityLine", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,    58,    64,    62,    61,    95,
      46,    45,    47,    43
};
# endif

#define YYPACT_NINF -74

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-74)))

#define YYTABLE_NINF -83

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
      50,   -74,    46,   -74,     4,   -74,    55,   -74,     8,   -74,
       7,    51,    46,    58,    60,    46,   -74,    54,    54,   -74,
     -74,   -74,   -74,    57,    32,   -74,   -74,   -74,   -74,   -74,
     -74,   -74,   -74,   -74,   -74,    67,    93,   -74,    54,    14,
     -74,   -74,     5,    77,    89,    25,    97,   -74,   -74,    36,
      46,   -74,    90,    46,    46,   -74,   -74,   -74,   101,   102,
     103,   -74,   106,   100,   104,    79,   -74,   -74,     1,   110,
       6,   -74,   -74,    46,   -74,   107,    46,   -74,   -74,   -74,
     -74,    46,    46,   -74,   -74,   115,   116,   118,    81,   -74,
     -74,    -2,   -74,   -74,   -74,   -74,    83,   -74,    46,   -74,
      86,   -74,   -74,   109,   -74,   -74,   -74,   111,   -74,   -74,
      85,   115,    85,   108,   -74,   -74,   -74,   -74,   113,   114,
     119,    85,    96,   -74,    96,   125,   -74,   -74,   117,    96,
     -74,   107,   127,   126,   107,   -74,   120,   121,   -74,   -74,
     -74,    30,   129,   -74,   -74,   -74,   -74,   120,   -74
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    10,     0,    69,     0,    12,    17,    21,     0,     9,
       0,     3,     0,     0,    16,     0,    19,     0,     0,     1,
      11,    99,     2,     0,     0,    75,    74,     4,    73,    70,
      72,    71,    25,    29,    15,    23,    24,     8,     0,    96,
      68,    18,    35,     0,    50,    48,    45,    22,   100,     0,
       0,    13,     0,     0,     0,    27,    31,    20,     0,     0,
      40,    76,    36,    68,    54,     0,    51,    57,    58,     0,
      46,    49,    44,     0,   103,    97,     0,    14,     5,    26,
      30,     0,     0,    94,    95,     0,     0,     0,    39,    56,
      55,    67,    59,    63,    53,    47,     0,    98,     0,   101,
       0,    28,    32,    81,    41,    43,    77,     0,    38,    65,
       0,     0,     0,    79,    80,   102,    33,    34,     0,     0,
       0,     0,    62,    60,    64,     0,     6,    83,     0,    66,
      78,     0,     0,     0,     7,    84,    90,     0,    88,    37,
      85,    93,     0,    92,    91,    89,    86,    90,    87
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -74,   -74,   -74,   -74,   -74,   128,    -1,   -74,   -74,   -74,
     -74,   -74,   122,   -74,   -74,   -74,   -74,   -74,   -74,   -74,
     -12,   -74,   -74,   -74,   -74,   -74,    94,   -74,   -74,   -74,
     -74,     0,    95,   -74,   -74,    28,   -74,   -74,   -74,   -74,
     -74,    -6,   -74,   -74,   -74,   -74,   -74,    11,   -73
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     8,    52,   100,   131,     9,    10,    11,    12,    17,
      38,    18,    34,    35,    53,    81,    36,    54,    82,   118,
      41,    85,    86,    42,    69,    43,    44,   111,   110,   112,
     121,    45,    62,    87,    71,   104,   119,   132,   137,   142,
     147,   139,   141,   145,    46,    22,    23,    75,    76
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      13,    14,    98,   -61,   -61,    15,    47,     1,    19,   -68,
     -68,    24,    60,    95,    37,     5,    91,   -68,   109,   -68,
      92,    93,    49,   -68,   -68,   -68,    57,    61,    96,   -52,
      25,    26,    67,    50,    58,   143,    59,    73,    68,   144,
      28,    32,    33,    65,    29,    30,    31,    61,    74,    77,
       1,     2,    79,    80,     5,     3,     4,    39,     5,     3,
      40,    98,    16,    25,    26,     5,     6,     7,    48,    32,
      33,    27,    97,    28,    21,    99,    55,    29,    30,    31,
     101,   102,     3,    63,    25,    26,   107,   108,   113,   114,
       3,    40,    68,    66,    28,   116,   117,   115,    29,    30,
      31,    25,    26,    56,    72,    78,    83,    84,    90,   -42,
     122,    28,   124,    88,    94,    29,    30,    31,    89,    74,
     103,   129,   105,   106,   -82,   128,   120,   125,   126,   127,
     130,   136,   133,   135,   146,   138,   140,    64,    20,   123,
      70,   148,   134,     0,     0,     0,    51
};

static const yytype_int16 yycheck[] =
{
       0,     2,    75,     5,     6,     1,    18,     0,     0,     5,
       6,    12,     7,     7,    15,     8,    15,    13,    20,    15,
      19,    20,    23,    19,    20,    21,    38,    22,    22,     4,
       5,     6,     7,     1,    20,     5,    22,     1,    13,     9,
      15,     9,    10,    43,    19,    20,    21,    22,    12,    50,
       0,     1,    53,    54,     8,     5,     6,     3,     8,     5,
       6,   134,     7,     5,     6,     8,    16,    17,    11,     9,
      10,    13,    73,    15,    23,    76,     9,    19,    20,    21,
      81,    82,     5,     6,     5,     6,     5,     6,     5,     6,
       5,     6,    13,     4,    15,     9,    10,    98,    19,    20,
      21,     5,     6,    10,     7,    15,     5,     5,     4,     6,
     110,    15,   112,     7,     4,    19,    20,    21,    18,    12,
       5,   121,     6,     5,    15,     6,    15,    19,    15,    15,
       5,     5,    15,     6,     5,    15,    15,    43,    10,   111,
      45,   147,   131,    -1,    -1,    -1,    24
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     0,     1,     5,     6,     8,    16,    17,    25,    29,
      30,    31,    32,    55,    30,     1,     7,    33,    35,     0,
      29,    23,    69,    70,    30,     5,     6,    13,    15,    19,
      20,    21,     9,    10,    36,    37,    40,    30,    34,     3,
       6,    44,    47,    49,    50,    55,    68,    44,    11,    30,
       1,    36,    26,    38,    41,     9,    10,    44,    20,    22,
       7,    22,    56,     6,    50,    55,     4,     7,    13,    48,
      56,    58,     7,     1,    12,    71,    72,    30,    15,    30,
      30,    39,    42,     5,     5,    45,    46,    57,     7,    18,
       4,    15,    19,    20,     4,     7,    22,    30,    72,    30,
      27,    30,    30,     5,    59,     6,     5,     5,     6,    20,
      52,    51,    53,     5,     6,    30,     9,    10,    43,    60,
      15,    54,    55,    59,    55,    19,    15,    15,     6,    55,
       5,    28,    61,    15,    71,     6,     5,    62,    15,    65,
      15,    66,    63,     5,     9,    67,     5,    64,    65
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    24,    25,    25,    26,    27,    28,    25,    25,    25,
      29,    29,    30,    31,    31,    31,    31,    33,    32,    34,
      32,    35,    32,    36,    36,    38,    37,    39,    37,    41,
      40,    42,    40,    43,    43,    44,    44,    44,    44,    44,
      45,    44,    46,    44,    44,    44,    44,    44,    44,    44,
      47,    47,    48,    47,    47,    47,    47,    49,    50,    51,
      50,    52,    50,    53,    50,    54,    50,    50,    55,    55,
      55,    55,    55,    55,    55,    55,    57,    56,    58,    58,
      58,    59,    60,    61,    62,    63,    64,    59,    66,    65,
      65,    67,    67,    67,    68,    68,    68,    69,    69,    70,
      70,    71,    71,    72
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     0,     0,     0,     9,     3,     1,
       1,     2,     1,     3,     4,     3,     2,     0,     3,     0,
       4,     0,     3,     1,     1,     0,     3,     0,     4,     0,
       3,     0,     4,     1,     1,     1,     2,     9,     4,     3,
       0,     4,     0,     4,     2,     1,     2,     3,     1,     2,
       1,     2,     0,     3,     2,     3,     3,     2,     2,     0,
       5,     0,     5,     0,     5,     0,     6,     3,     1,     1,
       2,     2,     2,     2,     2,     2,     0,     3,     5,     3,
       3,     1,     0,     0,     0,     0,     0,    11,     0,     3,
       0,     1,     1,     0,     3,     3,     1,     3,     4,     1,
       2,     2,     3,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
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


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, FASTQParseBlock* pb)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (pb);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, FASTQParseBlock* pb)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, pb);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
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
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, FASTQParseBlock* pb)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
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
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
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
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
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
            /* Fall through.  */
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

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
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
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

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
      int yyn = yypact[*yyssp];
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
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
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
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
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
          yyp++;
          yyformat++;
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

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

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
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
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
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

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

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
| yyreduce -- Do a reduction.  |
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

    { GrowSpotName(pb, &(yyvsp[0])); StopSpotName(pb); }

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

    { StartSpotName(pb, 1 + (yyvsp[0]).tokenLength); }

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

    { SetRead(pb, & (yyvsp[0])); }

    break;

  case 27:

    { SetRead(pb, & (yyvsp[0])); }

    break;

  case 29:

    { SetRead(pb, & (yyvsp[0])); }

    break;

  case 31:

    { SetRead(pb, & (yyvsp[0])); }

    break;

  case 33:

    { SetRead(pb, & (yyvsp[0])); pb->record->seq.is_colorspace = false; }

    break;

  case 34:

    { SetRead(pb, & (yyvsp[0])); pb->record->seq.is_colorspace = true; }

    break;

  case 38:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 39:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 40:

    { GrowSpotName(pb, &(yyvsp[-1])); StopSpotName(pb); }

    break;

  case 41:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 42:

    { GrowSpotName(pb, &(yyvsp[-1])); StopSpotName(pb); }

    break;

  case 43:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 44:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 45:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 47:

    { FASTQScan_skip_to_eol(pb); }

    break;

  case 51:

    { SetSpotGroup(pb, &(yyvsp[0])); }

    break;

  case 52:

    { StopSpotName(pb); }

    break;

  case 53:

    { SetSpotGroup(pb, &(yyvsp[0])); }

    break;

  case 55:

    { SetSpotGroup(pb, &(yyvsp[0])); }

    break;

  case 56:

    { RevertSpotName(pb); FASTQScan_skip_to_eol(pb); }

    break;

  case 57:

    {   /* 'name' without coordinates attached will be ignored if followed by a name with coordinates (see the previous production).
           however, if not followed, this will be the spot name, so we need to save the 'name's coordinates in case
           we need to revert to them later (see call to RevertSpotName() above) */
        SaveSpotName(pb);
        GrowSpotName(pb, &(yyvsp[0])); /* need to account for white space but it is not part of the spot name */
        RestartSpotName(pb); /* clean up for the potential nameWithCoords to start here */
    }

    break;

  case 58:

    { GrowSpotName(pb, &(yyvsp[0])); StopSpotName(pb); }

    break;

  case 59:

    {   /* another variation by Illumina, this time "_" is used as " /" */
                    GrowSpotName(pb, &(yyvsp[-1]));
                    StopSpotName(pb);
                    GrowSpotName(pb, &(yyvsp[0]));
                }

    break;

  case 61:

    { GrowSpotName(pb, &(yyvsp[-1])); GrowSpotName(pb, &(yyvsp[0]));}

    break;

  case 63:

    { GrowSpotName(pb, &(yyvsp[-1])); GrowSpotName(pb, &(yyvsp[0]));}

    break;

  case 65:

    { GrowSpotName(pb, &(yyvsp[-2])); GrowSpotName(pb, &(yyvsp[-1])); GrowSpotName(pb, &(yyvsp[0]));}

    break;

  case 67:

    { GrowSpotName(pb, &(yyvsp[-1])); GrowSpotName(pb, &(yyvsp[0])); StopSpotName(pb); }

    break;

  case 68:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 69:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 70:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 71:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 72:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 73:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 74:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 75:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 76:

    {   /* in PACBIO fastq, the first '/' and the following digits are treated as a continuation of the spot name, not a read number */
            if ( IS_PACBIO ( pb ) ) pb->spotNameDone = false;
            GrowSpotName(pb, &(yyvsp[0]));
        }

    break;

  case 77:

    {
            if ( IS_PACBIO ( pb ) )
            {
                GrowSpotName(pb, &(yyvsp[0]));
            }
            else
            {
                SetReadNumber(pb, &(yyvsp[0]));
                StopSpotName(pb);
            }
        }

    break;

  case 78:

    {
            if (IS_PACBIO(pb))
            {
                GrowSpotName(pb, &(yyvsp[-3]));
                GrowSpotName(pb, &(yyvsp[-2]));
                GrowSpotName(pb, &(yyvsp[-1]));
                GrowSpotName(pb, &(yyvsp[0]));
                StopSpotName(pb);
            }
        }

    break;

  case 79:

    {
            if (IS_PACBIO(pb))
            {
                GrowSpotName(pb, &(yyvsp[-1]));
                GrowSpotName(pb, &(yyvsp[0]));
                StopSpotName(pb);
            }
        }

    break;

  case 80:

    {
            if (IS_PACBIO(pb))
            {
                GrowSpotName(pb, &(yyvsp[-1]));
                GrowSpotName(pb, &(yyvsp[0]));
                StopSpotName(pb);
            }
        }

    break;

  case 81:

    { SetReadNumber(pb, &(yyvsp[0])); GrowSpotName(pb, &(yyvsp[0])); StopSpotName(pb); }

    break;

  case 82:

    { SetReadNumber(pb, &(yyvsp[0])); GrowSpotName(pb, &(yyvsp[0])); StopSpotName(pb); }

    break;

  case 83:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 84:

    { GrowSpotName(pb, &(yyvsp[0])); if ((yyvsp[0]).tokenLength == 1 && TokenTextPtr(pb, &(yyvsp[0]))[0] == 'Y') pb->record->seq.lowQuality = true; }

    break;

  case 85:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 86:

    { GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 88:

    { GrowSpotName(pb, &(yyvsp[0])); FASTQScan_inline_sequence(pb); }

    break;

  case 91:

    { SetSpotGroup(pb, &(yyvsp[0])); GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 92:

    { SetSpotGroup(pb, &(yyvsp[0])); GrowSpotName(pb, &(yyvsp[0])); }

    break;

  case 94:

    { GrowSpotName(pb, &(yyvsp[-2])); StopSpotName(pb); SetReadNumber(pb, &(yyvsp[0])); }

    break;

  case 95:

    { GrowSpotName(pb, &(yyvsp[-2])); StopSpotName(pb); SetReadNumber(pb, &(yyvsp[0])); }

    break;

  case 96:

    { GrowSpotName(pb, &(yyvsp[0])); StopSpotName(pb); }

    break;

  case 103:

    {  AddQuality(pb, & (yyvsp[0])); }

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

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

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
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
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

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

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
                  yystos[*yyssp], yyvsp, pb);
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

void AddQuality(FASTQParseBlock* pb, const FASTQToken* token)
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
            return;
        }
    }

    {   /* make sure all qualities fall into the required range */
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
                return;
            }
        }
    }

    if (pb->qualityLength == 0)
    {
        pb->qualityOffset = token->tokenStart;
        pb->qualityLength= token->tokenLength;
    }
    else
    {
        pb->qualityLength += token->tokenLength;
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

void GrowSpotName(FASTQParseBlock* pb, const FASTQToken* token)
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
    pb->readOffset = token->tokenStart;
    pb->readLength = token->tokenLength;
}
