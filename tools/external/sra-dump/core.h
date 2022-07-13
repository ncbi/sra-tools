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
#ifndef _h_tools_dump_core
#define _h_tools_dump_core

#include <klib/rc.h>

#include "factory.h"

typedef struct SRADumperFmt_Arg_struct {
    const char* abbr; /* NULL here means end of list */
    /* next 3 can be NULL */
    const char* full;
    const char* param;
    const char* descr[10];
} SRADumperFmt_Arg;

typedef struct SRADumperFmt SRADumperFmt;

/**
  * Setup formatter interfaces
  */
rc_t SRADumper_Init(SRADumperFmt* fmt);

typedef bool CC GetArg(const SRADumperFmt* fmt, char const* const abbr, char const* const full,
                       int* i, int argc, char *argv[], const char** value);

struct SRADumperFmt
{
    /* optional pointer to formatter arguments, NULL terminated array otherwise */
    const SRADumperFmt_Arg* arg_desc;

    /* optional - prints custom help page */
    rc_t (*usage)(const SRADumperFmt* fmt, const SRADumperFmt_Arg* core_args, int first );
    /* optional */
    rc_t (*release)(const SRADumperFmt* fmt);
    /* optional process current arg and advance i by number of processed args */
    bool (*add_arg)(const SRADumperFmt* fmt, GetArg* f, int* i, int argc, char *argv[]);

    /* mandatory return head of factories implemented in module, factories released by caller! */
    rc_t (*get_factory)(const SRADumperFmt* fmt, const SRASplitterFactory** factory);

    /* set by parent code, do not change!!! */
    const char* accession;
    const SRATable* table;
    bool gzip;
    bool bzip2;
    bool split_files; /* tell the core that the implementation splits into files... */
};

#endif /* _h_tools_dump_core */
