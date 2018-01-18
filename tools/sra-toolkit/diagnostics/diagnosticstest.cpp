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


