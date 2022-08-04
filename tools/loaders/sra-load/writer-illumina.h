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
#ifndef _sra_load_writer_illumina_
#define _sra_load_writer_illumina_

#include "loader-fmt.h"
#include "pstring.h"

#define ILLUMINAWRITER_MAX_NUM_READS 5
/* these readids have special meaning,
 * from file we can get only single digit positive readid so they never overlap
 * readid == -1 from file means no read id was found, must be whole spot data or single read submission
 */
#define ILLUMINAWRITER_READID_NONE -1

#define ILLUMINAWRITER_COLMASK_NOTSET 0x00
#define ILLUMINAWRITER_COLMASK_READ 0x01
#define ILLUMINAWRITER_COLMASK_QUALITY_PHRED 0x02
#define ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS1 0x04
#define ILLUMINAWRITER_COLMASK_QUALITY_LOGODDS4 0x08
#define ILLUMINAWRITER_COLMASK_SIGNAL 0x10
#define ILLUMINAWRITER_COLMASK_NOISE 0x20
#define ILLUMINAWRITER_COLMASK_INTENSITY 0x40
#define ILLUMINAWRITER_COLMASK_RDFILTER 0x80


typedef struct IlluminaRead_struct {
    int16_t read_id;
    pstring seq;
    int qual_type;
    pstring qual;
    pstring noise;
    pstring intensity;
    pstring signal;
    SRAReadFilter filter;
} IlluminaRead;

void IlluminaRead_Init(IlluminaRead* read, bool name_only);

typedef struct IlluminaSpot_struct {
    const pstring* name;
    const pstring* barcode;
    int32_t nreads;
    int qual_type;
    struct {
        int16_t read_id;
        const pstring* seq;
        const pstring* qual;
        const pstring* noise;
        const pstring* intensity;
        const pstring* signal;
        const SRAReadFilter* filter;
    } reads[ILLUMINAWRITER_MAX_NUM_READS];
} IlluminaSpot;

void IlluminaSpot_Init(IlluminaSpot* spot);

rc_t IlluminaSpot_Add(IlluminaSpot* spot, const pstring* name, const pstring* barcode, const IlluminaRead* read);


typedef struct SRAWriterIllumina SRAWriterIllumina;

rc_t SRAWriterIllumina_Make(const SRAWriterIllumina** cself, const SRALoaderConfig* config);

void SRAWriterIllumina_Whack(const SRAWriterIllumina* cself, SRATable** table);

rc_t SRAWriterIllumina_Write(const SRAWriterIllumina* cself, const SRALoaderFile* data_block_ref, IlluminaSpot* spot);

#endif /* _sra_load_writer_illumina_ */
