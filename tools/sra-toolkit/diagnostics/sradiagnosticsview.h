#ifndef DIAGNOSTICSVIEW_H
#define DIAGNOSTICSVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
QT_END_NAMESPACE

struct KConfig;
struct KDiagnose;
struct KDiagnoseTest;
class DiagnosticsTest;
class DiagnosticsThread;


class SRADiagnosticsView : public QWidget
{
    Q_OBJECT
public:
    explicit SRADiagnosticsView( KConfig *p_config, QWidget *parent = 0);
    ~SRADiagnosticsView ();

signals:

public slots:
    void handle_callback ( DiagnosticsTest *test );

private slots:
    void start_diagnostics ();
    void cancel_diagnostics ();

private:

    void init ();
    void init_metadata_view ();
    void init_tree_view ();
    void init_controls ();

    // Visual
    QVBoxLayout *self_layout;

    QTreeWidget *tree_view;
    QTreeWidgetItem *currentTest;

    QPushButton *start_button;
    QPushButton *cancel_button;

    // Data
    const KConfig *config;
    QString homeDir;

    // Event

    QThread *thread;
    DiagnosticsThread *worker;
};

#endif // DIAGNOSTICSVIEW_H
