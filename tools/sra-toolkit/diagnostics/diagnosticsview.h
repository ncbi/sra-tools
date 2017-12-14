#ifndef DIAGNOSTICSVIEW_H
#define DIAGNOSTICSVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QTreeView;
QT_END_NAMESPACE

class DiagnosticsTreeModel;

#include <diagnose/diagnose.h>

class DiagnosticsView : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsView(QWidget *parent = 0);
    ~DiagnosticsView ();

    void handle_results ( EKDiagTestState state, const KDiagnoseTest *test );

signals:

public slots:
private slots:
    void run_diagnostics ();


private:

    void setup_view ();

    // Visual
    QVBoxLayout *self_layout;

    QTreeView *tree_view;

    DiagnosticsTreeModel *model;
};

#endif // DIAGNOSTICSVIEW_H
