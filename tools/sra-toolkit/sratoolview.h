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
    void expand ( bool );
    void toolChanged ( int );

private:

    void init ();
    void paintEvent ( QPaintEvent * );

    KConfig *config;

    QWidget *home;
    SRAConfigView *config_view;
    SRADiagnosticsView *diagnostics_view;

    QWidget *currentView;
};

#endif // SRATOOLVIEW_H
