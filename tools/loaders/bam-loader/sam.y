/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

/* %{
   Prologue
   %}
   Declarations
   %%
   Grammar rules
   %%
   Epilogue
   */

%{
    #include <stdio.h>
    #include <ctype.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h>
    #include <regex.h>
    #include <stdint.h>
    #include "sam.tab.h"

    #define YYDEBUG 1

/*    int yylex(void); */
    int SAMlex(void);
/*    void yyerror(const char * s) { fprintf(stderr,"%s\n", s); } */
    void SAMerror(const char * s) { fprintf(stderr,"ERR: %s\n", s); }

    /* Pair of TAG, regexp: "/..." TODO: or integer range "1-12345" */
    const char * validations[] =
    {
        "VN", "/.*", // @PG also has "/[0-9]+\\.[0-9]+",
        "SO", "/unknown|unsorted|queryname|coordinate",
        "GO", "/none|query|reference",
        "SN", "/[!-)+-<>-~][!-~]*",
        "LN", "/[0]*[1-9][0-9]{0,10}", // TODO: range check 1..2**31-1
        "AS", "/.*",
        "M5", "/[0-9A-Z\\*]{32}",
        "SP", "/.*",
        "UR", "/.*:/.*",
        "ID", "/.*",
        "CN", "/.*",
        "DS", "/.*",
        "DT", "/.*",
        "FO", "/\\*|[ACMGRSVTWYHKDBN]+",
        "KS", "/.*",
        "LB", "/.*",
        "PG", "/.*",
        "PI", "/.*",
        "PL", "/.*",
        "PM", "/.*",
        "PU", "/.*",
        "SM", "/.*",
        "PN", "/.*",
        "CL", "/.*",
        "PP", "/.*",
        "DS", "/.*",
        "\0", "\0"
    };

    // Returns 1 if match found
    int regexcheck(const char *regex, const char * value)
    {
        regex_t preg;

//        fprintf(stderr,"Compiling %s\n", regex);
        int result=regcomp(&preg, regex, REG_EXTENDED);
        if (result)
        {
            size_t s=regerror(result, &preg, NULL, 0);
            char *errmsg=malloc(s);
            regerror(result, &preg, errmsg, s);
            fprintf(stderr,"regcomp error on '%s': %s\n", regex, errmsg);
            free(errmsg);
            regfree(&preg);
            return 0;
        }

        regmatch_t matches[1];
        if (regexec(&preg, value, 1, matches, 0))
        {
            fprintf(stderr,"Value: '%s' doesn't match regex '%s'\n", value, regex);
            regfree(&preg);
            return 0;
        }
        //fprintf(stderr,"match\n");
        regfree(&preg);
        return 1;
    }

    // Returns 1 if OK
    int validate(const char * tag, const char * value)
    {
        int ok=0;

        for (size_t i=0;;++i)
        {
            const char *valtag=validations[i*2];
            const char *valval=validations[i*2+1];
            if (*valtag=='\0')
            {
                fprintf(stderr,"No validation for tag %s\n", tag);
                ok=1;
                break;
            }
            if (!strcmp(tag, valtag))
            {
//                fprintf(stderr,"Checking %s\n", valtag);
                if (valval[0]=='/')
                {
                    ok=regexcheck(valval+1, value);
                    break;
                } else
                {
                // Parse integer range
                    fprintf(stderr,"range not implemented\n");
                    ok=1;
                }
            }
        }

        return ok;
    }

    size_t alignfields=2; // 1 based, QNAME is #1
    // TODO, make these dynamic
    char *tags=NULL; // Space delimited tags seen in current line
    char *seqnames=NULL;
    char *ids=NULL;

    void check_required_tag(const char * tag)
    {
        if (!strstr(tags,tag))
        {
            fprintf(stderr,"%s tag not seen in header\n", tag);
        }
    }

%}

/* Declarations */
%union {
 int intval;
 char * strval;
 double floatval;
}

%token <strval> HEADER
%token <strval> SEQUENCE
%token <strval> READGROUP
%token <strval> PROGRAM
%token <strval> COMMENT
%token <strval> TAG
%token <strval> VALUE
%token <strval> ALIGNVALUE
/* %token DIGITS */
%token <strval> QNAME
/*
%token RNAME
%token CIGAR
%token RNEXT
%token SEQ
%token QUAL
%token TTV
*/
%token COLON
%token TAB
%token CONTROLCHAR
%token EOL
%token END 0 "end of file"
%error-verbose
%name-prefix="SAM"
%require "2.5"


%%
 /* Grammar rules */
sam: /* beginning of input */
   /* empty %empty */
   | sam line
   ;

line:
   EOL /* Spec is unclear about empty lines, accept for now */
   | CONTROLCHAR { fprintf(stderr,"error: CONTROLCHAR\n"); }
   | comment { fprintf(stderr,"comment\n"); }
   | header EOL { fprintf(stderr,"header\n\n"); }
   | sequence {fprintf(stderr,"sequence\n\n"); }
   | program {fprintf(stderr,"program\n\n"); }
   | readgroup {fprintf(stderr,"readgroup\n\n"); }
   | alignment { fprintf(stderr,"alignment\n\n"); }

comment:
    COMMENT { }

header:
    HEADER tagvaluelist
    {
        fprintf(stderr,"header tagvaluelist %s\n", SAMlval.strval);
        check_required_tag("VN");
        if (!strcmp(tags,"SO ") && !strcmp(tags,"GO "))
           fprintf(stderr,"warn: Both SO and GO tags present\n");
        if (!(strcmp(tags,"SO ") || strcmp(tags,"GO ")))
           fprintf(stderr,"warn: neither SO or GO tags present\n");
        *tags='\0';
    }

sequence:
    SEQUENCE tagvaluelist
    {
        fprintf(stderr, "sequence\n");
        fprintf(stderr," sequences were: %s\n", seqnames);
        check_required_tag("SN");
        check_required_tag("LN");
        *tags='\0';
        }

program:
     PROGRAM tagvaluelist
     {
        fprintf(stderr,"ids were: %s\n", ids);
        fprintf(stderr, "program\n");
        check_required_tag("ID");
        *tags='\0';
     }


readgroup:
     READGROUP tagvaluelist
     {
        fprintf(stderr, "readgroup\n");
        fprintf(stderr,"ids were: %s\n", ids);
        check_required_tag("ID");
        *tags='\0';
     }

tagvaluelist: tagvalue { fprintf(stderr, " one tagvaluelist:%s\n", SAMlval.strval); }
  | tagvaluelist tagvalue { fprintf(stderr, " many tagvaluelist:%s\n", SAMlval.strval); }
  ;

tagvalue: TAB TAG COLON VALUE {
        fprintf(stderr,"tagvalue:%s=%s\n", $2, $4);
        const char * tag=$2;
        const char * value=$4;

        if (strlen(tag)!=2)
        {
            fprintf(stderr,"tag '%s' must be 2 characters\n", tag);
        }

        if (islower(tag[0] && islower(tag[1])))
        {
            fprintf(stderr,"optional tag\n");
        } else
        {
            validate(tag, value);
            strcat(tags,tag); strcat(tags," ");
            if (!strcmp(tag,"SN"))
            {
                if (strstr(seqnames,value))
                {
                    fprintf(stderr,"error: duplicate sequence %s\n", value);
                }
                strcat(seqnames,value); strcat(seqnames," ");
            }
            if (!strcmp(tag,"ID"))
            {
                if (strstr(ids,value))
                {
                    fprintf(stderr,"error: duplicate id %s\n", value);
                }
                strcat(ids,value); strcat(ids, " ");
            }
        }
        };
  | TAB TAB TAG COLON VALUE { fprintf(stderr,"two tabs\n"); }
  | TAB TAB EOL { fprintf(stderr,"empty tags\n"); }
  | TAB EOL { fprintf(stderr,"empty tags\n"); }

  ;

alignment:
    QNAME avlist
    {
        fprintf(stderr," avlist qname:%s fields=%zu\n", $1, alignfields);
        alignfields=2;
    }

avlist:
      av { fprintf(stderr," one av\n"); }
 |    avlist av { fprintf(stderr,":bison: many avlist\n"); }

av:
    TAB ALIGNVALUE
    {
        const char * value=$2;
        const char * opt="(required)";
        if (alignfields>=12) opt="(optional)";
        fprintf(stderr,"alignvalue #%zu%s: %s\n", alignfields, opt, value);
        switch (alignfields)
        {
            case 2: // FLAG
            {
                int flag;
                if (sscanf(value, "%d", &flag)!=1 || flag < 0 || flag > UINT16_MAX)
                {
                    fprintf(stderr,"error parsing FLAG: %s\n", value);
                }
                fprintf(stderr,"flag is %d\n",flag);
                break;
            }
            case 4: // POS
            {
                int pos;
                if (sscanf(value, "%d", &pos)!=1 || pos <= 0 || pos > INT32_MAX)
                {
                    fprintf(stderr,"error parsing POS: %s\n", value);
                }
                fprintf(stderr,"pos is %d\n",pos);
                break;
            }
            case 5: // MAPQ
            {
                int mapq;
                if (sscanf(value, "%d", &mapq)!=1 || mapq < 0 || mapq > UINT8_MAX)
                {
                    fprintf(stderr,"error parsing MAPQ: %s\n", value);
                }
                fprintf(stderr,"mapq is %d\n", mapq);
                break;
            }
            case 8: // PNEXT
            {
                int pnext;
                if (sscanf(value, "%d", &pnext)!=1 || pnext < 0 || pnext > INT32_MAX)
                {
                    fprintf(stderr,"error parsing PNEXT: %s\n", value);
                }
                fprintf(stderr,"pnext is %d\n",pnext);
                break;
            }
            case 9: // TLEN
            {
                int tlen;
                if (sscanf(value, "%d", &tlen)!=1 || tlen < INT32_MIN || tlen > INT32_MAX)
                {
                    fprintf(stderr,"error parsing TLEN: %s\n", value);
                }
                fprintf(stderr,"tlen is %d\n", tlen);
                break;
            }


        }
        ++alignfields;
    }

%%


 /* Epilogue */

int main(int argc, char * argv[])
{
    tags=calloc(255,1); // TODO, make dynamic
    seqnames=calloc(10000,1); // TODO, ""
    ids=calloc(10000,1); // TODO, ""

    return SAMparse();
}

