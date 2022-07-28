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
#ifndef _sra_load_writer_absolid_
#define _sra_load_writer_absolid_

#include "loader-fmt.h"
#include "pstring.h"

#define ABSOLID_FMT_MAX_NUM_READS 2

#define ABSOLID_FMT_COLMASK_NOTSET 0x00
#define ABSOLID_FMT_COLMASK_READ 0x01
#define ABSOLID_FMT_COLMASK_QUALITY 0x02
#define ABSOLID_FMT_COLMASK_FTC 0x04
#define ABSOLID_FMT_COLMASK_FAM 0x08
#define ABSOLID_FMT_COLMASK_CY3 0x10
#define ABSOLID_FMT_COLMASK_TXR 0x20
#define ABSOLID_FMT_COLMASK_CY5 0x40

typedef enum EAbisolidReadType_enum {
    eAbisolidReadType_Unknown = 0,
    eAbisolidReadType_SPOT = 1,
    eAbisolidReadType_F3 = 2,
    eAbisolidReadType_R3 = 3,
    eAbisolidReadType_F5_P2 = 4,
    eAbisolidReadType_F5_BC = 5,
    eAbisolidReadType_F5_RNA = 6,
    eAbisolidReadType_F5_DNA = 7,
    eAbisolidReadType_F3_DNA = 8,
    eAbisolidReadType_BC = 9
} EAbisolidReadType;

extern const int AbisolidReadType2ReadNumber [];
extern const char *AbisolidReadType2ReadLabel [];

typedef enum EAbisolidFSignalType_enum {
    eAbisolidFSignalType_NotSet = 0,
    eAbisolidFSignalType_FAM = 1,
    eAbisolidFSignalType_FTC = 2
} EAbisolidFSignalType;

typedef struct AbsolidRead_struct {
    pstring label;
    char cs_key;
    pstring seq;
    pstring qual;
    EAbisolidFSignalType fs_type;
    pstring fxx;
    pstring cy3;
    pstring txr;
    pstring cy5;
    SRAReadFilter filter;
} AbsolidRead;

void AbsolidRead_Init(AbsolidRead* read);

EAbisolidReadType AbsolidRead_Suffix2ReadType(const char* s);

typedef struct SRAWriteAbsolid SRAWriteAbsolid;

rc_t SRAWriteAbsolid_Make(const SRAWriteAbsolid** cself, const SRALoaderConfig* config);

rc_t SRAWriteAbsolid_MakeName(const pstring* prefix, const pstring* suffix, pstring* name);

void SRAWriteAbsolid_Whack(const SRAWriteAbsolid* cself, SRATable** table);

rc_t SRAWriteAbsolid_Write(const SRAWriteAbsolid* cself, const SRALoaderFile* file, 
                           const pstring* spot_name, const pstring* spot_group,
                           const AbsolidRead* f3, const AbsolidRead* r3);

#endif /* _sra_load_writer_absolid_ */
