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
#ifndef _sra_load_writer_ion_torrent_
#define _sra_load_writer_ion_torrent_

#include "loader-fmt.h"
#include "pstring.h"

typedef struct SRAWriterIonTorrent SRAWriterIonTorrent;

rc_t SRAWriterIonTorrent_Make(const SRAWriterIonTorrent** cself, const SRALoaderConfig* config);

void SRAWriterIonTorrent_Whack(const SRAWriterIonTorrent* cself, SRATable** table);

rc_t SRAWriterIonTorrent_WriteHead(const SRAWriterIonTorrent* cself, const pstring* flow_chars, const pstring* key_sequence);

rc_t SRAWriterIonTorrent_WriteRead(const SRAWriterIonTorrent* cself, const SRALoaderFile* data_block_ref,
                            const pstring* name, const pstring* sequence, const pstring* quality,
                            const pstring* signal, const pstring* position,
                            INSDC_coord_one q_left, INSDC_coord_one q_right,
                            INSDC_coord_one a_left, INSDC_coord_one a_right);

#endif /* _sra_load_writer_ion_torrent_ */
