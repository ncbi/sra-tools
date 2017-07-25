#include "diagnosticstreeitem.h"
#include "diagnosticstreemodel.h"

#include <QStringList>

DiagnosticsTreeModel :: DiagnosticsTreeModel ( const QString &data, QObject *parent )
    : QAbstractItemModel ( parent )
{
    QList < QVariant > rootData;
    rootData << "Test" << "Description" << "Result";
    rootItem = new DiagnosticsTreeItem ( rootData );
    setupModelData( data . split ( QString ( "\n" ) ), rootItem );
}

DiagnosticsTreeModel :: ~ DiagnosticsTreeModel ()
{
    delete rootItem;
}

QVariant DiagnosticsTreeModel :: data ( const QModelIndex &index, int role ) const
{
    if ( ! index . isValid () )
        return QVariant ();

    if ( role != Qt::DisplayRole )
        return QVariant ();

    DiagnosticsTreeItem *item = static_cast < DiagnosticsTreeItem * > ( index . internalPointer () );

    return item -> data ( index.column () );
}

Qt::ItemFlags DiagnosticsTreeModel :: flags ( const QModelIndex &index ) const
{
    if ( ! index . isValid () )
        return 0;

    return QAbstractItemModel :: flags ( index );
}

QVariant DiagnosticsTreeModel :: headerData ( int section, Qt::Orientation orientation,
         int role ) const
{
    if ( orientation == Qt::Horizontal && role == Qt::DisplayRole )
        return rootItem -> data ( section );

    return QVariant ();
}

QModelIndex DiagnosticsTreeModel :: index (int row, int column,
            const QModelIndex &parent ) const
{
    if ( ! hasIndex ( row, column, parent ) )
        return QModelIndex ();

    DiagnosticsTreeItem *parentItem;

    if ( ! parent . isValid () )
        parentItem = rootItem;
    else
        parentItem = static_cast< DiagnosticsTreeItem * > ( parent . internalPointer () );

    DiagnosticsTreeItem *childItem = parentItem -> child ( row );
    if ( childItem )
        return createIndex ( row, column, childItem );
    else
        return QModelIndex ();
}

QModelIndex DiagnosticsTreeModel :: parent ( const QModelIndex &index ) const
{
    if ( ! index . isValid () )
        return QModelIndex ();

    DiagnosticsTreeItem *childItem = static_cast < DiagnosticsTreeItem * > ( index . internalPointer () );
    DiagnosticsTreeItem *parentItem = childItem -> parentItem ();

    if ( parentItem == rootItem )
        return QModelIndex ();

    return createIndex ( parentItem -> row (), 0, parentItem );
}

int DiagnosticsTreeModel :: rowCount ( const QModelIndex &parent ) const
{
    DiagnosticsTreeItem *parentItem;
    if ( parent . column () > 0 )
        return 0;

    if (! parent . isValid () )
        parentItem = rootItem;
    else
        parentItem = static_cast < DiagnosticsTreeItem * > ( parent . internalPointer () );

    return parentItem -> childCount ();
}

int DiagnosticsTreeModel :: columnCount ( const QModelIndex &parent ) const
{
    if ( parent . isValid () )
        return static_cast < DiagnosticsTreeItem * > ( parent .internalPointer () ) -> columnCount ();
    else
        return rootItem -> columnCount();
}

void DiagnosticsTreeModel :: setupModelData ( const QStringList &lines, DiagnosticsTreeItem *parent )
{
    QList < DiagnosticsTreeItem * > parents;
    QList < int > indentations;
    parents << parent;
    indentations << 0;

    int number = 0;

    while ( number < lines . count () )
    {
        int position = 0;
        while ( position < lines [number] . length () )
        {
            if ( lines [ number ] . at ( position ) != ' ' )
                break;
            position++;
        }

        QString lineData = lines [ number ] . mid ( position ) . trimmed ();

        if ( ! lineData . isEmpty () )
        {
            // Read the column data from the rest of the line.
            QStringList columnStrings = lineData . split( "\t", QString::SkipEmptyParts );
            QList < QVariant > columnData;
            for ( int column = 0; column < columnStrings . count (); ++column )
                columnData << columnStrings [ column ];

            if ( position > indentations.last () )
            {
                // The last child of the current parent is now the new parent
                // unless the current parent has no children.

                if ( parents . last () -> childCount () > 0)
                {
                    parents << parents . last () -> child ( parents . last () -> childCount() -1 );
                    indentations << position;
                }
            }
            else
            {
                while ( position < indentations . last () && parents . count() > 0 )
                {
                    parents . pop_back ();
                    indentations . pop_back ();
                }
            }

            // Append a new item to the current parent's list of children.
            parents . last () -> appendChild ( new DiagnosticsTreeItem ( columnData, parents . last () ) );
        }

        ++number;
    }
}
