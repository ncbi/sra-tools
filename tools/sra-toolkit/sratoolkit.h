#ifndef SRATOOLKIT_H
#define SRATOOLKIT_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QHBoxLayout;
class QToolButton;
QT_END_NAMESPACE

class SRAToolBar;
class SRAToolView;

struct KConfig;

class SRAToolkit : public QMainWindow
{
    Q_OBJECT

public:
    SRAToolkit ( const QRect &avail_geometry, QWidget *parent = 0 );
    ~SRAToolkit ();

private:

    void init ();
    void init_menubar ();
    void init_view ();
    void paintEvent ( QPaintEvent * );

    QHBoxLayout *mainLayout;
    QWidget *mainWidget;

    KConfig *config;

    SRAToolBar *toolBar;
    SRAToolView *toolView;

};

#endif // SRATOOLKIT_H
