#ifndef SRATOOLKITPREFERENCES_H
#define SRATOOLKITPREFERENCES_H

#include <QObject>

class SRAToolkitPreferences : public QObject
{
    Q_OBJECT
public:
    explicit SRAToolkitPreferences(QObject *parent = 0);

signals:

public slots:
};

#endif // SRATOOLKITPREFERENCES_H