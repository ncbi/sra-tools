#ifndef TESTWIDGET_H
#define TESTWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QButtonGroup;
QT_END_NAMESPACE

class TestWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TestWidget(QWidget *parent = nullptr);

signals:

public slots:

private:

    void setupScrollView ();

    QVBoxLayout *layout;
    QButtonGroup *bg_remote_access;
    QButtonGroup *bg_local_caching;
};

#endif // TESTWIDGET_H
