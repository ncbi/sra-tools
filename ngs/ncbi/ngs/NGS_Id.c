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


#include "NGS_Id.h"

#include "NGS_String.h"

#include <kfc/except.h>
#include <kfc/xc.h>

#include <klib/printf.h>
#include <klib/text.h>

#include <strtol.h> /* for strtoi64 */
#include <string.h>

NGS_String * NGS_IdMake ( ctx_t ctx, const NGS_String * run, enum NGS_Object object, int64_t rowId )
{
    size_t num_writ;
    char buf[265];
    const char* obj;
    rc_t rc;
    
    switch ( object )
    {
    case NGSObject_Read:                obj = "R"; break;
    case NGSObject_PrimaryAlignment:    obj = "PA"; break;
    case NGSObject_SecondaryAlignment:  obj = "SA"; break;
    case NGSObject_ReadFragment:            
    case NGSObject_AlignmentFragment:            
        INTERNAL_ERROR ( xcParamUnexpected, "wrong object type NGSObject_Fragment", object);
        return NULL;
    default:
        INTERNAL_ERROR ( xcParamUnexpected, "unrecognized object type %i", object);
        return NULL;
    }
    
    rc = string_printf ( buf, 
                         sizeof ( buf ), 
                         & num_writ, 
                         "%.*s.%s.%li", 
                         NGS_StringSize ( run, ctx ), 
                         NGS_StringData ( run, ctx ), 
                         obj, 
                         rowId );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "string_printf rc = %R", rc );
        return NULL;
    }
    
    return NGS_StringMakeCopy ( ctx, buf, num_writ );
}

struct NGS_String * NGS_IdMakeFragment ( ctx_t ctx, const NGS_String * run, bool alignment, int64_t rowId, uint32_t frag_num )
{
    size_t num_writ;
    char buf[265];
    rc_t rc = string_printf ( buf, 
                              sizeof ( buf ), 
                              & num_writ, 
                              "%.*s.%s%i.%li", 
                              NGS_StringSize ( run, ctx ), 
                              NGS_StringData ( run, ctx ), 
                              alignment ? "FA" : "FR", 
                              frag_num, 
                              rowId );
    if ( rc != 0 )
    {
        INTERNAL_ERROR ( xcUnexpected, "string_printf rc = %R", rc );
        return NULL;
    }
    return NGS_StringMakeCopy ( ctx, buf, num_writ );
}

struct NGS_Id NGS_IdParse ( char const * self, size_t self_size, ctx_t ctx )
{
    struct NGS_Id ret;
    
    /* parse from the back using '.' as delimiters */
    const char* start = self;
    const char* dot = string_rchr ( start, self_size, '.' );
    
    memset ( & ret, 0, sizeof ( ret ) );
    
    if ( dot == NULL || dot == start )
    {
        INTERNAL_ERROR ( xcParamUnexpected, "Badly formed ID string: %.*s", self_size, self );
        return ret;
    }    

    /* rowid*/    
    ret . rowId = strtoi64 ( dot + 1, NULL, 10 );
    if ( ret . rowId == 0 )
    {
        INTERNAL_ERROR ( xcParamUnexpected, "Badly formed ID string (rowId): %.*s", self_size, self );
        return ret;
    }    
    
    /* object type and fragment number */
    dot = string_rchr ( start, dot - start - 1, '.' );
    if ( dot == NULL || dot == start )
    {
        INTERNAL_ERROR ( xcParamUnexpected, "Badly formed ID string (object type ?): %.*s", self_size, self );
        return ret;
    }    
    switch ( dot [ 1 ] )
    {
    case 'R': 
        ret . object =  NGSObject_Read;
        break;
        
    case 'P': 
        if ( dot [ 2 ] == 'A' )
            ret . object = NGSObject_PrimaryAlignment;
        else
        {
            INTERNAL_ERROR ( xcParamUnexpected, "Badly formed ID string (object type P?): %.*s", self_size, self );
            return ret;
        }
        break;
        
    case 'S': 
        if ( dot [ 2 ] == 'A' )
            ret . object = NGSObject_SecondaryAlignment;
        else
        {
            INTERNAL_ERROR ( xcParamUnexpected, "Badly formed ID string (object type S?): %.*s", self_size, self );
            return ret;
        }
        break;
        
    case 'F': 
        switch ( dot [ 2 ] )
        {
        case 'R' : 
            ret . object = NGSObject_ReadFragment;
            break;
        case 'A' : 
            ret . object = NGSObject_AlignmentFragment;
            break;
        default  : 
            INTERNAL_ERROR ( xcParamUnexpected, "Badly formed ID string (object type F?): %.*s", self_size, self );
            return ret;
        }
        ret . fragId = strtoul( dot + 3, NULL, 10 ); /* even if missing/invalid, set to 0 */
        break;
    }    
    
    /* run */
    StringInit ( & ret . run, start, dot - start, ( uint32_t )  ( dot - start ) );
    
    return ret;
}

