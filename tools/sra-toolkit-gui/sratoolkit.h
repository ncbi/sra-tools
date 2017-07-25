#ifndef SRATOOLKIT_H
#define SRATOOLKIT_H

#include <QMainWindow>

class DiagnosticsView;

class SRAToolkit : public QMainWindow
{
    Q_OBJECT

public:
    SRAToolkit ( const QRect &avail_geometry, QWidget *parent = 0 );
    ~SRAToolkit ();

private slots:

    void open_diagnostics ();

private:

    void setup_menubar ();

    DiagnosticsView *diagnostics;
};

#endif // SRATOOLKIT_H
