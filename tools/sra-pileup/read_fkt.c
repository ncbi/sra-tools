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

#include "read_fkt.h"
#include <sysalloc.h>

/* ------------------------------------------------------------------------------------------------------------------- */

rc_t read_bool( int64_t row_id, const VCursor * cursor, uint32_t idx, bool *res, bool dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) bool",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const bool * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) bool failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_bool_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const bool **res, uint32_t *res_len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) bool ptr",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		bool * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( res_len != NULL )
				*res_len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_uint8( int64_t row_id, const VCursor * cursor, uint32_t idx, uint8_t *res, uint8_t dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) uint8",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const uint8_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) char_ptr failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_uint8_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const uint8_t **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) uint8 ptr",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const uint8_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) char_ptr failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_uint32( int64_t row_id, const VCursor * cursor, uint32_t idx, uint32_t *res, uint32_t dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) uint32",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		uint32_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) uint32_t failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_uint32_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const uint32_t **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) uint32 ptr",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		uint32_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) uint32_t (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_int32( int64_t row_id, const VCursor * cursor, uint32_t idx, int32_t *res, int32_t dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) int32",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		int32_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) uint32_t failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_int32_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const int32_t **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) int32 ptr",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		int32_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) uint32_t (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_int64( int64_t row_id, const VCursor * cursor, uint32_t idx, int64_t *res, int64_t dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) int64",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const int64_t *value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) int64 failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_int64_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const int64_t **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) int64 ptr",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		int64_t * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) uint64_t (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_char_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const char **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) char ptr",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const char * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) char_ptr failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_INSDC_coord_zero( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_zero *res, INSDC_coord_zero dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_coord_zero",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		INSDC_coord_zero * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_coord_zero failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_INSDC_coord_zero_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_coord_zero **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_coord_zero (ptr)",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const INSDC_coord_zero * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_coord_zero (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_INSDC_coord_one( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_one *res, INSDC_coord_one dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_coord_one",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		INSDC_coord_one * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_coord_one failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_INSDC_coord_one_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_one **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_coord_one (ptr)",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		INSDC_coord_one * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_coord_one (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}



/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_INSDC_coord_len( int64_t row_id, const VCursor * cursor, uint32_t idx, INSDC_coord_len *res, INSDC_coord_len dflt, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_coord_len", 
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		INSDC_coord_len * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_coord_len failed", 
				"tr=%li,ti=%u,hi=$u", row_id, idx, hint ) );
		}
		else
		{
			*res = ( row_len > 0 ) ? *value : dflt;
		}
	}
    return rc;
}


rc_t read_INSDC_coord_len_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_coord_len **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_coord_len (ptr)", 
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const INSDC_coord_len * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_coord_len (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */


rc_t read_INSDC_read_type_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_read_type **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_read_type (ptr)", 
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const INSDC_read_type * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_read_type (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


rc_t read_INSDC_read_filter_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_read_filter **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_read_filter (ptr)",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const INSDC_read_filter * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_read_filter (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


rc_t read_INSDC_dna_text_ptr( int64_t row_id, const VCursor * cursor, uint32_t idx, const INSDC_dna_text **res, uint32_t *len, const char * hint )
{
	rc_t rc;
	if ( idx == INVALID_COLUMN )
	{
		rc = RC( rcExe, rcNoTarg, rcReading, rcItem, rcInvalid );	
        (void)PLOGERR( klogInt, ( klogInt, rc, "column idx invalid at row#$(tr) . $(hi) ) INSDC_dna_text (ptr)",
            "tr=%li,hi=%s", row_id, hint ) );
	}
	else
	{
		const INSDC_dna_text * value;
		uint32_t elem_bits, boff, row_len;
		rc = VCursorCellDataDirect( cursor, row_id, idx, &elem_bits, (const void**)&value, &boff, &row_len );
		if ( rc != 0 )
		{
			(void)PLOGERR( klogInt, ( klogInt, rc, "VCursorCellDataDirect( row#$(tr) . idx#$(ti) . $(hi) ) INSDC_dna_text (ptr) failed", 
				"tr=%li,ti=%u,hi=%s", row_id, idx, hint ) );
		}
		else
		{
			if ( row_len > 0 )
				*res = value;
			if ( len != NULL )
				*len = row_len;
		}
	}
    return rc;
}


/* ------------------------------------------------------------------------------------------------------------------- */

bool namelist_contains( const KNamelist *names, const char * a_name )
{
    bool res = false;
    uint32_t count;
    rc_t rc = KNamelistCount( names, &count );
    if ( rc == 0 && count > 0 )
    {
        uint32_t idx;
        size_t a_name_len = string_size( a_name );
        for ( idx = 0; idx < count && rc == 0 && !res; ++idx )
        {
            const char * s;
            rc = KNamelistGet( names, idx, &s );
            if ( rc == 0 && s != NULL )
            {
                size_t s_len = string_size( s );
                size_t max_len = a_name_len > s_len ? a_name_len : s_len;
                int cmp = string_cmp( a_name, a_name_len, s, s_len, max_len );
                if ( cmp == 0 )
                    res = true;
            }
        }
    }
    return res;
}


rc_t add_column( const VCursor * cursor, const char *colname, uint32_t * idx )
{
    rc_t rc = VCursorAddColumn( cursor, idx, "%s", colname );
    if ( rc != 0 )
    {
        (void)PLOGERR( klogInt, ( klogInt, rc, "VCursorAddColumn( $(cn) ) failed", "cn=%s", colname ) );
    }
    return rc;
}


static const char * col_name_without_type( const char * colname )
{
	const char * res = colname;
	const char * s = string_chr( colname, string_size( colname ), ')' );
	if ( s != NULL )
		res = ++s;
	return res;
}

void add_opt_column( const VCursor * cursor, const KNamelist *names, const char *colname, uint32_t * idx )
{
    bool available = namelist_contains( names, col_name_without_type( colname ) );
    if ( available )
    {
        rc_t rc = VCursorAddColumn( cursor, idx, "%s", colname );
        if ( rc != 0 )
            *idx = COL_NOT_AVAILABLE;
    }
    else
    {
        *idx = COL_NOT_AVAILABLE;
    }
}
