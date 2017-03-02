/* ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnologmsgy Information
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

#ifndef _h_ref_reader_
#define _h_ref_reader_

#include <klib/rc.h>

struct Refseq_Reader;

/* construct a reference-reader from a refseq-accession */
rc_t refseq_reader_make( struct Refseq_Reader ** rr, const char * acc );

/* releae a reference-reader */
rc_t refseq_reader_release( struct Refseq_Reader * rr );

/* check if the reference-reader was created with this name */
rc_t refseq_reader_has_name( struct Refseq_Reader * rr, const char * acc, bool * has_name );

/* get the total len of the reference */
rc_t refseq_reader_get_len( struct Refseq_Reader * rr, uint64_t * len );

/* read a chunk of bases from the reference */
rc_t refseq_reader_get( struct Refseq_Reader * rr, char ** buffer, uint64_t pos, size_t * available );

#endif
