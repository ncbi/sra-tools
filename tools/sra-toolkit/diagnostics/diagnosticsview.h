#ifndef DIAGNOSTICSVIEW_H
#define DIAGNOSTICSVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
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

    void setup_view ();

    // Visual
    QVBoxLayout *self_layout;

    QTreeView *tree_view;

};

#endif // DIAGNOSTICSVIEW_H
