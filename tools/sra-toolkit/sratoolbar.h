#ifndef SRATOOLBAR_H
#define SRATOOLBAR_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QPushButton;
QT_END_NAMESPACE

class SRAToolBar : public QWidget
{
    Q_OBJECT
public:

    enum SRATool
    {
        t_home,
        t_diagnostics,
    };

    explicit SRAToolBar(QWidget *parent = nullptr);

signals:
    void toolSwitched ( int );

public slots:
    void switchTool ( int tool );

private:

    void init ();
    void paintEvent ( QPaintEvent * );

    QPushButton *home_button;
    QPushButton *diagnostics_button;

};

#endif // SRATOOLBAR_H
