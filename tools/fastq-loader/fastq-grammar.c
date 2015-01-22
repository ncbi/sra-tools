/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison implementation for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.5"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse         FASTQ_parse
#define yylex           FASTQ_lex
#define yyerror         FASTQ_error
#define yylval          FASTQ_lval
#define yychar          FASTQ_char
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



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     fqENDOFTEXT = 0,
     fqRUNDOTSPOT = 258,
     fqNUMBER = 259,
     fqALPHANUM = 260,
     fqWS = 261,
     fqENDLINE = 262,
     fqBASESEQ = 263,
     fqCOLORSEQ = 264,
     fqTOKEN = 265,
     fqASCQUAL = 266,
     fqCOORDS = 267,
     fqUNRECOGNIZED = 268
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif


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
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
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
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int yyi)
#else
static int
YYID (yyi)
    int yyi;
#endif
{
  return yyi;
}
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#   if ! defined malloc && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
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
# define YYSTACK_RELOCATE(Stack_alloc, Stack)				\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack_alloc, Stack, yysize);			\
	Stack = &yyptr->Stack_alloc;					\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  22
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   139

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  24
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  52
/* YYNRULES -- Number of rules.  */
#define YYNRULES  101
/* YYNRULES -- Number of states.  */
#define YYNSTATES  144

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   268

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    21,     2,     2,     2,     2,
       2,     2,     2,    23,     2,    20,    19,    22,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    14,     2,
       2,    17,    16,     2,    15,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,    18,     2,     2,     2,     2,
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
       5,     6,     7,     8,     9,    10,    11,    12,    13
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     6,     8,    10,    11,    12,    13,    23,
      27,    29,    31,    34,    36,    40,    45,    49,    50,    54,
      55,    60,    61,    65,    67,    69,    70,    74,    75,    80,
      81,    85,    86,    91,    93,    95,    97,   100,   104,   105,
     110,   111,   116,   118,   121,   125,   127,   129,   132,   133,
     137,   140,   144,   148,   151,   154,   155,   161,   162,   168,
     169,   175,   176,   183,   187,   189,   191,   194,   197,   200,
     203,   206,   209,   210,   214,   215,   219,   220,   224,   225,
     230,   231,   232,   233,   234,   235,   247,   248,   252,   253,
     255,   257,   258,   262,   266,   268,   272,   277,   279,   282,
     285,   289
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      25,     0,    -1,    31,    72,    -1,    31,    -1,    72,    -1,
      -1,    -1,    -1,    55,    12,    26,    14,    27,    43,    14,
      28,    74,    -1,     5,     1,    30,    -1,    29,    -1,     0,
      -1,    30,    29,    -1,     7,    -1,    32,    30,    36,    -1,
      32,    30,     1,    30,    -1,     1,    30,    36,    -1,    -1,
      15,    33,    44,    -1,    -1,    15,     6,    34,    44,    -1,
      -1,    16,    35,    44,    -1,    37,    -1,    40,    -1,    -1,
       8,    38,    30,    -1,    -1,    37,     8,    39,    30,    -1,
      -1,     9,    41,    30,    -1,    -1,    40,     9,    42,    30,
      -1,     8,    -1,     9,    -1,    47,    -1,    47,    59,    -1,
      47,    59,     6,    -1,    -1,    47,     6,    45,    62,    -1,
      -1,    47,     6,    46,     5,    -1,    71,    -1,    55,    59,
      -1,    55,    59,     6,    -1,    55,    -1,    50,    -1,    50,
      56,    -1,    -1,    55,    48,    56,    -1,    49,    50,    -1,
      49,    50,    56,    -1,    49,     5,    17,    -1,    55,     6,
      -1,    55,    12,    -1,    -1,    55,    12,    18,    51,    62,
      -1,    -1,    55,    12,    14,    52,    55,    -1,    -1,    55,
      12,    19,    53,    55,    -1,    -1,    55,    12,    14,    19,
      54,    55,    -1,    55,    12,    14,    -1,     5,    -1,     4,
      -1,    55,    18,    -1,    55,    20,    -1,    55,    19,    -1,
      55,    14,    -1,    55,     5,    -1,    55,     4,    -1,    -1,
      21,    57,     4,    -1,    -1,    21,    58,     5,    -1,    -1,
      22,    60,     4,    -1,    -1,    59,    22,    61,    55,    -1,
      -1,    -1,    -1,    -1,    -1,     4,    63,    14,    64,     5,
      65,    14,    66,     4,    67,    68,    -1,    -1,    14,    69,
      70,    -1,    -1,     8,    -1,     4,    -1,    -1,     3,    19,
       4,    -1,     3,    22,     4,    -1,     3,    -1,    73,    30,
      74,    -1,    73,    30,     1,    30,    -1,    23,    -1,    73,
      10,    -1,    75,    30,    -1,    74,    75,    30,    -1,    11,
      -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    81,    81,    83,    85,    88,    89,    91,    87,    94,
      96,   100,   101,   105,   109,   110,   111,   115,   115,   116,
     116,   117,   117,   121,   122,   126,   126,   128,   128,   133,
     133,   135,   135,   140,   141,   146,   147,   148,   149,   149,
     150,   150,   151,   152,   153,   154,   158,   159,   160,   160,
     161,   162,   163,   167,   177,   179,   178,   185,   185,   186,
     186,   187,   187,   188,   192,   193,   194,   195,   196,   197,
     198,   199,   203,   203,   205,   205,   211,   210,   223,   222,
     234,   235,   236,   237,   238,   234,   243,   243,   244,   248,
     249,   250,   254,   255,   256,   262,   263,   267,   268,   272,
     273,   276
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "fqENDOFTEXT", "error", "$undefined", "fqRUNDOTSPOT", "fqNUMBER",
  "fqALPHANUM", "fqWS", "fqENDLINE", "fqBASESEQ", "fqCOLORSEQ", "fqTOKEN",
  "fqASCQUAL", "fqCOORDS", "fqUNRECOGNIZED", "':'", "'@'", "'>'", "'='",
  "'_'", "'.'", "'-'", "'#'", "'/'", "'+'", "$accept", "sequence", "$@1",
  "$@2", "$@3", "endfile", "endline", "readLines", "header", "$@4", "$@5",
  "$@6", "read", "baseRead", "$@7", "$@8", "csRead", "$@9", "$@10",
  "inlineRead", "tagLine", "$@11", "$@12", "nameSpotGroup", "$@13",
  "nameWS", "nameWithCoords", "$@14", "$@15", "$@16", "$@17", "name",
  "spotGroup", "$@18", "$@19", "readNumber", "$@20", "$@21", "casava1_8",
  "$@22", "$@23", "$@24", "$@25", "$@26", "indexSequence", "$@27", "index",
  "runSpotRead", "qualityLines", "qualityHeader", "quality", "qualityLine", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,    58,    64,    62,    61,    95,    46,
      45,    35,    47,    43
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    24,    25,    25,    25,    26,    27,    28,    25,    25,
      25,    29,    29,    30,    31,    31,    31,    33,    32,    34,
      32,    35,    32,    36,    36,    38,    37,    39,    37,    41,
      40,    42,    40,    43,    43,    44,    44,    44,    45,    44,
      46,    44,    44,    44,    44,    44,    47,    47,    48,    47,
      47,    47,    47,    49,    50,    51,    50,    52,    50,    53,
      50,    54,    50,    50,    55,    55,    55,    55,    55,    55,
      55,    55,    57,    56,    58,    56,    60,    59,    61,    59,
      63,    64,    65,    66,    67,    62,    69,    68,    68,    70,
      70,    70,    71,    71,    71,    72,    72,    73,    73,    74,
      74,    75
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     1,     0,     0,     0,     9,     3,
       1,     1,     2,     1,     3,     4,     3,     0,     3,     0,
       4,     0,     3,     1,     1,     0,     3,     0,     4,     0,
       3,     0,     4,     1,     1,     1,     2,     3,     0,     4,
       0,     4,     1,     2,     3,     1,     1,     2,     0,     3,
       2,     3,     3,     2,     2,     0,     5,     0,     5,     0,
       5,     0,     6,     3,     1,     1,     2,     2,     2,     2,
       2,     2,     0,     3,     0,     3,     0,     3,     0,     4,
       0,     0,     0,     0,     0,    11,     0,     3,     0,     1,
       1,     0,     3,     3,     1,     3,     4,     1,     2,     2,
       3,     1
};

/* YYDEFACT[STATE-NAME] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    11,     0,    65,     0,    13,    17,    21,    97,     0,
      10,     0,     3,     0,     0,     4,     0,     0,     0,    19,
       0,     0,     1,    12,     2,     0,    71,    70,     5,    69,
      66,    68,    67,    98,     0,    25,    29,    16,    23,    24,
       9,     0,    94,    64,    18,    35,     0,    46,    45,    42,
      22,     0,    14,     0,     0,   101,    95,     0,     0,     0,
      27,    31,    20,     0,     0,    38,    76,    36,    64,    50,
       0,    72,    47,    53,    54,     0,    43,    15,     6,    96,
       0,    99,    26,    30,     0,     0,    92,    93,     0,     0,
       0,    37,    78,    52,    51,     0,     0,    63,    55,    59,
      49,    44,     0,   100,    28,    32,    80,    39,    41,    77,
       0,    73,    75,    61,     0,     0,     0,    33,    34,     0,
       0,    79,     0,    58,    56,    60,     7,    81,    62,     0,
       0,     8,    82,     0,    83,     0,    84,    88,    86,    85,
      91,    90,    89,    87
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     9,    53,   102,   129,    10,    11,    12,    13,    20,
      41,    21,    37,    38,    58,    84,    39,    59,    85,   119,
      44,    88,    89,    45,    75,    46,    47,   115,   114,   116,
     122,    48,    72,    95,    96,    67,    90,   110,   107,   120,
     130,   133,   135,   137,   139,   140,   143,    49,    15,    16,
      56,    57
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -55
static const yytype_int8 yypact[] =
{
       3,   -55,    28,   -55,    19,   -55,    43,   -55,   -55,     6,
     -55,     9,    62,    28,    47,   -55,    53,    35,    28,   -55,
      90,    90,   -55,   -55,   -55,    39,   -55,   -55,   -55,   -55,
     -55,   -55,   -55,   -55,    31,   -55,   -55,   -55,    96,    89,
     -55,    90,    59,   -55,   -55,     5,    92,    88,    68,   -55,
     -55,    28,   -55,    94,    28,   -55,   100,    28,    28,    28,
     -55,   -55,   -55,   108,   109,   110,   -55,     7,   101,    88,
      50,   112,   -55,   -55,    57,    88,     8,   -55,   -55,   -55,
      28,   -55,   -55,   -55,    28,    28,   -55,   -55,   115,   116,
     119,   -55,   -55,   -55,   -55,   120,   121,    17,   -55,   -55,
     -55,   -55,    91,   -55,   -55,   -55,   -55,   -55,   -55,   -55,
      98,   -55,   -55,   -55,    98,   115,    98,   -55,   -55,   106,
     111,    87,    98,    87,   -55,    87,   -55,   -55,    87,   100,
     122,   100,   -55,   114,   -55,   125,   -55,   117,   -55,   -55,
      37,   -55,   -55,   -55
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -55,   -55,   -55,   -55,   -55,   123,    -1,   -55,   -55,   -55,
     -55,   -55,   105,   -55,   -55,   -55,   -55,   -55,   -55,   -55,
     -16,   -55,   -55,   -55,   -55,   -55,    86,   -55,   -55,   -55,
     -55,     0,   -41,   -55,   -55,    85,   -55,   -55,    20,   -55,
     -55,   -55,   -55,   -55,   -55,   -55,   -55,   -55,   124,   -55,
      10,   -54
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -75
static const yytype_int16 yytable[] =
{
      14,    17,    80,     1,     2,    50,    22,     3,     4,     1,
       5,    65,    25,    91,   101,    34,     5,    40,     6,     7,
      18,   -57,   -57,   -64,   -64,    62,     8,    66,    94,    92,
      92,   -64,    54,   -64,   100,     5,   113,   -64,   -64,   -64,
      51,   141,    55,    35,    36,   142,    70,    35,    36,    19,
      77,    26,    27,    79,    26,    27,    81,    82,    83,    28,
       5,    29,    74,    33,    29,    30,    31,    32,    30,    31,
      32,    97,    26,    27,    73,    98,    99,    80,    63,   103,
      74,    64,    29,   104,   105,     8,    30,    31,    32,   -48,
      66,    26,    27,    42,     3,    43,     3,    68,    61,   117,
     118,    29,     3,    43,    60,    30,    31,    32,    78,    71,
     121,    55,    86,    87,   123,   -40,   125,   -74,    93,   106,
     126,   108,   128,   109,   111,   127,   112,   132,   134,   136,
      52,   138,    69,    76,    23,   124,    24,     0,     0,   131
};

#define yypact_value_is_default(yystate) \
  ((yystate) == (-55))

#define yytable_value_is_error(yytable_value) \
  YYID (0)

static const yytype_int16 yycheck[] =
{
       0,     2,    56,     0,     1,    21,     0,     4,     5,     0,
       7,     6,    13,     6,     6,    16,     7,    18,    15,    16,
       1,     4,     5,     4,     5,    41,    23,    22,    69,    22,
      22,    12,     1,    14,    75,     7,    19,    18,    19,    20,
       1,     4,    11,     8,     9,     8,    46,     8,     9,     6,
      51,     4,     5,    54,     4,     5,    57,    58,    59,    12,
       7,    14,    12,    10,    14,    18,    19,    20,    18,    19,
      20,    14,     4,     5,     6,    18,    19,   131,    19,    80,
      12,    22,    14,    84,    85,    23,    18,    19,    20,    21,
      22,     4,     5,     3,     4,     5,     4,     5,     9,     8,
       9,    14,     4,     5,     8,    18,    19,    20,    14,    21,
     110,    11,     4,     4,   114,     5,   116,     5,    17,     4,
      14,     5,   122,     4,     4,    14,     5,     5,    14,     4,
      25,    14,    46,    48,    11,   115,    12,    -1,    -1,   129
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     0,     1,     4,     5,     7,    15,    16,    23,    25,
      29,    30,    31,    32,    55,    72,    73,    30,     1,     6,
      33,    35,     0,    29,    72,    30,     4,     5,    12,    14,
      18,    19,    20,    10,    30,     8,     9,    36,    37,    40,
      30,    34,     3,     5,    44,    47,    49,    50,    55,    71,
      44,     1,    36,    26,     1,    11,    74,    75,    38,    41,
       8,     9,    44,    19,    22,     6,    22,    59,     5,    50,
      55,    21,    56,     6,    12,    48,    59,    30,    14,    30,
      75,    30,    30,    30,    39,    42,     4,     4,    45,    46,
      60,     6,    22,    17,    56,    57,    58,    14,    18,    19,
      56,     6,    27,    30,    30,    30,     4,    62,     5,     4,
      61,     4,     5,    19,    52,    51,    53,     8,     9,    43,
      63,    55,    54,    55,    62,    55,    14,    14,    55,    28,
      64,    74,     5,    65,    14,    66,     4,    67,    14,    68,
      69,     4,     8,    70
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  However,
   YYFAIL appears to be in use.  Nevertheless, it is formally deprecated
   in Bison 2.4.2's NEWS entry, where a plan to phase it out is
   discussed.  */

#define YYFAIL		goto yyerrlab
#if defined YYFAIL
  /* This is here to suppress warnings from the GCC cpp's
     -Wunused-macros.  Normally we don't worry about that warning, but
     some users do, and we want to make it easy for users to remove
     YYFAIL uses, which will produce warnings from Bison 2.5.  */
#endif

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (pb, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* This macro is provided for backward compatibility. */

#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, pb)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, pb); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, FASTQParseBlock* pb)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, pb)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    FASTQParseBlock* pb;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (pb);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, FASTQParseBlock* pb)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, pb)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    FASTQParseBlock* pb;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, pb);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
#else
static void
yy_stack_print (yybottom, yytop)
    yytype_int16 *yybottom;
    yytype_int16 *yytop;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, FASTQParseBlock* pb)
#else
static void
yy_reduce_print (yyvsp, yyrule, pb)
    YYSTYPE *yyvsp;
    int yyrule;
    FASTQParseBlock* pb;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , pb);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, pb); \
} while (YYID (0))

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
#ifndef	YYINITDEPTH
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
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
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
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
  YYSIZE_T yysize0 = yytnamerr (0, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  YYSIZE_T yysize1;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = 0;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - Assume YYFAIL is not used.  It's too flawed to consider.  See
       <http://lists.gnu.org/archive/html/bison-patches/2009-12/msg00024.html>
       for details.  YYERROR is fine as it does not invoke this
       function.
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
                yysize1 = yysize + yytnamerr (0, yytname[yyx]);
                if (! (yysize <= yysize1
                       && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                  return 2;
                yysize = yysize1;
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

  yysize1 = yysize + yystrlen (yyformat);
  if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
    return 2;
  yysize = yysize1;

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

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, FASTQParseBlock* pb)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, pb)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    FASTQParseBlock* pb;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (pb);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */
#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (FASTQParseBlock* pb);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */


/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (FASTQParseBlock* pb)
#else
int
yyparse (pb)
    FASTQParseBlock* pb;
#endif
#endif
{
/* The lookahead symbol.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       `yyss': related to states.
       `yyvs': related to semantic values.

       Refer to the stacks thru separate pointers, to allow yyoverflow
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
  int yytoken;
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

  yytoken = 0;
  yyss = yyssa;
  yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */
  yyssp = yyss;
  yyvsp = yyvs;

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
      yychar = YYLEX;
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
  *++yyvsp = yylval;

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
     `$$ = $1'.

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

    { UNLEX; return 1; }
    break;

  case 5:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); StopSpotName(pb); }
    break;

  case 6:

    { FASTQScan_inline_sequence(pb); }
    break;

  case 7:

    { FASTQScan_inline_quality(pb); }
    break;

  case 8:

    { UNLEX; return 1; }
    break;

  case 9:

    { UNLEX; return 1; }
    break;

  case 10:

    { return 0; }
    break;

  case 17:

    { StartSpotName(pb, 1); }
    break;

  case 19:

    { StartSpotName(pb, 1 + (yyvsp[(2) - (2)]).tokenLength); }
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

    { SetRead(pb, & (yyvsp[(1) - (1)])); }
    break;

  case 27:

    { SetRead(pb, & (yyvsp[(2) - (2)])); }
    break;

  case 29:

    { SetRead(pb, & (yyvsp[(1) - (1)])); }
    break;

  case 31:

    { SetRead(pb, & (yyvsp[(2) - (2)])); }
    break;

  case 33:

    { SetRead(pb, & (yyvsp[(1) - (1)])); pb->record->seq.is_colorspace = false; }
    break;

  case 34:

    { SetRead(pb, & (yyvsp[(1) - (1)])); pb->record->seq.is_colorspace = true; }
    break;

  case 37:

    { FASTQScan_skip_to_eol(pb); }
    break;

  case 38:

    { GrowSpotName(pb, &(yyvsp[(1) - (2)])); StopSpotName(pb); }
    break;

  case 39:

    { FASTQScan_skip_to_eol(pb); }
    break;

  case 40:

    { GrowSpotName(pb, &(yyvsp[(1) - (2)])); StopSpotName(pb); }
    break;

  case 41:

    { FASTQScan_skip_to_eol(pb); }
    break;

  case 42:

    { FASTQScan_skip_to_eol(pb); }
    break;

  case 44:

    { FASTQScan_skip_to_eol(pb); }
    break;

  case 48:

    { StopSpotName(pb); }
    break;

  case 52:

    { RevertSpotName(pb); FASTQScan_skip_to_eol(pb); }
    break;

  case 53:

    {   /* 'name' without coordinates attached will be ignored if followed by a name with coordinates (see the previous production).
           however, if not followed, this will be the spot name, so we need to save the 'name's coordinates in case 
           we need to revert to them later (see call to RevertSpotName() above) */
        SaveSpotName(pb); 
        GrowSpotName(pb, &(yyvsp[(2) - (2)])); /* need to account for white space but it is not part of the spot name */
        RestartSpotName(pb); /* clean up for the potential nameWithCoords to start here */
    }
    break;

  case 54:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); StopSpotName(pb); }
    break;

  case 55:

    {   /* another variation by Illumina, this time "_" is used as " /" */
                    GrowSpotName(pb, &(yyvsp[(2) - (3)])); 
                    StopSpotName(pb);
                    GrowSpotName(pb, &(yyvsp[(3) - (3)]));
                }
    break;

  case 57:

    { GrowSpotName(pb, &(yyvsp[(2) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)]));}
    break;

  case 59:

    { GrowSpotName(pb, &(yyvsp[(2) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)]));}
    break;

  case 61:

    { GrowSpotName(pb, &(yyvsp[(2) - (4)])); GrowSpotName(pb, &(yyvsp[(3) - (4)])); GrowSpotName(pb, &(yyvsp[(4) - (4)]));}
    break;

  case 63:

    { GrowSpotName(pb, &(yyvsp[(2) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)])); StopSpotName(pb); }
    break;

  case 64:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); }
    break;

  case 65:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); }
    break;

  case 66:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); }
    break;

  case 67:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); }
    break;

  case 68:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); }
    break;

  case 69:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); }
    break;

  case 70:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); }
    break;

  case 71:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); }
    break;

  case 72:

    { StopSpotName(pb); }
    break;

  case 73:

    { SetSpotGroup(pb, &(yyvsp[(3) - (3)])); }
    break;

  case 74:

    { StopSpotName(pb); }
    break;

  case 75:

    { SetSpotGroup(pb, &(yyvsp[(3) - (3)])); }
    break;

  case 76:

    {   /* in PACBIO fastq, the first '/' and the following digits are treated as a continuation of the spot name, not a read number */
            if (IS_PACBIO(pb)) pb->spotNameDone = false; 
            GrowSpotName(pb, &(yyvsp[(1) - (1)])); 
        }
    break;

  case 77:

    { 
            if (!IS_PACBIO(pb)) SetReadNumber(pb, &(yyvsp[(3) - (3)])); 
            GrowSpotName(pb, &(yyvsp[(3) - (3)])); 
            StopSpotName(pb); 
        }
    break;

  case 78:

    { 
            if (IS_PACBIO(pb)) pb->spotNameDone = false; 
            GrowSpotName(pb, &(yyvsp[(2) - (2)])); 
        }
    break;

  case 79:

    { 
            if (IS_PACBIO(pb)) StopSpotName(pb); 
        }
    break;

  case 80:

    { SetReadNumber(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); StopSpotName(pb); }
    break;

  case 81:

    { GrowSpotName(pb, &(yyvsp[(3) - (3)])); }
    break;

  case 82:

    { GrowSpotName(pb, &(yyvsp[(5) - (5)])); if ((yyvsp[(5) - (5)]).tokenLength == 1 && TokenTextPtr(pb, &(yyvsp[(5) - (5)]))[0] == 'Y') pb->record->seq.lowQuality = true; }
    break;

  case 83:

    { GrowSpotName(pb, &(yyvsp[(7) - (7)])); }
    break;

  case 84:

    { GrowSpotName(pb, &(yyvsp[(9) - (9)])); }
    break;

  case 86:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); FASTQScan_inline_sequence(pb); }
    break;

  case 89:

    { SetSpotGroup(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); }
    break;

  case 90:

    { SetSpotGroup(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); }
    break;

  case 92:

    { SetReadNumber(pb, &(yyvsp[(3) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)])); StopSpotName(pb); }
    break;

  case 93:

    { SetReadNumber(pb, &(yyvsp[(3) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)])); StopSpotName(pb); }
    break;

  case 94:

    { StopSpotName(pb); }
    break;

  case 101:

    {  AddQuality(pb, & (yyvsp[(1) - (1)])); }
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

  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
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

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

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

  *++yyvsp = yylval;


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

#if !defined(yyoverflow) || YYERROR_VERBOSE
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
  /* Do not reclaim the symbols of the rule which action triggered
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
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}





void AddQuality(FASTQParseBlock* pb, const FASTQToken* token)
{
    if (pb->phredOffset != 0)
    {
        uint8_t floor   = pb->phredOffset == 33 ? MIN_PHRED_33 : MIN_PHRED_64;
        uint8_t ceiling = pb->maxPhred == 0 ? (pb->phredOffset == 33 ? MAX_PHRED_33 : MAX_PHRED_64) : pb->maxPhred;
        unsigned int i;
        for (i=0; i < token->tokenLength; ++i)
        {
            char buf[200];
			char ch = TokenTextPtr(pb, token)[i];
            if (ch < floor || ch > ceiling)
            {
                sprintf(buf, "Invalid quality value ('%c'=%d, position %d): for %s, valid range is from %d to %d.", 
                                                         ch,
														 ch,
														 i,
                                                         pb->phredOffset == 33 ? "Phred33": "Phred64", 
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
        if (token->tokenLength != 1 || TokenTextPtr(pb, token)[0] != '0') /* ignore spot group 0 */
        {
            pb->spotGroupOffset = token->tokenStart;    
            pb->spotGroupLength = token->tokenLength;
        }
    }
}

void SetRead(FASTQParseBlock* pb, const FASTQToken* token)
{ 
    pb->readOffset = token->tokenStart;
    pb->readLength = token->tokenLength;
}

