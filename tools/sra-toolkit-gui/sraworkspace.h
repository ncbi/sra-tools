#ifndef SRAWORKSPACE_H
#define SRAWORKSPACE_H

#include <QObject>

class SRAWorkspace : public QObject
{
    Q_OBJECT
public:

    QString getName () { return name; }

    SRAWorkspace ( const QString &w_name, QObject *parent = 0 );

signals:

public slots:

private:

    QString name;
};

#endif // SRAWORKSPACE_H
