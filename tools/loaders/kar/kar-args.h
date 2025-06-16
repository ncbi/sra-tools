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
#ifndef _h_kar_args_
#define _h_kar_args_

#ifndef _h_kfs_defs_
#include <kfs/defs.h>
#endif



#define OPTION_CREATE    "create"
#define OPTION_EXTRACT   "extract"
#define OPTION_TEST      "test"
#define OPTION_FORCE     "force"
#define OPTION_LONGLIST  "long-list"
#define OPTION_DIRECTORY "directory"
#define OPTION_STDOUT    "stdout"
#define OPTION_MD5       "md5"
/*TBD - add alignment option */


#define ALIAS_CREATE     "c"
#define ALIAS_EXTRACT    "x"
#define ALIAS_TEST       "t"
#define ALIAS_FORCE      "f"
#define ALIAS_LONGLIST   "l"
#define ALIAS_DIRECTORY  "d"
#define ALIAS_STDOUT     "Z"


struct Args;

typedef struct Params Params;
struct Params
{
    /* standalone files & directories that can be added to archive under create mode */
    const char ** members;
    
    /* path to archive being built or accessed */
    const char *archive_path;

    /* path to directory either for creating or extracting an archive */
    const char *directory_path;

    /* the number of members given for creating an archive */
    uint32_t mem_count;

    /* the number of times the directory option was specified */
    uint32_t dir_count;

    /* temporary information used for param validation and mode determination */
    uint32_t c_count;
    uint32_t x_count;
    uint32_t t_count;

    /* modifier to test mode for creating a long listing */
    bool long_list;

    /* modifier to create mode for overwriting an existing archive,
       or to extract mode to overwrite an existing output directory */
    bool force;

    /* output to stdout */
    bool stdout;
    
    /*modifier to create mode to create an md5sum compatible auxilary file*/
    bool md5sum;
};


rc_t parse_params ( Params *p, struct Args **args, int argc, char * argv [] );
rc_t validate_params ( Params *p );

#endif /*_h_kar_args_*/
