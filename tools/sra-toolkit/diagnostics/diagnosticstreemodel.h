#ifndef DIAGNOSTICSTREEMODEL_H
#define DIAGNOSTICSTREEMODEL_H

#include <QAbstractItemModel>

class DiagnosticsTreeItem;

class DiagnosticsTreeModel : public QAbstractItemModel
{
public:
    explicit DiagnosticsTreeModel ( const QString &data, QObject *parent = 0 );
    ~DiagnosticsTreeModel();

    QVariant data ( const QModelIndex &index, int role ) const override;
    Qt::ItemFlags flags ( const QModelIndex &index ) const override;
    QVariant headerData ( int section, Qt::Orientation orientation,
                          int role = Qt::DisplayRole ) const override;
    QModelIndex index (int row, int column,
                      const QModelIndex &parent = QModelIndex() ) const override;
    QModelIndex parent ( const QModelIndex &index ) const override;
    int rowCount ( const QModelIndex &parent = QModelIndex() ) const override;
    int columnCount ( const QModelIndex &parent = QModelIndex() ) const override;

private:
    void setupModelData ( const QStringList &lines, DiagnosticsTreeItem *parent );

    DiagnosticsTreeItem *rootItem;
};

#endif // DIAGNOSTICSTREEMODEL_H
