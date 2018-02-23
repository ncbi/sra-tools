/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

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
