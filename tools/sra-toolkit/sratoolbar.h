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
    void expanded ( bool );
    void toolSwitched ( int );

public slots:
    void expand ();
    void switchTool ( int tool );

private:

    void init ();
    void paintEvent ( QPaintEvent * );
    void updateButtonIcons ();

    bool isExpanded;

    QPushButton *home_button;
    QPushButton *config_button;
    QPushButton *diagnostics_button;
    QPushButton *expand_button;

};

#endif // SRATOOLBAR_H
