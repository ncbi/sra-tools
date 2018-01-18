#ifndef DIAGNOSTICSTREEITEM_H
#define DIAGNOSTICSTREEITEM_H

#include <QTreeWidgetItem>

typedef enum
{
    DTS_NotStarted,  /* test not started yet */
    DTS_Started,   /* test started */
    DTS_Succeed,   /* test finished successfully */
    DTS_Failed,    /* test finished with failure */
    DTS_Skipped,   /* test execution was skipped */
    DTS_Warning,   /* test finished successfully but has a warning for user */
    DTS_Paused,    /* KDiagnosePause was called */
    DTS_Resumed,   /* KDiagnoseResume was called */
    DTS_Canceled,  /* KDiagnoseCancel was called */
} DiagnosticsTestState;

class DiagnosticsTest
{
public:
    DiagnosticsTest ( QString p_name );
    ~DiagnosticsTest();

    void appendChild ( DiagnosticsTest *p_child );
    int childCount () const;
    DiagnosticsTest *getChild ( int p_row );
    QString getDescription ();
    uint32_t getLevel ();
    QString getName();
    DiagnosticsTest *getParent ();
    uint32_t getState ();
    int row () const;
    void setDescription ( QString p_desc );
    void setLevel ( uint32_t p_level );
    void setParent ( DiagnosticsTest *p_parent );
    void setState ( uint32_t p_state );

private:
    QList < DiagnosticsTest * > childItems;
    QString desc;
    uint32_t level;
    QString name;
    DiagnosticsTest *parent;
    uint32_t state;
};

#endif // DIAGNOSTICSTREEITEM_H
