#ifndef DIAGNOSTICSTREEITEM_H
#define DIAGNOSTICSTREEITEM_H

#include <QList>
#include <QVariant>

typedef enum
{
    diag_NotStarted,/* test not started yet */
    diag_Started,   /* test started */
    diag_Succeed,   /* test finished successfully */
    diag_Failed,    /* test finished with failure */
    diag_Skipped,   /* test execution was skipped */
    diag_Warning,   /* test finished successfully but has a warning for user */
    diag_Paused,    /* KDiagnosePause was called */
    diag_Resumed,   /* KDiagnoseResume was called */
    diag_Canceled,  /* KDiagnoseCancel was called */
} DiagnosticsTestState;


class DiagnosticsTreeItem
{
public:
    DiagnosticsTreeItem ( const QList < QVariant > &data, DiagnosticsTreeItem *parentItem = 0 );
    DiagnosticsTreeItem ( QString name, QString desc, uint32_t level, DiagnosticsTreeItem *parentItem = 0 );
    ~DiagnosticsTreeItem();

    void appendChild ( DiagnosticsTreeItem *child );

    DiagnosticsTreeItem *child ( int row );
    DiagnosticsTreeItem *parentItem ();

    int childCount () const;
    int columnCount () const;
    QVariant data ( int column ) const;
    int row () const;

    void setState ( DiagnosticsTestState state );

private:
    QList < DiagnosticsTreeItem * > m_childItems;
    QList < QVariant > m_itemData;
    DiagnosticsTreeItem *m_parentItem;

    uint32_t m_level;
    QString m_name;
    QString m_desc;
    DiagnosticsTestState m_state;
};

#endif // DIAGNOSTICSTREEITEM_H
