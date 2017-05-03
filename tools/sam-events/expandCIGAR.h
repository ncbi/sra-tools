/*==============================================================================
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
 */

#ifndef _h_expand_CIGAR_
#define _h_expand_CIGAR_

enum EventType {
    match = 1,
    mismatch,
    insertion,
    deletion
};

struct Event {
    int type; /* of EventType */
    unsigned length; /* length (number of bases) of the event */
    unsigned refPos; /* position in reference relative to start of alignment */
    unsigned seqPos; /* position in query sequence */
};

struct cFastaFile;

/* Note: Any string length can be 0 if the string is nul-terminated */

/* load - open and parse a FASTA file
 *   NB: this is NCBI's FASTA format; see https://blast.ncbi.nlm.nih.gov/Blast.cgi?CMD=Web&PAGE_TYPE=BlastDocs&DOC_TYPE=BlastHelp
 */
struct cFastaFile* loadFastaFile( unsigned length, char const path[ /* length */ ] );
struct cFastaFile* loadcSRA( char const * accession, size_t cache_capacity );

/* unload - free the resources associated with the file
 * this call invalides all references to the file object
 */
void unloadFastaFile(struct cFastaFile *file);

/* find the named sequence in the file
 * returns the index of the sequence or -1 if not found
 */
int FastaFile_getNamedSequence( struct cFastaFile *file, unsigned length, char const name[/* length */] );

/* get the reference sequence
 * returns the length
 */
unsigned FastaFile_getSequenceData(struct cFastaFile *file, int referenceNumber, char const **sequence);

/* validate that a CIGAR string is parseable
 * returns 0 if good
 * if refLength is not null, then *refLength will be the reference length computed from the CIGAR string
 * if seqLength is not null, then *seqLength will be the query sequence length computed from the CIGAR string
 */
int validateCIGAR(unsigned length, char const CIGAR[/* length */], unsigned *refLength, unsigned *seqLength);

/* expand the CIGAR string into an array of events
 * returns the number of events computed
 * returns -1 if there was an error
 * result is a pre-allocated array
 */
int expandCIGAR(  struct Event * const result       /* the event-vector to be filled out */
                , int result_len                    /* how many events we have in the result-vector */
                , int result_offset                 /* at which offset do we want the result */
                , int * remaining                   /* if the result-vector was not big enough */
                , unsigned cigar_len
                , char const * const CIGAR          /* the cigar of the alignment */
                , char const * const sequence       /* the sequence-bases of the alignment */
                , unsigned const position           /* the position of the alignment ( 0-based ) relative to the reference */
                , struct cFastaFile * const file    /* the reference-object */    
                , int referenceNumber );            /* the index into the reference-object */

#endif
