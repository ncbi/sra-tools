#ifndef DIAGNOSTICSVIEW_H
#define DIAGNOSTICSVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QCheckBox;
class QTreeView;
QT_END_NAMESPACE

class DiagnosticsView : public QWidget
{
    Q_OBJECT
public:
    explicit DiagnosticsView(QWidget *parent = 0);
    ~DiagnosticsView ();

signals:

public slots:
private slots:
    void run_diagnostics ();

private:

    void init_metadata_view ();
    void init_test_view ();
    void setup_view ();

    // Visual
    QVBoxLayout *self_layout;

    // Interactive
    QCheckBox *cb_all;
    QCheckBox *cb_config;
    QCheckBox *cb_network;
    QCheckBox *cb_fail;

    QTreeView *tree_view;

};

#endif // DIAGNOSTICSVIEW_H
