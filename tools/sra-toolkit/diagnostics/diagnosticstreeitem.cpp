#include "diagnosticstreeitem.h"

#include <QStringList>


DiagnosticsTreeItem :: DiagnosticsTreeItem ( const QList < QVariant > &data, DiagnosticsTreeItem *parent )
{
    m_parentItem = parent;
    m_itemData = data;
}

DiagnosticsTreeItem :: DiagnosticsTreeItem ( QString name, QString desc, uint32_t level, DiagnosticsTreeItem *parent )
{
    m_name = name;
    m_desc = desc;
    m_level = level;
    m_parentItem = parent;
}

DiagnosticsTreeItem :: ~DiagnosticsTreeItem()
{
    qDeleteAll ( m_childItems );
}

void DiagnosticsTreeItem :: appendChild ( DiagnosticsTreeItem *child )
{
    m_childItems . append ( child );
}

DiagnosticsTreeItem * DiagnosticsTreeItem :: child ( int row )
{
    return m_childItems . value ( row );
}

DiagnosticsTreeItem * DiagnosticsTreeItem :: parentItem ()
{
    return m_parentItem;
}

int DiagnosticsTreeItem :: childCount () const
{
    return m_childItems . count ();
}

int DiagnosticsTreeItem :: columnCount () const
{
    return m_itemData . count ();
}

QVariant DiagnosticsTreeItem :: data ( int column ) const
{
    return m_itemData . value ( column );
}

int DiagnosticsTreeItem :: row () const
{
    if ( m_parentItem )
        return m_parentItem -> m_childItems . indexOf ( const_cast < DiagnosticsTreeItem * > ( this ) );

    return 0;
}

void DiagnosticsTreeItem :: setState ( DiagnosticsTestState state )
{
    m_state = state;
}

