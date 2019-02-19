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

#include "diagnosticstest.h"

DiagnosticsTest :: DiagnosticsTest ( QString p_name )
    : childItems ( QList < DiagnosticsTest * > () )
    , level ( 0 )
    , name ( p_name )
    , parent ( 0 )
    , state ( 0 )
{
}

DiagnosticsTest :: ~DiagnosticsTest()
{
    qDeleteAll ( childItems );
}

void DiagnosticsTest :: appendChild (DiagnosticsTest *p_child )
{
    childItems . append ( p_child );
}

int DiagnosticsTest :: childCount () const
{
    return childItems . count ();
}

DiagnosticsTest * DiagnosticsTest :: getChild (int p_row )
{
    return childItems . value ( p_row );
}

QString DiagnosticsTest :: getDescription ()
{
    return desc;
}

uint32_t DiagnosticsTest :: getLevel ()
{
    return level;
}

QString DiagnosticsTest :: getName ()
{
    return name;
}

DiagnosticsTest * DiagnosticsTest :: getParent ()
{
    return parent;
}

uint32_t DiagnosticsTest :: getState ()
{
    return state;
}

int DiagnosticsTest ::  row () const
{
    if ( parent )
        return parent -> childItems . indexOf ( const_cast < DiagnosticsTest * > ( this ) );

    return 0;
}

void DiagnosticsTest :: setDescription ( QString p_desc )
{
    desc = p_desc;
}

void DiagnosticsTest :: setLevel ( uint32_t p_level )
{
    level = p_level;
}

void DiagnosticsTest :: setParent (DiagnosticsTest *p_parent )
{
    parent = p_parent;
}

void DiagnosticsTest :: setState ( uint32_t p_state )
{
    state = p_state;
}


