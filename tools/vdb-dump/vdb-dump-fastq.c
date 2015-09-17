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

#include "vdb-dump-fastq.h"
#include "vdb-dump-helper.h"

#include <stdlib.h>

#include <kdb/manager.h>
#include <vdb/vdb-priv.h>
#include <klib/log.h>
#include <klib/out.h>
#include <klib/num-gen.h>

#include <insdc/sra.h> /* for filter/types */


rc_t CC Quitting ( void );

#define INVALID_COLUMN 0xFFFFFFFF
#define DEF_FASTA_LEN 70

typedef struct fastq_ctx
{
    const char * run_name;
	const VTable * tbl;
    const VCursor * cursor;
	const struct num_gen_iter * row_iter;
    uint32_t idx_read;
    uint32_t idx_qual;
    uint32_t idx_name;
	uint32_t idx_read_start;
	uint32_t idx_read_len;
	uint32_t idx_read_type;
	uint32_t idx_read_filter;
} fastq_ctx;


static char * vdb_fastq_extract_run_name( const char * acc_or_path )
{
    char * delim = string_rchr ( acc_or_path, string_size( acc_or_path ), '/' );
    if ( delim == NULL )
        return string_dup_measure ( acc_or_path, NULL );
    else
        return string_dup_measure ( delim + 1, NULL );    
}


static void init_fastq_ctx( fastq_ctx * fctx, const char * acc_or_path )
{
	fctx->run_name = vdb_fastq_extract_run_name( acc_or_path );
	fctx->tbl      = NULL;	
	fctx->cursor   = NULL;
	fctx->row_iter = NULL;
	fctx->idx_read = INVALID_COLUMN;
	fctx->idx_qual = INVALID_COLUMN;
	fctx->idx_name = INVALID_COLUMN;
	fctx->idx_read_start  = INVALID_COLUMN;
	fctx->idx_read_len    = INVALID_COLUMN;
	fctx->idx_read_type   = INVALID_COLUMN;
	fctx->idx_read_filter = INVALID_COLUMN;
}


static void vdb_fastq_row_error( const char * fmt, rc_t rc, int64_t row_id )
{
    PLOGERR( klogInt, ( klogInt, rc, fmt, "row_nr=%li", row_id ) );
}


static bool is_name_in_list( KNamelist * col_names, const char * to_find )
{
	bool res = false;
	uint32_t count;
	rc_t rc = KNamelistCount( col_names, &count );
	DISP_RC( rc, "KNamelistCount() failed" );
	if ( rc == 0 )
	{
		uint32_t i;
		size_t to_find_len = string_size( to_find );
		for ( i = 0; i < count && rc == 0 && !res; ++i )
		{
			const char * col_name;
			rc = KNamelistGet( col_names, i, &col_name );
			DISP_RC( rc, "KNamelistGet() failed" );
			if ( rc == 0 )
			{
				size_t col_name_len = string_size( col_name );
				if ( col_name_len == to_find_len )
					res = ( string_cmp( to_find, to_find_len, col_name, col_name_len, col_name_len ) == 0 );
			}
		}
	}
	return res;
}


static rc_t prepare_column( fastq_ctx * fctx, KNamelist * col_names, uint32_t * col_idx,
						    const char * to_find, const char * col_spec )
{
	rc_t rc = 0;
	if ( is_name_in_list( col_names, to_find ) )
	{
		rc = VCursorAddColumn( fctx->cursor, col_idx, col_spec );
		if ( rc != 0 )
			PLOGERR( klogInt, ( klogInt, rc, "VCurosrAddColumn( '$(col)' ) failed", "col=%s", col_spec ) );
	}
	return rc;
}


static rc_t vdb_prepare_cursor( const p_dump_context ctx, fastq_ctx * fctx )
{
	KNamelist * col_names;
	rc_t rc = VTableListCol( fctx->tbl, &col_names );
	DISP_RC( rc, "VTableListCol() failed" );
	if ( rc == 0 )
	{
		rc = VTableCreateCachedCursorRead( fctx->tbl, &fctx->cursor, ctx->cur_cache_size );
		DISP_RC( rc, "VTableCreateCursorRead( fasta/fastq ) failed" );
		if ( rc == 0 )
			rc = prepare_column( fctx, col_names, &fctx->idx_read, "READ", "(INSDC:dna:text)READ" );
	
		if ( rc == 0 && ( ctx->format == df_fastq || ctx->format == df_fastq1 ) )
			rc = prepare_column( fctx, col_names, &fctx->idx_qual, "QUALITY", "(INSDC:quality:text:phred_33)QUALITY" );
		
		if ( rc == 0 )
		{
			if ( ctx->format == df_fasta2 )
			{
				rc = prepare_column( fctx, col_names, &fctx->idx_name, "SEQ_ID", "(ascii)SEQ_ID" );
				if ( fctx->idx_name == INVALID_COLUMN )
					rc = prepare_column( fctx, col_names, &fctx->idx_name, "NAME", "(ascii)NAME" );
			}
			else
				rc = prepare_column( fctx, col_names, &fctx->idx_name, "NAME", "(ascii)NAME" );
		}

		if ( rc == 0 && ctx->format == df_fastq1 )
			rc = prepare_column( fctx, col_names, &fctx->idx_read_start, "READ_START", "(INSDC:coord:zero)READ_START" );

		if ( rc == 0 && ctx->format == df_fastq1 )
			rc = prepare_column( fctx, col_names, &fctx->idx_read_len, "READ_LEN", "(INSDC:coord:len)READ_LEN" );

		if ( rc == 0 && ctx->format == df_fastq1 )
			rc = prepare_column( fctx, col_names, &fctx->idx_read_type, "READ_TYPE", "(INSDC:SRA:xread_type)READ_TYPE" );

		if ( rc == 0 && ctx->format == df_fastq1 )
			rc = prepare_column( fctx, col_names, &fctx->idx_read_filter, "READ_FILTER", "(INSDC:SRA:read_filter)READ_FILTER" );

		if ( rc == 0 )
		{
			rc = VCursorOpen ( fctx->cursor );
			DISP_RC( rc, "VCursorOpen( fasta/fastq ) failed" );
		}
		KNamelistRelease( col_names );
	}
    return rc;
}


typedef struct fastq_spot
{
	const char * name;
	const char * bases;
	const char * qual;
	const uint32_t * rd_start;
	const uint32_t * rd_len;
	const uint8_t * rd_type;
	const uint8_t * rd_filter;
	uint32_t name_len;
	uint32_t num_bases;
	uint32_t num_qual;
	uint32_t num_rd_start;
	uint32_t num_rd_len;
	uint32_t num_rd_type;
	uint32_t num_rd_filter;
} fastq_spot;


static rc_t vdb_fastq1_frag( fastq_spot * spot, int64_t row_id, const fastq_ctx * fctx )
{
	rc_t rc = 0;
	if ( spot->num_bases != spot->num_qual )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		PLOGERR( klogInt,
				 ( klogInt, rc, "invalid spot #$(row): bases.len( $(n_bases) ) != qual.len( $(n_qual)",
					"row=%li,n_bases=%d,n_qual=%d", row_id, spot->num_bases, spot->num_qual ) );
	}
	else if ( spot->num_rd_start != spot->num_rd_len ||
			   spot->num_rd_start != spot->num_rd_type ||
			   spot->num_rd_start != spot->num_rd_filter )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		PLOGERR( klogInt,
				 ( klogInt, rc, 
				   "invalid spot #$(row): #READ_START=$(rd_start), #READ_LEN=$(rd_len), #READ_TYPE=$(rd_type), #READ_FILTER=$(rd_filter) ",
				   "row=%li,rd_start=%d,rd_len=%d,rd_type=%d,rd_filter=%d",
				   row_id, spot->num_rd_start, spot->num_rd_len, spot->num_rd_type, spot->num_rd_filter ) );
	}
	else
	{
		uint32_t idx, frag, ofs;
		for ( idx = 0, frag = 1, ofs = 0; rc == 0 && idx < spot->num_rd_start; ++idx )
		{
			if ( spot->rd_type[ idx ] == READ_TYPE_BIOLOGICAL &&
			     spot->rd_filter[ idx ] == READ_FILTER_PASS &&
				 spot->rd_len[ idx ] > 0 )
			{
				rc = KOutMsg( "@%s.%li.%d %.*s length=%u\n%.*s\n+%s.%li.%d %.*s length=%u\n%.*s\n",
							  fctx->run_name, row_id, frag, spot->name_len, spot->name, spot->rd_len[ idx ],
							  spot->rd_len[ idx ], &( spot->bases[ ofs ] ),
							  fctx->run_name, row_id, frag, spot->name_len, spot->name, spot->rd_len[ idx ],
							  spot->rd_len[ idx ], &( spot->qual[ ofs ] )
							  );
				frag++;
			}
			ofs += spot->rd_len[ idx ];
		}
	}
	return rc;
}

static rc_t vdb_fastq1_loop( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc;
	if ( fctx->idx_read == INVALID_COLUMN || fctx->idx_name == INVALID_COLUMN ||
	     fctx->idx_qual == INVALID_COLUMN || fctx->idx_read_start == INVALID_COLUMN ||
		 fctx->idx_read_len == INVALID_COLUMN || fctx->idx_read_type == INVALID_COLUMN ||
		 fctx->idx_read_filter == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta-format, at least one of these columns not found: READ, NAME, QUALITY, READ_START, READ_LEN, READ_TYPE, READ_FILTER" );
	}
	else
	{
		int64_t row_id;
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff;
				fastq_spot spot;
				
				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_name, &elem_bits,
											(const void**)&spot.name, &boff, &spot.name_len );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
				else
				{
					rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
												(const void**)&spot.bases, &boff, &spot.num_bases );
					if ( rc != 0 )
						vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
					else
					{
						rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_qual, &elem_bits,
													(const void**)&spot.qual, &boff, &spot.num_qual );
						if ( rc != 0 )
							vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), QUALITY ) failed", rc, row_id );
						else
						{
							rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read_start, &elem_bits,
														(const void**)&spot.rd_start, &boff, &spot.num_rd_start );
							if ( rc != 0 )
								vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_START ) failed", rc, row_id );
							else
							{
								rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read_len, &elem_bits,
															(const void**)&spot.rd_len, &boff, &spot.num_rd_len );
								if ( rc != 0 )
									vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_LEN ) failed", rc, row_id );
								else
								{
									rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read_type, &elem_bits,
																(const void**)&spot.rd_type, &boff, &spot.num_rd_type );
									if ( rc != 0 )
										vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_TYPE ) failed", rc, row_id );
									else
									{
										rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read_filter, &elem_bits,
																	(const void**)&spot.rd_filter, &boff, &spot.num_rd_filter );
										if ( rc != 0 )
											vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ_FILTER ) failed", rc, row_id );
										else
											rc = vdb_fastq1_frag( &spot, row_id, fctx );
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return rc;
}


static rc_t vdb_fastq_loop_with_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc;
	if ( fctx->idx_read == INVALID_COLUMN || fctx->idx_name == INVALID_COLUMN || fctx->idx_qual == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta-format: READ, NAME and/or QUALITY column not found" );
	}
	else
	{
		int64_t row_id;
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff, row_len, name_len;
				const char * data;
				const char * name;

				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_name, &elem_bits,
											(const void**)&name, &boff, &name_len );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
				else
				{
					rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
												(const void**)&data, &boff, &row_len );
					if ( rc != 0 )
						vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
					else
					{
						rc = KOutMsg( "@%s.%li %.*s length=%u\n%.*s\n",
									  fctx->run_name, row_id, name_len, name, row_len, row_len, data );
						if ( rc == 0 )
						{
							rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_qual, &elem_bits,
														(const void**)&data, &boff, &row_len );
							if ( rc != 0 )
								vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), QUALITY ) failed", rc, row_id );
							else
								rc = KOutMsg( "+%s.%li %.*s length=%u\n%.*s\n",
											  fctx->run_name, row_id, name_len, name, row_len, row_len, data );
						}
					}
				}
			}
		}
	}
    return rc;
}


static rc_t print_bases( const char * bases, uint32_t num_bases, uint32_t max_line_len )
{
	rc_t rc;
	if ( max_line_len == 0 )
		rc = KOutMsg( "%.*s\n", num_bases, bases );
	else
	{
		uint32_t idx = 0, to_print = num_bases;
		rc = 0;
		while ( rc == 0 && idx < num_bases )
		{
			if ( to_print > max_line_len )
				to_print = max_line_len;

			rc = KOutMsg( "%.*s\n", to_print, &bases[ idx ] );
			if ( rc == 0 )
			{
				idx += to_print;
				to_print = ( num_bases - idx );
			}
		}
	}
	return rc;
}


static rc_t vdb_fasta_loop_with_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc;
	if ( fctx->idx_read == INVALID_COLUMN || fctx->idx_name == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta-format: READ and/or NAME column not found" );
	}
	else
	{
		int64_t row_id;
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff, name_len;
				const char * name;

				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_name, &elem_bits,
											(const void**)&name, &boff, &name_len );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
				else
				{
					const char * bases;
					uint32_t num_bases;
					rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
												(const void**)&bases, &boff, &num_bases );
					if ( rc != 0 )
						vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
					else
					{
						rc = KOutMsg( ">%s.%li %.*s length=%u\n", fctx->run_name, row_id, name_len, name, num_bases );
						if ( rc == 0 )
							rc = print_bases( bases, num_bases, ctx->max_line_len );
					}
				}
			}
		}
	}
    return rc;
}


static rc_t vdb_fastq_loop_without_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc;
	if ( fctx->idx_read == INVALID_COLUMN || fctx->idx_qual == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta-format: READ and/or QUALITY column not found" );
	}
	else
	{
		int64_t row_id;
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff, row_len;
				const char * data;

				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
											(const void**)&data, &boff, &row_len );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
				else
				{
					rc = KOutMsg( "@%s.%li %li length=%u\n%.*s\n",
								  fctx->run_name, row_id, row_id, row_len, row_len, data );
					if ( rc == 0 )
					{
						rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_qual, &elem_bits,
													(const void**)&data, &boff, &row_len );
						if ( rc != 0 )
							vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), QUALITY ) failed", rc, row_id );
						else
							rc = KOutMsg( "+%s.%li %li length=%u\n%.*s\n",
										  fctx->run_name, row_id, row_id, row_len, row_len, data );
					}
				}
			}
		}
	}
    return rc;
}


static rc_t vdb_fasta_loop_without_name( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc;
	if ( fctx->idx_read == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta-format: READ column not found" );
	}
	else
	{
		int64_t row_id;
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff, num_bases;
				const char * bases;

				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
											(const void**)&bases, &boff, &num_bases );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
				else
				{
					rc = KOutMsg( ">%s.%li %li length=%u\n", fctx->run_name, row_id, row_id, num_bases );
					if ( rc == 0 )
						rc = print_bases( bases, num_bases, ctx->max_line_len );
				}
			}
		}
	}
    return rc;
}


static rc_t vdb_fasta_accumulated( const char * bases, uint32_t num_bases, 
								   int32_t * chars_left_on_line, uint32_t max_line_len )
{
	rc_t rc = 0;
	if ( num_bases < ( *chars_left_on_line ) )
	{
		rc = KOutMsg( "%.*s", num_bases, bases );
		( *chars_left_on_line ) -= num_bases;
	}
	else if ( num_bases == ( *chars_left_on_line ) )
	{
		rc = KOutMsg( "%.*s\n", num_bases, bases );
		( *chars_left_on_line ) = max_line_len;
	}
	else
	{
		uint32_t ofs = 0;
		int32_t remaining = num_bases;
		while( rc == 0 && ofs < num_bases )
		{
			if ( remaining >= ( *chars_left_on_line ) )
			{
				rc = KOutMsg( "%.*s\n", ( *chars_left_on_line ), &bases[ ofs ] );
				ofs += ( *chars_left_on_line );
				remaining -= ( *chars_left_on_line );
				( *chars_left_on_line ) = max_line_len;
			}
			else
			{
				rc = KOutMsg( "%.*s", remaining, &bases[ ofs ] );
				ofs += remaining;
				( *chars_left_on_line ) -= remaining;
				remaining = 0;
			}
		}
	}
	return rc;
}

static rc_t vdb_fasta1_loop( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc;
	if ( fctx->idx_read == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta1-format: READ column not found" );
	}
	else
	{
		int64_t row_id;
		int32_t chars_left_on_line = ctx->max_line_len;
		
		rc = KOutMsg( ">%s\n", fctx->run_name );
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff, num_bases;
				const char * bases;

				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
											(const void**)&bases, &boff, &num_bases );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
				else
					rc = vdb_fasta_accumulated( bases, num_bases, &chars_left_on_line, ctx->max_line_len );
			}
		}
		rc = KOutMsg( "\n" );
	}
    return rc;
}


static rc_t vdb_fasta2_loop( const p_dump_context ctx, const fastq_ctx * fctx )
{
	rc_t rc = 0;
	if ( fctx->idx_name == INVALID_COLUMN || fctx->idx_read == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcConstructing, rcNoObj, rcInvalid );
		DISP_RC( rc, "cannot generate fasta2-format: READ and/or NAME column not found" );
	}
	else
	{
		char last_name[ 1024 ];
		size_t last_name_len = 0;
		int64_t row_id;
		int32_t chars_left_on_line = ctx->max_line_len;
		
		while ( rc == 0 && num_gen_iterator_next( fctx->row_iter, &row_id, &rc ) )
		{
			if ( rc == 0 )
				rc = Quitting();
			if ( rc == 0 )
			{
				uint32_t elem_bits, boff, name_len;
				const char * name;

				rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_name, &elem_bits,
											(const void**)&name, &boff, &name_len );
				if ( rc != 0 )
					vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), NAME ) failed", rc, row_id );
				else
				{
					bool print_ref_name = ( last_name_len == 0 );
					if ( !print_ref_name )
					{
						print_ref_name = ( last_name_len != name_len );
						if ( !print_ref_name )
							print_ref_name = ( string_cmp( last_name, last_name_len, name, name_len, name_len ) != 0 );
					}
					
					if ( print_ref_name )
					{
						if ( chars_left_on_line == ctx->max_line_len )
							rc = KOutMsg( ">%.*s\n", name_len, name );
						else
						{
							rc = KOutMsg( "\n>%.*s\n", name_len, name );
							chars_left_on_line = ctx->max_line_len;
						}
						last_name_len = string_copy ( last_name, sizeof last_name, name, name_len );
					}
					
					if ( rc == 0 )
					{
						const char * bases;
						uint32_t num_bases;
						rc = VCursorCellDataDirect( fctx->cursor, row_id, fctx->idx_read, &elem_bits,
													(const void**)&bases, &boff, &num_bases );
						if ( rc != 0 )
							vdb_fastq_row_error( "VCursorCellDataDirect( row#$(row_nr), READ ) failed", rc, row_id );
						else
							rc = vdb_fasta_accumulated( bases, num_bases, &chars_left_on_line, ctx->max_line_len );
					}
				}
			}
		}
	}
    return rc;
}


static rc_t vdb_fastq_tbl( const p_dump_context ctx, fastq_ctx * fctx )
{
    rc_t rc = vdb_prepare_cursor( ctx, fctx );
    DISP_RC( rc, "the table lacks READ and/or QUALITY column" );
    if ( rc == 0 )
    {
        int64_t  first;
        uint64_t count;
        rc = VCursorIdRange( fctx->cursor, fctx->idx_read, &first, &count );
        DISP_RC( rc, "VCursorIdRange() failed" );
        if ( rc == 0 )
        {
            if ( count == 0 )
            {
                KOutMsg( "this table is empty\n" );
            }
            else
            {
                /* if the user did not specify a row-range, take all rows */
                if ( ctx->rows == NULL )
                {
                    rc = num_gen_make_from_range( &ctx->rows, first, count );
                    DISP_RC( rc, "num_gen_make_from_range() failed" );
                }
                /* if the user did specify a row-range, check the boundaries */
                else
                {
                    rc = num_gen_trim( ctx->rows, first, count );
                    DISP_RC( rc, "num_gen_trim() failed" );
                }

                if ( rc == 0 && !num_gen_empty( ctx->rows ) )
                {
					rc = num_gen_iterator_make( ctx->rows, &fctx->row_iter );
					DISP_RC( rc, "num_gen_iterator_make() failed" );
					if ( rc == 0 )
					{
						switch( ctx->format )
						{
							case df_fastq : if ( fctx->idx_name == INVALID_COLUMN )
												rc = vdb_fastq_loop_without_name( ctx, fctx ); /* <--- */
											 else
												rc = vdb_fastq_loop_with_name( ctx, fctx ); /* <--- */
											 break;

							case df_fastq1 : rc = vdb_fastq1_loop( ctx, fctx ); /* <--- */
											  break;
											 
							case df_fasta : if ( ctx->max_line_len == 0 )
												ctx->max_line_len = DEF_FASTA_LEN;
											 if ( fctx->idx_name == INVALID_COLUMN )
												rc = vdb_fasta_loop_without_name( ctx, fctx ); /* <--- */
											 else
												rc = vdb_fasta_loop_with_name( ctx, fctx ); /* <--- */
											 break;
							
							case df_fasta1 : if ( ctx->max_line_len == 0 )
												ctx->max_line_len = DEF_FASTA_LEN;
											  rc = vdb_fasta1_loop( ctx, fctx ); /* <--- */
											 break;

							case df_fasta2 : if ( ctx->max_line_len == 0 )
												ctx->max_line_len = DEF_FASTA_LEN;
											  rc = vdb_fasta2_loop( ctx, fctx ); /* <--- */
											 break;

							default : break;
						}
						num_gen_iterator_destroy( fctx->row_iter );
					}
                }
                else
                    rc = RC( rcExe, rcDatabase, rcReading, rcRange, rcEmpty );
            }
        }
        VCursorRelease( fctx->cursor );
    }
    return rc;
}


static rc_t vdb_fastq_table( const p_dump_context ctx,
                             const VDBManager *mgr,
                             fastq_ctx * fctx )
{
     VSchema * schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx->schema_list), true /* ctx->force_sra_schema */ );

    rc = VDBManagerOpenTableRead( mgr, &fctx->tbl, schema, "%s", ctx->path );
    DISP_RC( rc, "VDBManagerOpenTableRead() failed" );
    if ( rc == 0 )
    {
        rc = vdb_fastq_tbl( ctx, fctx );
        VTableRelease( fctx->tbl );
    }
	
	if ( schema != NULL )
		VSchemaRelease( schema );

    return rc;
}


static rc_t vdb_fastq_database( const p_dump_context ctx,
                                const VDBManager *mgr,
                                fastq_ctx * fctx )
{
    const VDatabase * db;
    VSchema *schema = NULL;
    rc_t rc;

    vdh_parse_schema( mgr, &schema, &(ctx->schema_list), true /* ctx->force_sra_schema */ );

    rc = VDBManagerOpenDBRead( mgr, &db, schema, "%s", ctx->path );
    DISP_RC( rc, "VDBManagerOpenDBRead() failed" );
    if ( rc == 0 )
    {
        bool table_defined = ( ctx->table != NULL );
        if ( !table_defined )
            table_defined = vdh_take_this_table_from_db( ctx, db, "SEQUENCE" );

        if ( table_defined )
        {
            rc = VDatabaseOpenTableRead( db, &fctx->tbl, "%s", ctx->table );
            DISP_RC( rc, "VDatabaseOpenTableRead() failed" );
            if ( rc == 0 )
            {
                rc = vdb_fastq_tbl( ctx, fctx );
                VTableRelease( fctx->tbl );
            }
        }
        else
        {
            LOGMSG( klogInfo, "opened as vdb-database, but no table found/defined" );
            ctx->usage_requested = true;
        }
        VDatabaseRelease( db );
    }

	if ( schema != NULL )
		VSchemaRelease( schema );
    return rc;
}


static rc_t vdb_fastq_by_pathtype( const p_dump_context ctx,
                                   const VDBManager *mgr,
                                   fastq_ctx * fctx )                                   
{
    rc_t rc;
    int path_type = ( VDBManagerPathType ( mgr, "%s", ctx->path ) & ~ kptAlias );
    /* types defined in <kdb/manager.h> */
    switch ( path_type )
    {
    case kptDatabase    :  rc = vdb_fastq_database( ctx, mgr, fctx );
                            DISP_RC( rc, "dump_database() failed" );
                            break;

    case kptPrereleaseTbl:
    case kptTable       :  rc = vdb_fastq_table( ctx, mgr, fctx );
                            DISP_RC( rc, "dump_table() failed" );
                            break;

    default             :  rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
                            PLOGERR( klogInt, ( klogInt, rc,
                                "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                                "p=%s", ctx->path ) );
                            if ( vdco_schema_count( ctx ) == 0 )
                            {
                            LOGERR( klogInt, rc, "Maybe it is a legacy table. If so, specify a schema with the -S option" );
                            }
                            break;
    }
    return rc;
}


static rc_t vdb_fastq_by_probing( const p_dump_context ctx,
                                  const VDBManager *mgr,
                                  fastq_ctx * fctx )
{
    rc_t rc;
    if ( vdh_is_path_database( mgr, ctx->path, &(ctx->schema_list) ) )
    {
        rc = vdb_fastq_database( ctx, mgr, fctx );
        DISP_RC( rc, "dump_database() failed" );
    }
    else if ( vdh_is_path_table( mgr, ctx->path, &(ctx->schema_list) ) )
    {
        rc = vdb_fastq_table( ctx, mgr, fctx );
        DISP_RC( rc, "dump_table() failed" );
    }
    else
    {
        rc = RC( rcVDB, rcNoTarg, rcConstructing, rcItem, rcNotFound );
        PLOGERR( klogInt, ( klogInt, rc,
                 "the path '$(p)' cannot be opened as vdb-database or vdb-table",
                 "p=%s", ctx->path ) );
        if ( vdco_schema_count( ctx ) == 0 )
        {
            LOGERR( klogInt, rc, "Maybe it is a legacy table. If so, specify a schema with the -S option" );
        }
    }
    return rc;
}


rc_t vdf_main( const p_dump_context ctx, const VDBManager * mgr, const char * acc_or_path )
{
    rc_t rc = 0;
    fastq_ctx fctx;
	init_fastq_ctx( &fctx, acc_or_path );
    ctx->path = string_dup_measure ( acc_or_path, NULL );
	
    if ( USE_PATHTYPE_TO_DETECT_DB_OR_TAB ) /* in vdb-dump-context.h */
        rc = vdb_fastq_by_pathtype( ctx, mgr, &fctx );
    else
        rc = vdb_fastq_by_probing( ctx, mgr, &fctx );

    free( ( char* )ctx->path );
    free( ( void* )fctx.run_name );
    ctx->path = NULL;

    return rc;
}
