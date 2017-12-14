#ifndef DIAGNOSTICSTREEITEM_H
#define DIAGNOSTICSTREEITEM_H

#include <QList>
#include <QVariant>


class DiagnosticsTreeItem
{
public:
    explicit DiagnosticsTreeItem ( const QList < QVariant > &data, DiagnosticsTreeItem *parentItem = 0 );
    ~DiagnosticsTreeItem();

    void appendChild ( DiagnosticsTreeItem *child );

    DiagnosticsTreeItem *child ( int row );
    DiagnosticsTreeItem *parentItem ();

    int childCount () const;
    int columnCount () const;
    QVariant data ( int column ) const;
    int row () const;

private:
    QList < DiagnosticsTreeItem * > m_childItems;
    QList < QVariant > m_itemData;
    DiagnosticsTreeItem *m_parentItem;
};

#endif // DIAGNOSTICSTREEITEM_H
