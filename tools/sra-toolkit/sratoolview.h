#ifndef SRATOOLVIEW_H
#define SRATOOLVIEW_H

#include <QWidget>

QT_BEGIN_NAMESPACE

QT_END_NAMESPACE

struct KConfig;
class SRAConfigView;
class SRADiagnosticsView;

class SRAToolView : public QWidget
{
    Q_OBJECT

public:

    explicit SRAToolView ( KConfig *p_config, QWidget *parent = nullptr );

signals:

public slots:
    void toolChanged ( int view );

private:

    void init ();

    KConfig *config;

    QWidget *currentView;

    SRAConfigView *home;
    SRADiagnosticsView *diagnostics;
};

#endif // SRATOOLVIEW_H
