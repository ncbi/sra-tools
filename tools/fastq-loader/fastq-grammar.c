
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
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
#define YYBISON_VERSION "2.4.1"

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
# if YYENABLE_NLS
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
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
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
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
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

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  19
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   153

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  24
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  52
/* YYNRULES -- Number of rules.  */
#define YYNRULES  106
/* YYNRULES -- Number of states.  */
#define YYNSTATES  151

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
       0,     0,     3,     6,     8,     9,    10,    11,    21,    25,
      27,    29,    32,    34,    38,    43,    47,    50,    51,    55,
      56,    61,    62,    66,    68,    70,    71,    75,    76,    81,
      82,    86,    87,    92,    94,    96,    98,   101,   111,   116,
     120,   121,   126,   127,   132,   135,   137,   140,   144,   146,
     148,   151,   152,   156,   159,   163,   167,   170,   173,   174,
     180,   181,   187,   188,   194,   195,   202,   206,   208,   210,
     213,   216,   219,   222,   225,   228,   229,   233,   234,   238,
     240,   241,   245,   246,   251,   253,   254,   255,   256,   257,
     258,   270,   271,   275,   276,   278,   280,   281,   285,   289,
     291,   295,   300,   302,   305,   308,   312
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      25,     0,    -1,    31,    72,    -1,    31,    -1,    -1,    -1,
      -1,    55,    12,    26,    14,    27,    43,    14,    28,    74,
      -1,     5,     1,    30,    -1,    29,    -1,     0,    -1,    30,
      29,    -1,     7,    -1,    32,    30,    36,    -1,    32,    30,
       1,    30,    -1,     1,    30,    36,    -1,     1,    30,    -1,
      -1,    15,    33,    44,    -1,    -1,    15,     6,    34,    44,
      -1,    -1,    16,    35,    44,    -1,    37,    -1,    40,    -1,
      -1,     8,    38,    30,    -1,    -1,    37,     8,    39,    30,
      -1,    -1,     9,    41,    30,    -1,    -1,    40,     9,    42,
      30,    -1,     8,    -1,     9,    -1,    47,    -1,    47,    59,
      -1,    47,    59,     6,     4,    14,     5,    14,     4,    68,
      -1,    47,    59,     6,     5,    -1,    47,    59,     6,    -1,
      -1,    47,     6,    45,    62,    -1,    -1,    47,     6,    46,
       5,    -1,    71,     6,    -1,    71,    -1,    55,    59,    -1,
      55,    59,     6,    -1,    55,    -1,    50,    -1,    50,    56,
      -1,    -1,    55,    48,    56,    -1,    49,    50,    -1,    49,
      50,    56,    -1,    49,     5,    17,    -1,    55,     6,    -1,
      55,    12,    -1,    -1,    55,    12,    18,    51,    62,    -1,
      -1,    55,    12,    14,    52,    55,    -1,    -1,    55,    12,
      19,    53,    55,    -1,    -1,    55,    12,    14,    19,    54,
      55,    -1,    55,    12,    14,    -1,     5,    -1,     4,    -1,
      55,    18,    -1,    55,    20,    -1,    55,    19,    -1,    55,
      14,    -1,    55,     5,    -1,    55,     4,    -1,    -1,    21,
      57,     4,    -1,    -1,    21,    58,     5,    -1,    21,    -1,
      -1,    22,    60,     4,    -1,    -1,    59,    22,    61,    55,
      -1,     4,    -1,    -1,    -1,    -1,    -1,    -1,     4,    63,
      14,    64,     5,    65,    14,    66,     4,    67,    68,    -1,
      -1,    14,    69,    70,    -1,    -1,     8,    -1,     4,    -1,
      -1,     3,    19,     4,    -1,     3,    22,     4,    -1,     3,
      -1,    73,    30,    74,    -1,    73,    30,     1,    30,    -1,
      23,    -1,    73,    10,    -1,    75,    30,    -1,    74,    75,
      30,    -1,    11,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,    81,    81,    83,    88,    89,    91,    87,    94,    96,
     100,   101,   105,   109,   110,   111,   112,   116,   116,   117,
     117,   118,   118,   122,   123,   127,   127,   129,   129,   134,
     134,   136,   136,   141,   142,   147,   148,   150,   152,   153,
     155,   155,   156,   156,   157,   158,   159,   160,   161,   165,
     166,   167,   167,   168,   169,   170,   174,   184,   186,   185,
     192,   192,   193,   193,   194,   194,   195,   199,   200,   201,
     202,   203,   204,   205,   206,   210,   210,   212,   212,   214,
     219,   218,   231,   230,   242,   243,   244,   245,   246,   247,
     243,   252,   252,   253,   257,   258,   259,   263,   264,   265,
     271,   272,   276,   277,   281,   282,   285
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
       0,    24,    25,    25,    26,    27,    28,    25,    25,    25,
      29,    29,    30,    31,    31,    31,    31,    33,    32,    34,
      32,    35,    32,    36,    36,    38,    37,    39,    37,    41,
      40,    42,    40,    43,    43,    44,    44,    44,    44,    44,
      45,    44,    46,    44,    44,    44,    44,    44,    44,    47,
      47,    48,    47,    47,    47,    47,    49,    50,    51,    50,
      52,    50,    53,    50,    54,    50,    50,    55,    55,    55,
      55,    55,    55,    55,    55,    57,    56,    58,    56,    56,
      60,    59,    61,    59,    62,    63,    64,    65,    66,    67,
      62,    69,    68,    68,    70,    70,    70,    71,    71,    71,
      72,    72,    73,    73,    74,    74,    75
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     2,     1,     0,     0,     0,     9,     3,     1,
       1,     2,     1,     3,     4,     3,     2,     0,     3,     0,
       4,     0,     3,     1,     1,     0,     3,     0,     4,     0,
       3,     0,     4,     1,     1,     1,     2,     9,     4,     3,
       0,     4,     0,     4,     2,     1,     2,     3,     1,     1,
       2,     0,     3,     2,     3,     3,     2,     2,     0,     5,
       0,     5,     0,     5,     0,     6,     3,     1,     1,     2,
       2,     2,     2,     2,     2,     0,     3,     0,     3,     1,
       0,     3,     0,     4,     1,     0,     0,     0,     0,     0,
      11,     0,     3,     0,     1,     1,     0,     3,     3,     1,
       3,     4,     1,     2,     2,     3,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,    10,     0,    68,     0,    12,    17,    21,     0,     9,
       0,     3,     0,     0,    16,     0,    19,     0,     0,     1,
      11,   102,     2,     0,     0,    74,    73,     4,    72,    69,
      71,    70,    25,    29,    15,    23,    24,     8,     0,    99,
      67,    18,    35,     0,    49,    48,    45,    22,   103,     0,
       0,    13,     0,     0,     0,    27,    31,    20,     0,     0,
      40,    80,    36,    67,    53,     0,    79,    50,    56,    57,
       0,    46,    44,     0,   106,   100,     0,    14,     5,    26,
      30,     0,     0,    97,    98,     0,     0,     0,    39,    82,
      55,    54,     0,     0,    66,    58,    62,    52,    47,   101,
       0,   104,     0,    28,    32,    84,    41,    43,    81,     0,
      38,     0,    76,    78,    64,     0,     0,     0,   105,    33,
      34,     0,     0,     0,    83,     0,    61,    59,    63,     6,
      86,     0,    65,     0,     0,     0,     7,    87,    93,     0,
      91,    37,    88,    96,     0,    95,    94,    92,    89,    93,
      90
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     8,    52,   102,   133,     9,    10,    11,    12,    17,
      38,    18,    34,    35,    53,    81,    36,    54,    82,   121,
      41,    85,    86,    42,    70,    43,    44,   116,   115,   117,
     125,    45,    67,    92,    93,    62,    87,   111,   106,   122,
     134,   139,   144,   149,   141,   143,   147,    46,    22,    23,
      75,    76
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -74
static const yytype_int16 yypact[] =
{
      40,   -74,     1,   -74,    12,   -74,    36,   -74,    18,   -74,
      39,    25,     1,    46,    -2,     1,   -74,    90,    90,   -74,
     -74,   -74,   -74,    81,    89,   -74,   -74,   -74,   -74,   -74,
     -74,   -74,   -74,   -74,   -74,    51,    58,   -74,    90,    70,
     -74,   -74,     3,    99,    53,    15,    73,   -74,   -74,     4,
       1,   -74,   100,     1,     1,   -74,   -74,   -74,   112,   114,
     115,   -74,     6,   102,    53,    64,   101,   -74,   -74,    43,
      53,    32,   -74,     1,   -74,   110,     1,   -74,   -74,   -74,
     -74,     1,     1,   -74,   -74,   118,   119,   122,   103,   -74,
     -74,   -74,   123,   124,    66,   -74,   -74,   -74,   -74,   -74,
       1,   -74,   104,   -74,   -74,   109,   -74,   -74,   -74,   116,
     -74,   105,   -74,   -74,   -74,   105,   118,   105,   -74,   -74,
     -74,   117,   120,   127,    82,   105,    82,   -74,    82,   -74,
     -74,   121,    82,   110,   128,   132,   110,   -74,   125,   126,
     -74,   -74,   -74,    69,   133,   -74,   -74,   -74,   -74,   125,
     -74
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
     -74,   -74,   -74,   -74,   -74,   131,    -1,   -74,   -74,   -74,
     -74,   -74,   129,   -74,   -74,   -74,   -74,   -74,   -74,   -74,
     -15,   -74,   -74,   -74,   -74,   -74,    85,   -74,   -74,   -74,
     -74,     0,   -60,   -74,   -74,    93,   -74,   -74,    26,   -74,
     -74,   -74,   -74,   -74,    -6,   -74,   -74,   -74,   -74,   -74,
      11,   -73
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -86
static const yytype_int16 yytable[] =
{
      13,    14,   100,    47,    91,    73,    32,    33,     5,    60,
      97,    24,    88,    15,    37,    74,   -67,   -67,    19,    25,
      26,    68,    49,    57,   -67,    61,   -67,    69,    89,    28,
     -67,   -67,   -67,    29,    30,    31,   -51,    61,    98,     1,
       1,     2,    16,    65,     3,     4,     5,     5,    21,    77,
      25,    26,    79,    80,    89,     6,     7,    94,    27,    55,
      28,    95,    96,   100,    29,    30,    31,    56,    25,    26,
     -60,   -60,    99,   145,    66,   101,    69,   146,    28,    72,
     103,   104,    29,    30,    31,   114,    25,    26,     5,    58,
      50,    48,    59,    39,     3,    40,    28,    32,    33,   118,
      29,    30,    31,     3,    63,   -75,   -77,   109,   110,     3,
      40,   124,   119,   120,    78,   126,    83,   128,    84,    90,
     -42,    74,   105,   -85,   107,   132,   108,   112,    64,   113,
     123,   129,   131,   137,   130,   135,   138,   148,    71,   140,
     142,    20,   127,   150,   136,     0,     0,     0,     0,     0,
       0,     0,     0,    51
};

static const yytype_int16 yycheck[] =
{
       0,     2,    75,    18,    64,     1,     8,     9,     7,     6,
      70,    12,     6,     1,    15,    11,     4,     5,     0,     4,
       5,     6,    23,    38,    12,    22,    14,    12,    22,    14,
      18,    19,    20,    18,    19,    20,    21,    22,     6,     0,
       0,     1,     6,    43,     4,     5,     7,     7,    23,    50,
       4,     5,    53,    54,    22,    15,    16,    14,    12,     8,
      14,    18,    19,   136,    18,    19,    20,     9,     4,     5,
       4,     5,    73,     4,    21,    76,    12,     8,    14,     6,
      81,    82,    18,    19,    20,    19,     4,     5,     7,    19,
       1,    10,    22,     3,     4,     5,    14,     8,     9,   100,
      18,    19,    20,     4,     5,     4,     5,     4,     5,     4,
       5,   111,     8,     9,    14,   115,     4,   117,     4,    17,
       5,    11,     4,    14,     5,   125,     4,     4,    43,     5,
      14,    14,     5,     5,    14,    14,     4,     4,    45,    14,
      14,    10,   116,   149,   133,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    24
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     0,     1,     4,     5,     7,    15,    16,    25,    29,
      30,    31,    32,    55,    30,     1,     6,    33,    35,     0,
      29,    23,    72,    73,    30,     4,     5,    12,    14,    18,
      19,    20,     8,     9,    36,    37,    40,    30,    34,     3,
       5,    44,    47,    49,    50,    55,    71,    44,    10,    30,
       1,    36,    26,    38,    41,     8,     9,    44,    19,    22,
       6,    22,    59,     5,    50,    55,    21,    56,     6,    12,
      48,    59,     6,     1,    11,    74,    75,    30,    14,    30,
      30,    39,    42,     4,     4,    45,    46,    60,     6,    22,
      17,    56,    57,    58,    14,    18,    19,    56,     6,    30,
      75,    30,    27,    30,    30,     4,    62,     5,     4,     4,
       5,    61,     4,     5,    19,    52,    51,    53,    30,     8,
       9,    43,    63,    14,    55,    54,    55,    62,    55,    14,
      14,     5,    55,    28,    64,    14,    74,     5,     4,    65,
      14,    68,    14,    69,    66,     4,     8,    70,     4,    67,
      68
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
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
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


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
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

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
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





/*-------------------------.
| yyparse or yypush_parse.  |
`-------------------------*/

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
  if (yyn == YYPACT_NINF)
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
      if (yyn == 0 || yyn == YYTABLE_NINF)
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

    { UNLEX; return 1; ;}
    break;

  case 3:

    { UNLEX; return 1; ;}
    break;

  case 4:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); StopSpotName(pb); ;}
    break;

  case 5:

    { FASTQScan_inline_sequence(pb); ;}
    break;

  case 6:

    { FASTQScan_inline_quality(pb); ;}
    break;

  case 7:

    { UNLEX; return 1; ;}
    break;

  case 8:

    { UNLEX; return 1; ;}
    break;

  case 9:

    { return 0; ;}
    break;

  case 17:

    { StartSpotName(pb, 1); ;}
    break;

  case 19:

    { StartSpotName(pb, 1 + (yyvsp[(2) - (2)]).tokenLength); ;}
    break;

  case 21:

    { StartSpotName(pb, 1); ;}
    break;

  case 23:

    { pb->record->seq.is_colorspace = false; ;}
    break;

  case 24:

    { pb->record->seq.is_colorspace = true; ;}
    break;

  case 25:

    { SetRead(pb, & (yyvsp[(1) - (1)])); ;}
    break;

  case 27:

    { SetRead(pb, & (yyvsp[(2) - (2)])); ;}
    break;

  case 29:

    { SetRead(pb, & (yyvsp[(1) - (1)])); ;}
    break;

  case 31:

    { SetRead(pb, & (yyvsp[(2) - (2)])); ;}
    break;

  case 33:

    { SetRead(pb, & (yyvsp[(1) - (1)])); pb->record->seq.is_colorspace = false; ;}
    break;

  case 34:

    { SetRead(pb, & (yyvsp[(1) - (1)])); pb->record->seq.is_colorspace = true; ;}
    break;

  case 38:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 39:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 40:

    { GrowSpotName(pb, &(yyvsp[(1) - (2)])); StopSpotName(pb); ;}
    break;

  case 41:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 42:

    { GrowSpotName(pb, &(yyvsp[(1) - (2)])); StopSpotName(pb); ;}
    break;

  case 43:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 44:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 45:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 47:

    { FASTQScan_skip_to_eol(pb); ;}
    break;

  case 51:

    { StopSpotName(pb); ;}
    break;

  case 55:

    { RevertSpotName(pb); FASTQScan_skip_to_eol(pb); ;}
    break;

  case 56:

    {   /* 'name' without coordinates attached will be ignored if followed by a name with coordinates (see the previous production).
           however, if not followed, this will be the spot name, so we need to save the 'name's coordinates in case 
           we need to revert to them later (see call to RevertSpotName() above) */
        SaveSpotName(pb); 
        GrowSpotName(pb, &(yyvsp[(2) - (2)])); /* need to account for white space but it is not part of the spot name */
        RestartSpotName(pb); /* clean up for the potential nameWithCoords to start here */
    ;}
    break;

  case 57:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); StopSpotName(pb); ;}
    break;

  case 58:

    {   /* another variation by Illumina, this time "_" is used as " /" */
                    GrowSpotName(pb, &(yyvsp[(2) - (3)])); 
                    StopSpotName(pb);
                    GrowSpotName(pb, &(yyvsp[(3) - (3)]));
                ;}
    break;

  case 60:

    { GrowSpotName(pb, &(yyvsp[(2) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)]));;}
    break;

  case 62:

    { GrowSpotName(pb, &(yyvsp[(2) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)]));;}
    break;

  case 64:

    { GrowSpotName(pb, &(yyvsp[(2) - (4)])); GrowSpotName(pb, &(yyvsp[(3) - (4)])); GrowSpotName(pb, &(yyvsp[(4) - (4)]));;}
    break;

  case 66:

    { GrowSpotName(pb, &(yyvsp[(2) - (3)])); GrowSpotName(pb, &(yyvsp[(3) - (3)])); StopSpotName(pb); ;}
    break;

  case 67:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); ;}
    break;

  case 68:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); ;}
    break;

  case 69:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); ;}
    break;

  case 70:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); ;}
    break;

  case 71:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); ;}
    break;

  case 72:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); ;}
    break;

  case 73:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); ;}
    break;

  case 74:

    { GrowSpotName(pb, &(yyvsp[(2) - (2)])); ;}
    break;

  case 75:

    { StopSpotName(pb); ;}
    break;

  case 76:

    { SetSpotGroup(pb, &(yyvsp[(3) - (3)])); ;}
    break;

  case 77:

    { StopSpotName(pb); ;}
    break;

  case 78:

    { SetSpotGroup(pb, &(yyvsp[(3) - (3)])); ;}
    break;

  case 79:

    { StopSpotName(pb); ;}
    break;

  case 80:

    {   /* in PACBIO fastq, the first '/' and the following digits are treated as a continuation of the spot name, not a read number */
            if (IS_PACBIO(pb)) pb->spotNameDone = false; 
            GrowSpotName(pb, &(yyvsp[(1) - (1)])); 
        ;}
    break;

  case 81:

    { 
            if (!IS_PACBIO(pb)) SetReadNumber(pb, &(yyvsp[(3) - (3)])); 
            GrowSpotName(pb, &(yyvsp[(3) - (3)])); 
            StopSpotName(pb); 
        ;}
    break;

  case 82:

    { 
            if (IS_PACBIO(pb)) pb->spotNameDone = false; 
            GrowSpotName(pb, &(yyvsp[(2) - (2)])); 
        ;}
    break;

  case 83:

    { 
            if (IS_PACBIO(pb)) StopSpotName(pb); 
        ;}
    break;

  case 84:

    { SetReadNumber(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); StopSpotName(pb); ;}
    break;

  case 85:

    { SetReadNumber(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); StopSpotName(pb); ;}
    break;

  case 86:

    { GrowSpotName(pb, &(yyvsp[(3) - (3)])); ;}
    break;

  case 87:

    { GrowSpotName(pb, &(yyvsp[(5) - (5)])); if ((yyvsp[(5) - (5)]).tokenLength == 1 && TokenTextPtr(pb, &(yyvsp[(5) - (5)]))[0] == 'Y') pb->record->seq.lowQuality = true; ;}
    break;

  case 88:

    { GrowSpotName(pb, &(yyvsp[(7) - (7)])); ;}
    break;

  case 89:

    { GrowSpotName(pb, &(yyvsp[(9) - (9)])); ;}
    break;

  case 91:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); FASTQScan_inline_sequence(pb); ;}
    break;

  case 94:

    { SetSpotGroup(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); ;}
    break;

  case 95:

    { SetSpotGroup(pb, &(yyvsp[(1) - (1)])); GrowSpotName(pb, &(yyvsp[(1) - (1)])); ;}
    break;

  case 97:

    { GrowSpotName(pb, &(yyvsp[(1) - (3)])); StopSpotName(pb); SetReadNumber(pb, &(yyvsp[(3) - (3)])); ;}
    break;

  case 98:

    { GrowSpotName(pb, &(yyvsp[(1) - (3)])); StopSpotName(pb); SetReadNumber(pb, &(yyvsp[(3) - (3)])); ;}
    break;

  case 99:

    { GrowSpotName(pb, &(yyvsp[(1) - (1)])); StopSpotName(pb); ;}
    break;

  case 106:

    {  AddQuality(pb, & (yyvsp[(1) - (1)])); ;}
    break;



      default: break;
    }
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
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (pb, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (pb, yymsg);
	  }
	else
	  {
	    yyerror (pb, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
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
      if (yyn != YYPACT_NINF)
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
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, pb);
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

